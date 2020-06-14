#include "Predictor.hpp"

Predictor::Predictor(Shared* const sh) : shared(sh), models(sh, &stats), contextModel(sh, &stats, models), sse(sh, &stats), pr(2048) {
  shared->reset();
  //initiate pre-training
  if((shared->options & OPTION_TRAINTXT) != 0U ) {
    trainText("english.dic", 3);
    trainText("english.exp", 1);
  }
  if((shared->options & OPTION_TRAINEXE) != 0U ) {
    trainExe();
  }
}

auto Predictor::p() const -> int { return pr; }

void Predictor::update(uint8_t y) {
  stats.misses += stats.misses + static_cast<unsigned long long>((pr >> 11U) != y);

  // update global context: pos, bitPosition, c0, c4, c8, buf
  shared->y = y;
  shared->update();

  INJECT_SHARED_bpos
  INJECT_SHARED_c0
  const uint8_t characterGroup = (bpos > 0) ? asciiGroupC0[0][(1U << bpos) - 2 + (c0 & ((1U << bpos) - 1))] : 0;
  stats.Text.characterGroup = characterGroup;

  // predict
  pr = contextModel.p();

  // SSE Stage
  pr = sse.p(pr);
}

void Predictor::trainText(const char *const dictionary, int iterations) {
  NormalModel &normalModel = models.normalModel();
  WordModel &wordModel = models.wordModel();
  DummyMixer mDummy(shared, 
    NormalModel::MIXERINPUTS + WordModel::MIXERINPUTS, 
    NormalModel::MIXERCONTEXTS + WordModel::MIXERCONTEXTS,
    NormalModel::MIXERCONTEXTSETS + WordModel::MIXERCONTEXTSETS);
  stats.blockType = TEXT;
  INJECT_SHARED_pos
  assert(pos == 0 && stats.blPos == 0);
  FileDisk f;
  printf("Pre-training models with text...");
  OpenFromMyFolder::anotherFile(&f, dictionary);
  int c = 0;
  int trainingByteCount = 0;
  while( iterations-- > 0 ) {
    f.setpos(0);
    c = SPACE;
    trainingByteCount = 0;
    do {
      trainingByteCount++;
      uint8_t c1 = c == NEW_LINE ? SPACE : c;
      if( c != CARRIAGE_RETURN ) {
        for( int bpos = 0; bpos < 8; bpos++ ) {
          normalModel.mix(mDummy); //update (train) model
#ifndef DISABLE_TEXTMODEL
          wordModel.mix(mDummy); //update (train) model
#endif
          mDummy.p();
          shared->y = (c1 >> (7 - bpos)) & 1U;
          shared->update();
        }
      }
      // emulate a space before and after each word/expression
      // reset models in between
      if( c == NEW_LINE ) {
        normalModel.reset();
#ifndef DISABLE_TEXTMODEL
        wordModel.reset();
#endif
        for( int bpos = 0; bpos < 8; bpos++ ) {
          normalModel.mix(mDummy); //update (train) model
#ifndef DISABLE_TEXTMODEL
          wordModel.mix(mDummy); //update (train) model
#endif
          mDummy.p();
          shared->y = (c1 >> (7 - bpos)) & 1U;
          shared->update();
        }
      }
    } while((c = f.getchar()) != EOF);
  }
  normalModel.reset();
#ifndef DISABLE_TEXTMODEL
  wordModel.reset();
#endif
  shared->reset();
  stats.reset();
  printf(" done [%s, %d bytes]\n", dictionary, trainingByteCount);
  f.close();
}

void Predictor::trainExe() {
  ExeModel &exeModel = models.exeModel();
  DummyMixer dummyM(shared, ExeModel::MIXERINPUTS, ExeModel::MIXERCONTEXTS, ExeModel::MIXERCONTEXTSETS);
  INJECT_SHARED_pos
  assert(pos == 0 && stats.blPos == 0);
  FileDisk f;
  printf("Pre-training x86/x64 model...");
  OpenFromMyFolder::myself(&f);
  int c = 0;
  int trainingByteCount = 0;
  do {
    trainingByteCount++;
    for( uint32_t bpos = 0; bpos < 8; bpos++ ) {
      exeModel.mix(dummyM); //update (train) model
      dummyM.p();
      shared->y = (c >> (7 - bpos)) & 1U;
      shared->update();
    }
  } while((c = f.getchar()) != EOF);
  printf(" done [%d bytes]\n", trainingByteCount);
  f.close();
  shared->reset();
  stats.reset();
}