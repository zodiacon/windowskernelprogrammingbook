// DelTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>


void HandleResult(BOOL success) {
	if(success)
		printf("Success!\n");
	else
		printf("Error: %d\n", ::GetLastError());
}

int wmain(int argc, const wchar_t* argv[]) {
	if (argc < 3) {
		printf("Usage: deltest.exe <method> <filename>\n");
		printf("\tMethod: 1=DeleteFile, 2=delete on close, 3=SetFileInformation.\n");
		return 0;
	}

	auto method = _wtoi(argv[1]);
	auto filename = argv[2];
	HANDLE hFile;
	BOOL success;

	switch (method) {
		case 1:
			printf("Using DeleteFile:\n");
			success = ::DeleteFile(filename);
			HandleResult(success);
			break;

		case 2:
			printf("Using CreateFile with FILE_FLAG_DELETE_ON_CLOSE:\n");
			hFile = ::CreateFile(filename, DELETE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
			HandleResult(hFile != INVALID_HANDLE_VALUE);
			::CloseHandle(hFile);
			break;

		case 3:
			printf("Using SetFileInformationByHandle:\n");
			FILE_DISPOSITION_INFO info;
			info.DeleteFile = TRUE;
			hFile = ::CreateFile(filename, DELETE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			success = ::SetFileInformationByHandle(hFile, FileDispositionInfo, &info, sizeof(info));
			HandleResult(success);
			::CloseHandle(hFile);
			break;
	}

	return 0;
}

