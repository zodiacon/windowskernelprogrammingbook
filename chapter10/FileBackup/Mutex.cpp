#include "Mutex.h"


void Mutex::Init() {
	KeInitializeMutex(&_mutex, 0);
}

void Mutex::Lock() {
	KeWaitForSingleObject(&_mutex, Executive, KernelMode, FALSE, nullptr);
}

void Mutex::Unlock() {
	KeReleaseMutex(&_mutex, FALSE);
}
