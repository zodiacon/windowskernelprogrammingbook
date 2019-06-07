#pragma once

#include <ntddk.h>

class kstring final {
public:
	explicit kstring(const wchar_t* str = nullptr, POOL_TYPE pool = PagedPool, ULONG tag = 0);
	kstring(const wchar_t* str, ULONG count, POOL_TYPE pool = PagedPool, ULONG tag = 0);
	kstring(const kstring& other);
	explicit kstring(PCUNICODE_STRING str, POOL_TYPE pool = PagedPool, ULONG tag = 0);
	kstring& operator= (const kstring& other);
	kstring(kstring&& other);
	kstring& operator=(kstring&& other);

	~kstring();

	kstring& operator+=(const kstring& other);
	kstring& operator+=(PCWSTR str);

	bool operator==(const kstring& other);

	operator const wchar_t* () const {
		return m_str;
	}

	const wchar_t* Get() const {
		return m_str;
	}

	ULONG Length() const {
		return m_Len;
	}

	kstring ToLower() const;
	kstring& ToLower();
	kstring& Truncate(ULONG length);
	kstring& Append(PCWSTR str, ULONG len = 0);

	void Release();

	inline const wchar_t GetAt(size_t index) const;

	wchar_t& GetAt(size_t index);

	const wchar_t operator[](size_t index) const {
		return GetAt(index);
	}

	wchar_t& operator[](size_t index) {
		return GetAt(index);
	}

	UNICODE_STRING* GetUnicodeString(PUNICODE_STRING);

private:
	wchar_t* Allocate(size_t chars, const wchar_t* src = nullptr);

private:
	wchar_t* m_str;
	ULONG m_Len, m_Capacity;
	POOL_TYPE m_Pool;
	ULONG m_Tag;
};
