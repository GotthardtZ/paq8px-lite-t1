#include "StateTable.hpp"

auto StateTable::next(uint8_t const state, const int y) -> uint8_t {
  return stateTable[state][y];
}

void StateTable::update(uint8_t *const state, const int y) {
  *state = next(*state, y);
}

auto StateTable::certanity(const uint8_t state) -> uint16_t {
  return stateCertainty[state];
}

auto StateTable::prio(const uint8_t state) -> uint8_t {
  return statePrio[state];
}
