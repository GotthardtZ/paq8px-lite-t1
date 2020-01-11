#ifndef PAQ8PX_DUMMYMIXER_HPP
#define PAQ8PX_DUMMYMIXER_HPP

#include "UpdateBroadcaster.hpp"
#include "Mixer.hpp"

/**
 * for training NormalModel, WordModel and ExeModel
 */
class DummyMixer : public Mixer {
private:
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();
public:
    DummyMixer(int n, int m, int s);
    void update() override;
    int p() override;

    void setScaleFactor(const int, const int) override {}
};

#endif //PAQ8PX_DUMMYMIXER_HPP
