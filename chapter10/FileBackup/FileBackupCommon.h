#pragma once

struct FileBackupPortMessage {
	USHORT FileNameLength;
	WCHAR FileName[1];
};
