#include "pch.h"
#include "ExecutiveResource.h"

void ExecutiveResource::Init() {
	ExInitializeResourceLite(&_resource);
}

void ExecutiveResource::Lock() {
	ExAcquireResourceExclusiveLite(&_resource, TRUE);
}

void ExecutiveResource::Unlock() {
	ExReleaseResourceLite(&_resource);
}
