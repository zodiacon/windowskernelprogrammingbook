#include <ntddk.h>

#define DRIVER_TAG 'dcba'

UNICODE_STRING g_RegistryPath;

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);

	ExFreePool(g_RegistryPath.Buffer);
	KdPrint(("Sample driver Unload called\n"));
}

extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	g_RegistryPath.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, RegistryPath->Length, DRIVER_TAG);

	if (g_RegistryPath.Buffer == nullptr) {
		KdPrint(("Failed to allocate memory\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	g_RegistryPath.MaximumLength = RegistryPath->Length;
	RtlCopyUnicodeString(&g_RegistryPath, (PCUNICODE_STRING)RegistryPath);
	KdPrint(("Copied registry path: %wZ\n", &g_RegistryPath));

	DriverObject->DriverUnload = SampleUnload;

	RTL_OSVERSIONINFOW info = { sizeof(info) };
	RtlGetVersion(&info);
	KdPrint(("Windows Version: %d.%d.%d\n", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber));

	KdPrint(("Sample driver initialized successfully\n"));

	return STATUS_SUCCESS;
}
