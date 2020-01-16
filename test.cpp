#include "Mixer.hpp"
#include "MixerFactory.hpp"
#include "ModelStats.hpp"
#include "model/NormalModel.hpp"
#include <cstdint>
#include "3rd/cxxopts.hpp"
#include "model/MatchModel.hpp"
#include "file/FileName.hpp"
#include "file/FileDisk.hpp"
//#include "Encoder.hpp"
//#include "file/fileUtils2.hpp"
//#include "filter/Filters.hpp"
#include <iostream>

using std::cout;
using std::endl;

cxxopts::ParseResult parse(int argc, char *argv[]) {
  try {
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

    if( result.count("input")) {
      std::cout << "Input = " << result["input"].as<std::string>() << std::endl;
    }

    if( result.count("output")) {
      std::cout << "Output = " << result["output"].as<std::string>() << std::endl;
    }

    return result;

  } catch( const cxxopts::OptionException &e ) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    exit(1);
  }
}

auto main(int argc, char *argv[]) -> int {
  auto shared = Shared::getInstance();
  shared->chosenSimd = SIMD_AVX2;
  shared->setLevel(9);
  FileName input;
  FileName archiveName;
  FileDisk archive;
  if( input.strsize() == 0 ) {
    input += argv[1];
    input.replaceSlashes();
  }
  archiveName += input.c_str();
  archiveName += ".test";
  archive.create(archiveName.c_str());
  archive.putChar(shared->level);
//  Encoder en(COMPRESS, &archive);
  FileName fn;
  fn += input.c_str();
  const char *fName = fn.c_str();
//  uint64_t fSize = getFileSize(fName);
//  if( !shared->toScreen ) //we need a minimal feedback when redirected
//    fprintf(stderr, "\nFilename: %s (%" PRIu64 " bytes)\n", fName, fSize);
//  printf("\nFilename: %s (%" PRIu64 " bytes)\n", fName, fSize);
//  compressfile(fName, fSize, en, true);

  auto result = parse(argc, argv);
  const auto &arguments = result.arguments();
  constexpr uint8_t ys[16] = {0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0};
  auto mf = new MixerFactory();

  auto m = mf->createMixer(1 + MatchModel::MIXERINPUTS, MatchModel::MIXERCONTEXTS, MatchModel::MIXERCONTEXTSETS);
  m->setScaleFactor(1024, 128);
  auto *modelStats = new ModelStats();
//  NormalModel normalModel(modelStats, shared->mem * 32);
  MatchModel matchModel(modelStats, shared->mem * 4);
  auto updateBroadcaster = UpdateBroadcaster::getInstance();
  auto programChecker = ProgramChecker::getInstance();

  shared->reset();
  shared->buf.setSize(shared->mem * 8);
  for( int i = 0; i < 256; ++i ) {
    for( auto &&y : ys ) {
      m->add(256); //network bias
      shared->y = y;
      shared->update();
//      normalModel.mix(*m);
//      normalModel.mixPost(*m);
      matchModel.mix(*m);
      auto p = m->p();
      printf("%d, %d\n", y, p);
      updateBroadcaster->broadcastUpdate();
    }
  }
  programChecker->print();
  return 1;
}