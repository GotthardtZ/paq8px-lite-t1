#ifndef PAQ8PX_SSE_HPP
#define PAQ8PX_SSE_HPP

#include "APM.hpp"
#include "APM1.hpp"
#include "ModelStats.hpp"
#include "Hash.hpp"

/**
 * Filter the context model with APMs
 */
class SSE {
private:
    const Shared *const shared;
    ModelStats *stats;
    struct {
        APM APMs[4];
        APM1 APM1s[3];
    } Text;
    struct {
        struct {
            APM APMs[4];
            APM1 APM1s[2];
        } Color, Palette;
        struct {
            APM APMs[3];
        } Gray;
    } Image;
    struct {
        APM1 APM1s[7];
    } Generic;

public:
    SSE(const Shared *sh, ModelStats *st);
    int p(int pr0);
};

#endif //PAQ8PX_SSE_HPP
