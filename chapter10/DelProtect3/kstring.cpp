#include "kstring.h"

kstring::kstring(const wchar_t* str, POOL_TYPE pool, ULONG tag) : kstring(str, 0, pool, tag) {
}

kstring::kstring(const wchar_t * str, ULONG count, POOL_TYPE pool, ULONG tag) : m_Pool(pool), m_Tag(tag) {
	if (str) {
		m_Len = count == 0 ? static_cast<ULONG>(wcslen(str)) : count;
		m_Capacity = m_Len + 1;
		m_str = Allocate(m_Capacity, str);
		if (!m_str)
			ExRaiseStatus(STATUS_NO_MEMORY);
	}
	else {
		m_str = nullptr;
		m_Len = m_Capacity = 0;
	}
}

kstring::~kstring() {
	Release();
}

void kstring::Release() {
	if (m_str)
		ExFreePoolWithTag(m_str, m_Tag);
}

kstring::kstring(kstring&& other) {
	m_Len = other.m_Len;
	m_str = other.m_str;
	m_Pool = other.m_Pool;
	other.m_str = nullptr;
	other.m_Len = 0;
}

kstring& kstring::operator+=(const kstring& other) {
	return Append(other);
}

kstring& kstring::operator+=(PCWSTR str) {
	m_Len += static_cast<ULONG>(::wcslen(str));
	auto newBuffer = Allocate(m_Len, m_str);
	::wcscat_s(newBuffer, m_Len + 1, str);
	Release();
	m_str = newBuffer;
	return *this;
}

bool kstring::operator==(const kstring& other) {
	return wcscmp(m_str, other.m_str) == 0;
}

const wchar_t kstring::GetAt(size_t index) const {
	NT_ASSERT(index < m_Len);
	return m_str[index];
}

wchar_t& kstring::GetAt(size_t index) {
	NT_ASSERT(index < m_Len);
	return m_str[index];
}

kstring& kstring::operator=(kstring&& other) {
	if (this != &other) {
		if (m_str)
			ExFreePoolWithTag(m_str, m_Tag);
		m_Len = other.m_Len;
		m_str = other.m_str;
		other.m_str = nullptr;
		other.m_Len = 0;
	}
	return *this;
}

kstring::kstring(PCUNICODE_STRING str, POOL_TYPE pool, ULONG tag) : m_Pool(pool), m_Tag(tag) {
	m_Len = str->Length / sizeof(WCHAR);
	m_str = Allocate(m_Len, str->Buffer);
}

kstring::kstring(PUNICODE_STRING str, POOL_TYPE pool, ULONG tag) : m_Pool(pool), m_Tag(tag) {
	m_Len = str->Length / sizeof(WCHAR);
	m_str = Allocate(m_Len, str->Buffer);
}

kstring::kstring(const kstring& other) : m_Len(other.m_Len) {
	m_Pool = other.m_Pool;
	m_Tag = other.m_Tag;
	if (m_Len > 0) {
		m_str = Allocate(m_Len, other.m_str);
	}
	else {
		m_str = nullptr;
	}
}

kstring& kstring::operator=(const kstring& other) {
	if (this != &other) {
		if (m_str)
			ExFreePoolWithTag(m_str, m_Tag);
		m_Len = other.m_Len;
		m_Tag = other.m_Tag;
		m_Pool = other.m_Pool;
		if (other.m_str) {
			m_str = Allocate(m_Len, other.m_str);
		}
	}
	return *this;
}

UNICODE_STRING* kstring::GetUnicodeString(PUNICODE_STRING pUnicodeString) {
	RtlInitUnicodeString(pUnicodeString, m_str);
	return pUnicodeString;
}

wchar_t* kstring::Allocate(size_t chars, const wchar_t* src) {
	auto str = static_cast<wchar_t*>(ExAllocatePoolWithTag(m_Pool, sizeof(WCHAR) * (chars + 1), m_Tag));
	if (!str) {
		KdPrint(("Failed to allocate kstring of length %d chars\n", chars));
		return nullptr;
	}
	if (src) {
		wcscpy_s(str, chars + 1, src);
	}
	return str;
}

kstring kstring::ToLower() const {
	kstring temp(m_str);
	for (size_t i = 0; i < temp.Length(); ++i)
		temp[i] = ::towlower(temp[i]);
	return temp;
}

kstring& kstring::Truncate(ULONG count) {
	if (count >= m_Len) {
		NT_ASSERT(false);
	}
	else {
		m_Len = count;
		m_str[m_Len] = L'\0';
	}
	return *this;
}

kstring & kstring::Append(PCWSTR str, ULONG len) {
	if (len == 0)
		len = (ULONG)::wcslen(str);
	auto newBuffer = m_str;
	auto newAlloc = false;
	m_Len += len;
	if (m_Len + 1 > m_Capacity) {
		newBuffer = Allocate(m_Capacity = m_Len + 8, m_str);
		newAlloc = true;
	}
	::wcsncat(newBuffer, str, len);
	if (newAlloc) {
		Release();
		m_str = newBuffer;
	}
	return *this;
}
