#include <shlwapi.h> // Registry helper APIs
#include <shlobj.h>  // SHChangeNotify

// {66AC53F1-EB5D-4af9-861E-E3AE9C07EF14}
#define SZ_CLSID_US           L"{66AC53F1-EB5D-4af9-861E-E3AE9C07EF14}"
static const GUID CLSID_US =
{ 0x66ac53f1, 0xeb5d, 0x4af9, { 0x86, 0x1e, 0xe3, 0xae, 0x9c, 0x7, 0xef, 0x14 } };

// Registry path strings
#define SZ_APPROVEDSHELLEXTENSIONS        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"
#define SZ_TAGLIBPROPERTYHANDLER           L"TagLib Property Handler"

// Reference count for the DLL
LONG g_cLocks = 0;

// Handle the the DLL's module
HMODULE g_hmodThis;

// Standard DLL functions
BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	g_hmodThis = (HMODULE)hModule;

	return TRUE;
}

STDAPI DllCanUnloadNow()
{
	// Only allow the DLL to be unloaded after all outstanding references have been released
	return (g_cLocks == 0) ? S_OK : S_FALSE;
}

void DllAddRef()
{
	// Increment the reference count on the DLL
	InterlockedIncrement(&g_cLocks);
}

void DllRelease()
{
	// Decrement the reference count on the DLL
	InterlockedDecrement(&g_cLocks);
}


// Constructor for CTagLibPropertyStore
HRESULT CTagLibPropertyStore_CreateInstance(REFIID riid, void **ppv);

// Class factory for CTagLibPropertyHandler
class CTagLibPropertyHandlerClassFactory  : public IClassFactory
{
public:
	// IUnknown methods
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		static const QITAB qit[] = {
			QITABENT(CTagLibPropertyHandlerClassFactory, IClassFactory),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		DllAddRef();
		return 2; // Object's lifetime is tied to the DLL, so no need to track its refcount
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		DllRelease();
		return 1; // Object's lifetime is tied to the DLL, so no need to track its refcount or manually delete it
	}

	// IClassFactory methods
	IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
	{
		// Aggregation is not supported
		return pUnkOuter ? CLASS_E_NOAGGREGATION : CTagLibPropertyStore_CreateInstance(riid, ppv);
	}

	IFACEMETHODIMP LockServer(BOOL fLock)
	{
		if (fLock)
			DllAddRef();
		else
			DllRelease();

		return S_OK;
	}
};

// Single global instance of the class factory object
CTagLibPropertyHandlerClassFactory g_cfDebugPropertyHandler;

// Export called by CoCreateInstance to obtain a class factory for the specified CLSID
STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
	if (clsid == CLSID_US)
		return g_cfDebugPropertyHandler.QueryInterface(riid, ppv);

	return CLASS_E_CLASSNOTAVAILABLE;
}

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


// Creates a registry key (if needed) and sets the default value of the key
HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY &pRegistryEntry)
{
	HRESULT hr;

	// create the key, or obtain its handle if it already exists
	HKEY hKey;
	LONG lr = RegCreateKeyExW(pRegistryEntry.hkeyRoot,
		pRegistryEntry.pszKeyName,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		NULL);

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

// Registers this COM server
STDAPI DllRegisterServer()
{
	HRESULT hr = E_FAIL;

	WCHAR szModuleName[MAX_PATH];

	if (!GetModuleFileNameW(g_hmodThis, szModuleName, ARRAYSIZE(szModuleName)))
		hr = HRESULT_FROM_WIN32(GetLastError());
	else
	{
		// List of property-handler specific registry entries we need to create
		static const REGISTRY_ENTRY rgRegistryEntries[] =
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
					L"CLSID\\" SZ_CLSID_US,
					L"ManualSafeSave",
					REG_DWORD,
					NULL,
					1
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
					L"Apartment",
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

		hr = S_OK;
		for (int i = 0; i < ARRAYSIZE(rgRegistryEntries) && SUCCEEDED(hr); i++)
		{
			hr = CreateRegKeyAndSetValue(rgRegistryEntries[i]);
			if (FAILED(hr))
				return hr;
		}

		// inform Explorer, et al of the new handler
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
	}

	return hr;
}


//
// Unregisters this COM server
//
STDAPI DllUnregisterServer()
{
	HRESULT hr = S_OK;

	// attempt to delete everything, even if some operations fail
	DWORD dwCLSID =           SHDeleteKeyW(HKEY_CLASSES_ROOT, L"CLSID\\" SZ_CLSID_US);
	DWORD dwShellExtension =  SHDeleteValueW(HKEY_LOCAL_MACHINE, SZ_APPROVEDSHELLEXTENSIONS, SZ_CLSID_US);

	// return first error encountered as HRESULT
	if (dwCLSID != ERROR_SUCCESS)
	{
		hr = HRESULT_FROM_WIN32(dwCLSID);
	}
	else if (dwShellExtension != ERROR_SUCCESS)
	{
		hr = HRESULT_FROM_WIN32(dwShellExtension);
	}

	if (SUCCEEDED(hr))
	{
		// inform Explorer, et al that the handler has been unregistered
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
	}

	return hr;
}
