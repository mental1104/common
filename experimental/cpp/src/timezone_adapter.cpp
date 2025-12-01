#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

// Legacy component: assumes all business logic is in Asia/Shanghai (UTC+8).
struct ShanghaiClock
{
    std::time_t now() const
    {
        // Real UTC "now"
        std::time_t utc = std::time(nullptr);
        // Represent "Shanghai local" as UTC+8 encoded in time_t
        return utc + 8 * 60 * 60;
    }

    std::string format(std::time_t shanghai_time) const
    {
        // Interpret the stored value directly as "Shanghai local"
        std::tm* ptm = std::gmtime(&shanghai_time);
        std::ostringstream os;
        os << std::put_time(ptm, "%Y-%m-%d %H:%M:%S")
           << " (Asia/Shanghai)";
        return os.str();
    }
};

void log_shanghai_time(const ShanghaiClock& clock)
{
    std::cout << "[legacy] "
              << clock.format(clock.now())
              << '\n';
}


struct I18nClock
{
    virtual ~I18nClock() = default;
    virtual std::string now_in(std::string_view iana_tz) = 0;
};


struct ShanghaiClockAdapter : I18nClock
{
    explicit ShanghaiClockAdapter(const ShanghaiClock& clock)
        : clock_(clock)
    {}

    std::string now_in(std::string_view iana_tz) override
    {
        std::time_t shanghai_local = clock_.now(); // encoded as UTC+8

        const int shanghai_offset_hours = 8;
        const int target_offset_hours   = offset_hours_for(iana_tz);

        // Shift from "Shanghai local" to target local: UTC+target - UTC+8
        std::time_t target_local =
            shanghai_local
            + (target_offset_hours - shanghai_offset_hours) * 60 * 60;

        return format_time(target_local, iana_tz);
    }

private:
    const ShanghaiClock& clock_;

    static int offset_hours_for(std::string_view tz)
    {
        // Very simplified table, no DST, just for demo
        if (tz == "Asia/Shanghai")    return 8;
        if (tz == "Europe/London")    return 0;
        if (tz == "America/New_York") return -5;
        if (tz == "Asia/Tokyo")       return 9;
        // default to UTC
        return 0;
    }

    static std::string format_time(std::time_t local_in_target,
                                   std::string_view tz)
    {
        // Interpret "local_in_target" as local time for that zone
        std::tm* ptm = std::gmtime(&local_in_target);
        std::ostringstream os;
        os << std::put_time(ptm, "%Y-%m-%d %H:%M:%S")
           << " (" << tz << ')';
        return os.str();
    }
};


int main()
{
    ShanghaiClock legacy_clock;

    // Old code, unchanged, still works:
    log_shanghai_time(legacy_clock);

    // New code: depends only on I18nClock
    ShanghaiClockAdapter i18n_clock{legacy_clock};

    std::cout << i18n_clock.now_in("Asia/Shanghai")    << '\n';
    std::cout << i18n_clock.now_in("Europe/London")    << '\n';
    std::cout << i18n_clock.now_in("America/New_York") << '\n';
    std::cout << i18n_clock.now_in("Asia/Tokyo")       << '\n';

    return 0;
}
