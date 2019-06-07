#pragma once

#include "FastMutex.h"

const int MaxMonitoredDevices = 32;

struct MonitoredDevice {
	UNICODE_STRING DeviceName;
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_OBJECT LowerDeviceObject;
};

struct DeviceExtension {
	PDEVICE_OBJECT LowerDeviceObject;
};

class DevMonManager {
public:
	void Init(PDRIVER_OBJECT DriverObject);
	NTSTATUS AddDevice(PCWSTR name);
	int FindDevice(PCWSTR name) const;
	bool RemoveDevice(PCWSTR name);
	void RemoveAllDevices();

	PDEVICE_OBJECT CDO;

private:
	bool RemoveDevice(int index);

private:
	MonitoredDevice Devices[MaxMonitoredDevices];
	int MonitoredDeviceCount;
	FastMutex Lock;
	PDRIVER_OBJECT DriverObject;
};

