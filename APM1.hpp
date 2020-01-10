#ifndef PAQ8PX_APM1_HPP
#define PAQ8PX_APM1_HPP

#include <cstdint>
#include <cassert>
#include "Shared.hpp"
#include "UpdateBroadcaster.hpp"
#include "Squash.hpp"
#include "Stretch.hpp"

/**
 * APM1 maps a probability and a context into a new probability that bit y will next be 1.
 * After each guess it updates its state to improve future guesses.
 * Probabilities are scaled 12 bits (0-4095).
 */
class APM1 : IPredictor {
private:
    const Shared *const shared;
    int index; // last p, context
    const int n; // number of contexts
    Array<uint16_t> t; // [n][33]:  p, context -> p
    const int rate;
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();

public:
    /**
     * APM1 a(sh, n, r) creates an getInstance with n contexts and learning rate r.
     * rate determines the learning rate. smaller = faster, must be in the range (0, 32).
     * Uses 66*n bytes memory.
     * @param sh
     * @param n the number of contexts
     * @param r the learning rate
     */
    APM1(const Shared *sh, int n, int r);
    /**
     * a.p(pr, cx) returns adjusted probability in context cx (0 to n-1).
     * @param pr initial (pre-adjusted) probability
     * @param cxt the context
     * @return adjusted probability
     */
    int p(int pr, int cxt);
    void update() override;
};

#endif //PAQ8PX_APM1_HPP
