#include "Models.hpp"

NormalModel &Models::normalModel() {
  static NormalModel instance {stats, MEM * 32};
  return instance;
}

DmcForest &Models::dmcForest() {
  static DmcForest instance {MEM};
  return instance;
}

CharGroupModel &Models::charGroupModel() {
  static CharGroupModel instance {MEM / 2};
  return instance;
}

RecordModel &Models::recordModel() {
  static RecordModel instance {stats, MEM * 2};
  return instance;
}

SparseModel &Models::sparseModel() {
  static SparseModel instance {MEM * 2};
  return instance;
}

MatchModel &Models::matchModel() {
  static MatchModel instance {stats, MEM * 4, level};
  return instance;
}

SparseMatchModel &Models::sparseMatchModel() {
  static SparseMatchModel instance {MEM};
  return instance;
}

IndirectModel &Models::indirectModel() {
  static IndirectModel instance {MEM};
  return instance;
}

#ifdef USE_TEXTMODEL

TextModel &Models::textModel() {
  static TextModel instance {stats, MEM * 16};
  return instance;
}

WordModel &Models::wordModel() {
  static WordModel instance {stats, MEM * 16};
  return instance;
}

#else

WordModel &wordModel() {
      static WordModel instance {};
      return instance;
    }

#endif //USE_TEXTMODEL

NestModel &Models::nestModel() {
  static NestModel instance {MEM};
  return instance;
}

XMLModel &Models::xmlModel() {
  static XMLModel instance {MEM / 4};
  return instance;
}

ExeModel &Models::exeModel() {
  static ExeModel instance {stats, MEM * 4};
  return instance;
}

LinearPredictionModel &Models::linearPredictionModel() {
  static LinearPredictionModel instance {};
  return instance;
}

JpegModel &Models::jpegModel() {
  static JpegModel instance {MEM};
  return instance;
}

Image24BitModel &Models::image24BitModel() {
  static Image24BitModel instance {stats, MEM * 4};
  return instance;
}

Image8BitModel &Models::image8BitModel() {
  static Image8BitModel instance {stats, MEM * 4};
  return instance;
}

Image4BitModel &Models::image4BitModel() {
  static Image4BitModel instance {MEM / 2};
  return instance;
}

Image1BitModel &Models::image1BitModel() {
  static Image1BitModel instance {};
  return instance;
}

#ifdef USE_AUDIOMODEL

Audio8BitModel &Models::audio8BitModel() {
  static Audio8BitModel instance {stats};
  return instance;
}

Audio16BitModel &Models::audio16BitModel() {
  static Audio16BitModel instance {stats};
  return instance;
}

#endif //USE_AUDIOMODEL