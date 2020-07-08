#include <cstddef>
#include <iostream>
#include <unistd.h>
#include "access_objects.hpp"
#include "shared_mutex.hpp"

void testRoundRobin() {
	auto memory = get_memory_space();
	memory->restartMemory();
	SharedMutex _shared_mutex(PreferencePolicy::ROUNDROBIN);
	Writer writer_1(&_shared_mutex);
	Writer writer_2(&_shared_mutex);
	Writer writer_3(&_shared_mutex);
	writer_2.setDataGenerator(new CharDataGenerator('b'));
	writer_3.setDataGenerator(new CharDataGenerator('c'));
	writer_1.writeContinously();
	writer_2.writeContinously();
	writer_3.writeContinously();
	usleep(5*1000000);
	writer_1.stop();
	writer_2.stop();
	writer_3.stop();	
	std::cout<<*memory<<std::endl;
};

bool testNonePrevalenceNoExclusive() {
	bool ret = true;
	SharedMutex _shared_mutex(PreferencePolicy::NONE);
	Writer writer_1(&_shared_mutex);
	Writer writer_2(&_shared_mutex);
	Writer writer_3(&_shared_mutex);
	Writer writer_4(&_shared_mutex);
	Writer writer_5(&_shared_mutex);
	Writer writer_6(&_shared_mutex);
	Writer writer_7(&_shared_mutex);
	Writer writer_8(&_shared_mutex);
	Writer writer_9(&_shared_mutex);
	writer_1.writeContinously();
	writer_2.writeContinously();
	writer_3.writeContinously();
	writer_4.writeContinously();
	writer_5.writeContinously();
	writer_6.writeContinously();
	writer_7.writeContinously();
	writer_8.writeContinously();
	writer_9.writeContinously();
	for(uint16_t index = 0; index < 5000; index++) {
		if(_shared_mutex.wTrySharedLock() == true) {
			//Wait until all Writer threads stops				
			if(_shared_mutex.getNumberWriters() > 4) {
				std::cout<<"\tAcquiring lock when Unable: Writers: "<<_shared_mutex.getNumberWriters()<<std::endl;			
				ret = false;
			}
			else std::cout<<"\tNumber of Writers: "<<_shared_mutex.getNumberWriters()<<std::endl;
			_shared_mutex.wSharedUnlock();
		}
		usleep(1*(1000));
	}
	writer_1.stop();
	writer_2.stop();
	writer_3.stop();
	writer_4.stop();
	writer_5.stop();
	writer_6.stop();
	writer_7.stop();
	writer_8.stop();
	writer_9.stop();
	return ret;
};

bool testReadPrevalenceNoExclusive() {
	bool ret = true;
	SharedMutex _shared_mutex(PreferencePolicy::READER);
	Writer writer_1(&_shared_mutex);
	Writer writer_2(&_shared_mutex);
	Writer writer_3(&_shared_mutex);
	writer_1.writeContinously();
	writer_2.writeContinously();
	writer_3.writeContinously();
	for(uint16_t index = 0; index < 500; index++) {
		if(_shared_mutex.rTrySharedLock() == true) {
			//Wait until all Writer threads stops			
			usleep(10*(1000));	
			if(_shared_mutex.getNumberWriters() > 0) {
				std::cout<<"\tAcquiring lock when Unable: Writers: "<<_shared_mutex.getNumberWriters()<<std::endl;			
				ret = false;
			}
			_shared_mutex.rSharedUnlock();
		}
		usleep(1*(1000));
	}
	writer_1.stop();
	writer_2.stop();
	writer_3.stop();
	return ret;
};

bool testWritePrevalenceNoExclusive() {
	bool ret = true;
	SharedMutex _shared_mutex(PreferencePolicy::WRITER);
	Writer writer_1(&_shared_mutex);
	Writer writer_2(&_shared_mutex);
	Writer writer_3(&_shared_mutex);
	Reader reader_1(&_shared_mutex);
	writer_1.writeContinously();
	writer_2.writeContinously();
	writer_3.writeContinously();
	reader_1.readContinously();
	for(uint16_t index = 0; index < 5000; index++) {
		if(_shared_mutex.rTrySharedLock() == true) {		
			if(_shared_mutex.getNumberReaders() != 1) {
				std::cout<<"\tAcquiring lock when Unable"<<std::endl;			
				ret = false;
			}
			else std::cout<<"\tLock acquired but only one reader"<<std::endl;			
			_shared_mutex.rSharedUnlock();
			usleep(1*(1000));// 1 seconds
		}
	}
	writer_1.stop();
	writer_2.stop();
	writer_3.stop();
	for(uint16_t index = 0; index < 5000; index++) {
		if(_shared_mutex.rTrySharedLock() == false) {
			std::cout<<"\tNot Acquiring lock when able"<<std::endl;		
			ret = false;
		}
		else _shared_mutex.rSharedUnlock();
		usleep(1*(1000));	
	}
	reader_1.stop();
	return ret;
};

int main() {
	std::cout<<"Test WRITE Prevalence No exclusive: "<<std::endl;
	bool passed = testWritePrevalenceNoExclusive();
	std::cout<<"Passed: "<<std::boolalpha<< passed<<std::endl;

	std::cout<<"Test READ Prevalence No exclusive: "<<std::endl;
	passed = testReadPrevalenceNoExclusive();
	std::cout<<"Passed: "<<std::boolalpha<< passed<<std::endl;

	std::cout<<"Test NONE Prevalence No exclusive: "<<std::endl;
	passed = testNonePrevalenceNoExclusive();
	std::cout<<"Passed: "<<std::boolalpha<< passed<<std::endl;
	
	testRoundRobin();
};
