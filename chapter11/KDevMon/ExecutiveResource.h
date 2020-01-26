#pragma once

class ExecutiveResource {
public:
	~ExecutiveResource();

	void Init();
	void Lock();
	void Unlock();
	void LockShared();
	void UnlockShared();

private:
	ERESOURCE _resource;
};

