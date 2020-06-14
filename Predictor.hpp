#ifndef PAQ8PX_PREDICTOR_HPP
#define PAQ8PX_PREDICTOR_HPP

#include "file/FileDisk.hpp"
#include "DummyMixer.hpp"
#include "ModelStats.hpp"
#include "Models.hpp"
#include "SSE.hpp"
#include "Shared.hpp"
#include "UpdateBroadcaster.hpp"
#include "file/OpenFromMyFolder.hpp"
#include "model/ContextModel.hpp"
#include "model/ExeModel.hpp"
#include "model/NormalModel.hpp"
#include "model/WordModel.hpp"
#include "utils.hpp"

/**
 * A Predictor estimates the probability that the next bit of uncompressed data is 1.
 */
class Predictor {
private:
    static constexpr uint8_t asciiGroupC0[2][254] = {{0, 10, 0, 1, 10, 10, 0, 4, 2, 3, 10, 10, 10, 10, 0, 0, 5, 4, 2, 2, 3, 3, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 5, 5, 9, 4, 2, 2, 2, 2, 3, 3, 3, 3, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 5, 8, 8, 5, 9, 9, 6, 5, 2, 2, 2, 2, 2, 2, 2, 8, 3, 3, 3, 3, 3, 3, 3, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 8, 8, 8, 8, 8, 5, 5, 9, 9, 9, 9, 9, 7, 8, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 8, 8, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 8, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10},
                                                     {0, 6,  0, 1, 6,  6,  4, 5, 1, 1, 6,  6,  6,  6,  4, 0, 3, 2, 1, 1, 1, 1, 6,  6,  6,  6,  6,  6,  6,  6,  0, 4, 0, 0, 3, 3, 2, 5, 1, 1, 1, 1, 1, 1, 1, 1, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  0, 0, 4, 4, 0, 0, 0, 0, 3, 3, 3, 3, 2, 2, 5, 5, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6}};

    Shared *shared;
    ModelStats stats;
    Models models;
    ContextModel contextModel;
    SSE sse;
    int pr; // next prediction, scaled by 12 bits (0-4095)
    void trainText(const char *dictionary, int iterations);
    void trainExe();

public:
  Predictor(Shared* const sh);

    /**
     * Returns P(1) as a 12 bit number (0-4095).
     * @return the prediction
     */
    [[nodiscard]] auto p() const -> int;

    /**
     * Trains the models with the actual bit (0 or 1).
     * @param y the actual bit
     */
    void update(uint8_t y);
};

#endif //PAQ8PX_PREDICTOR_HPP
