#ifndef PAQ8PX_LSTMLAYER_HPP
#define PAQ8PX_LSTMLAYER_HPP

#include "Layer.hpp"
#include "Adam.hpp"
#include "Activations.hpp"
#include "PolynomialDecay.hpp"
#include "../utils.hpp"

template <SIMD simd, typename T>
class LstmLayer {
private:
  std::valarray<float> state, state_error, stored_error;
  std::valarray<std::valarray<float>> tanh_state, input_gate_state, last_state;
  float gradient_clip;
  std::size_t num_cells, epoch, horizon, input_size, output_size;
  std::uint64_t update_steps = 0;
  Layer<simd, Adam<simd, 25, 3, 9999, 4, 1, 6>, Logistic, PolynomialDecay<7, 3, 1, 6, 1, 2>, T> forget_gate;
  Layer<simd, Adam<simd, 25, 3, 9999, 4, 1, 6>, Tanh, PolynomialDecay<7, 3, 1, 6, 1, 2>, T> input_node;
  Layer<simd, Adam<simd, 25, 3, 9999, 4, 1, 6>, Logistic, PolynomialDecay<7, 3, 1, 6, 1, 2>, T> output_gate;
  void Clamp(std::valarray<float>* x) {
    for (std::size_t i = 0; i < x->size(); i++)
      (*x)[i] = std::max<float>(std::min<float>(gradient_clip, (*x)[i]), -gradient_clip);
  }
  static ALWAYS_INLINE float Rand(float const range) {
    return ((static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) - 0.5f) * range;
  }
public:
  LstmLayer(
    std::size_t const input_size,
    std::size_t const auxiliary_input_size,
    std::size_t const output_size,
    std::size_t const num_cells,
    std::size_t const horizon,
    float const gradient_clip,
    float const learning_rate,
    float const range = 0.4f) :
    state(num_cells), state_error(num_cells), stored_error(num_cells),
    tanh_state(std::valarray<float>(num_cells), horizon),
    input_gate_state(std::valarray<float>(num_cells), horizon),
    last_state(std::valarray<float>(num_cells), horizon),
    gradient_clip(gradient_clip),
    num_cells(num_cells), epoch(0), horizon(horizon),
    input_size(auxiliary_input_size), output_size(output_size),
    forget_gate(input_size, auxiliary_input_size, output_size, num_cells, horizon),
    input_node(input_size, auxiliary_input_size, output_size, num_cells, horizon),
    output_gate(input_size, auxiliary_input_size, output_size, num_cells, horizon)
  {
    for (std::size_t i = 0; i < num_cells; i++) {
      for (std::size_t j = 0; j < forget_gate.weights[i].size(); j++) {
        forget_gate.weights[i][j] = Rand(range);
        input_node.weights[i][j] = Rand(range);
        output_gate.weights[i][j] = Rand(range);
      }
      forget_gate.weights[i][forget_gate.weights[i].size() - 1] = 1.f;
    }
  }

  void ForwardPass(
    std::valarray<float> const& input,
    T const input_symbol,
    std::valarray<float>* hidden,
    std::size_t const hidden_start)
  {
    last_state[epoch] = state;
    forget_gate.ForwardPass(input, input_symbol, epoch);
    input_node.ForwardPass(input, input_symbol, epoch);
    output_gate.ForwardPass(input, input_symbol, epoch);
    input_gate_state[epoch] = 1.f - forget_gate.state[epoch];
    state *= forget_gate.state[epoch];
    state += input_node.state[epoch] * input_gate_state[epoch];
    tanh_state[epoch] = std::tanh(state);
    std::slice slice = std::slice(hidden_start, num_cells, 1);
    (*hidden)[slice] = output_gate.state[epoch] * tanh_state[epoch];
    epoch++;
    if (epoch == horizon) epoch = 0;
  }

  void BackwardPass(
    std::valarray<float> const& input,
    std::size_t const epoch,
    std::size_t const layer,
    T const input_symbol,
    std::valarray<float>* hidden_error)
  {
    if (epoch == horizon - 1) {
      stored_error = *hidden_error;
      state_error = 0;
    }
    else
      stored_error += *hidden_error;
    output_gate.error = tanh_state[epoch] * stored_error * output_gate.state[epoch] * (1.f - output_gate.state[epoch]);
    state_error += stored_error * output_gate.state[epoch] * (1.f - (tanh_state[epoch] * tanh_state[epoch]));
    input_node.error = state_error * input_gate_state[epoch] * (1.f - (input_node.state[epoch] * input_node.state[epoch]));
    forget_gate.error = (last_state[epoch] - input_node.state[epoch]) * state_error * forget_gate.state[epoch] * input_gate_state[epoch];
    *hidden_error = 0;
    if (epoch > 0) {
      state_error *= forget_gate.state[epoch];
      stored_error = 0;
    }
    else
      update_steps++;
    forget_gate.BackwardPass(input, hidden_error, &stored_error, update_steps, epoch, layer, input_symbol);
    input_node.BackwardPass(input, hidden_error, &stored_error, update_steps, epoch, layer, input_symbol);
    output_gate.BackwardPass(input, hidden_error, &stored_error, update_steps, epoch, layer, input_symbol);
    Clamp(&state_error);
    Clamp(&stored_error);
    Clamp(hidden_error);
  }
};

#endif //PAQ8PX_LSTMLAYER_HPP
