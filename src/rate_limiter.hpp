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
    double interval_;               ///< �������Ƶ����ʣ������ٵ�����ֵ����λ����/��
    double max_permits_;            ///< ����Ͱ����
    double stored_permits_;         ///< ��ǰ����Ͱ�ڵ�������

    unsigned long long next_free_;  ///< �������ƵĿ�ʼʱ��

    std::mutex mut_;
};


#endif
