#ifndef PAQ8PX_DUMMYMIXER_HPP
#define PAQ8PX_DUMMYMIXER_HPP

#include "UpdateBroadcaster.hpp"

/**
 * for training NormalModel, WordModel and ExeModel
 */
class DummyMixer : public Mixer {
private:
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();
public:
    DummyMixer(const int n, const int m, const int s) : Mixer(n, m, s) {}

    void update() override { reset(); }

    int p() override {
      updater->subscribe(this);
      return 2048;
    }

    void setScaleFactor(const int, const int) override {}
};

#endif //PAQ8PX_DUMMYMIXER_HPP
