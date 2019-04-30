// ZeroTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

int Error(const char* msg) {
	printf("%s: error=%d\n", msg, ::GetLastError());
	return 1;
}

int main() {
	HANDLE hDevice = ::CreateFile(L"\\\\.\\Zero", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		return Error("failed to open device");
	}

	// test read
	printf("Test read\n");
	BYTE buffer[64];
	for (int i = 0; i < sizeof(buffer); ++i)
		buffer[i] = i + 1;

	DWORD bytes;
	BOOL ok = ::ReadFile(hDevice, buffer, sizeof(buffer), &bytes, nullptr);
	if (!ok)
		return Error("failed to read");

	if (bytes != sizeof(buffer))
		printf("Wrong number of bytes\n");

	long total = 0;
	for (auto n : buffer)
		total += n;
	if (total != 0)
		printf("Wrong data\n");

	// test write
	printf("Test write\n");
	BYTE buffer2[1024];	// contains junk

	ok = ::WriteFile(hDevice, buffer2, sizeof(buffer2), &bytes, nullptr);
	if (!ok)
		return Error("failed to write");

	if (bytes != sizeof(buffer2))
		printf("Wrong byte count\n");

	::CloseHandle(hDevice);
}

