#ifndef PAQ8PX_DUMMYMIXER_HPP
#define PAQ8PX_DUMMYMIXER_HPP

#include "UpdateBroadcaster.hpp"
#include "Mixer.hpp"

/**
 * For training @ref NormalModel, @ref WordModel and @erf ExeModel
 */
class DummyMixer : public Mixer {
private:
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();
public:
    DummyMixer(int n, int m, int s);
    void update() override;
    auto p() -> int override;

    void setScaleFactor(const int /*sf0*/, const int /*sf1*/) override {}
};

#endif //PAQ8PX_DUMMYMIXER_HPP
