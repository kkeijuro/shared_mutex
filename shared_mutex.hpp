#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <stdint.h>
#include <thread>
#include <vector>

#pragma once

enum class PreferencePolicy {
	XCLUSIVE,
	ROUNDROBIN,	
	READER,
	WRITER,
	NONE,
};


class SharedMutex {
	public:
	SharedMutex(PreferencePolicy policy);
	~SharedMutex(){};
	
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

	uint32_t getNumberWriters() const;
	uint32_t getNumberReaders() const;
	uint32_t getNumberFutureReaders() const;

	//Just wanted to test a Round Robin
	void registerThread();
	void unregisterThread();
	//void rSharedLock(uint32_t thread_id);
	//void wSharedLock(uint32_t thread_id);
	static void setLimitReaders(uint16_t limit_readers);	
	private:
	//void sharedLock(){};
	//bool trySharedLock(){};
	//void sharedUnlock(){};
	//
	//Also for Round Robin
	std::thread::id getActualTurn();
	
	typedef std::function<bool(SharedMutex*)> f_policy;
	static SharedMutex::f_policy getReadPolicy(PreferencePolicy policy);
	static SharedMutex::f_policy getWritePolicy(PreferencePolicy policy);
	static uint16_t _limit_readers;
	bool _checkThreadRunnable();
	std::condition_variable _cv;
	bool _exclusive_acquired;
	SharedMutex::f_policy _read_policy;
	SharedMutex::f_policy _write_policy;
	std::mutex _mutex;
	std::mutex _try_mutex;
	std::mutex _register_mutex;
	std::vector<std::thread::id> _round_robin_turn;
	//We save actual threads ID to avoid thread lock reuse which cause deadlock
	std::set<std::thread::id> _threads_running;
	uint32_t _readers;
	uint32_t _future_readers;
	uint32_t _writers;
	int32_t _turn;
};
