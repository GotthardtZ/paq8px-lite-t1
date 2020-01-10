#ifndef PAQ8PX_CONTEXTMODEL_HPP
#define PAQ8PX_CONTEXTMODEL_HPP

#include "../text/TextModel.hpp"
#include "../ModelStats.hpp"
#include "../MixerFactory.hpp"
#include "../Mixer.hpp"
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
    uint32_t level{};

public:
    ContextModel(ModelStats *st, Models &models, uint32_t level);
    int p();
    ~ContextModel();
};
#endif //PAQ8PX_CONTEXTMODEL_HPP
