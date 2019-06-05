#pragma once

class ExecutiveResource {
public:
	void Init();
	void Lock();
	void Unlock();
	void LockShared();
	void UnlockShared();

private:
	ERESOURCE _resource;
};

