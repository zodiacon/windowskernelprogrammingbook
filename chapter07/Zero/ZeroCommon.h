#pragma once

#define IOCTL_ZERO_GET_STATS CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct ZeroStats {
	long long TotalRead;
	long long TotalWritten;
};
