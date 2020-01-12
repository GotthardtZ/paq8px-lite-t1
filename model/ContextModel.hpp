#ifndef PAQ8PX_CONTEXTMODEL_HPP
#define PAQ8PX_CONTEXTMODEL_HPP

#include "../text/TextModel.hpp"
#include "../Mixer.hpp"
#include "../MixerFactory.hpp"
#include "../ModelStats.hpp"
#include "../Models.hpp"

/**
 * This combines all the context models with a Mixer.
 */
class ContextModel {
    Shared *shared = Shared::getInstance();
    ModelStats *stats;
    Models models;
    Mixer *m;
    BlockType nextBlockType = DEFAULT, blockType = DEFAULT;
    int blockSize = 0, blockInfo = 0, bytesRead = 0;
    bool readSize = false;

public:
    ContextModel(ModelStats *st, Models &models);
    auto p() -> int;
    ~ContextModel();
};

#endif //PAQ8PX_CONTEXTMODEL_HPP
