#pragma once

#include <shlwapi.h> // Registry helper APIs
#include <shlobj.h>  // SHChangeNotify

// {66AC53F1-EB5D-4af9-861E-E3AE9C07EF14}
#define SZ_CLSID_US           L"{66AC53F1-EB5D-4af9-861E-E3AE9C07EF14}"
static const GUID CLSID_US =
{ 0x66ac53f1, 0xeb5d, 0x4af9, { 0x86, 0x1e, 0xe3, 0xae, 0x9c, 0x7, 0xef, 0x14 } };

// Registry path strings
#define SZ_APPROVEDSHELLEXTENSIONS        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"
#define SZ_TAGLIBPROPERTYHANDLER           L"TagLib Property Handler"

// A struct to hold the information required for a registry entry
struct REGISTRY_ENTRY
{
	HKEY    hkeyRoot;
	LPCWSTR pszKeyName;
	LPCWSTR pszValueName;
	DWORD   dwType;
	LPCWSTR pszData;
	DWORD   dwData;
};

LONG createRegKey(const HKEY base, const LPCWSTR name, HKEY& out);

typedef LONG (*createKeyFunc)(const HKEY,const LPCWSTR,HKEY &);

// Creates a registry key (if needed) and sets the default value of the key
HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY &pRegistryEntry, 
	createKeyFunc createKey = &createRegKey);

HRESULT doRegistration(LPWSTR szModuleName, 
	createKeyFunc createKey = &createRegKey);

