#include "UpdateBroadcaster.hpp"

UpdateBroadcaster *UpdateBroadcaster::mPInstance = nullptr;

UpdateBroadcaster *UpdateBroadcaster::getInstance() {
  if( !mPInstance )
    mPInstance = new UpdateBroadcaster();

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
