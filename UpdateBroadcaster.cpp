#include "UpdateBroadcaster.hpp"

UpdateBroadcaster *UpdateBroadcaster::mPInstance = nullptr;

auto UpdateBroadcaster::getInstance() -> UpdateBroadcaster * {
  if( mPInstance == nullptr ) {
    mPInstance = new UpdateBroadcaster();
  }

  return mPInstance;
}

void UpdateBroadcaster::subscribe(IPredictor *subscriber) {
  subscribers[n] = subscriber;
  n++;
  assert(n < 1024);
}

void UpdateBroadcaster::broadcastUpdate() {
  for( int i = 0; i < n; i++ ) {
    IPredictor *subscriber = subscribers[i];
    subscriber->update();
  }
  n = 0;
}
