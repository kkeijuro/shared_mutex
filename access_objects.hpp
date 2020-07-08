
#include <thread>
#include <mutex>

#pragma once

class SharedMutex;

class MemorySpace {
	public:
	MemorySpace();
	MemorySpace(uint32_t size);	
	size_t read(uint8_t* buffer, size_t size);
	size_t write(uint8_t* buffer, size_t size);
	size_t getSize() const;
	void restartMemory();
	friend std::ostream& operator<<(std::ostream& os, const MemorySpace& person);
	private:
	static uint32_t DEFAULT_SIZE;
	static uint32_t _RSLEEP;
	static uint32_t _WSLEEP;
	uint32_t _max_size;
	uint8_t* _memory_space;
	std::mutex _mutex;
	uint32_t _rw_position;
};

MemorySpace* get_memory_space();

class DataGenerator {
	public:
	virtual size_t getData(uint8_t* data) = 0;
};

class CharDataGenerator: public DataGenerator {
	public:
	CharDataGenerator(uint8_t value);
	size_t getData(uint8_t* data);
	private:
	uint8_t _value;
};

/*
Test class to Read Access
*/

class Reader {
	public:
	Reader(SharedMutex* shared_mutex);
	void readContinously();
	void punctualRead();
	void stop();
	private:
	MemorySpace* _memory_space;
	SharedMutex* _mutex;
	bool _out;
	std::thread* _thread;
	uint32_t _thread_uid;
	void continousRead();
};

/*
Test class to Write Access
*/
class Writer {
	public:
	Writer(SharedMutex* shared_mutex);
	void setDataGenerator(DataGenerator* data_generator);
	void stop();
	void writeContinously();
	private:
	DataGenerator* _data_generator;
	MemorySpace* _memory_space;
	SharedMutex* _mutex;
	bool _out;
	std::thread* _thread;
	uint32_t _thread_uid;
	void continousWrite();
};
