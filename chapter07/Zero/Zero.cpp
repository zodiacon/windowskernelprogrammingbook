#include "pch.h"
#include "ZeroCommon.h"

#define DRIVER_PREFIX "Zero: "

// prototypes

void ZeroUnload(PDRIVER_OBJECT DriverObject);
DRIVER_DISPATCH ZeroCreateClose, ZeroRead, ZeroWrite, ZeroDeviceControl;

// globals

long long g_TotalRead, g_TotalWritten;

// DriverEntry

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = ZeroUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ZeroDeviceControl;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\Zero");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	PDEVICE_OBJECT DeviceObject = nullptr;
	auto status = STATUS_SUCCESS;
	auto symLinkCreated = false;

	do {
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
			break;
		}
		DeviceObject->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create symbolic link (0x%08X)\n", status));
			break;
		}
		symLinkCreated = true;

	} while (false);

	if (!NT_SUCCESS(status)) {
		if (symLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
	}

	return status;
}

// implementation

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, 0);
	return status;
}

void ZeroUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ZeroCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	return CompleteIrp(Irp);
}

NTSTATUS ZeroRead(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Read.Length;
	if (len == 0)
		return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE);

	auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer)
		return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);

	memset(buffer, 0, len);
	//g_TotalRead += len;
	InterlockedAdd64(&g_TotalRead, len);

	return CompleteIrp(Irp, STATUS_SUCCESS, len);
}

NTSTATUS ZeroWrite(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Write.Length;
	//g_TotalWritten += len;
	InterlockedAdd64(&g_TotalWritten, len);
	return CompleteIrp(Irp, STATUS_SUCCESS, len);
}

NTSTATUS ZeroDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto& dic = stack->Parameters.DeviceIoControl;

	if (dic.IoControlCode != IOCTL_ZERO_GET_STATS)
		return CompleteIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);

	if (dic.OutputBufferLength < sizeof(ZeroStats))
		return CompleteIrp(Irp, STATUS_BUFFER_TOO_SMALL);

	auto stats = (ZeroStats*)Irp->AssociatedIrp.SystemBuffer;
	stats->TotalRead = g_TotalRead;
	stats->TotalWritten = g_TotalWritten;
	
	return CompleteIrp(Irp, STATUS_SUCCESS, sizeof(ZeroStats));
}
