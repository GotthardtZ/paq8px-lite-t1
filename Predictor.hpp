#ifndef PAQ8PX_PREDICTOR_HPP
#define PAQ8PX_PREDICTOR_HPP

#include "file/FileDisk.hpp"
#include "Shared.hpp"
#include "ModelStats.hpp"
#include "Models.hpp"
#include "model/ContextModel.hpp"
#include "SSE.hpp"
#include "model/NormalModel.hpp"
#include "model/WordModel.hpp"
#include "DummyMixer.hpp"
#include "file/OpenFromMyFolder.hpp"
#include "UpdateBroadcaster.hpp"
#include "model/ExeModel.hpp"
#include "utils.hpp"

/**
 * A Predictor estimates the probability that the next bit of uncompressed data is 1.
 */
class Predictor {
    ModelStats stats;
    Models models;
    ContextModel contextModel;
    SSE sse;
    int pr; // next prediction, scaled by 12 bits (0-4095)
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();
    Shared *shared = Shared::getInstance();
    void trainText(const char *dictionary, int iterations);
    void trainExe();

public:
    Predictor();

    /**
     * returns P(1) as a 12 bit number (0-4095).
     * @return the prediction
     */
    [[nodiscard]] int p() const;

    /**
     * Trains the models with the actual bit (0 or 1).
     * @param y the actual bit
     */
    void update(uint8_t y);
};

#endif //PAQ8PX_PREDICTOR_HPP
