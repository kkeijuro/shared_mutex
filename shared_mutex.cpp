#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>

#include "shared_mutex.hpp"

using namespace std::chrono;

SharedMutex::SharedMutex(PreferencePolicy policy):  _exclusive_acquired(false), _future_readers(0), _readers(0), _turn(0), _writers(0){
	this->_read_policy = SharedMutex::getReadPolicy(policy);
	this->_write_policy = SharedMutex::getWritePolicy(policy);
};

/*Abstraction for pluging in Read Policy*/
SharedMutex::f_policy SharedMutex::getReadPolicy(PreferencePolicy policy){
	switch(policy) {
		case PreferencePolicy::XCLUSIVE: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;			
			return ((_mutex->_readers + _mutex->_writers) == 0); 
		}; 
		/*
		FOR NONE all readers we can get except if one Writer
		*/
                case PreferencePolicy::NONE: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;
			if(_mutex->_writers > 0) return false;
			return true;			 
		}; 
		case PreferencePolicy::ROUNDROBIN: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;			
			if(thread_uid == 0) return (_mutex->_readers + _mutex->_writers) == 0;
			return (((_mutex->_readers + _mutex->_writers) == 0) && (_mutex->getActualTurn() == thread_uid));
		};
		/*If a reader already holds a shared lock, 
		any writers will wait until all current and future readers have finished
		*/
		case PreferencePolicy::READER: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;
			//if(_mutex->_writers > 0) return false;			
			return true;
		};

	        /*If a reader already holds a shared lock, 
		no additional readers will acquire until all writers have finished
		*/
		case PreferencePolicy::WRITER: return [](SharedMutex* _mutex, uint32_t thread_uid){		
			if(_mutex->_exclusive_acquired) return false;
			if((_mutex->_readers == 0) and (_mutex->_writers == 0)) return true;			
			return false;		
		};
	}
};

/*Abstraction for pluging in Write Policy*/
SharedMutex::f_policy SharedMutex::getWritePolicy(PreferencePolicy policy){
	switch(policy) {
		case PreferencePolicy::XCLUSIVE: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;			
			return ((_mutex->_readers + _mutex->_writers) == 0); 
		}; 
		/*
		FOR NONE Maximum 1 writer N readers
		*/
                case PreferencePolicy::NONE: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;
			if((_mutex->_writers == 0) and (_mutex->_readers == 0)) return true;
			return false;			 
		}; 
		case PreferencePolicy::ROUNDROBIN: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;			
			if(thread_uid == 0) return (_mutex->_readers + _mutex->_writers) == 0;
			return (((_mutex->_readers + _mutex->_writers) == 0) && (_mutex->getActualTurn() == thread_uid));
		};
		/*If a reader already holds a shared lock, 
		any writers will wait until all current and future readers have finished
		*/		
		case PreferencePolicy::READER: return [](SharedMutex* _mutex, uint32_t thread_uid){
			if(_mutex->_exclusive_acquired) return false;
			if(_mutex->_readers == 0  and _mutex->_future_readers == 0) return true;
			return false;
		};
	        /*If a reader already holds a shared lock, 
		no additional readers will acquire until all writers have finished
		*/
		case PreferencePolicy::WRITER: return [](SharedMutex* _mutex, uint32_t thread_uid){								
			if(_mutex->_exclusive_acquired) return false;
			//if(_mutex->_readers > 1) return false;
			return true;		
		};
	};
};

uint32_t SharedMutex::getNumberWriters() const{
	return _writers;
};

uint32_t SharedMutex::getNumberReaders() const{
	return _readers;
};

bool SharedMutex::_checkThreadRunnable() {
	if(this->_threads_running.find(std::this_thread::get_id()) != this->_threads_running.end()) {
		return false;
	}
	return true;
}

void SharedMutex::exclusiveLock() {
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	std::unique_lock<std::mutex> lk(_mutex);
	_cv.wait(lk, [this] {return ((!this->_exclusive_acquired) and (this->_writers == 0) and (this->_readers == 0));});
	this->_exclusive_acquired = true;
	_threads_running.insert(std::this_thread::get_id());
};

bool SharedMutex::tryExclusiveLock() {
	return this->tryExclusiveLock(0);
};

bool SharedMutex::tryExclusiveLock(uint16_t timeout) {
	if(!_checkThreadRunnable()) return false;
	std::unique_lock<std::mutex> lk(_mutex);
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return ((!this->_exclusive_acquired) and (this->_writers == 0) and (this->_readers == 0));});
	if(ret) {
		this->_exclusive_acquired = true;
		_threads_running.insert(std::this_thread::get_id());
	}
	return ret;
};

void SharedMutex::exclusiveUnlock() {
	_threads_running.erase(std::this_thread::get_id());	
	this->_exclusive_acquired = false;
	_cv.notify_all();
};

uint32_t SharedMutex::getActualTurn(){
	return _round_robin_turn.operator[](_turn);
};

void SharedMutex::registerThread(uint32_t thread_id){
	_round_robin_turn.push_back(thread_id);
};

void SharedMutex::rSharedLock(){
	this->rSharedLock(0);
};

/*Just for test the ROUND ROBIN*/
void SharedMutex::rSharedLock(uint32_t thread_id){
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	std::unique_lock<std::mutex> lk(_mutex);
	_future_readers++;
	_cv.wait(lk, [this, thread_id] {return _read_policy(this, thread_id);});
	_threads_running.insert(std::this_thread::get_id());
	_readers++;
	_future_readers--;	
	_turn++;
	if(_turn >= _round_robin_turn.size()) _turn = 0;
};

bool SharedMutex::rTrySharedLock() {
	return this->rTrySharedLock(0);
};
	
bool SharedMutex::rTrySharedLock(uint16_t timeout){
	if(!_checkThreadRunnable()) return false;	
	std::unique_lock<std::mutex> lk(_mutex);
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return _read_policy(this, 0);});
	if(ret) {
		_threads_running.insert(std::this_thread::get_id());
		_readers++;
	}
	//std::cout<<"Readers: " << _readers << " Writers: "<< _writers<<std::endl;
	return ret;
};


void SharedMutex::rSharedUnlock(){
	std::unique_lock<std::mutex> lk(_mutex);
	_readers--;
	_threads_running.erase(std::this_thread::get_id());
	_cv.notify_all();
};

void SharedMutex::wSharedLock(){
	this->wSharedLock(0);	
};


/*Just for test the ROUND ROBIN*/
void SharedMutex::wSharedLock(uint32_t thread_id){
	if(!_checkThreadRunnable()) throw std::runtime_error("Unable to relock thread");
	std::unique_lock<std::mutex> lk(_mutex);
	_cv.wait(lk, [this, thread_id] {return _write_policy(this, thread_id);});
	_threads_running.insert(std::this_thread::get_id());	
	_writers++;
	_turn++;
	if(_turn >= _round_robin_turn.size()) _turn = 0;
};

bool SharedMutex::wTrySharedLock() {
	return this->wTrySharedLock(0);
}

bool SharedMutex::wTrySharedLock(uint16_t timeout){
	if(!_checkThreadRunnable()) return false;
	std::unique_lock<std::mutex> lk(_mutex);
	bool ret = _cv.wait_until(lk, system_clock::now() + std::chrono::milliseconds(timeout), [this] {return _write_policy(this, 0);});
	if(ret) {
		_threads_running.insert(std::this_thread::get_id());	
		_writers++;
	}
	return ret;
};

void SharedMutex::wSharedUnlock(){
	std::unique_lock<std::mutex> lk(_mutex);
	_writers--;
	_threads_running.erase(std::this_thread::get_id());
	_cv.notify_all();
};
