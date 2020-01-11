#include <cstdint>
#include "model/NormalModel.hpp"
#include "Mixer.hpp"
#include "MixerFactory.hpp"
#include "ModelStats.hpp"

#define MEM (uint64_t(65536) << 1U)

int main() {
  constexpr uint8_t ys[16] = {0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0};
  Mixer *m = MixerFactory::createMixer(1 + NormalModel::MIXERINPUTS, NormalModel::MIXERCONTEXTS, NormalModel::MIXERCONTEXTSETS);
  m->setScaleFactor(1024, 128);
  auto *modelStats = new ModelStats();
  NormalModel normalModel(modelStats, MEM * 32);
  auto shared = Shared::getInstance();
  auto updateBroadcaster = UpdateBroadcaster::getInstance();
  auto programChecker = ProgramChecker::getInstance();

  shared->reset();
  shared->buf.setSize(MEM * 8);
  for( int i = 0; i < 256; ++i ) {
    for( auto &&y : ys ) {
      m->add(256); //network bias
      shared->y = y;
      shared->update();
      normalModel.mix(*m);
      normalModel.mixPost(*m);
      auto p = m->p();
      printf("%d, %d\n", y, p);
      updateBroadcaster->broadcastUpdate();
    }
  }
  programChecker->print();
  return 1;
}