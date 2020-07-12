#include <algorithm>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>

#include "shared_lock.hpp"

using namespace std::chrono;

const int32_t SharedLock::NO_LIMIT_READERS = -1;

int32_t SharedLock::_limit_readers = SharedLock::NO_LIMIT_READERS;

void SharedLock::setLimitReaders(int32_t limit_readers) {
	SharedLock::_limit_readers = limit_readers;
};

SharedLock::SharedLock(PreferencePolicy policy):  _exclusive_acquired(false), _future_readers(0), _readers(0), _turn(0), _writers(0){
	this->_read_policy = SharedLock::getReadPolicy(policy);
	this->_write_policy = SharedLock::getWritePolicy(policy);
};

/*Abstraction for pluging in Read Policy*/
SharedLock::f_policy SharedLock::getReadPolicy(PreferencePolicy policy){
	switch(policy) {
		case PreferencePolicy::XCLUSIVE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;
			if(SharedLock::_limit_readers != SharedLock::NO_LIMIT_READERS and _limit_readers >= _lock->_readers) return false;			
			return ((_lock->_readers + _lock->_writers) == 0); 
		}; 
		/*
		FOR NONE all readers we can get except if one Writer
		*/
                case PreferencePolicy::NONE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;
			if(_lock->_writers > 0) return false;
			if(SharedLock::_limit_readers != SharedLock::NO_LIMIT_READERS and _lock->_readers >= _limit_readers) return false;
			return true;			 
		}; 
		case PreferencePolicy::ROUNDROBIN: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;			
			if(SharedLock::_limit_readers != SharedLock::NO_LIMIT_READERS and _lock->_readers >= _limit_readers) return false;
			return (((_lock->_readers + _lock->_writers) == 0) and (_lock->getActualTurn() == std::this_thread::get_id()));
		};
		/*If a reader already holds a shared lock, 
		any writers will wait until all current and future readers have finished
		*/
		case PreferencePolicy::READER: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;
			if(SharedLock::_limit_readers != SharedLock::NO_LIMIT_READERS and _lock->_readers >= _limit_readers) return false;		
			return true;
		};

	        /*If a reader already holds a shared lock, 
		no additional readers will acquire until all writers have finished
		*/
		case PreferencePolicy::WRITER: return [](SharedLock* _lock){		
			if(_lock->_exclusive_acquired) return false;
			if(SharedLock::_limit_readers != SharedLock::NO_LIMIT_READERS and _lock->_readers >= _limit_readers) return false;
			if((_lock->_readers >= 1) and (_lock->_writers > 0)) return false;
			return true;		
		};
		/* default is freeride but clearly will never reach this point*/
		default: return [](SharedLock* _lock){		
			return true;		
		};
	}
};

/*Abstraction for pluging in Write Policy*/
SharedLock::f_policy SharedLock::getWritePolicy(PreferencePolicy policy){
	switch(policy) {
		case PreferencePolicy::XCLUSIVE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;			
			return ((_lock->_readers + _lock->_writers) == 0); 
		}; 
		/*
		FOR NONE Maximum 1 writer N readers
		*/
                case PreferencePolicy::NONE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;
			if((_lock->_writers == 0) and (_lock->_readers == 0)) return true;
			return false;			 
		}; 
		case PreferencePolicy::ROUNDROBIN: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;			
			return (((_lock->_readers + _lock->_writers) == 0) && (_lock->getActualTurn() == std::this_thread::get_id()));
		};
		/*If a reader already holds a shared lock, 
		any writers will wait until all current and future readers have finished
		*/		
		case PreferencePolicy::READER: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired) return false;
			if(_lock->_readers == 0  and _lock->_future_readers == 0) return true;
			return false;
		};
	        /*If a reader already holds a shared lock, 
		no additional readers will acquire until all writers have finished
		*/
		case PreferencePolicy::WRITER: return [](SharedLock* _lock){								
			if(_lock->_exclusive_acquired) return false;
			//if(_lock->_readers > 1) return false;
			return true;		
		};
		/* default is freeride but clearly will never reach this point*/
		default: return [](SharedLock* _lock){		
			return true;		
		};
	};
};

uint32_t SharedLock::getNumberWriters() const{
	return _writers;
};

uint32_t SharedLock::getNumberReaders() const{
	return _readers;
};

uint32_t SharedLock::getNumberFutureReaders() const {
	return _future_readers;	
};

bool SharedLock::_checkThreadRunnable() {
	if(this->_threads_running.find(std::this_thread::get_id()) != this->_threads_running.end()) {
		return false;
	}
	return true;
};

void SharedLock::exclusiveLock() {
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	std::unique_lock<std::mutex> lk(_lock);
	_cv.wait(lk, [this] {return ((!this->_exclusive_acquired) and (this->_writers == 0) and (this->_readers == 0));});
	this->_exclusive_acquired = true;
	_threads_running.insert(std::this_thread::get_id());
};

bool SharedLock::tryExclusiveLock() {
	return this->tryExclusiveLock(0);
};

bool SharedLock::tryExclusiveLock(uint16_t timeout) {
	if(!_checkThreadRunnable()) return false;
	std::unique_lock<std::mutex> lk(_lock);
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return ((!this->_exclusive_acquired) and (this->_writers == 0) and (this->_readers == 0));});
	if(ret) {
		this->_exclusive_acquired = true;
		_threads_running.insert(std::this_thread::get_id());
	}
	return ret;
};

void SharedLock::exclusiveUnlock() {
	_threads_running.erase(std::this_thread::get_id());	
	this->_exclusive_acquired = false;
	_cv.notify_all();
};

void SharedLock::rSharedLock(){
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	std::unique_lock<std::mutex> lk(_lock);
	_future_readers++;
	_cv.wait(lk, [this] {return _read_policy(this);});
	_threads_running.insert(std::this_thread::get_id());
	_readers++;
	_future_readers--;	
	_turn++;
	if(_turn >= _round_robin_turn.size()) _turn = 0;
};

bool SharedLock::rTrySharedLock() {
	return this->rTrySharedLock(0);
};
	
bool SharedLock::rTrySharedLock(uint16_t timeout){
	if(!_checkThreadRunnable()) return false;	
	std::unique_lock<std::mutex> lk(_lock);
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return _read_policy(this);});
	if(ret) {
		_threads_running.insert(std::this_thread::get_id());
		_readers++;
	}
	//std::cout<<"Readers: " << _readers << " Writers: "<< _writers<<std::endl;
	return ret;
};


void SharedLock::rSharedUnlock(){
	std::unique_lock<std::mutex> lk(_lock);
	_readers--;
	_threads_running.erase(std::this_thread::get_id());
	_cv.notify_all();
};

void SharedLock::wSharedLock(){
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	std::unique_lock<std::mutex> lk(_lock);
	_cv.wait(lk, [this] {return _write_policy(this);});
	_threads_running.insert(std::this_thread::get_id());	
	_writers++;
	_turn++;
	if(_turn >= _round_robin_turn.size()) _turn = 0;
};

bool SharedLock::wTrySharedLock() {
	return this->wTrySharedLock(0);
}

bool SharedLock::wTrySharedLock(uint16_t timeout){
	if(!_checkThreadRunnable()) return false;
	std::unique_lock<std::mutex> lk(_lock);
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return _write_policy(this);});
	if(ret) {
		_threads_running.insert(std::this_thread::get_id());	
		_writers++;
	}
	return ret;
};

void SharedLock::wSharedUnlock(){
	std::unique_lock<std::mutex> lk(_lock);
	_writers--;
	_threads_running.erase(std::this_thread::get_id());
	_cv.notify_all();
};


/*Just for ROUND ROBIN*/
std::thread::id SharedLock::getActualTurn(){
	std::unique_lock<std::mutex> lk(_register_lock);
	return _round_robin_turn.operator[](_turn);
};

void SharedLock::registerThread(){
	std::unique_lock<std::mutex> lk(_register_lock);
	_round_robin_turn.push_back(std::this_thread::get_id());
};

void SharedLock::unregisterThread(){
	std::unique_lock<std::mutex> lk(_register_lock);
	_round_robin_turn.erase(std::remove(_round_robin_turn.begin(), _round_robin_turn.end(), std::this_thread::get_id()), _round_robin_turn.end());
	//Avoid loosing order when a element is removed	
	_turn--;
	if(_turn < 0) _turn = 0;
};

