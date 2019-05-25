/*++

Module Name:

	FileBackup.c

Abstract:

	This is the main module of the FileBackup miniFilter driver.

Environment:

	Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>
#include "FileNameInformation.h"
#include "AutoLock.h"
#include "Mutex.h"
#include "FileBackupCommon.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;
PFLT_PORT FilterPort;
PFLT_PORT SendClientPort;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;

#define DRIVER_CONTEXT_TAG 'xcbF'
#define DRIVER_TAG 'bF'

struct FileContext {
	Mutex Lock;
	UNICODE_STRING FileName;
	BOOLEAN Written;
};

NTSTATUS BackupFile(_In_ PUNICODE_STRING FileName, _In_ PCFLT_RELATED_OBJECTS FltObjects);
bool IsBackupDirectory(_In_ PCUNICODE_STRING directory);

#define PT_DBG_PRINT( _dbgLevel, _string )          \
	(FlagOn(gTraceFlags,(_dbgLevel)) ?              \
		DbgPrint _string :                          \
		((int)0))

/*************************************************************************
	Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
FileBackupInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
FileBackupInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

VOID
FileBackupInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

NTSTATUS
FileBackupUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
FileBackupInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
FileBackupPreOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

VOID
FileBackupOperationStatusCallback(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
	_In_ NTSTATUS OperationStatus,
	_In_ PVOID RequesterContext
);

FLT_PREOP_CALLBACK_STATUS
FileBackupPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
FileBackupPostCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
FileBackupPostCleanup(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

NTSTATUS PortConnectNotify(
	_In_ PFLT_PORT ClientPort,
	_In_opt_ PVOID ServerPortCookie,
	_In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
	_In_ ULONG SizeOfContext,
	_Outptr_result_maybenull_ PVOID *ConnectionPortCookie);

void PortDisconnectNotify(_In_opt_ PVOID ConnectionCookie);

NTSTATUS PortMessageNotify(
	_In_opt_ PVOID PortCookie,
	_In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
	_In_ ULONG OutputBufferLength,
	_Out_ PULONG ReturnOutputBufferLength);

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FileBackupUnload)
#pragma alloc_text(PAGE, FileBackupInstanceQueryTeardown)
#pragma alloc_text(PAGE, FileBackupInstanceSetup)
#pragma alloc_text(PAGE, FileBackupInstanceTeardownStart)
#pragma alloc_text(PAGE, FileBackupInstanceTeardownComplete)
#endif

void FileContextCleanup(_In_ PFLT_CONTEXT Context, _In_ FLT_CONTEXT_TYPE /* ContextType */) {
	auto fileContext = (FileContext*)Context;
	if (fileContext->FileName.Buffer)
		ExFreePool(fileContext->FileName.Buffer);
}

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
	{ IRP_MJ_CREATE, 0, nullptr, FileBackupPostCreate },
	{ IRP_MJ_WRITE, FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO, FileBackupPreWrite },
	{ IRP_MJ_CLEANUP, 0, nullptr, FileBackupPostCleanup },

	{ IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

const FLT_CONTEXT_REGISTRATION Contexts[] = {
	{ FLT_FILE_CONTEXT, 0, nullptr, sizeof(FileContext), DRIVER_CONTEXT_TAG },
	{ FLT_CONTEXT_END }
};

CONST FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION),         //  Size
	FLT_REGISTRATION_VERSION,         //  Version
	0,                                //  Flags

	Contexts,                         //  Context
	Callbacks,                        //  Operation callbacks

	FileBackupUnload,                 //  MiniFilterUnload

	FileBackupInstanceSetup,
	FileBackupInstanceQueryTeardown,
	FileBackupInstanceTeardownStart,
	FileBackupInstanceTeardownComplete,

	nullptr,    //  GenerateFileName
	nullptr,    //  GenerateDestinationFileName
	nullptr     //  NormalizeNameComponent
};



NTSTATUS
FileBackupInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
/*++

Routine Description:

	This routine is called whenever a new instance is created on a volume. This
	gives us a chance to decide if we need to attach to this volume or not.

	If this routine is not defined in the registration structure, automatic
	instances are always created.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Flags describing the reason for this attach request.

Return Value:

	STATUS_SUCCESS - attach
	STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);

	PAGED_CODE();

	if (VolumeFilesystemType != FLT_FSTYPE_NTFS) {
		KdPrint(("Not attaching to non-NTFS volume\n"));
		return STATUS_FLT_DO_NOT_ATTACH;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
FileBackupInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

	This is called when an instance is being manually deleted by a
	call to FltDetachVolume or FilterDetach thereby giving us a
	chance to fail that detach request.

	If this routine is not defined in the registration structure, explicit
	detach requests via FltDetachVolume or FilterDetach will always be
	failed.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Indicating where this detach request came from.

Return Value:

	Returns the status of this operation.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("FileBackup!FileBackupInstanceQueryTeardown: Entered\n"));

	return STATUS_SUCCESS;
}

FLT_PREOP_CALLBACK_STATUS FileBackupPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext) {
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Data);

	// get the file context if exists
	FileContext* context;

	auto status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
	if (!NT_SUCCESS(status) || context == nullptr) {
		// no context, continue normally
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	{
		// acquire the fast mutex in case of multiple writes
		AutoLock<Mutex> locker(context->Lock);

		if (!context->Written) {
			status = BackupFile(&context->FileName, FltObjects);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Failed to backup file! (0x%X)\n", status));
			}
			else {
				// send message to user mode
				if (SendClientPort) {
					USHORT nameLen = context->FileName.Length;
					USHORT len = sizeof(FileBackupPortMessage) + nameLen;
					auto msg = (FileBackupPortMessage*)ExAllocatePoolWithTag(PagedPool, len, DRIVER_TAG);
					if (msg) {
						msg->FileNameLength = nameLen / sizeof(WCHAR);
						RtlCopyMemory(msg->FileName, context->FileName.Buffer, nameLen);
						LARGE_INTEGER timeout;
						timeout.QuadPart = -10000 * 100;	// 100msec
						FltSendMessage(gFilterHandle, &SendClientPort, msg, len,
							nullptr, nullptr, &timeout);
						ExFreePool(msg);
					}
				}
			}
			context->Written = TRUE;
		}
	}
	FltReleaseContext(context);

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS FileBackupPostCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags) {
	UNREFERENCED_PARAMETER(CompletionContext);

	if (Flags & FLTFL_POST_OPERATION_DRAINING)
		return FLT_POSTOP_FINISHED_PROCESSING;

	const auto& params = Data->Iopb->Parameters.Create;
	if (Data->RequestorMode == KernelMode 
		|| (params.SecurityContext->DesiredAccess & FILE_WRITE_DATA) == 0 
		|| Data->IoStatus.Information == FILE_DOES_NOT_EXIST) {
		// kernel caller, not write access or a new file - skip
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	// get file name
	FilterFileNameInformation fileNameInfo(Data);
	if (!fileNameInfo) {
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	if (!NT_SUCCESS(fileNameInfo.Parse()))
		return FLT_POSTOP_FINISHED_PROCESSING;

	if (!IsBackupDirectory(&fileNameInfo->ParentDir))
		return FLT_POSTOP_FINISHED_PROCESSING;

	// if it's not the default stream, we don't care
	if (fileNameInfo->Stream.Length > 0)
		return FLT_POSTOP_FINISHED_PROCESSING;

	// allocate and initialize a file context
	FileContext* context;
	auto status = FltAllocateContext(FltObjects->Filter, FLT_FILE_CONTEXT, sizeof(FileContext), PagedPool, (PFLT_CONTEXT*)&context);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to allocate file context (0x%08X)\n", status));
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	context->Written = FALSE;
	context->FileName.MaximumLength = fileNameInfo->Name.Length;
	context->FileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, fileNameInfo->Name.Length, DRIVER_TAG);
	if (!context->FileName.Buffer) {
		FltReleaseContext(context);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}
	RtlCopyUnicodeString(&context->FileName, &fileNameInfo->Name);
	context->Lock.Init();
	status = FltSetFileContext(FltObjects->Instance, FltObjects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, context, nullptr);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to set file context (0x%08X)\n", status));
		ExFreePool(context->FileName.Buffer);
	}
	FltReleaseContext(context);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_POSTOP_CALLBACK_STATUS FileBackupPostCleanup(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Data);

	FileContext* context;

	auto status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
	if (!NT_SUCCESS(status) || context == nullptr) {
		// no context, continue normally
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	if (context->FileName.Buffer)
		ExFreePool(context->FileName.Buffer);
	FltReleaseContext(context);
	FltDeleteContext(context);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

_Use_decl_annotations_
NTSTATUS PortConnectNotify(PFLT_PORT ClientPort, PVOID ServerPortCookie, PVOID ConnectionContext, ULONG SizeOfContext, PVOID* ConnectionPortCookie) {
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionPortCookie);

	SendClientPort = ClientPort;

	return STATUS_SUCCESS;
}

void PortDisconnectNotify(PVOID ConnectionCookie) {
	UNREFERENCED_PARAMETER(ConnectionCookie);

	FltCloseClientPort(gFilterHandle, &SendClientPort);
	SendClientPort = nullptr;
}

NTSTATUS PortMessageNotify(PVOID PortCookie, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnOutputBufferLength) {
	UNREFERENCED_PARAMETER(PortCookie);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

	return STATUS_SUCCESS;
}

VOID
FileBackupInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

	This routine is called at the start of instance teardown.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Reason why this instance is being deleted.

Return Value:

	None.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("FileBackup!FileBackupInstanceTeardownStart: Entered\n"));
}


VOID
FileBackupInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

	This routine is called at the end of instance teardown.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Reason why this instance is being deleted.

Return Value:

	None.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("FileBackup!FileBackupInstanceTeardownComplete: Entered\n"));
}

NTSTATUS BackupFile(_In_ PUNICODE_STRING FileName, _In_ PCFLT_RELATED_OBJECTS FltObjects) {
	HANDLE hTargetFile = nullptr;
	HANDLE hSourceFile = nullptr;
	IO_STATUS_BLOCK ioStatus;
	auto status = STATUS_SUCCESS;
	void* buffer = nullptr;

	// get source file size
	LARGE_INTEGER fileSize;
	status = FsRtlGetFileSize(FltObjects->FileObject, &fileSize);
	if (!NT_SUCCESS(status) || fileSize.QuadPart == 0)
		return status;

	do {
		// open source file
		OBJECT_ATTRIBUTES sourceFileAttr;
		InitializeObjectAttributes(&sourceFileAttr, FileName,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

		status = FltCreateFile(
			FltObjects->Filter,		// filter object
			FltObjects->Instance,	// filter instance
			&hSourceFile,			// resulting handle
			FILE_READ_DATA | SYNCHRONIZE, // access mask
			&sourceFileAttr,		// object attributes
			&ioStatus,				// resulting status
			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
			FILE_SHARE_READ | FILE_SHARE_WRITE,		// share flags
			FILE_OPEN,		// create disposition
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
			nullptr, 0,				// extended attributes, EA length
			IO_IGNORE_SHARE_ACCESS_CHECK);	// flags

		if (!NT_SUCCESS(status))
			break;

		// open target file
		UNICODE_STRING targetFileName;
		const WCHAR backupStream[] = L":backup";
		targetFileName.MaximumLength = FileName->Length + sizeof(backupStream);
		targetFileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, targetFileName.MaximumLength, DRIVER_TAG);
		if (targetFileName.Buffer == nullptr)
			return STATUS_INSUFFICIENT_RESOURCES;

		RtlCopyUnicodeString(&targetFileName, FileName);
		RtlAppendUnicodeToString(&targetFileName, backupStream);

		OBJECT_ATTRIBUTES targetFileAttr;
		InitializeObjectAttributes(&targetFileAttr, &targetFileName,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

		status = FltCreateFile(
			FltObjects->Filter,		// filter object
			FltObjects->Instance,	// filter instance
			&hTargetFile,			// resulting handle
			GENERIC_WRITE | SYNCHRONIZE, // access mask
			&targetFileAttr,		// object attributes
			&ioStatus,				// resulting status
			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
			0,		// share flags
			FILE_OVERWRITE_IF,		// create disposition
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
			nullptr, 0,		// extended attributes, EA length
			0 /*IO_IGNORE_SHARE_ACCESS_CHECK*/);	// flags

		ExFreePool(targetFileName.Buffer);

		if (!NT_SUCCESS(status))
			break;

		// allocate buffer for copying purposes
		ULONG size = 1 << 21;	// 2 MB
		buffer = ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
		if (!buffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		// loop - read from source, write to target
		LARGE_INTEGER offset = { 0 };		// read
		LARGE_INTEGER writeOffset = { 0 };	// write

		ULONG bytes;
		auto saveSize = fileSize;
		while (fileSize.QuadPart > 0) {
			status = ZwReadFile(
				hSourceFile,
				nullptr,	// optional KEVENT
				nullptr, nullptr,	// no APC
				&ioStatus,
				buffer,
				(ULONG)min((LONGLONG)size, fileSize.QuadPart),	// # of bytes
				&offset,	// offset
				nullptr);	// optional key
			if (!NT_SUCCESS(status))
				break;

			bytes = (ULONG)ioStatus.Information;

			// write to target file
			status = ZwWriteFile(
				hTargetFile,	// target handle
				nullptr,		// optional KEVENT
				nullptr, nullptr, // APC routine, APC context
				&ioStatus,		// I/O status result
				buffer,			// data to write
				bytes, // # bytes to write
				&writeOffset,	// offset
				nullptr);		// optional key

			if (!NT_SUCCESS(status))
				break;

			// update byte count and offsets
			offset.QuadPart += bytes;
			writeOffset.QuadPart += bytes;
			fileSize.QuadPart -= bytes;
		}

		FILE_END_OF_FILE_INFORMATION info;
		info.EndOfFile = saveSize;
		NT_VERIFY(NT_SUCCESS(ZwSetInformationFile(hTargetFile, &ioStatus, &info, sizeof(info), FileEndOfFileInformation)));
	} while (false);

	if (buffer)
		ExFreePool(buffer);
	if (hSourceFile)
		FltClose(hSourceFile);
	if (hTargetFile)
		FltClose(hTargetFile);

	return status;
}

bool IsBackupDirectory(_In_ PCUNICODE_STRING directory) {
	// no counted version of wcsstr :(

	ULONG maxSize = 1024;
	if (directory->Length > maxSize)
		return false;

	auto copy = (WCHAR*)ExAllocatePoolWithTag(PagedPool, maxSize + sizeof(WCHAR), DRIVER_TAG);
	if (!copy)
		return false;

	RtlZeroMemory(copy, maxSize + sizeof(WCHAR));
	wcsncpy_s(copy, 1 + maxSize / sizeof(WCHAR), directory->Buffer, directory->Length / sizeof(WCHAR));
	_wcslwr(copy);
	bool doBackup = wcsstr(copy, L"\\pictures\\") || wcsstr(copy, L"\\documents\\");
	ExFreePool(copy);

	return doBackup;
}


/*************************************************************************
	MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

	This is the initialization routine for this miniFilter driver.  This
	registers with FltMgr and initializes all global data structures.

Arguments:

	DriverObject - Pointer to driver object created by the system to
		represent this driver.

	RegistryPath - Unicode string identifying where the parameters for this
		driver are located in the registry.

Return Value:

	Routine can return non success error codes.

--*/
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(RegistryPath);

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("FileBackup!DriverEntry: Entered\n"));

	//
	//  Register with FltMgr to tell it our callback routines
	//

	status = FltRegisterFilter(DriverObject,
		&FilterRegistration,
		&gFilterHandle);

	FLT_ASSERT(NT_SUCCESS(status));
	if (!NT_SUCCESS(status))
		return status;
	
	do {
		UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\FileBackupPort");
		PSECURITY_DESCRIPTOR sd;

		status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
		if (!NT_SUCCESS(status))
			break;

		OBJECT_ATTRIBUTES attr;
		InitializeObjectAttributes(&attr, &name, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, sd);

		status = FltCreateCommunicationPort(gFilterHandle, &FilterPort, &attr, nullptr,
			PortConnectNotify, PortDisconnectNotify, PortMessageNotify, 1);

		FltFreeSecurityDescriptor(sd);
		if (!NT_SUCCESS(status))
			break;

		//  Start filtering i/o

		status = FltStartFiltering(gFilterHandle);

	} while (false);

	if (!NT_SUCCESS(status)) {
		FltUnregisterFilter(gFilterHandle);
	}

	return status;
}

NTSTATUS
FileBackupUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
/*++

Routine Description:

	This is the unload routine for this miniFilter driver. This is called
	when the minifilter is about to be unloaded. We can fail this unload
	request if this is not a mandatory unload indicated by the Flags
	parameter.

Arguments:

	Flags - Indicating if this is a mandatory unload.

Return Value:

	Returns STATUS_SUCCESS.

--*/
{
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("FileBackup!FileBackupUnload: Entered\n"));

	FltCloseCommunicationPort(FilterPort);
	FltUnregisterFilter(gFilterHandle);

	return STATUS_SUCCESS;
}


/*************************************************************************
	MiniFilter callback routines.
*************************************************************************/

FLT_POSTOP_CALLBACK_STATUS
FileBackupPostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
/*++

Routine Description:

	This routine is the post-operation completion routine for this
	miniFilter.

	This is non-pageable because it may be called at DPC level.

Arguments:

	Data - Pointer to the filter callbackData that is passed to us.

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance, its associated volume and
		file object.

	CompletionContext - The completion context set in the pre-operation routine.

	Flags - Denotes whether the completion is successful or is being drained.

Return Value:

	The return value is the status of the operation.

--*/
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("FileBackup!FileBackupPostOperation: Entered\n"));

	return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
FileBackupPreOperationNoPostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
/*++

Routine Description:

	This routine is a pre-operation dispatch routine for this miniFilter.

	This is non-pageable because it could be called on the paging path

Arguments:

	Data - Pointer to the filter callbackData that is passed to us.

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance, its associated volume and
		file object.

	CompletionContext - The context for the completion routine for this
		operation.

Return Value:

	The return value is the status of the operation.

--*/
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("FileBackup!FileBackupPreOperationNoPostOperation: Entered\n"));

	// This template code does not do anything with the callbackData, but
	// rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
	// This passes the request down to the next miniFilter in the chain.

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


