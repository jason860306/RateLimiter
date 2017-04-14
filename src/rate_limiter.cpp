#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "rate_limiter.hpp"

#if 0
///< Google开源工具包Guava提供的限流工具类RateLimiter的主要实现代码:

public double acquire() {
    return acquire(1);
}

public double acquire(int permits) {
    checkPermits(permits);  //检查参数是否合法（是否大于0）
    long microsToWait;
    synchronized (mutex) { //应对并发情况需要同步
        microsToWait = reserveNextTicket(permits, readSafeMicros()); //获得需要等待的时间
    }
    ticker.sleepMicrosUninterruptibly(microsToWait); //等待，当未达到限制时，microsToWait为0
    return 1.0 * microsToWait / TimeUnit.SECONDS.toMicros(1L);
}

private long reserveNextTicket(double requiredPermits, long nowMicros) {
    resync(nowMicros); //补充令牌
    long microsToNextFreeTicket = nextFreeTicketMicros - nowMicros;
    double storedPermitsToSpend = Math.min(requiredPermits, this.storedPermits); //获取这次请求消耗的令牌数目
    double freshPermits = requiredPermits - storedPermitsToSpend;

    long waitMicros = storedPermitsToWaitTime(this.storedPermits, storedPermitsToSpend)
            + (long) (freshPermits * stableIntervalMicros);

    this.nextFreeTicketMicros = nextFreeTicketMicros + waitMicros;
    this.storedPermits -= storedPermitsToSpend; // 减去消耗的令牌
    return microsToNextFreeTicket;
}

private void resync(long nowMicros) {
    // if nextFreeTicket is in the past, resync to now
    if (nowMicros > nextFreeTicketMicros) {
        storedPermits = Math.min(maxPermits,
                                 storedPermits + (nowMicros - nextFreeTicketMicros) / stableIntervalMicros);
        nextFreeTicketMicros = nowMicros;
    }
}
#endif

RateLimiter::RateLimiter()
    : interval_(0), max_permits_(0), stored_permits_(0), next_free_(0) {
}
long RateLimiter::aquire() {
    return aquire(1);
}
long RateLimiter::aquire(int permits) {
    if (permits <= 0) {
        throw std::runtime_error("RateLimiter: Must request positive amount of permits");
    }

    auto wait_time = claim_next(permits);
    std::this_thread::sleep_for(wait_time);

    return wait_time.count() / 1000.0;
}

bool RateLimiter::try_aquire(int permits) {
    return try_aquire(permits, 0);
}
bool RateLimiter::try_aquire(int permits, int timeout) {
    using namespace std::chrono;

    unsigned long long now = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();

    // Check to see if the next free aquire time is within the
    // specified timeout. If it's not, return false and DO NOT BLOCK,
    // otherwise, calculate time needed to claim, and block
    if (next_free_ > now + timeout * 1000)
        return false;
    else {
        aquire(permits);
    }

    return true;
}

void RateLimiter::sync(unsigned long long now) {
    // If we're passed the next_free, then recalculate
    // stored permits, and update next_free_
    if (now > next_free_) {
        /**
         * 取当前令牌桶里的令牌数，不能大于令牌桶的容量
         * (now - next_free_) / interval_: 时间段(now - next_free_)内新产生的令牌数
         * stored_permits_ + (now - next_free_) / interval_: 当前令牌桶内剩余及新产生的令牌数
         */
        stored_permits_ = std::min(max_permits_, stored_permits_ + (now - next_free_) / interval_);
        next_free_ = now; ///< 重置生成令牌的开始时间
    }
}
std::chrono::microseconds RateLimiter::claim_next(double permits) {
    using namespace std::chrono;

    std::lock_guard<std::mutex> lock(mut_);

    unsigned long long now = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();

    // Make sure we're synced
    sync(now);

    // Since we synced before hand, this will always be >= 0.
    unsigned long long wait = next_free_ - now; ///< 到产生令牌的起始时间所需要等待的时间

    // Determine how many stored and freh permits to consume
    double stored = std::min(permits, stored_permits_); ///< 当前实际需要且可用的令牌数
    double fresh = permits - stored; ///< 当前令牌桶内不足但实际需要而需要新产生的令牌数

    // In the general RateLimiter, stored permits have no wait time,
    // and thus we only have to wait for however many fresh permits we consume
    long next_free = (long)(fresh * interval_); ///< 用令牌生成速率将不足的令牌数转换成时间戳

    next_free_ += next_free;
    stored_permits_ -= stored; ///< 消耗掉stored个令牌

    return microseconds(wait);
}

double RateLimiter::get_rate() const {
    return 1000000.0 / interval_;
}
void RateLimiter::set_rate(double rate) {
    if (rate <= 0.0) {
        throw std::runtime_error("RateLimiter: Rate must be greater than 0");
    }

    /**
     * 生成令牌的速率单位值
     * interval_: 秒/个
     * rate: 个/秒
     */
    std::lock_guard<std::mutex> lock(mut_);
    interval_ = 1000000.0 / rate;
}
