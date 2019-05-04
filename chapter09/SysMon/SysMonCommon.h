#pragma once

enum class ItemType : short {
	None,
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit,
	ImageLoad,
	RegistrySetValue
};

struct ItemHeader {
	ItemType Type;
	USHORT Size;
	LARGE_INTEGER Time;
};

struct ProcessExitInfo : ItemHeader {
	ULONG ProcessId;
};

struct ProcessCreateInfo : ItemHeader {
	ULONG ProcessId;
	ULONG ParentProcessId;
	USHORT CommandLineLength;
	USHORT CommandLineOffset;
};

struct ThreadCreateExitInfo : ItemHeader {
	ULONG ThreadId;
	ULONG ProcessId;
};

const int MaxImageFileSize = 300;

struct ImageLoadInfo : ItemHeader {
	ULONG ProcessId;
	void* LoadAddress;
	ULONG_PTR ImageSize;
	WCHAR ImageFileName[MaxImageFileSize + 1];
};

struct RegistrySetValueInfo : ItemHeader {
    ULONG ProcessId;
    ULONG ThreadId;
    WCHAR KeyName[256];		// full key name
    WCHAR ValueName[64];	// value name
    ULONG DataType;			// REG_xxx
    UCHAR Data[128];		// data
    ULONG DataSize;			// size of data
};
