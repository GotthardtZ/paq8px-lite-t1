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
    static auto getInstance() -> UpdateBroadcaster *;
    void subscribe(IPredictor *subscriber);
    void broadcastUpdate();
private:
    static UpdateBroadcaster *mPInstance;
    int n {0}; /**< number of subscribed predictors, (number of items in "subscribers" array) */
    IPredictor *subscribers[1024] {};

    UpdateBroadcaster() = default;

    /**
     * Copy constructor is private so that it cannot be called
     */
    UpdateBroadcaster(UpdateBroadcaster const & /*unused*/) {}

    /**
     * Assignment operator is private so that it cannot be called
     */
    auto operator=(UpdateBroadcaster const & /*unused*/) -> UpdateBroadcaster & { return *this; }
};

#endif //PAQ8PX_UPDATEBROADCASTER_HPP