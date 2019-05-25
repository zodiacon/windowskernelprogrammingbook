// FileBackupMon.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#include "..\FileBackup\FileBackupCommon.h"

#pragma comment(lib, "fltlib")


void HandleMessage(const BYTE* buffer) {
	auto msg = (FileBackupPortMessage*)buffer;
	std::wstring filename(msg->FileName, msg->FileNameLength);

	printf("file backed up: %ws\n", filename.c_str());
}

int main() {
	HANDLE hPort;
	auto hr = ::FilterConnectCommunicationPort(L"\\FileBackupPort", 0, nullptr, 0, nullptr, &hPort);
	if (FAILED(hr)) {
		printf("Error connecting to port (HR=0x%08X)\n", hr);
		return 1;
	}

	BYTE buffer[1 << 12];	// 4 KB
	auto message = (FILTER_MESSAGE_HEADER*)buffer;

	for (;;) {
		hr = ::FilterGetMessage(hPort, message, sizeof(buffer), nullptr);
		if (FAILED(hr)) {
			printf("Error receiving message (0x%08X)\n", hr);
			break;
		}
		HandleMessage(buffer + sizeof(FILTER_MESSAGE_HEADER));
	}

	::CloseHandle(hPort);

	return 0;
}

