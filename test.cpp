#include "Mixer.hpp"
#include "MixerFactory.hpp"
#include "ModelStats.hpp"
#include "model/ContextModel.hpp"
#include <cstdint>
#include "3rd/cxxopts.hpp"
#include "file/FileName.hpp"
#include "file/FileDisk.hpp"
#include "file/fileUtils2.hpp"
#include <iostream>

using std::cout;
using std::endl;

auto main(int argc, char *argv[]) -> int {
  cxxopts::Options options(argv[0]);

  uint8_t level = 0;

  options.addOptions()("h,help", "Print help");
  options.addOptions()("l,level", "compression level", cxxopts::value<uint8_t>(level)->defaultValue("0"));
  options.addOptions("Switches")("b,brute-force", "Brute-force detection of DEFLATE streams");
  options.addOptions("Switches")("e,exe-train", "Pre-train x86/x64 model");
  options.addOptions("Switches")("t,text-train", "Pre-train main model with word and expression list (english.dic, english.exp)");
  options.addOptions("Switches")("a,adaptive", "Adaptive learning rate");
  options.addOptions("Switches")("s,skip-color-transform", "Skip the color transform, just reorder the RGB channels");
  options.addOptions()("list", "List");
  options.addOptions()("test", "Test");
  options.addOptions()("d,decompress", "Decompress");
  options.addOptions()("v,verbose", "Print more detailed (verbose) information to screen.");
  options.addOptions()("simd", "Overrides detected SIMD instruction set for neural network operations. One of `NONE`, `SSE2`, or `AVX2`.",
                       cxxopts::value<SIMD>());
  options.addOptions()("log",
                       "Logs (appends) compression results in the specified tab separated LOGFILE. Logging is only applicable for compression.",
                       cxxopts::value<std::string>());
  options.addOptions()("i,input", "Input file", cxxopts::value<std::string>());
  options.addOptions()("o,output", "Output file", cxxopts::value<std::string>());

//    options.parsePositional({"input", "output"});

  auto result = options.parse(argc, argv);

  if( result.count("help")) {
    cout << options.help({"", "Switches"}) << endl;
    exit(0);
  }

  const auto &arguments = result.arguments();
  auto shared = Shared::getInstance();
  shared->chosenSimd = SIMD_AVX2;
  shared->setLevel(result["level"].as<uint8_t>());
  FileName input;
  if( input.strsize() == 0 ) {
    input += argv[1];
    input.replaceSlashes();
  }
  FileDisk f;
  f.open(input.c_str(), true);
  uint64_t fSize = getFileSize(input.c_str());
  printf("\nFilename: %s (%" PRIu64 " bytes)\n", input.c_str(), fSize);
  //  FileName archiveName;
//  FileDisk archive;
//  archiveName += input.c_str();
//  archiveName += ".test";
//  archive.create(archiveName.c_str());
//  archive.putChar(shared->level);
//  Encoder en(COMPRESS, &archive);
//  FileName fn;
//  fn += input.c_str();
//  const char *fName = fn.c_str();
  auto mf = new MixerFactory();

//  constexpr int mixerInputs = 1 + MatchModel::MIXERINPUTS;// + NormalModel::MIXERINPUTS;
//  constexpr int mixerContexts = MatchModel::MIXERCONTEXTS;// + NormalModel::MIXERCONTEXTS;
//  constexpr int mixerContextSets = MatchModel::MIXERCONTEXTSETS;// + NormalModel::MIXERCONTEXTSETS;

//  auto m = mf->createMixer(mixerInputs, mixerContexts, mixerContextSets);
//  m->setScaleFactor(1024, 128);
  auto *modelStats = new ModelStats();
  auto *models = new Models(modelStats);
  ContextModel contextModel(modelStats, *models);
//  MatchModel matchModel(modelStats, shared->mem * 4);
//  NormalModel normalModel(modelStats, shared->mem * 32);
//  Audio16BitModel audio16BitModel(modelStats);
//  TextModel textModel(modelStats, shared->mem * 16);
//  WordModel wordModel(modelStats, shared->mem * 16);
  auto updateBroadcaster = UpdateBroadcaster::getInstance();
  auto programChecker = ProgramChecker::getInstance();

  shared->reset();
  shared->buf.setSize(shared->mem * 8);
  int c;
  uint8_t y;
  auto results = (uint16_t *) malloc(8 * fSize * sizeof(uint16_t));
  auto ys = (uint8_t *) malloc(8 * fSize * sizeof(uint8_t));
  uint64_t position = 0;
  for( int j = 0; j < fSize; ++j ) {
    c = f.getchar();
    for( int i = 7; i >= 0; --i ) {
      y = (c >> i) & 1U;
//      m->add(256);
      shared->y = y;
      shared->update();
//      matchModel.mix(*m);
//      normalModel.mix(*m);
//      audio16BitModel.mix(*m);
//      textModel.mix(*m);
//      wordModel.mix(*m);
//      auto p = m->p();
      auto p = contextModel.p();
      results[position] = p;
      ys[position] = y;
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

  uint64_t sum = 0;
  for( uint64_t i = 0; i < position; ++i ) {
    y = ys[i];
    uint16_t target = y == 0 ? 0 : 4095;
    sum += abs(target - results[i]);
    printf("%d, %d\n", y, results[i]);
  }
  printf("(%llu - %llu) %f\n", 0ULL, position, double(sum) / double(position));
  programChecker->print();
  return 1;
}