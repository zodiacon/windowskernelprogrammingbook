#pragma once

#include <ntddk.h>

class Mutex {
public:
	void Init();

	void Lock();
	void Unlock();

private:
	KMUTEX _mutex;
};

