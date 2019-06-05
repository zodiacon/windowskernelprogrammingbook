#pragma once

#include "FastMutex.h"

const int MaxMonitoredDevices = 32;

struct MonitoredDevice {
	UNICODE_STRING DeviceName;
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_OBJECT LowerDeviceObject;
};

struct DeviceExtension {
	int Index;
};

class DevMonManager {
public:
	void Init(PDRIVER_OBJECT DriverObject);
	NTSTATUS AddDevice(PCWSTR name);
	int FindDevice(PCWSTR name);
	bool RemoveDevice(PCWSTR name);
	void RemoveAllDevices();

	void Unload();

	PDEVICE_OBJECT CDO;

	MonitoredDevice Devices[MaxMonitoredDevices];
	int MonitoredDeviceCount;
	FastMutex Lock;
	PDRIVER_OBJECT DriverObject;
	bool IsMonitoring;

private:
	bool RemoveDevice(int index);
};

