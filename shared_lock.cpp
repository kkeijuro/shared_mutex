#include <algorithm>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>

#include "shared_lock.hpp"

using namespace std::chrono;

const int32_t SharedLock::NO_LIMIT_READERS = -1;
std::mutex SharedLock::_static_lock;

int32_t SharedLock::_limit_readers = SharedLock::NO_LIMIT_READERS;

void SharedLock::setLimitReaders(int32_t limit_readers) {
	std::unique_lock<std::mutex> lk(SharedLock::_static_lock);
	SharedLock::_limit_readers = limit_readers;
};

int32_t SharedLock::getLimitReaders() {
	std::unique_lock<std::mutex> lk(SharedLock::_static_lock);
	return SharedLock::_limit_readers;
};

SharedLock::SharedLock(PreferencePolicy policy):  _exclusive_acquired(false), _exclusive_asked(false), _future_readers(0), _locked_readers(false), _locked_writers(false), _readers(0), _turn(0), _writers(0){
	this->_policy_read = SharedLock::getReadPolicy(policy);
	this->_policy_write = SharedLock::getWritePolicy(policy);
};

/*Abstraction for pluging in Read Policy*/
SharedLock::f_policy SharedLock::getReadPolicy(PreferencePolicy policy){
	switch(policy) {
		case PreferencePolicy::XCLUSIVE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_readers) return false;
			if(SharedLock::getLimitReaders() != SharedLock::NO_LIMIT_READERS and _limit_readers >= _lock->_readers) return false;			
			return ((_lock->_readers + _lock->_writers) == 0); 
		}; 
		/*
		FOR NONE all readers we can get except if one Writer
		*/
                case PreferencePolicy::NONE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_readers) return false;
			if(_lock->_writers > 0) return false;
			if(SharedLock::getLimitReaders() != SharedLock::NO_LIMIT_READERS and _lock->_readers >= getLimitReaders()) return false;
			return true;			 
		}; 
		case PreferencePolicy::ROUNDROBIN: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_readers) return false;			
			if(SharedLock::getLimitReaders() != SharedLock::NO_LIMIT_READERS and _lock->_readers >= getLimitReaders()) return false;
			return (((_lock->_readers + _lock->_writers) == 0) and (_lock->getActualTurn() == std::this_thread::get_id()));
		};
		/*If a reader already holds a shared lock, 
		any writers will wait until all current and future readers have finished
		*/
		case PreferencePolicy::READER: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_readers) return false;
			if(SharedLock::getLimitReaders() != SharedLock::NO_LIMIT_READERS and _lock->_readers >= getLimitReaders()) return false;					
			return true;
		};

	        /*If a reader already holds a shared lock, 
		no additional readers will acquire until all writers have finished
		*/
		case PreferencePolicy::WRITER: return [](SharedLock* _lock){		
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_readers) return false;
			if(SharedLock::getLimitReaders() != SharedLock::NO_LIMIT_READERS and _lock->_readers >= getLimitReaders()) return false;
			if((_lock->_readers >= 1) and (_lock->_writers > 0)) return false;
			return true;		
		};
		/* default is freeride but clearly will never reach this point*/
		default: return [](SharedLock* _lock){		
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_readers) return false;
			return true;		
		};
	}
};

/*Abstraction for pluging in Write Policy*/
SharedLock::f_policy SharedLock::getWritePolicy(PreferencePolicy policy){
	switch(policy) {
		case PreferencePolicy::XCLUSIVE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_writers) return false;			
			return ((_lock->_readers + _lock->_writers) == 0); 
		}; 
		/*
		FOR NONE Maximum 1 writer N readers
		*/
                case PreferencePolicy::NONE: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_writers) return false;
			if((_lock->_writers == 0) and (_lock->_readers == 0)) return true;
			return false;			 
		}; 
		case PreferencePolicy::ROUNDROBIN: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_writers) return false;
			return (((_lock->_readers + _lock->_writers) == 0) && (_lock->getActualTurn() == std::this_thread::get_id()));
		};
		/*If a reader already holds a shared lock, 
		any writers will wait until all current and future readers have finished
		*/		
		case PreferencePolicy::READER: return [](SharedLock* _lock){
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_writers) return false;
			if(_lock->_readers == 0  and _lock->_future_readers == 0) return true;
			return false;
		};
	        /*If a reader already holds a shared lock, 
		no additional readers will acquire until all writers have finished
		*/
		case PreferencePolicy::WRITER: return [](SharedLock* _lock){								
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_writers) return false;
			//if(_lock->_readers > 1) return false;
			return true;		
		};
		/* default is freeride but clearly will never reach this point*/
		default: return [](SharedLock* _lock){		
			if(_lock->_exclusive_acquired || _lock->_exclusive_asked) return false;
			if(_lock->_locked_writers) return false;
			return true;		
		};
	};
};

int32_t SharedLock::getNumberWriters() const{
	std::unique_lock<std::mutex> lk(_lock);
	return _writers;
};

int32_t SharedLock::getNumberReaders() const{
	std::unique_lock<std::mutex> lk(_lock);
	return _readers;
};

int32_t SharedLock::getNumberFutureReaders() const {
	std::unique_lock<std::mutex> lk(_lock);
	return _future_readers;	
};

void SharedLock::lockReaders(){
	std::unique_lock<std::mutex> lk(_lock);
	_locked_readers = true;
	_cv.notify_all();
};

void SharedLock::lockShared(){
	std::unique_lock<std::mutex> lk(_lock);
	_locked_readers = true;
	_locked_writers = true;
	_cv.notify_all();
};

void SharedLock::lockWriters(){
	std::unique_lock<std::mutex> lk(_lock);
	_locked_writers = true;
	_cv.notify_all();
};

void SharedLock::unlockReaders(){
	std::unique_lock<std::mutex> lk(_lock);
	_locked_readers = false;
	_cv.notify_all();
};


void SharedLock::unlockShared(){
	std::unique_lock<std::mutex> lk(_lock);
	_locked_readers = false;
	_locked_writers = false;
	_cv.notify_all();
};

void SharedLock::unlockWriters(){
	std::unique_lock<std::mutex> lk(_lock);
	_locked_writers = false;
	_cv.notify_all();
};

bool SharedLock::_checkThreadRunnable() {
	if(this->_threads_running.find(std::this_thread::get_id()) != this->_threads_running.end()) {
		return false;
	}
	return true;
};

void SharedLock::exclusiveLock() {
	std::unique_lock<std::mutex> lk(_lock);
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	_exclusive_asked = true;
	_cv.wait(lk, [this] {return ((!this->_exclusive_acquired) and (this->_writers == 0) and (this->_readers == 0));});
	_exclusive_asked = false;	
	this->_exclusive_acquired = true;
	_threads_running.insert(std::this_thread::get_id());
};

bool SharedLock::tryExclusiveLock() {
	return this->tryExclusiveLock(0);
};

bool SharedLock::tryExclusiveLock(uint16_t timeout) {
	std::unique_lock<std::mutex> lk(_lock);
	_exclusive_asked = true;
	if(!_checkThreadRunnable()) return false;
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return ((!this->_exclusive_acquired) and (this->_writers == 0) and (this->_readers == 0));});
	if(ret) {
		this->_exclusive_acquired = true;
		_threads_running.insert(std::this_thread::get_id());
	}
	_exclusive_asked = false;
	return ret;
};

void SharedLock::exclusiveUnlock() {
	std::unique_lock<std::mutex> lk(_lock);
	_threads_running.erase(std::this_thread::get_id());	
	this->_exclusive_acquired = false;
	_cv.notify_all();
};

void SharedLock::rSharedLock(){
	std::unique_lock<std::mutex> lk(_lock);
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	_future_readers++;
	_cv.wait(lk, [this] {return _policy_read(this);});
	_threads_running.insert(std::this_thread::get_id());
	_readers++;
	_future_readers--;
	//std::unique_lock<std::mutex> turn_lk(_t_lock);
	_turn++;
	if(_turn >= _round_robin_turn.size()) _turn = 0;
};

bool SharedLock::rTrySharedLock() {
	return this->rTrySharedLock(0);
};
	
bool SharedLock::rTrySharedLock(uint16_t timeout){
	std::unique_lock<std::mutex> lk(_lock);
	if(!_checkThreadRunnable()) return false;	
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return _policy_read(this);});
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
	std::unique_lock<std::mutex> lk(_lock);
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	_cv.wait(lk, [this] {return _policy_write(this);});
	_threads_running.insert(std::this_thread::get_id());	
	_writers++;
	//std::unique_lock<std::mutex> turn_lk(_t_lock);	
	_turn++;
	if(_turn >= _round_robin_turn.size()) _turn = 0;
};

bool SharedLock::wTrySharedLock() {
	return this->wTrySharedLock(0);
};

bool SharedLock::wTrySharedLock(uint16_t timeout){
	std::unique_lock<std::mutex> lk(_lock);
	if(!_checkThreadRunnable()) return false;
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return _policy_write(this);});
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

void SharedLock::notify(){
	_cv.notify_all();
};

/*Just for ROUND ROBIN*/
std::thread::id SharedLock::getActualTurn(){
	return _round_robin_turn.operator[](_turn);
};

void SharedLock::registerThread(){
	std::unique_lock<std::mutex> lk(_lock);
	_round_robin_turn.push_back(std::this_thread::get_id());
};

void SharedLock::unregisterThread(){
	std::unique_lock<std::mutex> lk(_lock);
	_round_robin_turn.erase(std::remove(_round_robin_turn.begin(), _round_robin_turn.end(), std::this_thread::get_id()), _round_robin_turn.end());
	//Avoid loosing order when a element is removed	
	_turn--;
	if(_turn < 0) _turn = 0;
};

