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
public:
    Models(ModelStats *st);
    NormalModel &normalModel();
    DmcForest &dmcForest();
    CharGroupModel &charGroupModel();
    RecordModel &recordModel();
    SparseModel &sparseModel();
    MatchModel &matchModel();
    SparseMatchModel &sparseMatchModel();
    IndirectModel &indirectModel();
    TextModel &textModel();
    WordModel &wordModel();
    NestModel &nestModel();
    XMLModel &xmlModel();
    ExeModel &exeModel();
    static LinearPredictionModel &linearPredictionModel();
    JpegModel &jpegModel();
    Image24BitModel &image24BitModel();
    Image8BitModel &image8BitModel();
    Image4BitModel &image4BitModel();
    static Image1BitModel &image1BitModel();
#ifdef USE_AUDIOMODEL
    Audio8BitModel &audio8BitModel();
    Audio16BitModel &audio16BitModel();
#endif //USE_AUDIOMODEL
};

#endif //PAQ8PX_MODELS_HPP
