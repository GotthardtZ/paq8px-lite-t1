#include "DummyMixer.hpp"

DummyMixer::DummyMixer(const int n, const int m, const int s) : Mixer(n, m, s) {}

void DummyMixer::update() { reset(); }

int DummyMixer::p() {
  updater->subscribe(this);
  return 2048;
}
