#ifndef PAQ8PX_LSTM_HPP
#define PAQ8PX_LSTM_HPP

#include "SimdFunctions.hpp"
#include "LstmLayer.hpp"
#include "../utils.hpp"
#include <vector>
#include <memory>

/**
 * Long Short-Term Memory neural network.
 * Based on the LSTM implementation in cmix by Byron Knoll.
 */
template <SIMD simd, typename T>
class Lstm {
  static_assert(std::is_integral<T>::value && (!std::is_same<T, bool>::value), "LSTM input type must be integral and non-boolean");
private:
  std::vector<std::unique_ptr<LstmLayer<simd, T>>> layers;
  std::valarray<std::valarray<std::valarray<float>>> layer_input, output_layer;
  std::valarray<std::valarray<float>> output;
  std::valarray<float> hidden, hidden_error;
  std::vector<T> input_history;
  float learning_rate;
  std::size_t num_cells, horizon, input_size, output_size;

#if (defined(__GNUC__) || defined(__clang__)) && (!defined(__ARM_FEATURE_SIMD32) && !defined(__ARM_NEON))
  __attribute__((target("avx2,fma")))
#endif
  void SoftMaxSimdAVX2() {
#if !defined(__i386__) && !defined(__x86_64__) && !defined(_M_X64)
    return;
#else
    static constexpr std::size_t SIMDW = 8;
    std::size_t const limit = output_size & static_cast<std::size_t>(-static_cast<std::ptrdiff_t>(SIMDW)), len = hidden.size();
    std::size_t remainder = output_size & (SIMDW - 1);
    __m256 v_sum = _mm256_setzero_ps();
    for (std::size_t i = 0; i < limit; i++)
      output[epoch][i] = dot256_ps_fma3(&hidden[0], &output_layer[epoch][i][0], len, 0.f);
    for (std::size_t i = 0; i < limit; i += SIMDW) {
      __m256 v_exp = exp256_ps_fma3(_mm256_loadu_ps(&output[epoch][i]));
      _mm256_storeu_ps(&output[epoch][i], v_exp);
      v_sum = _mm256_add_ps(v_sum, v_exp);
    }
    float sum = hsum256_ps_avx(v_sum);
    for (; remainder > 0; remainder--) {
      const std::size_t i = output_size - remainder;
      output[epoch][i] = std::exp(dot256_ps_fma3(&hidden[0], &output_layer[epoch][i][0], len, 0.f));
      sum += output[epoch][i];
    }
    output[epoch] /= sum;
#endif
  }
  void SoftMaxSimdNone() {
    for (unsigned int i = 0; i < output_size; ++i)
      output[epoch][i] = std::exp((hidden * output_layer[epoch][i]).sum());
    output[epoch] /= output[epoch].sum();
  }
public:
  std::size_t epoch;
  Lstm(
    std::size_t const input_size,
    std::size_t const output_size,
    std::size_t const num_cells,
    std::size_t const num_layers,
    std::size_t const horizon,
    float const learning_rate,
    float const gradient_clip) :
    layer_input(std::valarray<std::valarray<float>>(std::valarray<float>(input_size + 1 + num_cells * 2), num_layers), horizon),
    output_layer(std::valarray<std::valarray<float>>(std::valarray<float>(num_cells * num_layers + 1), output_size), horizon),
    output(std::valarray<float>(1.0f / output_size, output_size), horizon),
    hidden(num_cells * num_layers + 1),
    hidden_error(num_cells),
    input_history(horizon),
    learning_rate(learning_rate),
    num_cells(num_cells),
    horizon(horizon),
    input_size(input_size),
    output_size(output_size),
    epoch(0)
  {
    hidden[hidden.size() - 1] = 1.f;
    for (std::size_t epoch = 0; epoch < horizon; epoch++) {
      layer_input[epoch][0].resize(1 + num_cells + input_size);
      for (std::size_t i = 0; i < num_layers; i++)
        layer_input[epoch][i][layer_input[epoch][i].size() - 1] = 1.f;
    }
    for (std::size_t i = 0; i < num_layers; i++) {
      layers.push_back(std::unique_ptr<LstmLayer<simd, T>>(new LstmLayer<simd, T>(
        layer_input[0][i].size() + output_size,
        input_size, output_size,
        num_cells, horizon, gradient_clip, learning_rate
        )));
    }
  }

  void SetInput(std::valarray<float> const& input) {
    for (std::size_t i = 0; i < layers.size(); i++)
      std::copy(std::begin(input), std::begin(input) + input_size, std::begin(layer_input[epoch][i]));
  }

  std::valarray<float>& Predict(T const input) {
    for (std::size_t i = 0; i < layers.size(); i++) {
      auto start = std::begin(hidden) + i * num_cells;
      std::copy(start, start + num_cells, std::begin(layer_input[epoch][i]) + input_size);
      layers[i]->ForwardPass(layer_input[epoch][i], input, &hidden, i * num_cells);
      if (i < layers.size() - 1) {
        auto start2 = std::begin(layer_input[epoch][i + 1]) + num_cells + input_size;
        std::copy(start, start + num_cells, start2);
      }
    }
    if (simd == SIMD_AVX2)
      SoftMaxSimdAVX2();
    else
      SoftMaxSimdNone();
    std::size_t const epoch_ = epoch;
    epoch++;
    if (epoch == horizon) epoch = 0;
    return output[epoch_];
  }

  std::valarray<float>& Perceive(const T input) {
    std::size_t const last_epoch = ((epoch > 0) ? epoch : horizon) - 1;
    T const old_input = input_history[last_epoch];
    input_history[last_epoch] = input;
    if (epoch == 0) {
      for (int epoch_ = static_cast<int>(horizon) - 1; epoch_ >= 0; epoch_--) {
        for (int layer = static_cast<int>(layers.size()) - 1; layer >= 0; layer--) {
          int offset = layer * static_cast<int>(num_cells);
          for (std::size_t i = 0; i < output_size; i++) {
            float const error = (i == input_history[epoch_]) ? output[epoch_][i] - 1.f : output[epoch_][i];
            for (std::size_t j = 0; j < hidden_error.size(); j++)
              hidden_error[j] += output_layer[epoch_][i][j + offset] * error;
          }
          std::size_t const prev_epoch = ((epoch_ > 0) ? epoch_ : horizon) - 1;
          T const input_symbol = (epoch_ > 0) ? input_history[prev_epoch] : old_input;
          layers[layer]->BackwardPass(layer_input[epoch_][layer], epoch_, layer, input_symbol, &hidden_error);
        }
      }
    }
    for (std::size_t i = 0; i < output_size; i++) {
      float const error = (i == input) ? output[last_epoch][i] - 1.f : output[last_epoch][i];
      output_layer[epoch][i] = output_layer[last_epoch][i];
      output_layer[epoch][i] -= learning_rate * error * hidden;
    }
    return Predict(input);
  }
};

#endif //PAQ8PX_LSTM_HPP
