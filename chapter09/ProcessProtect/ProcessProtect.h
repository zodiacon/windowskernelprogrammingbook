#pragma once

#define DRIVER_PREFIX "ProcessProtect: "

#define PROCESS_TERMINATE 1

#include "FastMutex.h"

const int MaxPids = 256;

struct Globals {
	int PidsCount;			// currently protected process count
	ULONG Pids[MaxPids];	// protected PIDs
	FastMutex Lock;
	PVOID RegHandle;

	void Init() {
		Lock.Init();
	}
};
