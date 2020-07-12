#ifndef PAQ8PX_SIMDLSTMMODEL_HPP
#define PAQ8PX_SIMDLSTMMODEL_HPP

#include "LstmModel.hpp"
#include "SimdFunctions.hpp"
#include "Lstm.hpp"
#include "../Shared.hpp"
#include "../Mixer.hpp"
#include "../RingBuffer.hpp"
#include "../APM.hpp"
#include "../IndirectContext.hpp"
#include "../utils.hpp"

template <SIMD simd, std::size_t Bits = 8>
class SIMDLstmModel :
  public LstmModel<Bits> {
private:
  Lstm<simd, std::uint8_t> lstm;
public:
  SIMDLstmModel(
    const Shared* const sh,
    std::size_t const num_cells,
    std::size_t const num_layers,
    std::size_t const horizon,
    float const learning_rate,
    float const gradient_clip) :
    LstmModel<Bits>(sh, num_cells, num_layers, horizon, learning_rate, gradient_clip),
    lstm(0, this->Size, num_cells, num_layers, horizon, learning_rate, gradient_clip)
  {}

  void mix(Mixer& m) {
    const uint8_t bpos = this->shared->State.bitPosition;
    const uint8_t y = this->shared->State.y;
    const uint8_t c0 = this->shared->State.c0;
    if (y)
      this->bot = this->mid + 1;
    else
      this->top = this->mid;
    if (bpos == 0) {
      const uint8_t c1 = this->shared->State.c1;
      auto const& output = lstm.Perceive(c1);
      std::copy(std::begin(output), std::end(output), std::begin(this->probs));
      this->top = this->Size - 1;
      this->bot = 0;
    }
    this->mid = this->bot + ((this->top - this->bot) / 2);
    float prediction, num, denom;
    if (simd == SIMD_AVX2) {
      num = sum256_ps(&this->probs[this->mid + 1], this->top - this->mid, 0.f);
      denom = sum256_ps(&this->probs[this->bot], this->mid + 1 - this->bot, num);
    }
    else {
      num = std::accumulate(&this->probs[this->mid + 1], &this->probs[this->top + 1], 0.0f);
      denom = std::accumulate(&this->probs[this->bot], &this->probs[this->mid + 1], num);
    }
    prediction = (denom == 0.f) ? 0.5f : num / denom;
    this->expected = static_cast<std::size_t>(std::distance(std::begin(this->probs), std::max_element(std::begin(this->probs) + this->bot, std::begin(this->probs) + this->top)));
    this->iCtx +=y, this->iCtx = (bpos << 8) | static_cast<std::uint8_t>(this->expected);
    int const p = std::lround(prediction * 4095.);
    m.add(stretch(p));
    m.add((p - 2048) >> 2);
    int const pr1 = this->apm1.p(p, (c0 << 8) | (this->shared->State.misses & 0xFF), 0xFF);
    int const pr2 = this->apm2.p(p, (bpos << 8) | static_cast<std::uint8_t>(this->expected), 0xFF);
    int const pr3 = this->apm3.p(pr2, this->iCtx(), 0xFF);
    m.add(stretch(pr1) >> 1);
    m.add(stretch(pr2) >> 1);
    m.add(stretch(pr3) >> 1);
    m.set((bpos << 8) | static_cast<std::uint8_t>(this->expected), 8 * 256);
    m.set(static_cast<std::uint32_t>(lstm.epoch) * 8 + bpos, 8 * 100);
  }
};

#endif //PAQ8PX_SIMDLSTMMODEL_HPP
