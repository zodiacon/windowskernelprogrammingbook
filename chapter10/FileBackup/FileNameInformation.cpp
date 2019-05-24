#include "FileNameInformation.h"

FilterFileNameInformation::FilterFileNameInformation(PFLT_CALLBACK_DATA data, FileNameOptions options) {
	auto status = FltGetFileNameInformation(data, (FLT_FILE_NAME_OPTIONS)options, &_info);
	if (!NT_SUCCESS(status))
		_info = nullptr;
}

FilterFileNameInformation::~FilterFileNameInformation() {
	if (_info)
		FltReleaseFileNameInformation(_info);
}

NTSTATUS FilterFileNameInformation::Parse() {
	return FltParseFileNameInformation(_info);
}
