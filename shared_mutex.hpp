#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdint.h>
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
	
	void exclusiveLock();
	bool tryExclusiveLock();
	void exclusiveUnlock();
	void rSharedLock(uint32_t thread_id);
	void rSharedLock();
	bool rTrySharedLock();
	void rSharedUnlock();
	void wSharedLock(uint32_t thread_id);
	void wSharedLock();
	bool wTrySharedLock();
	void wSharedUnlock();
	void registerThread(uint32_t thread_id);
	uint32_t getNumberWriters() const;
	uint32_t getNumberReaders() const;	
	private:
	//void sharedLock(){};
	//bool trySharedLock(){};
	//void sharedUnlock(){};
	//
	uint32_t getActualTurn();
	typedef std::function<bool(SharedMutex*, int32_t thread_id)> f_policy;
	static SharedMutex::f_policy getReadPolicy(PreferencePolicy policy);
	static SharedMutex::f_policy getWritePolicy(PreferencePolicy policy);
	std::condition_variable _cv;
	bool _exclusive_acquired;
	SharedMutex::f_policy _read_policy;
	SharedMutex::f_policy _write_policy;
	std::mutex _mutex;
	std::mutex _try_mutex;
	std::vector<uint32_t> _round_robin_turn;
	uint32_t _readers;
	uint32_t _writers;
	uint32_t _turn;
};
