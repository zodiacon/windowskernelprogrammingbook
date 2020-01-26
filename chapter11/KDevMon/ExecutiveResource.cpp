#include "pch.h"
#include "ExecutiveResource.h"

ExecutiveResource::~ExecutiveResource() {
	ExDeleteResourceLite(&_resource);
}

void ExecutiveResource::Init() {
	ExInitializeResourceLite(&_resource);
}

void ExecutiveResource::Lock() {
	ExAcquireResourceExclusiveLite(&_resource, TRUE);
}

void ExecutiveResource::Unlock() {
	ExReleaseResourceLite(&_resource);
}

void ExecutiveResource::LockShared() {
	ExAcquireResourceSharedLite(&_resource, TRUE);
}

void ExecutiveResource::UnlockShared() {
	ExAcquireResourceSharedLite(&_resource, TRUE);
}
