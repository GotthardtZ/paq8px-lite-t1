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
#include "model/Audio8BitModel.hpp"
#include "model/Audio16BitModel.hpp"

/**
 * This is a factory class for lazy object creation for models.
 * Objects created within this class are instantiated on first use and guaranteed to be destroyed.
 */
class Models {
private:
    Shared *shared = Shared::getInstance();
    ModelStats *stats; //read-write
    uint32_t level {};
public:
    Models(ModelStats *st, uint32_t level) : stats(st), level(level) {}

public:
    NormalModel &normalModel() {
      static NormalModel instance {stats, MEM * 32};
      return instance;
    }

    DmcForest &dmcForest() {
      static DmcForest instance {MEM};
      return instance;
    }

    CharGroupModel &charGroupModel() {
      static CharGroupModel instance {MEM / 2};
      return instance;
    }

    RecordModel &recordModel() {
      static RecordModel instance {stats, MEM * 2};
      return instance;
    }

    SparseModel &sparseModel() {
      static SparseModel instance {MEM * 2};
      return instance;
    }

    MatchModel &matchModel() {
      static MatchModel instance {stats, MEM * 4, level};
      return instance;
    }

    SparseMatchModel &sparseMatchModel() {
      static SparseMatchModel instance {MEM};
      return instance;
    }

    IndirectModel &indirectModel() {
      static IndirectModel instance {MEM};
      return instance;
    }

#ifdef USE_TEXTMODEL

    TextModel &textModel() {
      static TextModel instance {stats, MEM * 16};
      return instance;
    }

    WordModel &wordModel() {
      static WordModel instance {stats, MEM * 16};
      return instance;
    }

#else

    WordModel &wordModel() {
      static WordModel instance {};
      return instance;
    }

#endif //USE_TEXTMODEL

    NestModel &nestModel() {
      static NestModel instance {MEM};
      return instance;
    }

    XMLModel &xmlModel() {
      static XMLModel instance {MEM / 4};
      return instance;
    }

    ExeModel &exeModel() {
      static ExeModel instance {stats, MEM * 4};
      return instance;
    }

    LinearPredictionModel &linearPredictionModel() {
      static LinearPredictionModel instance {};
      return instance;
    }

    JpegModel &jpegModel() {
      static JpegModel instance {MEM};
      return instance;
    }

    Image24BitModel &image24BitModel() {
      static Image24BitModel instance {stats, MEM * 4};
      return instance;
    }

    Image8BitModel &image8BitModel() {
      static Image8BitModel instance {stats, MEM * 4};
      return instance;
    }

    Image4BitModel &image4BitModel() {
      static Image4BitModel instance {MEM / 2};
      return instance;
    }

    Image1BitModel &image1BitModel() {
      static Image1BitModel instance {};
      return instance;
    }

#ifdef USE_AUDIOMODEL

    Audio8BitModel &audio8BitModel() {
      static Audio8BitModel instance {stats};
      return instance;
    }

    Audio16BitModel &audio16BitModel() {
      static Audio16BitModel instance {stats};
      return instance;
    }

#endif //USE_AUDIOMODEL
};

#endif //PAQ8PX_MODELS_HPP
