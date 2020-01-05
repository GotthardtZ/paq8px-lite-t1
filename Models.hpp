#ifndef PAQ8PX_MODELS_HPP
#define PAQ8PX_MODELS_HPP

#include "text/TextModel.hpp"

// this is a factory class for lazy object creation for models
// objects created within this class are instantiated on first use and guaranteed to be destroyed
class Models {
private:
    const Shared *const shared; //read only
    ModelStats *stats; //read-write
public:
    Models(const Shared *const sh, ModelStats *st) : shared(sh), stats(st) {}

public:
    NormalModel &normalModel() {
      static NormalModel instance {shared, stats, MEM * 32};
      return instance;
    }

    DmcForest &dmcForest() {
      static DmcForest instance {shared, MEM};
      return instance;
    }

    CharGroupModel &charGroupModel() {
      static CharGroupModel instance {shared, MEM / 2};
      return instance;
    }

    RecordModel &recordModel() {
      static RecordModel instance {shared, stats, MEM * 2};
      return instance;
    }

    SparseModel &sparseModel() {
      static SparseModel instance {shared, MEM * 2};
      return instance;
    }

    MatchModel &matchModel() {
      static MatchModel instance {shared, stats, MEM * 4};
      return instance;
    }

    SparseMatchModel &sparseMatchModel() {
      static SparseMatchModel instance {shared, MEM};
      return instance;
    }

    IndirectModel &indirectModel() {
      static IndirectModel instance {shared, MEM};
      return instance;
    }

#ifdef USE_TEXTMODEL

    TextModel &textModel() {
      static TextModel instance {shared, stats, MEM * 16};
      return instance;
    }

    WordModel &wordModel() {
      static WordModel instance {shared, stats, MEM * 16};
      return instance;
    }

#else
    WordModel &wordModel() {
      static WordModel instance {};
      return instance;
    }
#endif //USE_TEXTMODEL

    NestModel &nestModel() {
      static NestModel instance {shared, MEM};
      return instance;
    }

    XMLModel &xmlModel() {
      static XMLModel instance {shared, MEM / 4};
      return instance;
    }

    ExeModel &exeModel() {
      static ExeModel instance {shared, stats, MEM * 4};
      return instance;
    }

    LinearPredictionModel &linearPredictionModel() {
      static LinearPredictionModel instance {shared};
      return instance;
    }

    JpegModel &jpegModel() {
      static JpegModel instance {shared, MEM};
      return instance;
    }

    Image24bitModel &image24BitModel() {
      static Image24bitModel instance {shared, stats, MEM * 4};
      return instance;
    }

    Image8bitModel &image8BitModel() {
      static Image8bitModel instance {shared, stats, MEM * 4};
      return instance;
    }

    Image4BitModel &image4BitModel() {
      static Image4BitModel instance {shared, MEM / 2};
      return instance;
    }

    Image1BitModel &image1BitModel() {
      static Image1BitModel instance {shared};
      return instance;
    }

#ifdef USE_AUDIOMODEL

    Audio8BitModel &audio8BitModel() {
      static Audio8BitModel instance {shared, stats};
      return instance;
    }

    Audio16BitModel &audio16BitModel() {
      static Audio16BitModel instance {shared, stats};
      return instance;
    }

#endif //USE_AUDIOMODEL
};

#endif //PAQ8PX_MODELS_HPP
