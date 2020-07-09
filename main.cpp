#include <algorithm>
#include <cstddef>
#include <iostream>
#include <unistd.h>
#include <vector>
#include "test_objects.hpp"
#include "shared_mutex.hpp"

std::vector<Writer*>& createNWriters(SharedMutex& shared_mutex, uint8_t n) {
	std::vector<Writer*>* w_vector = new std::vector<Writer*>(n);  
	for(uint8_t index = 0; index < n; index++) w_vector->operator[](index) = new Writer(&shared_mutex);
	return *w_vector;
};

void startWriters(std::vector<Writer*>& writers) {
	std::for_each(writers.begin(), writers.end(), [](Writer*& w){std::flush(std::cout);w->writeContinously();});
};


void stopWriters(std::vector<Writer*>& writers) {
	std::for_each(writers.begin(), writers.end(), [](Writer*& w){w->stop();});
};


std::vector<Reader*>& createNReaders(SharedMutex& shared_mutex, uint8_t n) {
	std::vector<Reader*>* r_vector = new std::vector<Reader*>(n);  
	for(uint8_t index = 0; index<n; index++) r_vector->operator[](index) = new Reader(&shared_mutex);
	return *r_vector;
};

void startReaders(std::vector<Reader*>& readers) {
	std::for_each(readers.begin(), readers.end(), [](Reader*& r){r->readContinously();});
};


void stopReaders(std::vector<Reader*>& readers) {
	std::for_each(readers.begin(), readers.end(), [](Reader*& r){r->stop();});
};


void testSimpleWriters() {
	SharedMutex _shared_mutex(PreferencePolicy::ROUNDROBIN);
	auto writers_vector = createNWriters(_shared_mutex, 9);
	startWriters(writers_vector);
	for(uint16_t index = 0; index < 500; index++) {
		std::cout<<"Number of Writers: " << _shared_mutex.getNumberWriters() << std::endl;
		usleep(10*1000);	
	}
	stopWriters(writers_vector);
};

void testRoundRobin() {

	auto memory = get_memory_space();
	memory->restartMemory();
	SharedMutex _shared_mutex(PreferencePolicy::ROUNDROBIN);
	auto writers_vector = createNWriters(_shared_mutex, 3);
	writers_vector.operator[](1)->setDataGenerator(new CharDataGenerator('b'));
	writers_vector.operator[](2)->setDataGenerator(new CharDataGenerator('c'));
	startWriters(writers_vector);
	usleep(5*1000000);
	stopWriters(writers_vector);
	std::cout<<*memory<<std::endl;
};

bool testNonePrevalenceNoExclusive() {
	/*
	Launch N writers
	Launch N readers
	Try to Acquire lock with one Write client
	Test OK: Only One writer with 0 readers
	*/	
	bool ret = true;
	SharedMutex _shared_mutex(PreferencePolicy::NONE);
	auto writers_vector = createNWriters(_shared_mutex, 4);
	auto readers_vector = createNReaders(_shared_mutex, 4);
	startWriters(writers_vector);
	startReaders(readers_vector);
	for(uint16_t index = 0; index < 20; index++) {
		if(_shared_mutex.wTrySharedLock(100) == true) {
			//Wait until all Writer threads stops				
			if(_shared_mutex.getNumberWriters() > 1 || _shared_mutex.getNumberReaders() > 0) {
				std::cout<<"\tAcquiring lock when Unable: Writers: "<<_shared_mutex.getNumberWriters()<<std::endl;			
				ret = false;
			}
			else std::cout<<"\tNumber of Writers: "<<_shared_mutex.getNumberWriters()<<" Number of readers: "<<_shared_mutex.getNumberReaders()<<std::endl;
			_shared_mutex.wSharedUnlock();
		}
		usleep(1*(500000));
	}
	stopReaders(readers_vector);
	stopWriters(writers_vector);
	return ret;
};

bool testReadPrevalenceNoExclusive() {
	/*
	Launch N writers
	Try to Acquire lock with one Read client
	Test OK: No writers should be running when Read acquire Lock
	*/	
	bool ret = true;
	SharedMutex _shared_mutex(PreferencePolicy::READER);
	auto writers_vector = createNWriters(_shared_mutex, 3);
	auto readers_vector = createNReaders(_shared_mutex, 20);
	startWriters(writers_vector);
	startReaders(readers_vector);
	usleep(1*1000000);
	for(uint16_t index = 0; index < 20; index++) {
		if(_shared_mutex.rTrySharedLock(100) == true) {
			//Wait until all Writer threads stops			
			usleep(10*(1000));	
			if(_shared_mutex.getNumberWriters() > 0) {
				std::cout<<"\tAcquiring lock when Unable: Writers: "<<_shared_mutex.getNumberWriters()<<std::endl;
				ret = false;
			}
			else std::cout<<"\tAcquiring lock when Able: Writers: "<<_shared_mutex.getNumberWriters()<<" Readers: "<< _shared_mutex.getNumberReaders()<<std::endl;
			_shared_mutex.rSharedUnlock();
		}
		usleep(1*(100000));
	}
	stopReaders(readers_vector);
	stopWriters(writers_vector);
	return ret;
};

bool testWritePrevalenceNoExclusive() {
	/*
	Launch N writers
	Launch 1 reader
	Try to Acquire lock with one client
	Test OK: If we get Lock but only one reader
	*/
	bool ret = true;
	SharedMutex _shared_mutex(PreferencePolicy::WRITER);
	auto writers_vector = createNWriters(_shared_mutex, 9);
	Reader reader_1(&_shared_mutex);
	startWriters(writers_vector);
	reader_1.readContinously();
	for(uint16_t index = 0; index < 20; index++) {
		if(_shared_mutex.rTrySharedLock(100) == true) {		
			if(_shared_mutex.getNumberReaders() > 1) {
				std::cout<<"\tAcquiring lock when Unable"<<std::endl;			
				ret = false;
			}
			else std::cout<<"\tLock acquired but only one reader"<<std::endl;			
			_shared_mutex.rSharedUnlock();
			usleep(1*(500000));// 0.5 seconds
		}
	}
	stopWriters(writers_vector);
	for(uint16_t index = 0; index < 50; index++) {
		if(_shared_mutex.rTrySharedLock(100) == false) {
			std::cout<<"\tNot Acquiring lock when able"<<std::endl;		
			ret = false;
		}
		else _shared_mutex.rSharedUnlock();
		usleep(1*(1000));	
	}
	reader_1.stop();
	return ret;
};

bool testExclusiveThread() {
	/*
	N writers launched, check that memory is being writed
	begin exclusive thread: check that memory doesnt grow
	stop exclusive thread: check that memory keeps growing
	*/
	auto memory = get_memory_space();
	memory->restartMemory();
	bool ret = true;
	SharedMutex _shared_mutex(PreferencePolicy::WRITER);
	auto writers_vector = createNWriters(_shared_mutex, 9);
	startWriters(writers_vector);
	uint32_t value = memory->getSize();		
	usleep(1*1000000);
	for(uint16_t index = 0; index < 10; index++) {
		uint32_t new_value = memory->getSize();
		std::cout<<"\tLast Value: "<< value<<" New Value: "<<new_value<< std::endl;	
		if(new_value <= value) ret = false;
		value = new_value;
		usleep(1*1000000);
	}
	if(_shared_mutex.tryExclusiveLock(1000) == true) {
		std::cout<<"\tExclusive lock acquired! "<<std::endl;
		uint32_t value = memory->getSize();	
		for(uint16_t index = 0; index < 10; index++) {
			uint32_t new_value = memory->getSize();	
			std::cout<<"\tLast Value: "<< value<<" New Value: "<<new_value<< std::endl;
			if(new_value != value) ret = false;			
			usleep(1*1000000);
		}
		_shared_mutex.exclusiveUnlock();
		std::cout<<"\tExclusive lock released! "<<std::endl;
	}
	value = memory->getSize();	
	usleep(1*1000000);		
	for(uint16_t index = 0; index < 10; index++) {
		uint32_t new_value = memory->getSize();	
		std::cout<<"\tLast Value: "<< value<<" New Value: "<<new_value<< std::endl;
		if(new_value <= value) ret = false;
		value = new_value;			
		usleep(1*1000000);
	}
	stopWriters(writers_vector);
	return ret;
};

bool testReadAccess() {
	/*
	Test same Read thread trying to acquire twice the same lock
	*/
	bool ret = false;
	SharedMutex _shared_mutex(PreferencePolicy::NONE);
	_shared_mutex.rSharedLock();
	_shared_mutex.rSharedUnlock();
	_shared_mutex.rSharedLock();	
	try {
		_shared_mutex.exclusiveLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_mutex.rSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_mutex.wSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	_shared_mutex.rSharedUnlock();
	return ret;
};

bool testWriteAccess() {
	/*
	Test same Read thread trying to acquire twice the same lock
	*/
	bool ret = false;
	SharedMutex _shared_mutex(PreferencePolicy::NONE);
	_shared_mutex.wSharedLock();
	_shared_mutex.wSharedUnlock();
	_shared_mutex.wSharedLock();	
	try {
		_shared_mutex.exclusiveLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_mutex.rSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_mutex.wSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	_shared_mutex.wSharedUnlock();
	return ret;
};

bool testExclusiveAccess() {
	/*
	Test same Write thread trying to acquire twice the same lock
	*/
	bool ret = false;
	SharedMutex _shared_mutex(PreferencePolicy::NONE);
	_shared_mutex.exclusiveLock();
	_shared_mutex.exclusiveUnlock();
	_shared_mutex.exclusiveLock();	
	try {
		_shared_mutex.exclusiveLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_mutex.rSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_mutex.wSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	_shared_mutex.exclusiveUnlock();
	return ret;
};

int main() {
	//testSimpleWriters();
	//testRoundRobin();
	bool passed;
	std::cout<<"Test Xclusive Access: "<<std::endl;
	passed = testExclusiveAccess();	
	std::cout<<"Passed: "<<std::boolalpha<<passed<<std::endl;

	std::cout<<"Test READ Access: "<<std::endl;
	passed = testReadAccess();	
	std::cout<<"Passed: "<<std::boolalpha<<passed<<std::endl;

	std::cout<<"Test WRITE Access: "<<std::endl;
	passed = testWriteAccess();	
	std::cout<<"Passed: "<<std::boolalpha<<passed<<std::endl;

	std::cout<<"Test WRITE Prevalence No exclusive: "<<std::endl;
	passed = testWritePrevalenceNoExclusive();
	std::cout<<"Passed: "<<std::boolalpha<< passed<<std::endl;

	std::cout<<"Test READ Prevalence No exclusive: "<<std::endl;
	passed = testReadPrevalenceNoExclusive();
	std::cout<<"Passed: "<<std::boolalpha<< passed<<std::endl;

	std::cout<<"Test NONE Prevalence No exclusive: "<<std::endl;
	passed = testNonePrevalenceNoExclusive();
	std::cout<<"Passed: "<<std::boolalpha<< passed<<std::endl;

	std::cout<<"Test Xclusive Thread: "<<std::endl;
	passed = testExclusiveThread();
	std::cout<<"Passed: "<<std::boolalpha<< passed<<std::endl;
	
	testRoundRobin();
	
};
