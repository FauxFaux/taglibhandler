#include "dllregister.h"

// Reference count for the DLL
LONG g_cLocks = 0;

// Handle the the DLL's module
HMODULE g_hmodThis;

// Standard DLL functions
BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD, LPVOID)
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

// Registers this COM server
STDAPI DllRegisterServer()
{
	HRESULT hr = E_FAIL;

	WCHAR szModuleName[MAX_PATH];

	if (!GetModuleFileNameW(g_hmodThis, szModuleName, ARRAYSIZE(szModuleName)))
		hr = HRESULT_FROM_WIN32(GetLastError());
	else
	{
		if (FAILED(hr = doRegistration(szModuleName)))
			return hr;

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
