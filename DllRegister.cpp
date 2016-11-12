#include "dllregister.h"
#include <iostream>

LONG createRegKey(const HKEY base, const LPCWSTR name, HKEY& out)
{
	return RegCreateKeyExW(base, name, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &out, NULL);
}

// Creates a registry key (if needed) and sets the default value of the key
HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY &pRegistryEntry, 
	LONG createKey(const HKEY,const LPCWSTR,HKEY &))
{
	HRESULT hr;

	// create the key, or obtain its handle if it already exists
	HKEY hKey;
	LONG lr = createKey(pRegistryEntry.hkeyRoot,
		pRegistryEntry.pszKeyName,
		hKey);

	hr = HRESULT_FROM_WIN32(lr);

	if (SUCCEEDED(hr))
	{
		// extract the data from the struct according to its type
		LPBYTE pData = NULL;
		DWORD cbData = 0;
		hr = S_OK;
		switch (pRegistryEntry.dwType)
		{
		case REG_SZ:
			pData = (LPBYTE) pRegistryEntry.pszData;
			cbData = ((DWORD) wcslen(pRegistryEntry.pszData) + 1) * sizeof(WCHAR);
			break;

		case REG_DWORD:
			pData = (LPBYTE) &pRegistryEntry.dwData;
			cbData = sizeof(pRegistryEntry.dwData);
			break;

		default:
			hr = E_INVALIDARG;
			break;
		}

		if (SUCCEEDED(hr))
		{
			// attempt to set the value
			lr = RegSetValueExW(hKey,
				pRegistryEntry.pszValueName,
				0,
				pRegistryEntry.dwType,
				pData,
				cbData);
			hr = HRESULT_FROM_WIN32(lr);
		}

		RegCloseKey(hKey);
	}

	return hr;
}

HRESULT doRegistration(LPWSTR szModuleName, 
	LONG createKey(const HKEY,const LPCWSTR,HKEY &))
{
	{
		wchar_t user_profile[MAX_PATH];
		if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, user_profile))) {
			if (wcsstr(szModuleName, user_profile) == szModuleName)
				std::cout << "WARNING: you are registering this module within your user folder. Search indexing will not work unless it is registered from a system-wide path." << std::endl;
		}
	}

	// List of property-handler specific registry entries we need to create
	const REGISTRY_ENTRY rgRegistryEntries[] =
	{
		// COM information
		{
			HKEY_CLASSES_ROOT,
			L"CLSID\\" SZ_CLSID_US,
			NULL,
			REG_SZ,
			SZ_TAGLIBPROPERTYHANDLER,
			0
		},
		{
			HKEY_CLASSES_ROOT,
			L"CLSID\\" SZ_CLSID_US L"\\InProcServer32",
			NULL,
			REG_SZ,
			szModuleName,
			0
		},
		{
		HKEY_CLASSES_ROOT,
			L"CLSID\\" SZ_CLSID_US L"\\InProcServer32",
			L"ThreadingModel",
			REG_SZ,
			L"Both",
			0
		},

		// Shell information
		{
			HKEY_LOCAL_MACHINE,
			SZ_APPROVEDSHELLEXTENSIONS,
			SZ_CLSID_US,
			REG_SZ,
			SZ_TAGLIBPROPERTYHANDLER,
			0
		}
	};

	HRESULT hr;
	hr = S_OK;
	for (int i = 0; i < ARRAYSIZE(rgRegistryEntries) && SUCCEEDED(hr); i++)
	{
		hr = CreateRegKeyAndSetValue(rgRegistryEntries[i], createKey);
		if (FAILED(hr))
			return hr;
	}
	return hr;
}
