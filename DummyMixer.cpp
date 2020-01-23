#include "DummyMixer.hpp"

DummyMixer::DummyMixer(const int n, const int m, const int s) : Mixer(n, m, s) {}

void DummyMixer::update() { reset(); }

auto DummyMixer::p() -> int {
  updater->subscribe(this);
  return 2048;
}
