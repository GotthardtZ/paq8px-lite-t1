#ifndef PAQ8PX_MODELSTATS_HPP
#define PAQ8PX_MODELSTATS_HPP

#include "utils.hpp"

/**
 * Information shared by models
 * Order: in appearance -> models may use information from models that appears above them
 */
struct ModelStats {
    //General shared information
    BlockType blockType {}; //used by wordModel, recordModel, SSE stage
    uint32_t blpos {}; //relative position in block, used by many models
    uint64_t misses {}; //used by SSE stage

    //MatchModel
    struct {
        uint32_t length3; //used by SSE stage and RecordModel
        uint8_t expectedByte; //used by SSE stage
    } Match {};

    //NormalModel
    int order {};

    //image models
    struct {
        struct {
            uint8_t WW, W, NN, N, Wp1, Np1;
        } pixels; //used by SSE stage
        uint8_t plane; //used by SSE stage
        uint8_t ctx; //used by SSE stage
    } Image {};

    //AudioModel
    uint32_t wav {}; //used by recordModel
    uint8_t Audio {};

    //JpegModel
    //SparseMatchModel
    //SparseModel
    //CharGroupModel
    //WordModel
    //IndirectModel
    //Dmcforest
    //NestModel
    //XMLModel

    //TextModel
    struct {
        uint8_t characterGroup; //used by RecordModel, TextModel - Quantized partial byte as ASCII group
        uint8_t firstLetter; //used by SSE stage
        uint8_t mask; //used by SSE stage
    } Text {};

    //RecordModel
    //ExeModel

    void reset() {
      memset(this, 0, sizeof(ModelStats));
    }
};

#endif //PAQ8PX_MODELSTATS_HPP
