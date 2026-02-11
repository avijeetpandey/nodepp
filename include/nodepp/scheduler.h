#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/scheduler.h — Timers, intervals, and cron-like scheduling
// ═══════════════════════════════════════════════════════════════════

#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <memory>
#include <sstream>

namespace nodepp::scheduler {

// ── Timer handle for cancellation ──
class TimerHandle {
public:
    void cancel() { cancelled_.store(true); }
    bool isCancelled() const { return cancelled_.load(); }
private:
    std::atomic<bool> cancelled_{false};
};

// ── setTimeout — run once after delay ──
inline std::shared_ptr<TimerHandle> setTimeout(std::function<void()> callback, int ms) {
    auto handle = std::make_shared<TimerHandle>();
    std::thread([handle, callback = std::move(callback), ms]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        if (!handle->isCancelled()) callback();
    }).detach();
    return handle;
}

// ── setInterval — run repeatedly ──
inline std::shared_ptr<TimerHandle> setInterval(std::function<void()> callback, int ms) {
    auto handle = std::make_shared<TimerHandle>();
    std::thread([handle, callback = std::move(callback), ms]() {
        while (!handle->isCancelled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            if (!handle->isCancelled()) callback();
        }
    }).detach();
    return handle;
}

// ── clearTimeout / clearInterval ──
inline void clearTimeout(std::shared_ptr<TimerHandle>& handle) {
    if (handle) handle->cancel();
}
inline void clearInterval(std::shared_ptr<TimerHandle>& handle) {
    if (handle) handle->cancel();
}

// ═══════════════════════════════════════════
//  Simple cron expression parser
//  Supports: minute hour day month weekday
//  Values: * (any), specific number, */N (every N)
// ═══════════════════════════════════════════
namespace detail {
struct CronField {
    bool any = true;
    int value = 0;
    int step = 0;   // for */N

    bool matches(int v) const {
        if (any && step == 0) return true;
        if (step > 0) return (v % step) == 0;
        return v == value;
    }
};

inline CronField parseCronField(const std::string& s) {
    CronField f;
    if (s == "*") { f.any = true; return f; }
    if (s.substr(0, 2) == "*/") {
        f.any = true;
        f.step = std::stoi(s.substr(2));
        return f;
    }
    f.any = false;
    f.value = std::stoi(s);
    return f;
}
} // namespace detail

struct CronExpression {
    detail::CronField minute, hour, dayOfMonth, month, dayOfWeek;

    bool matchesNow() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time);
        return minute.matches(tm.tm_min) &&
               hour.matches(tm.tm_hour) &&
               dayOfMonth.matches(tm.tm_mday) &&
               month.matches(tm.tm_mon + 1) &&
               dayOfWeek.matches(tm.tm_wday);
    }
};

inline CronExpression parseCron(const std::string& expr) {
    CronExpression cron;
    std::istringstream iss(expr);
    std::string field;
    int idx = 0;
    while (iss >> field && idx < 5) {
        auto parsed = detail::parseCronField(field);
        switch (idx) {
            case 0: cron.minute = parsed; break;
            case 1: cron.hour = parsed; break;
            case 2: cron.dayOfMonth = parsed; break;
            case 3: cron.month = parsed; break;
            case 4: cron.dayOfWeek = parsed; break;
        }
        idx++;
    }
    return cron;
}

// ── Cron job — checks every minute ──
inline std::shared_ptr<TimerHandle> cron(const std::string& expression,
                                          std::function<void()> callback) {
    auto cronExpr = parseCron(expression);
    auto handle = std::make_shared<TimerHandle>();
    std::thread([handle, cronExpr, callback = std::move(callback)]() {
        int lastMinute = -1;
        while (!handle->isCancelled()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time);

            if (tm.tm_min != lastMinute && cronExpr.matchesNow()) {
                lastMinute = tm.tm_min;
                if (!handle->isCancelled()) callback();
            }
        }
    }).detach();
    return handle;
}

} // namespace nodepp::scheduler
