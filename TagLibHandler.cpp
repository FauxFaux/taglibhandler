#define NOMINMAX

#include <string>
#include <sstream>
#include <map>
#include <assert.h>

#include <initguid.h>
#include <mmdeviceapi.h>

#include <shobjidl.h>    // IInitializeWithStream, IDestinationStreamFactory
#include <propsys.h>     // Property System APIs and interfaces
#include <propkey.h>     // System PROPERTYKEY definitions
#include <propvarutil.h> // PROPVARIANT and VARIANT helper APIs

#include <shlwapi.h>
#include <thumbcache.h>

#include <tag.h>
#include <fileref.h>

#include "exttag.h"
#include "events.h"

template <typename T>
class COMWrapper
{
	T* comPtr;
public:
	COMWrapper(T * const comPtr = nullptr) : comPtr(comPtr) {}
	COMWrapper(const COMWrapper& other) : comPtr(other)
	{
		if (other)
			other->AddRef();
	}
	~COMWrapper()
	{
		Release();
	}
	
	T* operator->() const { return comPtr; }
	operator T*() const { return comPtr; }
	
	void operator=(T* newPtr)
	{
		if (comPtr == newPtr)
			return;

		Release();
		comPtr = newPtr; // don't add a reference
	}
	T** GetForModify()
	{
		assert(!comPtr); // this method should be used only to initialise an empty COMWrapper.
		return &comPtr;
	}
	
	void Release()
	{
		if (comPtr) comPtr->Release();
		comPtr = nullptr;
	}
};

std::string HResultMessage(HRESULT hr)
{
	char errorBuf[256];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBuf, 256, nullptr);
	return errorBuf;
}

#include "toolkit/tdebuglistener.h"
class DebugListener : TagLib::DebugListener
{
	static std::string LastError;
public:

	DebugListener()
	{
		TagLib::setDebugListener(this);
	}

	void printMessage(const TagLib::String &msg) override
	{
		LastError = msg.to8Bit();
	}

	static std::string GetLastError() { return LastError; }
};

std::string DebugListener::LastError = "No errors recorded";

// DLL lifetime management functions
void DllAddRef();
void DllRelease();

const PROPERTYKEY keys[] = {
	// If you change this array, also change the "PropertyNames" map in events.man.
	PKEY_Music_AlbumTitle, PKEY_Music_Artist,
	PKEY_Music_TrackNumber, PKEY_Music_Genre,
	PKEY_Title, PKEY_Media_Year, PKEY_Audio_ChannelCount,
	PKEY_Media_Duration, PKEY_Audio_EncodingBitrate,
	PKEY_Audio_SampleRate, PKEY_Rating, PKEY_Music_AlbumArtist,
	PKEY_Music_Composer, PKEY_Music_Conductor,
	PKEY_Media_Publisher, PKEY_Media_SubTitle,
	PKEY_Media_Producer, PKEY_Music_Mood, PKEY_Copyright,
	PKEY_Music_PartOfSet,
	PKEY_Keywords, PKEY_Comment, PKEY_Media_DateReleased
};

bool operator<(const PROPERTYKEY& a, const PROPERTYKEY& b)
{
	const int guid_compare = memcmp(&a.fmtid, &b.fmtid, sizeof(GUID));

	if (guid_compare == 0)
		return a.pid < b.pid;
	else
		return guid_compare < 0;
}
typedef std::map<PROPERTYKEY, unsigned int> KeyMap;
static KeyMap KeyIDs = []
{
	KeyMap out;
	for (size_t i = 0; i < ARRAYSIZE(keys); ++i)
	{
		out[keys[i]] = (unsigned int)i;
	}
	return out;
}();

// Debug property handler class definition
class CTagLibPropertyStore :
	public IPropertyStore,
	public IPropertyStoreCapabilities,
	public IThumbnailProvider,
	public IInitializeWithStream,
	public IInitializeWithFile
{
public:
	static HRESULT CreateInstance(REFIID riid, void **ppv);

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		static const QITAB qit[] = {
			QITABENT(CTagLibPropertyStore, IPropertyStore),
			QITABENT(CTagLibPropertyStore, IPropertyStoreCapabilities),
			QITABENT(CTagLibPropertyStore, IThumbnailProvider),
			QITABENT(CTagLibPropertyStore, IInitializeWithStream),
			QITABENT(CTagLibPropertyStore, IInitializeWithFile),
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

	// IThumbnailProvider
	IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

	// IInitializeWithStream
	IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

	// IInitializeWithFile
	IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode);

protected:
	CTagLibPropertyStore() : _cRef(1), _pStream(NULL), _grfMode(0), taglibfile()
	{
		EventRegisterTagLibHandler();
		EventWriteStartup();

		DllAddRef();
		HRESULT hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(_pCache.GetForModify()));
		if (FAILED(hr))
			EventWriteStartupError((std::string("PSCreateMemoryPropertyStore failed: ") + HResultMessage(hr)).c_str());

		PROPVARIANT pv = {};
		pv.vt = VT_EMPTY;
		for (size_t i = 0; i < ARRAYSIZE(keys); ++i)
		{
			_pCache->SetValue(keys[i], pv);
		}

	}

	~CTagLibPropertyStore()
	{
		EventWriteShutdown();
		EventUnregisterTagLibHandler();
	}

	IStream*             _pStream; // data stream passed in to Initialize, and saved to on Commit. We don't manage the lifetime of this object so just use a raw pointer.
	COMWrapper<IPropertyStoreCache> _pCache;
	DWORD                _grfMode; // STGM mode passed to Initialize
	TagLib::FileRef taglibfile;
private:
	DebugListener _listener;
	long _cRef;
};

struct Dbstr
{
	const BSTR val;
	Dbstr(const std::wstring &str) : val(SysAllocString(str.c_str()))
	{
		OutputDebugString(L"bstr()");
	}

	~Dbstr()
	{
		OutputDebugString(L"~bstr");
		SysFreeString(val);
	}

	operator BSTR()
	{
		OutputDebugString(L"op bstr");
		return val;
	}
};

#define TRY_BSTR(func)                                                \
	try                                                               \
	{                                                                 \
		InitPropVariantFromString(func(taglibfile).c_str(), pPropVar);\
	}                                                                 \
	catch (std::domain_error &)                                       \
	{                                                                 \
		pPropVar->vt = VT_EMPTY;                                      \
	}


HRESULT CTagLibPropertyStore::GetValue(REFPROPERTYKEY key, PROPVARIANT *pPropVar)
{
	try
	{
		const TagLib::AudioProperties *ap = taglibfile.audioProperties();
		const TagLib::Tag *tag = taglibfile.tag();

		// If the tag is empty, treat it as if it doesn't exist.
		if (tag && tag->isEmpty())
			tag = NULL;

		if (ap && key == PKEY_Audio_ChannelCount)
		{
			pPropVar->uintVal = ap->channels();
			pPropVar->vt = VT_UI4;
		}
		else if (ap && key == PKEY_Media_Duration)
		{
			pPropVar->ulVal = ap->length() * 10000000;
			pPropVar->vt = VT_UI8;
		}
		else if (ap && key == PKEY_Audio_EncodingBitrate)
		{
			pPropVar->uintVal = ap->bitrate() * 1024;
			pPropVar->vt = VT_UI4;
		}
		else if (ap && key == PKEY_Audio_SampleRate)
		{
			pPropVar->uintVal = ap->sampleRate();
			pPropVar->vt = VT_UI4;
		}
		else if (tag && key == PKEY_Music_AlbumArtist)
			TRY_BSTR(albumArtist)
		else if (tag && key == PKEY_Music_AlbumTitle)
			InitPropVariantFromString(tag->album().toWString().c_str(), pPropVar);
		else if (tag && key == PKEY_Music_Artist)
			InitPropVariantFromString(tag->artist().toWString().c_str(), pPropVar);
		else if (tag && key == PKEY_Music_Genre)
			InitPropVariantFromString(tag->genre().toWString().c_str(), pPropVar);
		else if (tag && key == PKEY_Title)
			InitPropVariantFromString(tag->title().toWString().c_str(), pPropVar);
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
			pPropVar->uintVal = rating(taglibfile);
			pPropVar->vt = VT_UI4;
		}
		else if (tag && key == PKEY_Keywords)
		{
			const wstrvec_t keys = keywords(taglibfile);
			if (!keys.empty())
			{
				pPropVar->vt = VT_ARRAY | VT_BSTR;
				SAFEARRAYBOUND aDim[1] = {};

				aDim[0].cElements = (ULONG)keys.size();

				pPropVar->parray = SafeArrayCreate(VT_BSTR, 1, aDim);
				if (!pPropVar->parray)
					return E_OUTOFMEMORY;

				long aLong[1] = {};
				for (wstrvec_t::const_iterator it = keys.begin(); it != keys.end(); ++it)
				{
					SafeArrayPutElement(pPropVar->parray, aLong, Dbstr(*it));
					++aLong[0];
				}
			}
		}
		else if (tag && key == PKEY_Comment)
			InitPropVariantFromString(tag->comment().toWString().c_str(), pPropVar);
		else if (tag && key == PKEY_Media_DateReleased)
		{
			SYSTEMTIME date = releasedate(taglibfile);
			// Attempt to recover in case of failure.:
			// GetDateFormat, at least in my locale, fails if at least the day, month and year aren't set:
			if (date.wMonth != 0 && date.wDay != 0)
			{
				std::vector<WCHAR> buf;
#define GDF(x)  GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, NULL, x, static_cast<int>(buf.size()))
				buf.resize(GDF(NULL));
				if (GDF(&buf.at(0)))
				{
					pPropVar->bstrVal = SysAllocString(&buf.at(0));
					pPropVar->vt = VT_BSTR;
				}
			}
			else
				if (int year = tag->year())
				{
					std::wstringstream ss; ss << year;
					InitPropVariantFromString(ss.str().c_str(), pPropVar);
				}
		}
		else if (tag && key == PKEY_Music_Composer)
			TRY_BSTR(composer)
		else if (tag && key == PKEY_Music_Conductor)
			TRY_BSTR(conductor)
		else if (tag && key == PKEY_Media_SubTitle)
			TRY_BSTR(subtitle)
		else if (tag && key == PKEY_Media_Publisher)
			TRY_BSTR(label)
		else if (tag && key == PKEY_Media_Producer)
			TRY_BSTR(producer)
		else if (tag && key == PKEY_Music_Mood)
			TRY_BSTR(mood)
		else if (tag && key == PKEY_Copyright)
			TRY_BSTR(copyright)
		else if (tag && key == PKEY_Music_PartOfSet)
			TRY_BSTR(partofset)
		else
		{
			EventWriteReadProperty_Unknown(&key.fmtid, key.pid);
			return S_FALSE;
		}

		switch (pPropVar->vt)
		{
		case VT_LPWSTR:
			EventWriteReadPropertyString(KeyIDs[key], pPropVar->pwszVal);
			break;
		case VT_UI4:
			EventWriteReadPropertyInt(KeyIDs[key], pPropVar->uintVal);
			break;
		case VT_EMPTY:
			EventWriteReadPropertyEmpty(KeyIDs[key]);
			break;
		}
		return S_OK;
	}
	catch (std::exception &e)
	{
		OutputDebugStringA(e.what());
		EventWriteReadProperty_Error(KeyIDs[key], e.what());
#if MESSAGE_BOXES
		MessageBoxA(0, e.what(), "TagLibHandler fatal error", 0);
#endif
		pPropVar->vt = VT_EMPTY;
		return ERROR_INTERNAL_ERROR;
	}
	catch (...)
	{
		// This has only ever been reached when taglib gives us a non-null but invalid tag;
		//  not reproducable. Worse, the exception is only when wstring's constructor has
		//  caught it, there's a chance of a complete crash here, otoh, with a non-abort(),
		//  the app/system may deal gracefully.
		OutputDebugString(L"TaglibHandler encountered unexpected exception in GetValue");
#if MESSAGE_BOXES
		MessageBoxA(0, "Unknown error", "TagLibHandler fatal error", 0);
#endif
		EventWriteReadProperty_Error(KeyIDs[key], "Unknown error.");
		pPropVar->vt = VT_EMPTY;
		return ERROR_INTERNAL_ERROR;
	}
}

inline void Verify(HRESULT hr)
{
	if (hr != S_OK)
		throw std::runtime_error(HResultMessage(hr));
}

#include "Wincodec.h"
#include <limits>

static HRESULT ExtractImage(char* data, size_t dataSize, UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
#if MESSAGE_BOXES
	MessageBoxA(nullptr, "ExtractImage", "TagLibHandler debug", 0);
#endif
	if (data == nullptr)
		throw std::runtime_error("data cannot be null");
	if (dataSize > std::numeric_limits<UINT>().max())
		throw std::overflow_error("dataSize is too large.");

	const COMWrapper<IStream> stream = SHCreateMemStream((BYTE*)data, (UINT)dataSize);

	COMWrapper<IWICImagingFactory> pImgFac;
	Verify(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(pImgFac.GetForModify())));

	COMWrapper<IWICBitmapDecoder> pDecoder;
	// The WIC system can deduce the format of a stream.
	Verify(pImgFac->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, pDecoder.GetForModify()));

	COMWrapper<IWICBitmapSource> albumArt;
	{
		COMWrapper<IWICBitmapFrameDecode> frame;
		Verify(pDecoder->GetFrame(0, frame.GetForModify()));
		Verify(WICConvertBitmapSource(GUID_WICPixelFormat24bppBGR, frame, albumArt.GetForModify()));
	}

	UINT width, height;
	Verify(albumArt->GetSize(&width, &height));

	if (width > cx || height > cx)
	{
		float scale = 1.0f / (std::max(width, height) / (float)cx);
		LONG NewWidth = (LONG)(width *scale);
		LONG NewHeight = (LONG)(height *scale);

		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
		bmi.bmiHeader.biWidth = NewWidth;
		bmi.bmiHeader.biHeight = -NewHeight;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biCompression = BI_RGB;

		BYTE* pBits;
		HBITMAP ResizedHBmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)&pBits, nullptr, 0);
		Verify(ResizedHBmp ? S_OK : E_OUTOFMEMORY);

		try
		{
			COMWrapper<IWICBitmapScaler> pIScaler;
			Verify(pImgFac->CreateBitmapScaler(pIScaler.GetForModify()));
			Verify(pIScaler->Initialize(albumArt, NewWidth, NewHeight, WICBitmapInterpolationModeFant));
			Verify(pIScaler->CopyPixels(nullptr, NewWidth * 3, NewWidth * NewHeight * 3, pBits));
		}
		catch (...)
		{
			DeleteObject(ResizedHBmp);
			throw;
		}
		*phbmp = ResizedHBmp;
	}
	else
	{
		auto pixels = std::vector<BYTE>(width*height * 3);
		Verify(albumArt->CopyPixels(nullptr, width * 3, (UINT)pixels.size(), &pixels.front()));
		*phbmp = CreateBitmap(width, height, 1, 24, &pixels.front());
	}
	*pdwAlpha = WTSAT_RGB;
	return S_OK;
}


#include <ogg/xiphcomment.h>
#include <flac/flacfile.h>
#include <flacpicture.h>

#include <asf/asftag.h>
#include <asf/asfpicture.h>

#include <mpeg/mpegfile.h>
#include <mpeg/id3v2/id3v2tag.h>
#include <mpeg/id3v2/frames/attachedpictureframe.h>

IFACEMETHODIMP CTagLibPropertyStore::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
	EventWriteRequestThumbnail(cx);
	try
	{
		auto ogg = dynamic_cast<TagLib::Ogg::XiphComment*>(taglibfile.tag());
		if (ogg)
			for (auto picture : ogg->pictureList())
				if (picture->data().size())
					return ExtractImage(picture->data().data(), picture->data().size(), cx, phbmp, pdwAlpha);

		auto flac = dynamic_cast<TagLib::FLAC::File*>(&taglibfile);
		if (flac)
			for (auto picture : flac->pictureList())
				if (picture->data().size())
					return ExtractImage(picture->data().data(), picture->data().size(), cx, phbmp, pdwAlpha);

		auto mpeg = dynamic_cast<TagLib::MPEG::File*>(&taglibfile);
		if (mpeg && mpeg->ID3v2Tag()) // ID3v1 doesn't support album art
			for (auto frame : mpeg->ID3v2Tag()->frameListMap()["APIC"])
			{
				auto apic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame);
				if (apic && apic->picture().size())
					return ExtractImage(apic->picture().data(), apic->picture().size(), cx, phbmp, pdwAlpha);
			}

		auto asf = dynamic_cast<TagLib::ASF::Tag*>(taglibfile.tag());
		if (asf)
			for (auto picture : asf->attributeListMap()["WM/Picture"])
				if (picture.toPicture().dataSize())
					return ExtractImage(picture.toPicture().picture().data(), picture.toPicture().dataSize(), cx, phbmp, pdwAlpha);
	}
	catch (const std::exception& err)
	{
		OutputDebugStringA(err.what());
		EventWriteRequestThumbnail_Error(err.what());
#if MESSAGE_BOXES
		MessageBoxA(0, err.what(), "TagLibHandler fatal error", 0);
#endif
		return ERROR_INTERNAL_ERROR;
	}

	EventWriteRequestThumbnail_Error("File format unsupported.");

	return S_FALSE;
}

HRESULT CTagLibPropertyStore::CreateInstance(REFIID riid, void **ppv)
{
	COMWrapper<CTagLibPropertyStore> pNew = new CTagLibPropertyStore();
	return pNew ? pNew->QueryInterface(riid, ppv) : E_OUTOFMEMORY;
}

HRESULT CTagLibPropertyStore_CreateInstance(REFIID riid, void **ppv)
{
	return CTagLibPropertyStore::CreateInstance(riid, ppv);
}

HRESULT CTagLibPropertyStore::GetCount(__out DWORD *pcProps)
{
	*pcProps = ARRAYSIZE(keys);
	return S_OK;
}

HRESULT CTagLibPropertyStore::GetAt(DWORD iProp, __out PROPERTYKEY *pkey)
{
#if MESSAGE_BOXES
	MessageBoxA(nullptr, "GetAt", "TagLibHandler debug", 0);
#endif
	EventWriteGetKey((UCHAR)iProp);
	return _pCache->GetAt(iProp, pkey);
}

// SetValue just updates the internal value cache
HRESULT CTagLibPropertyStore::SetValue(REFPROPERTYKEY, REFPROPVARIANT)
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
HRESULT CTagLibPropertyStore::IsPropertyWritable(REFPROPERTYKEY)
{
	return S_FALSE;
}

struct IStreamAccessor : public TagLib::IOStream
{
	IStreamAccessor(IStream *stream) : stream(stream) {}
	IStream *stream;
	bool isOpen() const
	{
		return true;
	}

	TagLib::ByteVector readBlock(unsigned long length) override
	{
		ULONG read = 0;
		TagLib::ByteVector out((unsigned int)length);
		stream->Read(out.data(), length, &read);
		return out;
	}

	long length() override
	{
		ULARGE_INTEGER prevpos, endpos;
		LARGE_INTEGER dist = {};

		stream->Seek(dist, STREAM_SEEK_CUR, &prevpos); // record current position
		stream->Seek(dist, STREAM_SEEK_END, &endpos);  // record end position
		stream->Seek(dist, STREAM_SEEK_SET, &prevpos); // return to current position
		return (long)endpos.QuadPart;
	}

	void seek(long offset, Position p = Beginning) override
	{
		LARGE_INTEGER dist;
		dist.QuadPart = offset;
		stream->Seek(dist, (DWORD)p, nullptr);
	}

	long tell() const override
	{
		ULARGE_INTEGER newpos;
		LARGE_INTEGER dist = {};
		stream->Seek(dist, STREAM_SEEK_CUR, &newpos);
		return (long)newpos.QuadPart;
	}

	TagLib::FileName name() const override
	{
		STATSTG a;
		if (FAILED(stream->Stat(&a, STATFLAG_DEFAULT)))
			return TagLib::FileName("");
		return TagLib::FileName(a.pwcsName);
	}

	bool readOnly() const override { return true; }

	// Unsupported write operations
	void writeBlock(const TagLib::ByteVector &data) override { }
	void insert(const TagLib::ByteVector &data, unsigned long start = 0, unsigned long replace = 0) {}
	void truncate(long length) override {}
	void removeBlock(unsigned long start = 0, unsigned long length = 0) {}
	void clear() override {}
};

// Initialize populates the internal value cache with data from the specified stream
// S_OK | E_UNEXPECTED | ERROR_READ_FAULT | ERROR_FILE_CORRUPT | ERROR_INTERNAL_ERROR
HRESULT CTagLibPropertyStore::Initialize(IStream *pStream, DWORD grfMode)
{
	if (!taglibfile.isNull())
		HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

	EventWriteInitWithStream();
	_pStream = pStream;
	taglibfile = TagLib::FileRef(new IStreamAccessor(pStream));

	if (taglibfile.isNull())
	{
		EventWriteInitError(DebugListener::GetLastError().c_str());
		return ERROR_FILE_CORRUPT;
	}
	return S_OK;
}

IFACEMETHODIMP CTagLibPropertyStore::Initialize(LPCWSTR pszFilePath, DWORD grfMode)
{
	if (!taglibfile.isNull())
		HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

	EventWriteInitWithFile(pszFilePath);
	taglibfile = TagLib::FileRef(pszFilePath);

	if (taglibfile.isNull())
	{
		EventWriteInitError(DebugListener::GetLastError().c_str());
		return ERROR_FILE_CORRUPT;
	}
	return S_OK;
}
