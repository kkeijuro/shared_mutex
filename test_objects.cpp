#include <cstddef>
#include <functional>
#include <iostream>
#include <stdint.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include "test_objects.hpp"
#include "shared_lock.hpp"

uint32_t MemorySpace::DEFAULT_SIZE = 1024*1024*1024; //1M

static MemorySpace* _memory_space = NULL;

uint32_t MemorySpace::_RSLEEP = 1*1000; // 1 ms
uint32_t MemorySpace::_WSLEEP = 5*1000; //5 ms

MemorySpace* get_memory_space() {
	if(_memory_space == NULL) _memory_space = new MemorySpace();
	return _memory_space; 	
};
MemorySpace::MemorySpace(): _max_size(DEFAULT_SIZE), _rw_position(0){
	_memory_space = new uint8_t[DEFAULT_SIZE];
};

MemorySpace::MemorySpace(uint32_t size): _max_size(size), _rw_position(0) {
	_memory_space = new uint8_t[size];
};

std::ostream& operator<<(std::ostream& os, const MemorySpace& memory) {
	std::unique_lock<std::mutex> lk(memory._mutex);	
	os<<"Memory Space: "<<memory._rw_position<<std::endl;
	for(uint32_t index = 0; index < memory._rw_position;) {
		os<<"index: "<< std::dec<<index;		
		for(uint32_t index_2 = 0; index_2 < 10; index_2++) {
			os<< std::hex<<" 0x"<<memory._memory_space[index + index_2];
		}
		os<<std::endl;
		index += 10;
	}
	os<<std::dec;
	return os;
};

void MemorySpace::restartMemory(){
	std::unique_lock<std::mutex> lk(_mutex);
	delete[] _memory_space;
	this->_rw_position = 0;
	_memory_space = new uint8_t[_max_size];
};

size_t MemorySpace::getSize() const {
	std::unique_lock<std::mutex> lk(_mutex);
	return this->_rw_position;
};

size_t MemorySpace::read(uint8_t* buffer, size_t size) {
		{
		std::unique_lock<std::mutex> lk(_mutex);
		if(!((_rw_position - size) < 0)) return 0;
		memcpy(buffer, _memory_space + _rw_position - size, size);
		//_rw_position -= size;
	}
	//Simulate X time on non shared resource
	usleep(MemorySpace::_RSLEEP);
	return size;
};

size_t MemorySpace::write(uint8_t* buffer, size_t size) {
	//std::cout<<"position: "<< _rw_position<<" size: "<< _max_size<<std::endl;	
		{
		std::unique_lock<std::mutex> lk(_mutex);
		if(!((_rw_position + size) > this->_max_size)) {
			memcpy(_memory_space + _rw_position, buffer, size);
			_rw_position += size;
			}
		}
	//Simulate X time on non shared resource
	usleep(MemorySpace::_WSLEEP);
	return size;
};


CharDataGenerator::CharDataGenerator(uint8_t value): _value(value){};

size_t CharDataGenerator::getData(uint8_t* data){
	data[0] = _value;
	return 1;
};

/*
*/

RWOut::RWOut(){
	_out = false;
};

RWOut::~RWOut(){};	

void RWOut::reset(){
	std::unique_lock<std::mutex> lk(_lock);
	_out = false;
};


void RWOut::set(){
	std::unique_lock<std::mutex> lk(_lock);
	_out = true;
};

bool RWOut::status(){
	std::unique_lock<std::mutex> lk(_lock);
	return _out;
};


/*
READER
*/
uint32_t Reader::_SLEEP = 1*1000;

Reader::Reader(SharedLock* shared_lock): _lock(shared_lock){
	_memory_space = get_memory_space();
};

void Reader::readContinously() {
	_thread = new std::thread(&Reader::continousRead, this);        
};

void Reader::stop() {
	if (_thread == NULL and !_thread->joinable()) return;
	_out.set();
	_thread->join();
	_out.reset();
	delete _thread;
};

void Reader::continousRead(){
	_lock->registerThread();
	while(!_out.status()) {
		_lock->rSharedLock();
		size_t size = _memory_space->getSize();
		uint8_t* buffer = new uint8_t[size];		
		_memory_space->read(buffer, size);
		_lock->rSharedUnlock();
		usleep(Reader::_SLEEP);
		delete buffer;
	}
	_lock->unregisterThread();
};

size_t Reader::punctualRead(uint8_t* buffer, size_t size){
	_lock->rSharedLock();
	size_t ret = _memory_space->read(buffer, size);
	_lock->rSharedUnlock();
	return ret;
};

/*
WRITER
*/
uint32_t Writer::_SLEEP = 1*1000;

Writer::Writer(SharedLock* shared_lock): _lock(shared_lock){
	_data_generator = new CharDataGenerator('a');
	_memory_space = get_memory_space();
	_thread = NULL;
};


void Writer::setDataGenerator(DataGenerator* data_generator){
	delete _data_generator;
	_data_generator = data_generator;
};

void Writer::stop(){
	if (_thread == NULL and !_thread->joinable()) return;
	_out.set();
	_thread->join();
	_out.reset();
	delete _thread;
};

void Writer::writeContinously() {
	_thread = new std::thread(&Writer::continousWrite, this);
};



void Writer::continousWrite(){
	uint8_t* buffer = new uint8_t[15];
	_lock->registerThread();	
	while(!_out.status()) {
		size_t size = _data_generator->getData(buffer);
		//std::cout<<"Writing: "<< buffer<<std::endl;
		_lock->wSharedLock();
		_memory_space->write(buffer, size);
		_lock->wSharedUnlock();
		usleep(Writer::_SLEEP);
	}
	_lock->unregisterThread();	
	delete[] buffer;
};
