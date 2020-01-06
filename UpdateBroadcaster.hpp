#ifndef PAQ8PX_UPDATEBROADCASTER_HPP
#define PAQ8PX_UPDATEBROADCASTER_HPP

#include "IPredictor.hpp"
#include <cassert>

/**
 * The purpose of this class is to inform probability predictors when
 * the next bit is known by calling the update() method of each predictor.
 */
class UpdateBroadcaster {
private:
    int n; /**< number of subscribed predictors, (number of items in "subscribers" array)*/
    IPredictor *subscribers[1024];
public:
    UpdateBroadcaster() : n(0) {}

    void subscribe(IPredictor *subscriber) {
      subscribers[n] = subscriber;
      n++;
      assert(n < 1024);
    }

    void broadcastUpdate() {
      for( int i = 0; i < n; i++ ) {
        IPredictor *subscriber = subscribers[i];
        subscriber->update();
      }
      n = 0;
    }
} updater;

#endif //PAQ8PX_UPDATEBROADCASTER_HPP
