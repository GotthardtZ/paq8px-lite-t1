#ifndef PAQ8PX_UPDATEBROADCASTER_HPP
#define PAQ8PX_UPDATEBROADCASTER_HPP

#include "IPredictor.hpp"
#include <cassert>

/**
 * The purpose of this class is to inform probability predictors when
 * the next bit is known by calling the update() method of each predictor.
 */
class UpdateBroadcaster {
public:
    static UpdateBroadcaster *getInstance();
    void subscribe(IPredictor *subscriber);
    void broadcastUpdate();
private:
    UpdateBroadcaster() : n(0) {};  // Private so that it can  not be called
    UpdateBroadcaster(UpdateBroadcaster const &) {};             // copy constructor is private
    UpdateBroadcaster &operator=(UpdateBroadcaster const &) {return *this;};  // assignment operator is private
    static UpdateBroadcaster *mPInstance;
    int n; /**< number of subscribed predictors, (number of items in "subscribers" array)*/
    IPredictor *subscribers[1024];
};

#endif //PAQ8PX_UPDATEBROADCASTER_HPP