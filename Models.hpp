#ifndef PAQ8PX_MODELS_HPP
#define PAQ8PX_MODELS_HPP

#include "text/TextModel.hpp"
#include "model/NormalModel.hpp"
#include "model/DmcForest.hpp"
#include "ModelStats.hpp"
#include "model/CharGroupModel.hpp"
#include "model/RecordModel.hpp"
#include "model/SparseModel.hpp"
#include "model/MatchModel.hpp"
#include "model/SparseMatchModel.hpp"
#include "model/IndirectModel.hpp"
#include "model/NestModel.hpp"
#include "model/XMLModel.hpp"
#include "model/ExeModel.hpp"
#include "model/LinearPredictionModel.hpp"
#include "model/JpegModel.hpp"
#include "model/Image24BitModel.hpp"
#include "model/Image8BitModel.hpp"
#include "model/Image4BitModel.hpp"
#include "model/Image1BitModel.hpp"
#include "model/WordModel.hpp"

/**
 * This is a factory class for lazy object creation for models.
 * Objects created within this class are instantiated on first use and guaranteed to be destroyed.
 */
class Models {
private:
    const Shared *const shared; //read only
    ModelStats *stats; //read-write
    uint32_t level {};
public:
    Models(const Shared *const sh, ModelStats *st, uint32_t level) : shared(sh), stats(st), level(level) {}

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
      static MatchModel instance {shared, stats, MEM * 4, level};
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

    Image24BitModel &image24BitModel() {
      static Image24BitModel instance {shared, stats, MEM * 4};
      return instance;
    }

    Image8BitModel &image8BitModel() {
      static Image8BitModel instance {shared, stats, MEM * 4};
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
