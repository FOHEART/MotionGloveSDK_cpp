/**
 * @file  readWriteLock.h
 * @brief 轻量读写锁（ReadWriteLock）：多读单写，可选写者优先。
 * @ingroup mgsdk_sync
 *
 * @details
 * 模块边界：基于 std::mutex + std::condition_variable 的共享读 / 互斥写锁，
 * 供 SDK 内部保护数据流缓存等共享数据；头文件内联实现，无外部依赖。
 * 线程安全：锁本身线程安全；跨线程配对使用（哪个线程加锁，哪个线程解锁）。
 *
 * 约定：
 *  - 多个读者可同时持有读锁；写者与其它任何线程（读/写）互斥；
 *  - 构造参数 isPreferWriter = true 时，有写者等待时新读者会被挂起，
 *    避免读流饥饿写者（默认 false，偏向吞吐）；
 *  - 必须严格配对调用：readLock()/readUnLock()、writeLock()/writeUnLock()；
 *    不支持同线程重入（递归加锁会自锁）；
 *  - 不提供 tryLock：调用方应保证持锁时间可控，避免长时间阻塞接收线程。
 */
#ifndef __MVSDK_READWRITELOCK
#define __MVSDK_READWRITELOCK

#include <mutex>
#include <condition_variable>

/**
 * @brief 读写锁：允许多个读者并发，写者独占，可选写者优先。
 * @details 状态由四个计数器组成（reading/writing/readWaiting/writeWaiting），
 *          全部受内部互斥量保护；条件唤醒策略：写者释放后唤醒全部等待者，
 *          读者全部退出且无写者竞争时唤醒一个等待者。
 */
class ReadWriteLock {
private:
	int readWaiting = 0;  ///< 等待加读锁的线程数。
	int writeWaiting = 0; ///< 等待加写锁的线程数。
	int reading = 0;      ///< 正在持有读锁的线程数。
	int writing = 0;      ///< 正在持有写锁的线程数（0 或 1）。
	std::mutex mx;        ///< 保护以上计数器的互斥量。
	std::condition_variable cond; ///< 状态变化时的唤醒通道。
	bool preferWriter;    ///< true 时写者优先：有写者等待则新读者挂起。

public:
	/**
	 * @brief 构造读写锁。
	 * @param[in] isPreferWriter 是否写者优先，默认 false（偏向读吞吐）。
	 */
	ReadWriteLock(bool isPreferWriter = false) :preferWriter(isPreferWriter) {}

	/**
	 * @brief 加读锁（共享）：阻塞直到无写者持有且（写者优先模式下）无写者等待。
	 * @note 调用方必须在完成后调用 readUnLock()，否则写者将永久饥饿。
	 */
	void readLock() {
		std::unique_lock<std::mutex>lock(mx);
		++readWaiting;
		cond.wait(lock, [&]() {return writing <= 0 && (!preferWriter || writeWaiting <= 0); });
		++reading;
		--readWaiting;
	}

	/**
	 * @brief 加写锁（独占）：阻塞直到既无读者也无写者。
	 * @note 调用方必须在完成后调用 writeUnLock()；同线程重入会自锁。
	 */
	void writeLock() {
		std::unique_lock<std::mutex>lock(mx);
		++writeWaiting;
		cond.wait(lock, [&]() {return reading <= 0 && writing <= 0; });
		++writing;
		--writeWaiting;
	}

	/** @brief 释放读锁；全部读者退出时唤醒一个等待的写者。 */
	void readUnLock() {
		std::unique_lock<std::mutex>lock(mx);
		--reading;
		//当前没有读者时，唤醒一个写者
		if (reading <= 0)
			cond.notify_one();
	}

	/** @brief 释放写锁；唤醒全部等待的读者与写者（由它们重新竞争）。 */
	void writeUnLock() {
		std::unique_lock<std::mutex>lock(mx);
		--writing;
		//唤醒所有读者、写者
		cond.notify_all();
	}
};


#endif