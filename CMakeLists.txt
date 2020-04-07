cmake_minimum_required(VERSION 3.10)
project(paq8px)

find_package(ZLIB)
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

option(USE_TEXTMODEL "Whether or not to use the TextModel and WordModel" ON)
option(USE_AUDIOMODEL "Whether or not to use the AudioModels" ON)
option(USE_ZLIB "Whether or not to use zlib" ON)
option(NDEBUG "Whether to suppress debug info" ON)
option(NVERBOSE "Whether to suppress verbose output" ON)
option(NHASHCONFIG "Whether to support custom hash configuration" ON)

if (USE_TEXTMODEL)
    add_definitions(-DUSE_TEXTMODEL)
endif (USE_TEXTMODEL)

if (USE_AUDIOMODEL)
    add_definitions(-DUSE_AUDIOMODEL)
endif (USE_AUDIOMODEL)

if (USE_ZLIB)
    add_definitions(-DUSE_ZLIB)
endif (USE_ZLIB)

if (NDEBUG)
    add_definitions(-DNDEBUG)
endif (NDEBUG)

if (NVERBOSE)
    add_definitions(-DNVERBOSE)
endif (NVERBOSE)

if (NHASHCONFIG)
    add_definitions(-DNHASHCONFIG)
endif (NHASHCONFIG)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-O3 -march=native -mtune=native -fno-stack-protector")

add_executable(paq8px ProgramChecker.cpp paq8px.cpp MTFList.cpp Random.cpp String.cpp Predictor.cpp Models.cpp model/ExeModel.cpp APM1.cpp model/Image1BitModel.cpp model/Image4BitModel.cpp model/SparseModel.cpp Ilog.cpp model/ContextModel.cpp SSE.cpp UpdateBroadcaster.cpp model/Audio8BitModel.cpp Shared.cpp text/TextModel.cpp file/File.cpp file/FileDisk.cpp file/FileName.cpp file/FileTmp.cpp file/ListOfFiles.cpp file/OpenFromMyFolder.cpp model/Audio16BitModel.cpp model/AudioModel.cpp model/CharGroupModel.cpp model/DmcForest.cpp model/DmcModel.cpp model/DmcNode.cpp model/Image24BitModel.cpp model/IndirectModel.cpp model/JpegModel.cpp model/LinearPredictionModel.cpp model/MatchModel.cpp model/NestModel.cpp model/NormalModel.cpp model/RecordModel.cpp model/SparseMatchModel.cpp model/WordModel.cpp model/XMLModel.cpp text/English.cpp text/EnglishStemmer.cpp text/French.cpp text/FrenchStemmer.cpp text/German.cpp text/GermanStemmer.cpp text/Stemmer.cpp text/Word.cpp text/WordEmbeddingDictionary.cpp StationaryMap.cpp StateTable.cpp StateMap.cpp SmallStationaryContextMap.cpp ModelStats.cpp MixerFactory.cpp Mixer.cpp IndirectMap.cpp DummyMixer.cpp ContextMap2.cpp ContextMap.cpp APM.cpp AdaptiveMap.cpp filter/LZWDictionary.cpp filter/TextParserStateInfo.cpp model/Image8BitModel.cpp Encoder.cpp model/Info.cpp)
add_executable(experiment test.cpp ProgramChecker.cpp MTFList.cpp Random.cpp String.cpp Predictor.cpp Models.cpp model/ExeModel.cpp APM1.cpp model/Image1BitModel.cpp model/Image4BitModel.cpp model/SparseModel.cpp Ilog.cpp model/ContextModel.cpp SSE.cpp UpdateBroadcaster.cpp model/Audio8BitModel.cpp Shared.cpp text/TextModel.cpp file/File.cpp file/FileDisk.cpp file/FileName.cpp file/FileTmp.cpp file/ListOfFiles.cpp file/OpenFromMyFolder.cpp model/Audio16BitModel.cpp model/AudioModel.cpp model/CharGroupModel.cpp model/DmcForest.cpp model/DmcModel.cpp model/DmcNode.cpp model/Image24BitModel.cpp model/IndirectModel.cpp model/JpegModel.cpp model/LinearPredictionModel.cpp model/MatchModel.cpp model/NestModel.cpp model/NormalModel.cpp model/RecordModel.cpp model/SparseMatchModel.cpp model/WordModel.cpp model/XMLModel.cpp text/English.cpp text/EnglishStemmer.cpp text/French.cpp text/FrenchStemmer.cpp text/German.cpp text/GermanStemmer.cpp text/Stemmer.cpp text/Word.cpp text/WordEmbeddingDictionary.cpp StationaryMap.cpp StateTable.cpp StateMap.cpp SmallStationaryContextMap.cpp ModelStats.cpp MixerFactory.cpp Mixer.cpp IndirectMap.cpp DummyMixer.cpp ContextMap2.cpp ContextMap.cpp APM.cpp AdaptiveMap.cpp filter/LZWDictionary.cpp filter/TextParserStateInfo.cpp model/Image8BitModel.cpp Encoder.cpp model/Info.cpp)
#add_executable(train_bench bench/train.cpp)
#add_executable(paq8pxd paq8pxd.cpp)

if (supported)
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET paq8px PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET experiment PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    #    set_property(TARGET train_bench PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    #    set_property(TARGET paq8pxd PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else ()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif ()
#target_compile_options(paq8px PRIVATE
#        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
#        -Wall -Wextra -Weverything>
#        $<$<CXX_COMPILER_ID:MSVC>:
#        /W4>
#        $<$<CXX_COMPILER_ID:GNU>:
#        -Wall -Wextra>)

target_link_libraries(paq8px ${ZLIB_LIBRARIES})
#target_link_libraries(paq8pxd ${ZLIB_LIBRARIES})