#ifndef PAQ8PX_NORMALMODEL_HPP
#define PAQ8PX_NORMALMODEL_HPP

#include "../ContextMap2.hpp"

/**
 * Model for order 0-14 contexts
 * Contexts are hashes of previous 0..14 bytes.
 * Order 0..6, 8 and 14 are used for prediction.
 * Note: order 7+ contexts are modeled by matchModel as well.
 */
class NormalModel {
private:
    static constexpr int nCM = ContextMap2::C; // 8
    static constexpr int nSM = 8;
    Shared * const shared;
    uint64_t utf8c1{};
    uint64_t utf8c2{};
    uint64_t utf8c3{};
    uint64_t utf8c4{};
    uint64_t utf8c5{};
    uint64_t utf8c6{};
    uint64_t utf8c7{};
    uint64_t wordhash{};
    uint64_t gaphash{};
    uint64_t utf8hash{};
    uint8_t utf8left{};
    uint8_t type{};
    uint8_t lasttokentype{};
public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap2::MIXERINPUTS) + nSM; // 32
    static constexpr int MIXERCONTEXTS =
      (ContextMap2::C + 1) * 8 * 7 + //504
      255 * 8 * 7 + //14280
      (ContextMap2::C + 1) * 2 * 2 * 256 + //9216
      6561 // 3^8
    ; // 30561
    static constexpr int MIXERCONTEXTSETS = 4;
    NormalModel(Shared* const sh, const uint64_t cmSize);

    ContextMap2 cm;
    StateMap smOrder0;
    StateMap smOrder1;
    StateMap smOrder2;

    void mix(Mixer &m);
};

#endif //PAQ8PX_NORMALMODEL_HPP
