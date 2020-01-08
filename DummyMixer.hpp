#ifndef PAQ8PX_DUMMYMIXER_HPP
#define PAQ8PX_DUMMYMIXER_HPP

/**
 * for training NormalModel, WordModel and ExeModel
 */
class DummyMixer : public Mixer {
public:
    DummyMixer(const Shared *const sh, const int n, const int m, const int s) : Mixer(sh, n, m, s) {}

    void update() override { reset(); }

    int p() override {
      updater.subscribe(this);
      return 2048;
    }

    void setScaleFactor(const int, const int) override {};
};

#endif //PAQ8PX_DUMMYMIXER_HPP
