#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <stdint.h>
#include <thread>
#include <vector>

#pragma once

enum class PreferencePolicy {
	XCLUSIVE, // ONLY ONE THREAD, THREAD IS SELECTED ONE BY ONE RANDOMLY
	ROUNDROBIN, // ONLY ONE THREAD, THREAD ARE SELECTED IN ROUND ROBIN
	READER,
	WRITER,
	NONE,
};


class SharedLock {
	public:
	SharedLock(PreferencePolicy policy);
	~SharedLock(){};
	
	//exclusive Access
	void exclusiveLock();
	bool tryExclusiveLock();
	bool tryExclusiveLock(uint16_t timeout);
	void exclusiveUnlock();
	
	//read Access	
	void rSharedLock();
	bool rTrySharedLock();
	bool rTrySharedLock(uint16_t timeout);
	void rSharedUnlock();
	
	//write Access	
	void wSharedLock();
	bool wTrySharedLock();
	bool wTrySharedLock(uint16_t timeout);
	void wSharedUnlock();

	int32_t getNumberWriters() const;
	int32_t getNumberReaders() const;
	int32_t getNumberFutureReaders() const;

	void lockReaders();
	void lockWriters();
	void lockShared();
	void unlockReaders();
	void unlockWriters();
	void unlockShared();
	//Just wanted to test a Round Robin
	void registerThread();
	void unregisterThread();
	void notify();
	//void rSharedLock(uint32_t thread_id);
	//void wSharedLock(uint32_t thread_id);
	static void setLimitReaders(int32_t limit_readers);
	static int32_t getLimitReaders();	
	static const int32_t NO_LIMIT_READERS;
	private:
	//void sharedLock(){};
	//bool trySharedLock(){};
	//void sharedUnlock(){};
	//
	//Also for Round Robin
	std::thread::id getActualTurn();
	bool _checkThreadRunnable();

	typedef std::function<bool(SharedLock*)> f_policy;
	static SharedLock::f_policy getReadPolicy(PreferencePolicy policy);
	static SharedLock::f_policy getWritePolicy(PreferencePolicy policy);
	static std::mutex _static_lock;
	static int32_t _limit_readers;
	std::condition_variable _cv;
	bool _exclusive_acquired;
	bool _exclusive_asked;
	int32_t _future_readers;
	bool _locked_readers;
	bool _locked_writers;
	mutable std::mutex _lock;
	SharedLock::f_policy _policy_read;
	SharedLock::f_policy _policy_write;
	std::mutex _t_lock;
	std::vector<std::thread::id> _round_robin_turn;
	//We save actual threads ID to avoid thread lock reuse which cause deadlock
	std::set<std::thread::id> _threads_running;
	int32_t _readers;
	int32_t _turn;
	int32_t _writers;
};
