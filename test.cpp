#include "Mixer.hpp"
#include "MixerFactory.hpp"
#include "ModelStats.hpp"
#include "file/FileDisk.hpp"
#include "file/FileName.hpp"
#include "file/fileUtils2.hpp"
#include "model/ContextModel.hpp"
#include <cstdint>
#include "Models.hpp"

auto main(int argc, char *argv[]) -> int {
  auto shared = Shared::getInstance();
  shared->chosenSimd = SIMD_AVX2;
  shared->setLevel(9);
  FileName input;
  if( input.strsize() == 0 ) {
    input += argv[1];
    input.replaceSlashes();
  }
  FileDisk f;
  f.open(input.c_str(), true);
  uint64_t fSize = getFileSize(input.c_str());
  auto mf = new MixerFactory();

  constexpr int mixerInputs = 1 + MatchModel::MIXERINPUTS;// + NormalModel::MIXERINPUTS;
  constexpr int mixerContexts = MatchModel::MIXERCONTEXTS;// + NormalModel::MIXERCONTEXTS;
  constexpr int mixerContextSets = MatchModel::MIXERCONTEXTSETS;// + NormalModel::MIXERCONTEXTSETS;
//
  auto m = mf->createMixer(mixerInputs, mixerContexts, mixerContextSets);
  m->setScaleFactor(1024, 128);
  auto *modelStats = new ModelStats();
//  auto *models = new Models(modelStats);
//  ContextModel contextModel(modelStats, *models);
  MatchModel matchModel(modelStats, shared->mem * 4);
//  NormalModel normalModel(modelStats, shared->mem * 32);
//  Audio16BitModel audio16BitModel(modelStats);
//  TextModel textModel(modelStats, shared->mem * 16);
//  WordModel wordModel(modelStats, shared->mem * 16);
  auto updateBroadcaster = UpdateBroadcaster::getInstance();
  auto programChecker = ProgramChecker::getInstance();

  shared->reset();
  shared->buf.setSize(shared->mem * 8);
  int c = 0;
  uint8_t y = 0;
//  auto results = static_cast<uint16_t *>(malloc(8 * fSize * sizeof(uint16_t)));
//  auto ys = static_cast<uint8_t *>(malloc(8 * fSize * sizeof(uint8_t)));
  uint64_t position = 0;
  for( int j = 0; j < fSize; ++j ) {
    c = f.getchar();
    for( int i = 7; i >= 0; --i ) {
//      auto p = contextModel.p();
      matchModel.mix(*m);
      auto p = m->p();
//      results[position] = p;
      y = (c >> i) & 1U;
      static FILE *dbg = fopen("log.txt", "wb");
      fprintf(dbg, "%i %i\n", y, p);

//      m->add(256);
//      normalModel.mix(*m);
//      audio16BitModel.mix(*m);
//      textModel.mix(*m);
//      wordModel.mix(*m);

      shared->y = y;
      shared->update();
//      ys[position] = y;
//      printf("%llu: %d, %d\n", position, y, p);
      ++position;
      updateBroadcaster->broadcastUpdate();
    }
//    constexpr int printInterval = 1024 * 100;
//    if( position % printInterval == 0 ) {
//      uint64_t sum = 0;
//      for( uint64_t i = position - printInterval; i < position; ++i ) {
//        y = ys[i];
//        uint16_t target = y << 12U;
//        sum += abs(target - results[i]);
//      }
//      printf("(%llu - %llu) %f\n", position - printInterval, position, double(sum) / double(printInterval));
//    }
  }

//  uint64_t sum = 0;
//  for( uint64_t i = 0; i < position; ++i ) {
//    y = ys[i];
//    uint16_t target = y == 0 ? 0 : 4095;
//    printf("%llu, %d, %d\n", i, target, results[i]);
//    sum += abs(target - results[i]);
//    printf("%d, %d\n", y, results[i]);
//  }
//  printf("(%llu - %llu) %f\n", 0ULL, position, double(sum) / double(position));
  programChecker->print();
  return 1;
}