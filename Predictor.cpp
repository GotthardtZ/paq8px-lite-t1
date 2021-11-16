#include "Predictor.hpp"

Predictor::Predictor(Shared* const sh) : 
  shared(sh),
  mixerFactory(sh),
  normalModel(sh, sh->mem),
  m(mixerFactory.createMixer(
    1 +  //bias
    NormalModel::MIXERINPUTS
    ,
    NormalModel::MIXERCONTEXTS
    ,
    NormalModel::MIXERCONTEXTSETS
  ))
{
  shared->reset();
  m->setScaleFactor(1150, 240);
}

void Predictor::Update() {
  normalModel.cm.update();
  normalModel.smOrder0.update();
  normalModel.smOrder1.update();
  normalModel.smOrder2.update();
  m->update();
}

uint32_t Predictor::p() {

  m->add(256); //network bias

  normalModel.mix(*m);
  uint32_t pr=m->p();

  pr=pr<<4;
  return pr;
}


