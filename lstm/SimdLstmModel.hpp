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
    std::uint8_t const bpos = this->shared->State.bitPosition;
    std::uint8_t const y = this->shared->State.y;
    std::uint8_t const c0 = this->shared->State.c0;
    if (y)
      this->bot = this->mid + 1;
    else
      this->top = this->mid;
    if (bpos == 0) {
      std::uint8_t const c1 = this->shared->State.c1;
      auto const& output = lstm.Perceive(c1);
      memcpy(&this->probs[0], &output[0], (1 << Bits) * sizeof(float));
      this->top = (1 << Bits) - 1;
      this->bot = 0;

      if (((this->shared->options & OPTION_LSTM_RETRAINING) != 0u) && ((this->shared->State.blockType == TEXT) || (this->shared->State.blockType == TEXT_EOL)) && (this->txt_pos < this->RetrainWindow)) {
        this->count++;
        if (((this->count >= this->threshold) && ((c1 == SPACE) || (c1 == CARRIAGE_RETURN) || (this->count >= this->threshold + this->RetrainAllowedOverflow)))) {
          this->lstm.SaveTimeStep();
          RingBuffer<std::uint8_t> const& buf = this->shared->buf;
          std::uint32_t const blockPos = this->shared->State.blockPos;
          if (blockPos < this->RetrainFromBlockStartWindow) {
            for (std::uint32_t i = blockPos; i > 0u; i--)
              lstm.Perceive(buf(i));
            this->count = 0u;
          }
          else {
            for (; this->count > 0u; this->count--)
              lstm.Perceive(buf(static_cast<std::uint32_t>(this->count)));
          }
          this->lstm.RestoreTimeStep();
          this->threshold = std::min<std::size_t>(this->threshold * 2u, this->RetrainMaxThreshold);
        }
      }
      else {
        this->count = 0u;
        this->threshold = this->initial_threshold;
      }
    }

    this->mid = (this->bot + this->top)>>1;
    float prediction, num, denom;
    if (simd == SIMD_AVX2) {
      num = sum256_ps(&this->probs[this->mid + 1], this->top - this->mid, 0.f);
      denom = sum256_ps(&this->probs[this->bot], this->mid + 1 - this->bot, num);
    }
    else {
      num = 0.0f; for (size_t i = this->mid + 1; i <= this->top; i++) num += this->probs[i];
      denom = num; for (size_t i = this->bot;  i <= this->mid; i++) denom += this->probs[i];
    }
    prediction = (denom == 0.f) ? 0.5f : num / denom;
    
    this->expected = static_cast<std::uint8_t>(this->bot);
    float max_prob_val = this->probs[this->bot];
    for (std::size_t i = this->bot + 1; i <= this->top; i++) {
      if (this->probs[i] > max_prob_val) {
        max_prob_val = this->probs[i];
        this->expected = static_cast<std::uint8_t>(i);
      }
    }

    this->iCtx += 2+y, this->iCtx = (bpos << 8) | this->expected;
    std::uint32_t mask = 0u, i = 0u;
    for (std::uint32_t ctx = this->iCtx(); ctx > 0u; mask |= (ctx & 1u) << i, i++, ctx >>= 2);
    mask |= 1u << i;

    int const p = min(max(std::lround(prediction * 4096.0f), 1), 4095);
    m.add(stretch(p));
    m.add((p - 2048) >> 2);
    int const pr1 = this->apm1.p(p, (c0 << 8) | (this->shared->State.misses & 0xFF), 0xFF);
    int const pr2 = this->apm2.p(p, (bpos << 8) | this->expected, 0xFF);
    int const pr3 = this->apm3.p(pr2, mask, 0xFF);
    m.add(stretch(pr1) >> 1);
    m.add(stretch(pr2) >> 1);
    m.add(stretch(pr3) >> 1);
    m.set((bpos << 8) | this->expected, 8 * 256);
    m.set(static_cast<std::uint32_t>(lstm.epoch) << 3 | bpos, 100 * 8);
  }
};

#endif //PAQ8PX_SIMDLSTMMODEL_HPP
