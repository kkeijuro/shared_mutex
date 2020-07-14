#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iomanip> 
#include <unistd.h>
#include <utility>
#include <vector>
#include "test_objects.hpp"
#include "shared_lock.hpp"


std::vector<Writer*>& createNWriters(SharedLock& shared_lock, uint16_t n) {
	std::vector<Writer*>* w_vector = new std::vector<Writer*>(n);  
	for(uint8_t index = 0; index < n; index++) w_vector->operator[](index) = new Writer(&shared_lock);
	return *w_vector;
};

void startWriters(std::vector<Writer*>& writers) {
	std::for_each(writers.begin(), writers.end(), [](Writer*& w){std::flush(std::cout);w->writeContinously();});
};


void stopWriters(std::vector<Writer*>& writers) {
	std::for_each(writers.begin(), writers.end(), [](Writer*& w){w->stop();});
};


std::vector<Reader*>& createNReaders(SharedLock& shared_lock, uint16_t n) {
	std::vector<Reader*>* r_vector = new std::vector<Reader*>(n);  
	for(uint8_t index = 0; index<n; index++) r_vector->operator[](index) = new Reader(&shared_lock);
	return *r_vector;
};

void startReaders(std::vector<Reader*>& readers) {
	std::for_each(readers.begin(), readers.end(), [](Reader*& r){r->readContinously();});
};


void stopReaders(std::vector<Reader*>& readers) {
	std::for_each(readers.begin(), readers.end(), [](Reader*& r){r->stop();});
};


void testSimpleWriters() {
	bool RUN = true;
	if(!RUN) return;
	uint32_t NUM_WRITERS = 9;

	SharedLock _shared_lock(PreferencePolicy::ROUNDROBIN);
	auto writers_vector = createNWriters(_shared_lock, NUM_WRITERS);
	startWriters(writers_vector);
	for(uint16_t index = 0; index < 500; index++) {
		std::cout<<"Number of Writers: " << _shared_lock.getNumberWriters() << std::endl;
		usleep(10*1000);	
	}
	stopWriters(writers_vector);
};

void testRoundRobin() {

	bool RUN = true;
	if(!RUN) return;

	auto memory = get_memory_space();
	memory->restartMemory();
	SharedLock _shared_lock(PreferencePolicy::ROUNDROBIN);
	auto writers_vector = createNWriters(_shared_lock, 5);
	writers_vector.operator[](1)->setDataGenerator(new CharDataGenerator('b'));
	writers_vector.operator[](2)->setDataGenerator(new CharDataGenerator('c'));
	writers_vector.operator[](3)->setDataGenerator(new CharDataGenerator('d'));
	writers_vector.operator[](4)->setDataGenerator(new CharDataGenerator('e'));
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

	bool RUN = true;
	if(!RUN) return false;
	uint32_t NUM_WRITERS = 4;
	uint32_t NUM_READERS = 4;
	uint16_t ACCESS_RETRIES = 20;

	bool ret = true;
	SharedLock _shared_lock(PreferencePolicy::NONE);
	auto writers_vector = createNWriters(_shared_lock, NUM_WRITERS);
	auto readers_vector = createNReaders(_shared_lock, NUM_READERS);
	startWriters(writers_vector);
	startReaders(readers_vector);
	for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
		if(_shared_lock.wTrySharedLock(100) == true) {
			_shared_lock.lockShared();				
			if(_shared_lock.getNumberWriters() > 1 || _shared_lock.getNumberReaders() > 0) {
				std::cout<<"\tAcquiring lock when Unable: Writers: "<<_shared_lock.getNumberWriters()<<std::endl;			
				ret = false;
			}
			else std::cout<<"\tNumber of Writers: "<<_shared_lock.getNumberWriters()<<" Number of readers: "<<_shared_lock.getNumberReaders()<<std::endl;
			_shared_lock.unlockShared();			
			_shared_lock.wSharedUnlock();
		}
		usleep(1*(50000));
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

	bool RUN = true;
	if(!RUN) return false;
	uint32_t NUM_WRITERS = 200;
	uint32_t NUM_READERS = 2;
	uint16_t ACCESS_RETRIES = 20;

	bool ret = true;
	SharedLock _shared_lock(PreferencePolicy::READER);
	auto writers_vector = createNWriters(_shared_lock, NUM_WRITERS);
	auto readers_vector = createNReaders(_shared_lock, NUM_READERS);
	startWriters(writers_vector);
	startReaders(readers_vector);
	usleep(1*1000000);
	for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
		if(_shared_lock.rTrySharedLock(100) == true) {
			//wait until all actual write threads finnish			
			// With RLock no new writes should be given			
			usleep(1*(1000000));
			if(_shared_lock.getNumberWriters() > 0) {
				std::cout<<"\tAcquiring lock when Unable: Writers: "<<_shared_lock.getNumberWriters()<<std::endl;
				ret = false;
			}
			else std::cout<<"\tAcquiring lock when Able: Writers: "<<_shared_lock.getNumberWriters()<<" Readers: "<< _shared_lock.getNumberReaders()<<std::endl;
			_shared_lock.rSharedUnlock();
		}
		usleep(1*(1000000));
		if(_shared_lock.getNumberWriters() == 0) {
			ret = false;
			std::cout<<"\tNum. Writers is Zero should be N: "<<_shared_lock.getNumberWriters()<<std::endl;		
		}
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
	
	//TEST VALUES
	bool RUN = true;
	if(!RUN) return false;
	uint32_t NUM_WRITERS = 200;
	uint16_t ACCESS_RETRIES = 2000;
	uint16_t ACCESS_RETRIES_2 = 200;	

	bool ret = true;
	SharedLock _shared_lock(PreferencePolicy::WRITER);
	auto writers_vector = createNWriters(_shared_lock, NUM_WRITERS);
	Reader reader_1(&_shared_lock);
	startWriters(writers_vector);
	reader_1.readContinously();
	for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
		if(_shared_lock.rTrySharedLock() == true) {
			if(_shared_lock.getNumberReaders() > 1 and _shared_lock.getNumberWriters() != 0) {
				std::cout<<"\tAcquiring lock when Unable"<<" Writers: "<<_shared_lock.getNumberWriters()
								<<" Readers: "<< _shared_lock.getNumberReaders()<<std::endl;			
				ret = false;
			}
			else std::cout<<"\tLock acquired but: "<<"Writers: "<<_shared_lock.getNumberWriters()
								<<" Readers: "<< _shared_lock.getNumberReaders()<<std::endl;			
			_shared_lock.rSharedUnlock();
			usleep(1*(50000));// 50 mseconds
		}
	}
	_shared_lock.lockWriters();
	std::cout<<"\t Waiting for locking all writers"<<std::endl;	
	usleep(1*(1000000));
	for(uint16_t index = 0; index < ACCESS_RETRIES_2; index++) {
		_shared_lock.lockWriters();
		if(_shared_lock.rTrySharedLock(100) == false) {
			std::cout<<"\tNot Acquiring lock when able"<<std::endl;		
			ret = false;
		}
		else {
			std::cout<<"\tRlock acquired with: "<<_shared_lock.getNumberWriters()<<" Writers"								
							    <<" Readers: "<< _shared_lock.getNumberReaders()<<std::endl;
			_shared_lock.rSharedUnlock();
		}
		usleep(1*(50000));	
	}
	_shared_lock.unlockWriters();
	reader_1.stop();
	stopWriters(writers_vector);
	return ret;
};

bool testExclusiveThread() {
	/*
	N writers launched, check that memory is being wrote
	begin exclusive thread: check that memory doesnt grow
	stop exclusive thread: check that memory written keeps growing
	*/
	//TEST VALUES
	bool RUN = true;
	if(!RUN) return false;
	uint32_t NUM_WRITERS = 100;
	uint16_t ACCESS_RETRIES = 10;
	
	auto memory = get_memory_space();
	memory->restartMemory();
	bool ret = true;
	SharedLock _shared_lock(PreferencePolicy::WRITER);
	auto writers_vector = createNWriters(_shared_lock, NUM_WRITERS);
	startWriters(writers_vector);
	uint32_t value = memory->getSize();		
	usleep(1*1000000);
	for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
		uint32_t new_value = memory->getSize();
		std::cout<<"\tLast Value: "<< value<<" New Value: "<<new_value<< std::endl;	
		if(new_value <= value) ret = false;
		value = new_value;
		usleep(1*1000000);
	}
	if(_shared_lock.tryExclusiveLock(10*1000) == true) {
		std::cout<<"\tExclusive lock acquired! "<<std::endl;
		uint32_t value = memory->getSize();	
		for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
			uint32_t new_value = memory->getSize();	
			std::cout<<"\tLast Value: "<< value<<" New Value: "<<new_value<< std::endl;
			if(new_value != value) ret = false;			
			usleep(1*1000000);
		}
		_shared_lock.exclusiveUnlock();
		std::cout<<"\tExclusive lock released! "<<std::endl;
	}
	value = memory->getSize();	
	usleep(1*1000000);		
	for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
		uint32_t new_value = memory->getSize();	
		std::cout<<"\tLast Value: "<< value<<" New Value: "<<new_value<< std::endl;
		if(new_value <= value) ret = false;
		value = new_value;			
		usleep(1*1000000);
	}
	stopWriters(writers_vector);
	return ret;
};

bool testLimitReaders(uint32_t limit_readers) {
	bool RUN = true;
	if(!RUN) return false;
	uint32_t NUM_READERS = 100;
	uint16_t ACCESS_RETRIES = 200;

	bool ret = true;
	SharedLock _shared_lock(PreferencePolicy::NONE);
	SharedLock::setLimitReaders(limit_readers);
	auto readers_vector = createNReaders(_shared_lock, NUM_READERS);
	startReaders(readers_vector);
	usleep(1*100000);//100 msecond
	for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
		std::cout<<"\tReaders: "<<_shared_lock.getNumberReaders()<<" Future Readers: "<< _shared_lock.getNumberFutureReaders()<<std::endl;
		usleep(1*100000);//100 msecond
		if(_shared_lock.getNumberReaders() > limit_readers) ret = false;
	}
	stopReaders(readers_vector);
	SharedLock::setLimitReaders(0);
	return ret;
};

bool testFutureReadersBlocksWriter() {
	bool RUN = true;
	if(!RUN) return false;
	uint32_t NUM_READERS = 1;
	uint16_t ACCESS_RETRIES = 200;

	bool ret = true;
	SharedLock _shared_lock(PreferencePolicy::READER);
	//Create a bottleneck. With lots of waiting readers
	SharedLock::setLimitReaders(0);
	auto readers_vector = createNReaders(_shared_lock, NUM_READERS);
	startReaders(readers_vector);
	usleep(1*100000);//1 msecond
	for(uint16_t index = 0; index < ACCESS_RETRIES; index++) {
		if(_shared_lock.wTrySharedLock() == true) {
			ret = false;				
			std::cout<<"\tAcquiring lock when Unable: "<<" Number of readers: "<<_shared_lock.getNumberReaders()
				<<" Future Readers: " << _shared_lock.getNumberFutureReaders() <<std::endl;
			_shared_lock.wSharedUnlock();
		}
		else 
			std::cout<<"\tNot Acquiring: "<<" Number of readers: "<<_shared_lock.getNumberReaders()
				<<" Future Readers: " << _shared_lock.getNumberFutureReaders() <<std::endl;
		usleep(1*(50000)); // 50 msec
	}
	SharedLock::setLimitReaders(SharedLock::NO_LIMIT_READERS);
	//Little hack to reevaluate Evaluation Conditions	
	_shared_lock.notify();	
	stopReaders(readers_vector);
	if(_shared_lock.wTrySharedLock(100) == false) {
		ret = false;
		std::cout<<"Unable to take write lock when able"<<" Number of readers: "<<_shared_lock.getNumberReaders()
				<<" Future Readers: " << _shared_lock.getNumberFutureReaders() <<std::endl;
	};
	return ret;
};

bool testReadAccess() {
	/*
	Test same Read thread trying to acquire twice the same lock
	*/
	bool ret = false;
	SharedLock _shared_lock(PreferencePolicy::NONE);
	_shared_lock.rSharedLock();
	_shared_lock.rSharedUnlock();
	_shared_lock.rSharedLock();	
	try {
		_shared_lock.exclusiveLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_lock.rSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_lock.wSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	_shared_lock.rSharedUnlock();
	return ret;
};

bool testWriteAccess() {
	/*
	Test same Write thread trying to acquire twice the same lock
	*/
	bool ret = false;
	SharedLock _shared_lock(PreferencePolicy::NONE);
	_shared_lock.wSharedLock();
	_shared_lock.wSharedUnlock();
	_shared_lock.wSharedLock();	
	try {
		_shared_lock.exclusiveLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_lock.rSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_lock.wSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	_shared_lock.wSharedUnlock();
	return ret;
};

bool testExclusiveAccess() {
	/*
	Test same Exclusive thread trying to acquire twice the same lock
	*/
	bool ret = false;
	SharedLock _shared_lock(PreferencePolicy::NONE);
	_shared_lock.exclusiveLock();
	_shared_lock.exclusiveUnlock();
	_shared_lock.exclusiveLock();	
	try {
		_shared_lock.exclusiveLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_lock.rSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	try {
		_shared_lock.wSharedLock();
		ret = false;
	}
	catch (...) {
		ret = true;
	}
	_shared_lock.exclusiveUnlock();
	return ret;
};

int main() {

	bool passed;
	std::vector<std::pair<const std::string, bool>> result;


	std::cout<<"Launching Test Future Readers Block Writers: "<<std::endl;
	passed = testFutureReadersBlocksWriter();
	result.push_back({"testFutureReadersBlocksWriter", passed});

	std::cout<<"Launching Test Xclusive Access: "<<std::endl;
	passed = testExclusiveAccess();	
	result.push_back({"testExclusiveAccess", passed});

	std::cout<<"Launching Test READ Access: "<<std::endl;
	passed = testReadAccess();	
	result.push_back({"testReadAccess", passed});

	std::cout<<"Launching Test WRITE Access: "<<std::endl;
	passed = testWriteAccess();	
	result.push_back({"testWriteAccess", passed});

	std::cout<<"Launching Test READ Prevalence No exclusive: "<<std::endl;
	passed = testReadPrevalenceNoExclusive();
	result.push_back({"testReadPrevalenceNoExclusive", passed});

	std::cout<<"Launching Test WRITE Prevalence No exclusive: "<<std::endl;
	passed = testWritePrevalenceNoExclusive();
	result.push_back({"testWritePrevalenceNoExclusive", passed});

	std::cout<<"Launching Test NONE Prevalence No exclusive: "<<std::endl;
	passed = testNonePrevalenceNoExclusive();
	result.push_back({"testNonePrevalenceNoExclusive", passed});

	std::cout<<"Launching Test Xclusive Thread: "<<std::endl;
	passed = testExclusiveThread();
	result.push_back({"testExclusiveThread", passed});

	std::cout<<"Launching Test Limit Readers: "<<std::endl;
	passed = testLimitReaders(5);
	result.push_back({"testLimitReaders", passed});

	testRoundRobin();

	std::for_each(result.begin(), result.end(), [&](std::pair<const std::string, bool> & result){std::cout<<"Test: " << std::left<< std::setw(35) << result.first<< " Passed: "<< std::boolalpha <<result.second<<std::endl;});


};
