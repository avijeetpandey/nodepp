// ═══════════════════════════════════════════════════════════════════
//  test_scheduler.cpp — Tests for timers and scheduling
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/scheduler.h>
#include <atomic>
#include <thread>

using namespace nodepp::scheduler;

TEST(SchedulerTest, SetTimeoutExecutes) {
    std::atomic<bool> executed{false};
    auto handle = setTimeout([&]() { executed.store(true); }, 50);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(executed.load());
}

TEST(SchedulerTest, SetTimeoutCanBeCancelled) {
    std::atomic<bool> executed{false};
    auto handle = setTimeout([&]() { executed.store(true); }, 200);

    clearTimeout(handle);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_FALSE(executed.load());
}

TEST(SchedulerTest, SetIntervalExecutesMultipleTimes) {
    std::atomic<int> count{0};
    auto handle = setInterval([&]() { count++; }, 50);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    clearInterval(handle);

    int finalCount = count.load();
    EXPECT_GE(finalCount, 3);
}

TEST(SchedulerTest, SetIntervalCanBeCancelled) {
    std::atomic<int> count{0};
    auto handle = setInterval([&]() { count++; }, 50);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    clearInterval(handle);
    int countAtCancel = count.load();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(count.load(), countAtCancel);
}

TEST(CronParserTest, ParseEveryMinute) {
    auto expr = parseCron("* * * * *");
    EXPECT_TRUE(expr.minute.any);
    EXPECT_TRUE(expr.hour.any);
}

TEST(CronParserTest, ParseSpecificTime) {
    auto expr = parseCron("30 9 * * *");
    EXPECT_FALSE(expr.minute.any);
    EXPECT_EQ(expr.minute.value, 30);
    EXPECT_FALSE(expr.hour.any);
    EXPECT_EQ(expr.hour.value, 9);
}

TEST(CronParserTest, ParseStep) {
    auto expr = parseCron("*/5 * * * *");
    EXPECT_TRUE(expr.minute.any);
    EXPECT_EQ(expr.minute.step, 5);
}

TEST(CronFieldTest, AnyMatchesAll) {
    detail::CronField field;
    field.any = true;
    EXPECT_TRUE(field.matches(0));
    EXPECT_TRUE(field.matches(59));
}

TEST(CronFieldTest, SpecificValueMatches) {
    detail::CronField field;
    field.any = false;
    field.value = 30;
    EXPECT_TRUE(field.matches(30));
    EXPECT_FALSE(field.matches(31));
}

TEST(CronFieldTest, StepMatches) {
    detail::CronField field;
    field.any = true;
    field.step = 5;
    EXPECT_TRUE(field.matches(0));
    EXPECT_TRUE(field.matches(5));
    EXPECT_TRUE(field.matches(10));
    EXPECT_FALSE(field.matches(3));
}
