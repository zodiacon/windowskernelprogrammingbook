#include "pch.h"

#include "ProcessProtectCommon.h"
#include "ProcessProtect.h"
#include "AutoLock.h"

// PROTOTYPES

DRIVER_UNLOAD ProcessProtectUnload;
DRIVER_DISPATCH ProcessProtectCreateClose, ProcessProtectDeviceControl;

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info);

bool FindProcess(ULONG pid);
bool AddProcess(ULONG pid);
bool RemoveProcess(ULONG pid);

// GLOBALS

Globals g_Data;

// DriverEntry

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	KdPrint((DRIVER_PREFIX "DriverEntry entered\n"));

	g_Data.Init();

	OB_OPERATION_REGISTRATION operations[] = {
		{ 
			PsProcessType,		// object type
			OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE, 
			OnPreOpenProcess, nullptr	// pre, post
		}
	};
	OB_CALLBACK_REGISTRATION reg = {
		OB_FLT_REGISTRATION_VERSION, 
		1,				// operation count
		RTL_CONSTANT_STRING(L"12345.6171"),		// altitude
		nullptr,		// context
		operations
	};

	auto status = STATUS_SUCCESS;
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\" PROCESS_PROTECT_NAME);
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\" PROCESS_PROTECT_NAME);
	PDEVICE_OBJECT DeviceObject = nullptr;

	do {
		status = ObRegisterCallbacks(&reg, &g_Data.RegHandle);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to register callbacks (status=%08X)\n", status));
			break;
		}

		status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device object (status=%08X)\n", status));
			break;
		}

		status = IoCreateSymbolicLink(&symName, &deviceName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create symbolic link (status=%08X)\n", status));
			break;
		}
	} while (false);

	if (!NT_SUCCESS(status)) {
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
		if(g_Data.RegHandle)
			ObUnRegisterCallbacks(&g_Data.RegHandle);
	}

	DriverObject->DriverUnload = ProcessProtectUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = ProcessProtectCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProcessProtectDeviceControl;

	KdPrint((DRIVER_PREFIX "DriverEntry completed successfully\n"));

	return status;
}

void ProcessProtectUnload(PDRIVER_OBJECT DriverObject) {
	ObUnRegisterCallbacks(g_Data.RegHandle);

	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\" PROCESS_PROTECT_NAME);
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ProcessProtectCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS ProcessProtectDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	auto len = 0;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_PROCESS_PROTECT_BY_PID:
		{
			auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
			if (size % sizeof(ULONG) != 0) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;

			AutoLock locker(g_Data.Lock);

			for (int i = 0; i < size / sizeof(ULONG); i++) {
				auto pid = data[i];
				if (pid == 0) {
					status = STATUS_INVALID_PARAMETER;
					break;
				}
				if (FindProcess(pid))
					continue;

				if (g_Data.PidsCount == MaxPids) {
					status = STATUS_TOO_MANY_CONTEXT_IDS;
					break;
				}

				if (!AddProcess(pid)) {
					status = STATUS_UNSUCCESSFUL;
					break;
				}

				len += sizeof(ULONG);
			}

			break;
		}

		case IOCTL_PROCESS_UNPROTECT_BY_PID:
		{
			auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
			if (size % sizeof(ULONG) != 0) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;

			AutoLock locker(g_Data.Lock);

			for (int i = 0; i < size / sizeof(ULONG); i++) {
				auto pid = data[i];
				if (pid == 0) {
					status = STATUS_INVALID_PARAMETER;
					break;
				}
				if (!RemoveProcess(pid))
					continue;

				len += sizeof(ULONG);

				if (g_Data.PidsCount == 0)
					break;
			}

			break;
		}

		case IOCTL_PROCESS_PROTECT_CLEAR:
		{
			AutoLock locker(g_Data.Lock);
			::memset(&g_Data.Pids, 0, sizeof(g_Data.Pids));
			g_Data.PidsCount = 0;
			break;
		}

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID /* RegistrationContext */, POB_PRE_OPERATION_INFORMATION Info) {
	if(Info->KernelHandle)
		return OB_PREOP_SUCCESS;

	auto process = (PEPROCESS)Info->Object;
	auto pid = HandleToULong(PsGetProcessId(process));

	AutoLock locker(g_Data.Lock);
	if (FindProcess(pid)) {
		// found in list, remove terminate access
		Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
	}

	return OB_PREOP_SUCCESS;
}

bool FindProcess(ULONG pid) {
	for (int i = 0; i < MaxPids; i++)
		if (g_Data.Pids[i] == pid)
			return true;
	return false;
}

bool AddProcess(ULONG pid) {
    for(int i = 0; i < MaxPids; i++)
        if (g_Data.Pids[i] == 0) {
            // empty slot
            g_Data.Pids[i] = pid;
            g_Data.PidsCount++;
            return true;
        }
    return false;
}

bool RemoveProcess(ULONG pid) {
    for (int i = 0; i < MaxPids; i++)
        if (g_Data.Pids[i] == pid) {
            g_Data.Pids[i] = 0;
            g_Data.PidsCount--;
            return true;
        }
    return false;
}

