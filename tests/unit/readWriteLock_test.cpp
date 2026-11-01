/**
 * @file  readWriteLock_test.cpp
 * @brief 读写锁（ReadWriteLock）单元测试，框架为 GoogleTest。
 *
 * 覆盖策略
 *  - 并发语义：多个读者可同时持锁（共享读）；写者持锁期间读者必须被阻塞，
 *    直到写者释放（互斥写）；
 *  - 全部用例通过 std::async + future 限时等待双保险：若实现存在互斥缺陷，
 *    用例会超时失败而不是挂死整个测试进程（避免 CI 卡死）。
 *
 * 该层为纯并发测试，不依赖窗口环境；耗时上限约 2 秒（仅在缺陷场景下）。
 */
#include "inc/readWriteLock.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

// TEST 宏展开体不参与 Doxygen 文档化（避免 WARN_NO_PARAMDOC 逐个误报）
/// @cond DOC_HIDDEN
namespace {

/// 用例内等待共享状态的最长时间（毫秒）：只用于并发同步，不用固定 sleep 代替。
constexpr int kWaitLoopMs = 2000;
constexpr int kWaitStepMs = 1;

}  // namespace

/**
 * @test   ReadWriteLock.TwoReadersHoldReadLockConcurrently
 * @brief  并发：两个读者应同时持有读锁（共享语义）。
 * @details 目的：ReadWriteLock 的核心语义是"多读单写"。若被误实现为互斥，
 *          第二个读者将永远等不到锁——本用例的超时保护使其变为失败而不是挂死。
 *          设计思路：两个异步读者各自持锁，等待"对方也进入临界区"这一
 *          共享状态；只有真正的共享读才能让双方同时返回。
 * @expected 两个读者都在限时内观察到对方进入临界区（共享读成立）。
 * @priority P0
 */
TEST(ReadWriteLock, TwoReadersHoldReadLockConcurrently)
{
    ReadWriteLock lock;
    std::atomic<int> readersInside{0};

    auto reader = [&lock, &readersInside]() -> bool {
        lock.readLock();
        readersInside.fetch_add(1);
        // 等待另一个读者也进入：若读锁是互斥的，这里最多自旋 kWaitLoopMs 后失败
        for (int i = 0; i < kWaitLoopMs && readersInside.load() < 2; i += kWaitStepMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(kWaitStepMs));
        }
        const bool bothIn = (readersInside.load() >= 2);
        lock.readUnLock();
        return bothIn;
    };

    std::future<bool> f1 = std::async(std::launch::async, reader);
    std::future<bool> f2 = std::async(std::launch::async, reader);

    EXPECT_TRUE(f1.get());
    EXPECT_TRUE(f2.get());
}

/**
 * @test   ReadWriteLock.WriterLockBlocksReadersUntilWriteUnLock
 * @brief  并发：写者持锁期间读者必须被阻塞，写者释放后才放行。
 * @details 目的：防止"写者未独占时读者读走了写了一半的数据"这一数据竞争。
 *          设计思路：异步写者持锁 150ms 后释放；读者线程与写者同时启动，
 *          在写者仍在持锁的时间窗内（60ms）读者不得完成——若读写互斥失效，
 *          读者会立即拿到锁并提前返回。
 * @expected 写者持锁期间读者处于等待（future 未就绪），写者释放后读者完成。
 * @priority P0
 */
TEST(ReadWriteLock, WriterLockBlocksReadersUntilWriteUnLock)
{
    ReadWriteLock lock;
    std::atomic<bool> writeAcquired{false};

    std::future<bool> writer = std::async(std::launch::async, [&lock, &writeAcquired]() -> bool {
        lock.writeLock();
        writeAcquired.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        lock.writeUnLock();
        return true;
    });

    // 等写者确实拿到锁
    for (int i = 0; i < kWaitLoopMs && !writeAcquired.load(); i += kWaitStepMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kWaitStepMs));
    }
    ASSERT_TRUE(writeAcquired.load());

    // 读者与写者并发启动：在写者释放（150ms）前的 60ms 窗口内，读者不应完成
    std::future<bool> reader = std::async(std::launch::async, [&lock]() -> bool {
        lock.readLock();
        lock.readUnLock();
        return true;
    });

    EXPECT_EQ(reader.wait_for(std::chrono::milliseconds(60)), std::future_status::timeout);
    EXPECT_TRUE(writer.get());
    EXPECT_TRUE(reader.get());
}
/// @endcond