#include "stdafx.h"

const wchar_t default_guid[] = L"{875CB1A1-0F29-45de-A1AE-CFB4950D0B78}";

#define fail() { std::cout << std::hex << (unsigned int) hr << " " << GetLastError(); return 1; }
const HRESULT BROKEN_FILE = 0xc00d0026;
const HRESULT EMPTY_FILE =  0xc00d001f; // empty file (audacity), file containing only id3v2 utf-8 tags

int _tmain(int argc, _TCHAR* argv[])
{
	CLSID wid;
	CoInitialize(NULL);
	CLSIDFromString(const_cast<wchar_t*>(default_guid), &wid);
	IInitializeWithFile *oo;
	HRESULT hr;
	if ((hr = CoCreateInstance(wid, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&oo))) != S_OK)
		fail();
#if 0
	IStorage *isto;
	if ((hr = StgCreateDocfile(L"t.mp3", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DIRECT, 0, &isto)) != S_OK)
		fail();
	IStream *is;
	if ((hr = isto->CreateStream(L"lol", STGM_DIRECT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &is)) != S_OK)
		fail();
#endif
	IPropertyStore *ips;
	IPropertyStoreCapabilities *ipcs;
	if ((hr = oo->QueryInterface(IID_PPV_ARGS(&ips))) != S_OK)
		fail();

	if ((hr = oo->QueryInterface(IID_PPV_ARGS(&ipcs))) != S_OK)
		fail();

	//iws->Initialize(is, STGM_READWRITE);
	if ((hr = oo->Initialize(L"t.mp3", STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DIRECT)) != S_OK)
		fail();

	DWORD props;
	if ((hr = ips->GetCount(&props)) != S_OK)
		fail();

	for (DWORD i = 0; i < props; ++i)
	{
		PROPERTYKEY pkey;
		ips->GetAt(i, &pkey);
		WCHAR *sz;
		PSGetNameFromPropertyKey(pkey, &sz);
		std::wcout << (ipcs->IsPropertyWritable(pkey) == S_OK) << "\t" << sz << std::endl;
	}
	return 0;
}
