#pragma once

#include "number/numeric.hpp"


// This class exists mostly for performance reasons. We cannot simply convert
// time values to strings upon every frame, and we do not want to perform
// expensive division or mod operations to determine whether the clock counted
// up by one second.


class SpeedrunClock {
public:

    int whole_seconds() const
    {
        return whole_seconds_;
    }

    void update(Microseconds delta)
    {
        fractional_time_ += delta;
        
        if (fractional_time_ > seconds(1)) {
            whole_seconds_ -= 1;
            fractional_time_ -= seconds(1);
        }
    }

    void reset()
    {
        whole_seconds_ = 60 * 3;
        fractional_time_ = 0;
    }
    
private:
    u32 whole_seconds_ = 60 * 3;
    Microseconds fractional_time_ = 0;
};
