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
    const Shared * const shared;
    ModelStats *stats;
    Models models;
    Mixer *m;
    BlockType nextBlockType = DEFAULT;
    BlockType blockType = DEFAULT;
    int blockSize = 0;
    int blockInfo = 0;
    int bytesRead = 0;
    bool readSize = false;

public:
    ContextModel(const Shared* const sh, ModelStats *st, Models &models);
    auto p() -> int;
    ~ContextModel();
};

#endif //PAQ8PX_CONTEXTMODEL_HPP
