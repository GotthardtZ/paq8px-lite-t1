#ifndef PAQ8PX_CONTEXTMODEL_HPP
#define PAQ8PX_CONTEXTMODEL_HPP

#include "../BlockType.hpp"
#include "../Mixer.hpp"
#include "../MixerFactory.hpp"
#include "../Models.hpp"

/**
 * This combines all the context models with a Mixer.
 */
class ContextModel {
    Shared * const shared;
    Models * const models;
    BlockType nextBlockType = BlockType::DEFAULT;
    BlockType blockType = BlockType::DEFAULT;
    int blockSize = 1;
    int blockInfo = 0;
    int bytesRead = 0;
    bool readSize = false;

public:
    ContextModel(Shared* const sh, Models* const models);
    int p();
};

#endif //PAQ8PX_CONTEXTMODEL_HPP
