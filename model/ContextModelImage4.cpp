#include "../MixerFactory.hpp"
#include "../Models.hpp"

class ContextModelImage4 {

private:
  Shared* const shared;
  Models* const models;
  Mixer* m;

public:
  ContextModelImage4(Shared* const sh, Models* const models) : shared(sh), models(models) {
    MixerFactory mf{};
    m = mf.createMixer (
      sh,
      1 +  //bias
      MatchModel::MIXERINPUTS + NormalModel::MIXERINPUTS + 
      Image4BitModel::MIXERINPUTS +
      (((shared->options & OPTION_LSTM) != 0u) ? LstmModel<>::MIXERINPUTS : 0)
      ,
      MatchModel::MIXERCONTEXTS + NormalModel::MIXERCONTEXTS_PRE +
      Image4BitModel::MIXERCONTEXTS +
      (((shared->options & OPTION_LSTM) != 0u) ? LstmModel<>::MIXERCONTEXTS : 0)
      ,
      MatchModel::MIXERCONTEXTSETS + NormalModel::MIXERCONTEXTSETS_PRE + 
      Image4BitModel::MIXERCONTEXTSETS +
      (((shared->options & OPTION_LSTM) != 0u) ? LstmModel<>::MIXERCONTEXTSETS : 0)
    );
    m->setScaleFactor(2048, 256);
  }


  int p(int blockInfo) {

    m->add(256); //network bias

    NormalModel& normalModel = models->normalModel();
    normalModel.mix(*m);

    MatchModel& matchModel = models->matchModel();
    matchModel.mix(*m);

    //?
    if ((shared->options & OPTION_LSTM) != 0u) {
      LstmModel<>& lstmModel = models->lstmModel();
      lstmModel.mix(*m);
    }

    Image4BitModel& image4BitModel = models->image4BitModel();
    INJECT_SHARED_blockType
    int widthInBytes = blockType == BlockType::MRB4 ? ((((blockInfo >> 16) & 0xfff) * 4 + 15) / 16) * 2 : blockInfo;
    image4BitModel.setParam(widthInBytes);
    image4BitModel.mix(*m);

    return m->p();
  }

  ~ContextModelImage4() {
    delete m;
  }

};