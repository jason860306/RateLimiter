#ifndef _rate_limiter_h_
#define _rate_limiter_h_

#include <mutex>
#include "rate_limiter_interface.hpp"

class RateLimiter : public RateLimiterInterface {
public:
    RateLimiter();
    long aquire();
    long aquire(int permits);

    bool try_aquire(int timeouts);
    bool try_aquire(int permits, int timeout);

    double get_rate() const;
    void set_rate(double rate);
private:
    void sync(unsigned long long now);
    std::chrono::microseconds claim_next(double permits);
private:
    double interval_;               ///< 生成令牌的速率，即限速的速率值，单位：秒/个
    double max_permits_;            ///< 令牌桶容量
    double stored_permits_;         ///< 当前令牌桶内的令牌数

    unsigned long long next_free_;  ///< 生成令牌的开始时间

    std::mutex mut_;
};


#endif