#include "StateTable.hpp"

auto StateTable::next(uint8_t const state, const int y) -> uint8_t {
  return stateTable[state][y];
}

auto StateTable::next(uint8_t const oldState, const int y, Random &rnd) -> uint8_t {
  uint8_t newState = stateTable[oldState][y];
  if( newState >= 205 ) { // for all groups of four states higher than idx 205
    if( oldState < 249 ) { // still climbing
      const int shift = (460 - newState) >> 3U;
      //for each group the probability to advance to a higher state is less and less as we climb higher and higher
      if((rnd() << shift) != 0 ) {
        newState -= 4;
      }
    }
  }
  return newState;
}

void StateTable::update(uint8_t *const state, const int y, Random &rnd) {
  *state = next(*state, y, rnd);
}

auto StateTable::group(const uint8_t state) -> uint8_t {
  return stateGroup[state];
}
