
#include <thread>
#include <mutex>

#pragma once

class SharedLock;

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
	mutable std::mutex _mutex;
	uint32_t _rw_position;
};

MemorySpace* get_memory_space();

class DataGenerator {
	public:
	virtual size_t getData(uint8_t* data) = 0;
	virtual ~DataGenerator(){};
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

class RWOut {
	public:
	RWOut();
	~RWOut();
	void reset();	
	void set();
	bool status();
	private:
	bool _out;
	std::mutex _lock;
};

class Reader {
	public:
	Reader(SharedLock* shared_lock);
	void readContinously();
	size_t punctualRead(uint8_t* buffer, size_t lenght);
	void stop();
	private:
	static uint32_t _SLEEP;
	MemorySpace* _memory_space;
	SharedLock* _lock;
	RWOut _out;
	std::thread* _thread;
	uint32_t _thread_uid;
	void continousRead();
};

/*
Test class to Write Access
*/
class Writer {
	public:
	Writer(SharedLock* shared_lock);
	void setDataGenerator(DataGenerator* data_generator);
	void stop();
	void writeContinously();
	private:
	static uint32_t _SLEEP;
	DataGenerator* _data_generator;
	MemorySpace* _memory_space;
	SharedLock* _lock;
	RWOut _out;
	std::thread* _thread;
	uint32_t _thread_uid;
	void continousWrite();
};
