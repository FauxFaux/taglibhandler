//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//

#define NOMINMAX

#include <string>

#include <initguid.h>
#include <mmdeviceapi.h>

#include <shobjidl.h>    // IInitializeWithStream, IDestinationStreamFactory
#include <propsys.h>     // Property System APIs and interfaces
#include <propkey.h>     // System PROPERTYKEY definitions
#include <propvarutil.h> // PROPVARIANT and VARIANT helper APIs

#include <tag.h>
#include <fileref.h>

#include "exttag.h"

//
// Releases the specified pointer if not NULL
//
#define SAFE_RELEASE(p)     \
	if (p)                  \
	{                       \
		(p)->Release();     \
		(p) = NULL;         \
	}

// DLL lifetime management functions
void DllAddRef();
void DllRelease();

// Debug property handler class definition
class CTagLibPropertyStore :
	public IPropertyStore,
	public IPropertyStoreCapabilities,
	public IInitializeWithStream
{
public:
	static HRESULT CreateInstance(REFIID riid, void **ppv);

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		static const QITAB qit[] = {
			QITABENT(CTagLibPropertyStore, IPropertyStore),
			QITABENT(CTagLibPropertyStore, IPropertyStoreCapabilities),
			QITABENT(CTagLibPropertyStore, IInitializeWithStream),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		DllAddRef();
		return InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		DllRelease();
		ULONG cRef = InterlockedDecrement(&_cRef);
		if (!cRef)
		{
			delete this;
		}
		return cRef;
	}

	// IPropertyStore
	IFACEMETHODIMP GetCount(__out DWORD *pcProps);
	IFACEMETHODIMP GetAt(DWORD iProp, __out PROPERTYKEY *pkey);
	IFACEMETHODIMP GetValue(REFPROPERTYKEY key, __out PROPVARIANT *pPropVar);
	IFACEMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propVar);
	IFACEMETHODIMP Commit();

	// IPropertyStoreCapabilities
	IFACEMETHODIMP IsPropertyWritable(REFPROPERTYKEY key);

	// IInitializeWithStream
	IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

protected:
	CTagLibPropertyStore() : _cRef(1), _pStream(NULL), _grfMode(0)
	{
		DllAddRef();
	}

	~CTagLibPropertyStore()
	{
		SAFE_RELEASE(_pStream);
	}

	IStream*             _pStream; // data stream passed in to Initialize, and saved to on Commit
	DWORD                _grfMode; // STGM mode passed to Initialize
	TagLib::FileRef taglibfile;
private:

	long _cRef;
};

bool operator==(REFPROPERTYKEY left, REFPROPERTYKEY right)
{
	return IsEqualPropertyKey(left, right);
}

HRESULT CTagLibPropertyStore::GetValue(REFPROPERTYKEY key, __out PROPVARIANT *pPropVar)
{
	const TagLib::AudioProperties *ap = taglibfile.audioProperties();
	const TagLib::Tag *tag = taglibfile.tag();

	if (ap && key == PKEY_Audio_ChannelCount)
	{
		pPropVar->uintVal = ap->channels();
		pPropVar->vt = VT_UI4;
	}
	else if (ap && key == PKEY_Media_Duration)
	{
		pPropVar->ulVal = ap->length()*10000000;
		pPropVar->vt = VT_UI8;
	}
	else if (ap && key == PKEY_Audio_EncodingBitrate)
	{
		pPropVar->uintVal = ap->bitrate()*1024;
		pPropVar->vt = VT_UI4;
	}
	else if (ap && key == PKEY_Audio_SampleRate)
	{
		pPropVar->uintVal = ap->sampleRate();
		pPropVar->vt = VT_UI4;
	}
	else if (tag && key == PKEY_Music_AlbumTitle)
	{
		pPropVar->vt = VT_BSTR;
		pPropVar->bstrVal = SysAllocString(tag->album().toWString().c_str());
	}
	else if (tag && key == PKEY_Music_Artist)
	{
		pPropVar->vt = VT_BSTR;
		pPropVar->bstrVal = SysAllocString(tag->artist().toWString().c_str());
	}
	else if (tag && key == PKEY_Music_Genre)
	{
		pPropVar->vt = VT_BSTR;
		pPropVar->bstrVal = SysAllocString(tag->genre().toWString().c_str());
	}
	else if (tag && key == PKEY_Title)
	{
		pPropVar->vt = VT_BSTR;
		pPropVar->pwszVal = SysAllocString(tag->title().toWString().c_str());
	}
	else if (tag && key == PKEY_Music_TrackNumber)
	{
		if (tag->track() == 0)
			pPropVar->vt = VT_EMPTY;
		else
		{
			pPropVar->uintVal = tag->track();
			pPropVar->vt = VT_UI4;
		}
	}
	else if (tag && key == PKEY_Media_Year)
	{
		if (tag->year() == 0)
			pPropVar->vt = VT_EMPTY;
		else
		{
			pPropVar->uintVal = tag->year();
			pPropVar->vt = VT_UI4;
		}
	}
	else if (tag && key == PKEY_Rating)
	{
		pPropVar->uintVal = rating(tag);
		pPropVar->vt = VT_UI4;
	}
	else
		return S_FALSE;

	return S_OK;
}

HRESULT CTagLibPropertyStore::CreateInstance(REFIID riid, void **ppv)
{
	HRESULT hr = E_OUTOFMEMORY;
	CTagLibPropertyStore *pNew = new CTagLibPropertyStore();
	if (pNew)
	{
		hr = pNew->QueryInterface(riid, ppv);
		SAFE_RELEASE(pNew);
	}

	return hr;
}

HRESULT CTagLibPropertyStore_CreateInstance(REFIID riid, void **ppv)
{
	return CTagLibPropertyStore::CreateInstance(riid, ppv);
}

const PROPERTYKEY keys[] = {
	PKEY_Music_AlbumTitle, PKEY_Music_Artist,
	PKEY_Music_TrackNumber, PKEY_Music_Genre,
	PKEY_Title, PKEY_Media_Year, PKEY_Audio_ChannelCount,
	PKEY_Media_Duration, PKEY_Audio_EncodingBitrate,
	PKEY_Audio_SampleRate, PKEY_Rating,
};

HRESULT CTagLibPropertyStore::GetCount(__out DWORD *pcProps)
{
	return ARRAYSIZE(keys);
}

HRESULT CTagLibPropertyStore::GetAt(DWORD iProp, __out PROPERTYKEY *pkey)
{
	pkey = const_cast<PROPERTYKEY*>(&keys[iProp]);
	return S_OK;
}

// SetValue just updates the internal value cache
HRESULT CTagLibPropertyStore::SetValue(REFPROPERTYKEY key, REFPROPVARIANT propVar)
{
	return STG_E_ACCESSDENIED; // Ok, but read-only.
}

// Commit writes the internal value cache back out to the stream passed to Initialize
HRESULT CTagLibPropertyStore::Commit()
{
	return S_OK; // Ok, we wrote all we could (nothing).
}

// Indicates whether the users should be able to edit values for the given property key
// S_OK | S_FALSE
HRESULT CTagLibPropertyStore::IsPropertyWritable(REFPROPERTYKEY key)
{
	return S_FALSE;
}

struct IStreamAccessor : public TagLib::FileAccessor
{
	IStreamAccessor(IStream *stream) : stream(stream) {}
	IStream *stream;
	bool isOpen() const
	{
		return true;
	}

	size_t fread(void *pv, size_t s1, size_t s2) const
	{
		ULONG read = 0;
		stream->Read(pv, s1*s2, &read);
		return read;
	}

	size_t fwrite(const void *,size_t,size_t)
	{
		return 0;
	}

	int fseek(long distance, int direction)
	{
		LARGE_INTEGER dist;
		dist.QuadPart = distance;
		return FAILED(stream->Seek(dist, direction, NULL)); // 0 on success.
	}

	void clearError()
	{
		// Uh oh.
	}

	long tell() const
	{
		ULARGE_INTEGER newpos;
		LARGE_INTEGER dist = {};
		stream->Seek(dist, STREAM_SEEK_CUR, &newpos);
		return newpos.QuadPart;
	}

	int truncate(long length)
	{
		return -1; // error
	}

	TagLib::FileNameHandle name() const
	{
		STATSTG a;
		if (FAILED(stream->Stat(&a, STATFLAG_DEFAULT)))
			return TagLib::FileName("");
		return TagLib::FileName(a.pwcsName);
	}

	bool readOnly() const
	{
		return true;
	}

};

// Initialize populates the internal value cache with data from the specified stream
// S_OK | E_UNEXPECTED | ERROR_READ_FAULT | ERROR_FILE_CORRUPT | ERROR_INTERNAL_ERROR
HRESULT CTagLibPropertyStore::Initialize(IStream *pStream, DWORD grfMode)
{
	taglibfile = TagLib::FileRef(new IStreamAccessor(pStream));
	if (taglibfile.isNull())
		return ERROR_FILE_CORRUPT;
	return S_OK;
}
