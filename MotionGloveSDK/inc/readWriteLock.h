#ifndef __MVSDK_READWRITELOCK
#define __MVSDK_READWRITELOCK

#include <mutex>
#include <condition_variable>

class ReadWriteLock {
private:
	int readWaiting = 0;  //等待读
	int writeWaiting = 0; //等待写
	int reading = 0; //正在读
	int writing = 0;  //正在写
	std::mutex mx;
	std::condition_variable cond;
	bool preferWriter;  //偏向读
public:
	ReadWriteLock(bool isPreferWriter = false) :preferWriter(isPreferWriter) {}

	void readLock() {
		std::unique_lock<std::mutex>lock(mx);
		++readWaiting;
		cond.wait(lock, [&]() {return writing <= 0 && (!preferWriter || writeWaiting <= 0); });
		++reading;
		--readWaiting;
	}

	void writeLock() {
		std::unique_lock<std::mutex>lock(mx);
		++writeWaiting;
		cond.wait(lock, [&]() {return reading <= 0 && writing <= 0; });
		++writing;
		--writeWaiting;
	}

	void readUnLock() {
		std::unique_lock<std::mutex>lock(mx);
		--reading;
		//当前没有读者时，唤醒一个写者
		if (reading <= 0)
			cond.notify_one();
	}

	void writeUnLock() {
		std::unique_lock<std::mutex>lock(mx);
		--writing;
		//唤醒所有读者、写者
		cond.notify_all();
	}
};


#endif
