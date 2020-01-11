#ifndef PAQ8PX_APM_HPP
#define PAQ8PX_APM_HPP

#include "AdaptiveMap.hpp"
#include "Squash.hpp"
#include "Stretch.hpp"
#include "UpdateBroadcaster.hpp"

/**
 * An APM maps a probability and a context to a new probability.
 */
class APM : public AdaptiveMap {
private:
    const int N; // Number of contexts
    const int steps;
    int cxt; // context index of last prediction
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();
public:
    /**
     * APM a(n,STEPS) creates with n contexts using 4*STEPS*n bytes memory.
     * @param n the number of contexts
     * @param s the number of steps
     */
    APM(int n, int s);

    /**
     * a.update() updates probability map. y=(0..1) is the last bit
     */
    void update() override;

    /**
     * a.p(pr, cx, limit) returns a new probability (0..4095) like with StateMap.
     * cx=(0..n-1) is the context.
     * pr=(0..4095) is considered part of the context.
     * The output is computed by interpolating pr into STEPS ranges nonlinearly
     * with smaller ranges near the ends.  The initial output is pr.
     * limit=(0..1023): set a lower limit (like 255) for faster adaptation.
     * @param pr the initial probability. Considered part of the context.
     * @param cx the context
     * @param lim limit; lower limit means faster adaptation
     * @return the adjusted probability
     */
    int p(int pr, int cx, int lim);
};

#endif //PAQ8PX_APM_HPP
