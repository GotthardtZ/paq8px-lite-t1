cmake_minimum_required(VERSION 3.14)
project(paq8px)

find_package(ZLIB)
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

add_executable(paq8px ProgramChecker.cpp paq8px.cpp MTFList.cpp Random.cpp String.cpp Predictor.cpp Models.cpp model/ExeModel.cpp APM1.cpp model/Image1BitModel.cpp model/Image4BitModel.cpp model/SparseModel.cpp Ilog.cpp model/ContextModel.cpp SSE.cpp UpdateBroadcaster.cpp model/Audio8BitModel.cpp Shared.cpp text/TextModel.cpp file/File.cpp file/FileDisk.cpp file/FileName.cpp file/FileTmp.cpp file/ListOfFiles.cpp file/OpenFromMyFolder.cpp model/Audio16BitModel.cpp model/AudioModel.cpp model/CharGroupModel.cpp model/DmcForest.cpp model/DmcModel.cpp model/DmcNode.cpp model/Image24BitModel.cpp model/IndirectModel.cpp model/JpegModel.cpp model/LinearPredictionModel.cpp model/MatchModel.cpp model/NestModel.cpp model/NormalModel.cpp model/RecordModel.cpp model/SparseMatchModel.cpp model/WordModel.cpp model/XMLModel.cpp text/English.cpp text/EnglishStemmer.cpp text/French.cpp text/FrenchStemmer.cpp text/German.cpp text/GermanStemmer.cpp text/Stemmer.cpp text/Word.cpp text/WordEmbeddingDictionary.cpp StationaryMap.cpp StateTable.cpp StateMap.cpp SmallStationaryContextMap.cpp ModelStats.cpp MixerFactory.cpp Mixer.cpp IndirectMap.cpp DummyMixer.cpp ContextMap2.cpp ContextMap.cpp APM.cpp AdaptiveMap.cpp filter/LZWDictionary.cpp filter/TextParserStateInfo.cpp)
add_executable(test test.cpp Mixer.cpp MixerFactory.cpp ContextMap2.cpp ProgramChecker.cpp UpdateBroadcaster.cpp Random.cpp StateMap.cpp StateTable.cpp AdaptiveMap.cpp Shared.cpp Ilog.cpp model/NormalModel.cpp file/FileDisk.cpp file/OpenFromMyFolder.cpp String.cpp)

if (supported)
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET paq8px PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else ()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif ()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-O3 -march=native -mtune=native -fno-stack-protector -Wno-c++98-compat -Wno-sign-conversion -Wno-padded -Wno-shadow-field-in-constructor -Wno-implicit-int-conversion -Wno-c++98-compat-pedantic -Wno-newline-eof -Wno-c++17-extensions -Wno-cast-align -Wno-shadow-field -Wno-unused-private-field -Wno-unused-function -Wno-undefined-func-template -Wno-weak-vtables -Wno-comma -Wno-implicit-fallthrough -Wno-shadow -Wno-old-style-cast -Wno-missing-braces -Wno-documentation -Wno-shorten-64-to-32 -Wno-vla-extension -Wno-sign-compare -Wno-cast-qual -Wno-global-constructors -Wno-missing-variable-declarations -Wno-unused-parameter -Wno-overloaded-virtual -Wno-vla -Wno-exit-time-destructors -Wno-unused-macros -Wno-missing-prototypes -Wno-implicit-int-float-conversion -Wno-char-subscripts -Wno-double-promotion -Wno-switch-enum -Wno-shift-sign-overflow")
add_definitions(-DUSE_TEXTMODEL -DUSE_AUDIOMODEL -DUSE_ZLIB -DNVERBOSE -DNDEBUG)
#target_compile_options(paq8px PRIVATE
#        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
#        -Wall -Wextra -Weverything>
#        $<$<CXX_COMPILER_ID:MSVC>:
#        /W4>
#        $<$<CXX_COMPILER_ID:GNU>:
#        -Wall -Wextra>)

target_link_libraries(paq8px ${ZLIB_LIBRARIES})