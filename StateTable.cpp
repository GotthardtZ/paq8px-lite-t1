#include "StateTable.hpp"

uint8_t StateTable::next(uint8_t const state, const int y) {
  return stateTable[state][y];
}

uint8_t StateTable::next(uint8_t const oldState, const int y, Random &rnd) {
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

uint8_t StateTable::group(const uint8_t state) {
  return stateGroup[state];
}
