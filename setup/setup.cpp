#include "stdafx.h"
#include "../dllregister.h"

#define RANGE(x) (x).begin(), (x).end()

using boost::assign::map_list_of;

const wchar_t default_guid[] = L"{875CB1A1-0F29-45de-A1AE-CFB4950D0B78}";
const wchar_t *default_assoc[] = { L"mp3", L"wma", L"wav", L"asf" };

CAppModule _Module;
HWND msgbox = 0;
HANDLE transaction;

bool onx64 = false;

const wchar_t *extensions[] = { L"ogg", L"flac", L"oga", L"mp3", L"mpc", L"wv", L"spx", L"tta", L"wma",
	L"asf", L"m4a", L"m4b", L"m4p", L"3g2", L"mp4", L"aif", L"aiff", L"wav" };

struct NoWoW64
{
	NoWoW64()
	{
		Wow64DisableWow64FsRedirection(&orig);
	}
	~NoWoW64()
	{
		Wow64RevertWow64FsRedirection(&orig);
	}
private: 
	void *orig;
};

const std::wstring suffix(L"\\TaglibHandler\\");
// HKLM:
const wchar_t ps_path[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\";
const wchar_t kind_path[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\explorer\\KindMap\\";

// HKCR:
const wchar_t sfa_path[] = L"SystemFileAssociations\\";
typedef const std::map<const wchar_t*, const wchar_t*> wcwcmap;
wcwcmap sfa_defaults = map_list_of
	(L"ExtendedTileInfo", L"prop:System.ItemType;System.Size;System.Music.Artist;System.Media.Duration;System.OfflineAvailability")
	(L"FullDetails", L"prop:System.PropGroup.Description;System.Title;System.Media.SubTitle;System.Rating;"
		L"System.Comment;System.PropGroup.Media;System.Music.Artist;System.Music.AlbumArtist;System.Music.AlbumTitle;"
		L"System.Media.Year;System.Music.TrackNumber;System.Music.Genre;System.Media.Duration;System.PropGroup.Audio;"
		L"System.Audio.EncodingBitrate;System.PropGroup.Origin;System.Media.Publisher;System.Media.EncodedBy;"
		L"System.Media.AuthorUrl;System.Copyright;System.PropGroup.Content;System.ParentalRatingReason;System.Music.Composer;"
		L"System.Music.Conductor;System.Music.ContentGroupDescription;System.Music.Mood;System.Music.PartOfSet;"
		L"System.Music.InitialKey;System.Music.BeatsPerMinute;System.DRM.IsProtected;System.PropGroup.FileSystem;"
		L"System.ItemNameDisplay;System.ItemType;System.ItemFolderPathDisplay;System.DateCreated;System.DateModified;"
		L"System.Size;System.FileAttributes;System.OfflineAvailability;System.OfflineStatus;System.SharedWith;"
		L"System.FileOwner;System.ComputerName")
	(L"InfoTip", L"prop:System.ItemType;System.Size;System.Music.Artist;System.Media.Duration;System.OfflineAvailability")
	(L"PreviewDetails", L"prop:System.Music.Artist;System.Music.AlbumTitle;System.Music.Genre;*System.Media.Duration;"
		L"System.Rating;System.Media.Year;*System.Size;System.Music.TrackNumber;System.Music.AlbumArtist;System.Title;"
		L"*System.Audio.EncodingBitrate;*System.DateModified;System.Keywords;System.ParentalRating;*System.OfflineAvailability;"
		L"*System.OfflineStatus;*System.DateCreated;*System.SharedWith")
	;

std::wstring getProgramFiles(bool x64 = false)
{
	if (x64)
	{
		wchar_t *path = NULL;
		HRESULT res = SHGetKnownFolderPath(FOLDERID_ProgramFilesX64, KF_FLAG_DONT_VERIFY, NULL, &path);
		// This cannot fail like this, but it does, so, attempt to read from the environment variable.
		if (FAILED(res))
		{
			const wchar_t *envv = _wgetenv(L"ProgramW6432");
			if (envv != NULL)
				return envv;

			// Otherwise, give up:
			return L"c:\\Program Files";
		}

		makeGuard(CoTaskMemFree, path);
		return path+suffix;
	}
	else
	{
		wchar_t path[MAX_PATH] = {};
		SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, path);
		return path;
	}
}

std::wstring getDocuments()
{
	wchar_t path[MAX_PATH] = {};
	SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path);
	return path;
}

std::wstring expanded_msg, content_text;

struct win32_error
{
	std::wstring msg;
	win32_error() {}
	win32_error(const std::wstring msg_, HRESULT err = GetLastError())
	{
		msg = msg_;
		wchar_t buf[MAX_PATH] = {};
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, buf, MAX_PATH, NULL);
		msg = msg + L": " + (wchar_t*)buf;
		LocalFree(buf);
	}

	std::wstring what() const
	{
		return msg;
	}
};

#define throw_if_op(x,op) if (op(hr = x)) throw win32_error(BOOST_PP_WSTRINGIZE(x), hr)
#define throw_if(x) throw_if_op(x,)
#define throw_if_fail(x) throw_if_op(x, FAILED)

std::wstring hyperlink(const std::wstring thing, const std::wstring text = std::wstring())
{
	return L"<a href=\"" + thing + L"\">" + (text.empty() ? thing : text)+ L"</a>";
}

typedef std::map<std::wstring, std::wstring> extguid_t;

struct RegCloser
{
	HKEY what;
	RegCloser() : what(NULL) {}
	RegCloser(const HKEY& what) : what(what) {}
	~RegCloser() { if (what) RegCloseKey(what); what = NULL; }
};

std::wstring regreaddefault(HKEY& keh, const std::wstring& what)
{
	DWORD data = MAX_PATH * sizeof(wchar_t);
	wchar_t buf[MAX_PATH] = {};

	HKEY nk;
	HRESULT hr = RegOpenKeyTransactedW(keh, what.c_str(), 0, KEY_QUERY_VALUE, &nk, transaction, NULL);
	if (hr != ERROR_SUCCESS)
		return L"";
	makeGuard(RegCloseKey, nk);

	hr = (HRESULT)RegQueryValueEx(nk, NULL, NULL, NULL, (BYTE*)buf, &data);
	if (hr != ERROR_SUCCESS)
		return L"";

	return buf;
}

extguid_t read_extension_guids(bool isx64 = false)
{
	HRESULT hr;

	HKEY base;
	throw_if(RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, ps_path, 0, KEY_QUERY_VALUE | (isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY), &base, transaction, NULL));
	makeGuard(RegCloseKey, base);

	extguid_t extguid;
	for (size_t i=0; i<ARRAYSIZE(extensions); ++i)
	{
		std::wstring guid = regreaddefault(base, (std::wstring(L".") + extensions[i]));
		if (!guid.empty())
			extguid[extensions[i]] = guid;
	}

	return extguid;
}

std::wstring guidtoname(const std::wstring& guid, bool isx64)
{
	HKEY keh;
	if (guid.empty())
		return L"[nothing]";

	HRESULT hr = RegOpenKeyTransacted(HKEY_CLASSES_ROOT, (L"CLSID\\" + guid).c_str(), 0, KEY_QUERY_VALUE | (isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY), &keh, transaction, NULL);
	if (hr == ERROR_FILE_NOT_FOUND)
		return L"[guid " + guid + L" not registered]";
	else if (hr != ERROR_SUCCESS)
		throw win32_error(L"Couldn't look up name for " + guid, hr);
	makeGuard(RegCloseKey, keh);
	wchar_t data[MAX_PATH];
	DWORD datalen = MAX_PATH;
	throw_if(RegQueryValueEx(keh, NULL, 0, NULL, (LPBYTE)data, &datalen));
	return data;
}


DWORD WINAPI extInstallStuff(void *v);

LONG createKeyTransacted(const HKEY base, const LPCWSTR name, HKEY& out, bool isx64)
{
	return RegCreateKeyTransacted(base, name, 0, NULL, 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | (isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY),
		NULL, &out, NULL, transaction, NULL);
}

LONG createKeyTransacted32(const HKEY base, const LPCWSTR name, HKEY& out)
{
	return createKeyTransacted(base, name, out, false);
}

LONG createKeyTransacted64(const HKEY base, const LPCWSTR name, HKEY& out)
{
	return createKeyTransacted(base, name, out, true);
}

enum MainButtons
{
    Button_DefaultInstall = 101,
    Button_AllExts,
	Button_RestoreDefault,
};

class ProgressDialog : public CTaskDialogImpl<ProgressDialog>
{
public:
	win32_error err;
	HANDLE thread;
	std::wstring installloc_x86, installloc_x64;
	extguid_t extguid_x86, extguid_x64;
	MainButtons button;

	ProgressDialog(const std::wstring& installloc_x86, const std::wstring& installloc_x64,
		const extguid_t& extguid_x86, const extguid_t& extguid_x64, MainButtons button) : 
		thread(INVALID_HANDLE_VALUE), installloc_x86(installloc_x86), 
			installloc_x64(installloc_x64), extguid_x86(extguid_x86), extguid_x64(extguid_x64),
			button(button)
	{
		SetWindowTitle(L"Installing Taglib Handler...");
		SetMainInstructionText(L"Preparing...");

		ModifyFlags(0, TDF_SHOW_PROGRESS_BAR | 
			TDF_CALLBACK_TIMER |
			TDF_ALLOW_DIALOG_CANCELLATION);

		SetCommonButtons(TDCBF_CANCEL_BUTTON);
	}

	void OnDialogConstructed()
	{
		msgbox = m_hWnd;
		SetProgressBarRange(0, 100);
		if (INVALID_HANDLE_VALUE == (thread = CreateThread(NULL, 0, &extInstallStuff, this, 0, NULL)))
			throw win32_error(L"Couldn't set-up installer thread");
		SetProgressBarPos(1);
	}

	bool OnButtonClicked(int /*buttonId*/)
	{
		cleanup();
		return false; // close dialog
	}

	void createDirectory(const std::wstring& dir)
	{
		if (!CreateDirectoryTransacted(NULL, dir.c_str(), NULL, transaction))
		{
			HRESULT hr = GetLastError();
			if (hr != ERROR_ALREADY_EXISTS)
				throw win32_error(L"Couldn't create directory '" + dir + L"'", hr); 
		}
	}

	void copyFile(const std::wstring& from, const std::wstring& to, bool failifexists = false)
	{
		if (!CopyFileTransacted(from.c_str(), to.c_str(), NULL, NULL, NULL,
			(failifexists ? COPY_FILE_FAIL_IF_EXISTS : 0), transaction))
			throw win32_error(L"Couldn't copy '" + from + L"' to '" + to + L"'");
	}

	DWORD doInstallStuff() throw()
	{
		try
		{
			std::wstring dllname(L"taglibhandler.dll");
			//std::wstring pdbname(L"taglibhandler.pdb");
			std::wstring dlltargetx86(installloc_x86 + dllname),
				dlltargetx64(installloc_x64 + dllname);
			std::wstring x64suffix(L"x64\\");

			SetElementText(TDE_MAIN_INSTRUCTION, L"Creating targets..."); Sleep(0);
			createDirectory(installloc_x86);
			SetProgressBarPos(3); Sleep(0);
			if (onx64)
			{
				NoWoW64 nw;
				createDirectory(installloc_x64);
			}
			SetProgressBarPos(5); Sleep(0);

			SetElementText(TDE_MAIN_INSTRUCTION, L"Copying files..."); Sleep(0);
			copyFile(dllname, dlltargetx86); SetProgressBarPos(20); Sleep(0);
			//copyFile(pdbname, installloc_x86 + pdbname); SetProgressBarPos(30); Sleep(0);

			if (onx64)
			{
				NoWoW64 nw;
				copyFile(x64suffix + dllname, dlltargetx64);
				SetProgressBarPos(40); Sleep(0);
				//copyFile(x64suffix + pdbname, installloc_x64 + pdbname);
				SetProgressBarPos(50); Sleep(0);
			}

			SetElementText(TDE_MAIN_INSTRUCTION, L"Registering...");

			HRESULT hr;

			throw_if_fail(doRegistration(const_cast<LPWSTR>(dlltargetx86.c_str()), &createKeyTransacted32));
			SetProgressBarPos(60); Sleep(0);

			if (onx64)
				throw_if_fail(doRegistration(const_cast<LPWSTR>(dlltargetx64.c_str()), &createKeyTransacted64));
			SetProgressBarPos(70); Sleep(0);

			SetElementText(TDE_MAIN_INSTRUCTION, L"Associating...");
			const bool restrict = button == Button_DefaultInstall;
			addToSupportedExtensionsApartFrom(restrict ? extguid_x86 : extguid_t(), false);
			SetProgressBarPos(80); Sleep(0);
			if (onx64)
				addToSupportedExtensionsApartFrom(restrict ? extguid_x64 : extguid_t(), true);
			SetProgressBarPos(90); Sleep(0);

			SetElementText(TDE_MAIN_INSTRUCTION, L"Confirming...");

			int mb = MessageBox(m_hWnd, L"Are you sure you want to proceed with the installation?", L"Install Taglib Handler?", MB_YESNO | MB_ICONQUESTION);

			if (mb == 0)
				throw win32_error(L"Couldn't create MessageBox");
			else if (mb != IDYES)
				return 0;

			SetElementText(TDE_MAIN_INSTRUCTION, L"Finalising...");
			Sleep(0);

			if (!CommitTransaction(transaction))
				throw win32_error(L"Couldn't commit installation");

			SetProgressBarPos(100); Sleep(0);
			SetElementText(TDE_MAIN_INSTRUCTION, L"Done!");
			SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
			MessageBox(m_hWnd, L"Installation successful!", L"Taglib Handler Setup", MB_OK | MB_ICONINFORMATION);
		}
		catch (win32_error& e)
		{
			// Prevent another installation launching while we're mid error-report...
			ModifyFlags(TDF_ALLOW_DIALOG_CANCELLATION, 0);
			EnableWindow(m_hWnd, FALSE);
			err = e;
			return 3;
		}
		return 0;
	}

	void addToSupportedExtensionsApartFrom(extguid_t what, bool isx64)
	{
		HRESULT hr;
		HKEY kind_key;
		throw_if_fail(RegCreateKeyTransacted(HKEY_LOCAL_MACHINE, kind_path, 0, NULL, 
			REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | (isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY),
			NULL, &kind_key, NULL, transaction, NULL));
		makeGuard(RegCloseKey, kind_key);

		for (size_t i=0; i<ARRAYSIZE(extensions); ++i)
		{
			extguid_t::const_iterator it = what.find(extensions[i]);
			if (it != what.end() && !it->second.empty())
				continue;

			HKEY out;
			const std::wstring dotext (std::wstring(L".") + extensions[i]);
			throw_if_fail(RegCreateKeyTransacted(HKEY_LOCAL_MACHINE, (ps_path + dotext).c_str(), 
				0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | (isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY),
				NULL, &out, NULL, transaction, NULL));
			makeGuard(RegCloseKey, out);
			throw_if_fail(RegSetValueEx(out, NULL, 0, REG_SZ, reinterpret_cast<LPBYTE>(SZ_CLSID_US), 
				ARRAYSIZE(SZ_CLSID_US) * sizeof(wchar_t))); 

			throw_if_fail(RegSetValueEx(kind_key, dotext.c_str(), 0, REG_SZ,
				reinterpret_cast<LPBYTE>(L"music"), 6 * sizeof(wchar_t)));

			HKEY sfa_key;
			throw_if_fail(RegCreateKeyTransacted(HKEY_CLASSES_ROOT, (sfa_path + dotext).c_str(), 0, NULL, 
				REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | (isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY),
				NULL, &sfa_key, NULL, transaction, NULL));
			makeGuard(RegCloseKey, kind_key);

			for (wcwcmap::const_iterator it = sfa_defaults.begin(); it != sfa_defaults.end(); ++it)
				throw_if_fail(RegSetValueEx(sfa_key, it->first, 0, REG_SZ,
					reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(it->second)), wcslen(it->second) * sizeof(wchar_t)));


		}
	}

	bool OnTimer(DWORD ms)
	{
		DWORD ev = 0;
		if (thread != INVALID_HANDLE_VALUE && GetExitCodeThread(thread, &ev))
		{
			if (ev == 0)
				PostQuitMessage(0);
			else if (ev != STILL_ACTIVE)
				throw err;
		}
		return false;
	}

	void cleanup()
	{
		SetProgressBarState(PBST_ERROR);
		if (thread != INVALID_HANDLE_VALUE)
		{
			TerminateThread(thread, 1);
			thread = INVALID_HANDLE_VALUE;
		}

		if (transaction != INVALID_HANDLE_VALUE)
		{
			RollbackTransaction(transaction);
			CloseHandle(transaction);
			transaction = INVALID_HANDLE_VALUE;
		}
	}

	~ProgressDialog()
	{
		cleanup();
	}

private:
};

std::wstring hrlist(const std::set<std::wstring>& st)
{
	std::wstringstream wss;
	int i = 0;
	for (std::set<std::wstring>::const_iterator it = st.begin(); it != st.end(); ++it, ++i)
	{
		wss << *it;
		if (st.size() - i == 2)
			wss << L" and ";
		else if (st.size() - i != 1)
			wss << L", ";
	}

	return wss.str();
}

DWORD WINAPI extInstallStuff(void *v)
{
	return static_cast<ProgressDialog *>(v)->doInstallStuff();
}

class InstallDialog : public CTaskDialogImpl<InstallDialog>
{
	std::wstring installloc_x86, installloc_x64;
	extguid_t extguid_x32, extguid_x64;
public:
    InstallDialog()
    {
        SetWindowTitle(L"Taglib Handler Setup");
        SetMainInstructionText(L"Choose an install type");
		SetCommonButtons(TDCBF_CANCEL_BUTTON);
		content_text = L"To work, Taglib Handler needs to be both registered with the system, and registered with the file extensions which it will serve.";
		SetMainIcon(TD_SHIELD_ICON);
		SetContentText(content_text.c_str());
		SetDefaultButton(Button_DefaultInstall);

		installloc_x86 = getProgramFiles();
		if (onx64)
			installloc_x64 = getProgramFiles(true);

		expanded_msg = L"The installation will proceed as follows:\n"
			L"1) The files will be copied to '" + hyperlink(installloc_x86) + hyperlink(installloc_x86 + suffix, suffix) 
			+ (onx64 ? L"' (for the 32-bit binaries, and to '"
			+ hyperlink(installloc_x64) + hyperlink(installloc_x64 + suffix, suffix) + L"' for the 64-bit binaries)" : L"") +
			L".\n2) The DLL" + (onx64 ? L"s" : L"") + L" will be registered with the system.\n"
			L"3) Optionally, the file associations will be made:\n" +
			L"  • ";
		
		for (size_t i=0; i<ARRAYSIZE(extensions); ++i)
			expanded_msg = expanded_msg + extensions[i] +
				(i == ARRAYSIZE(extensions) - 2 ? L" and " : 
				(i != ARRAYSIZE(extensions) - 1 ? L", " : L" are supported.\n"));

		extguid_x32 = read_extension_guids(false);
		if (onx64)
		{
			extguid_x64 = read_extension_guids(true);

#if 0
			typedef std::map<std::wstring, std::pair<std::wstring, std::wstring> > guidext_t;

			guidext_t nonmatching;

			for (size_t i=0; i<ARRAYSIZE(extensions); ++i)
				if (extguid_x64[extensions[i]] != extguid_x32[extensions[i]])
					nonmatching[extensions[i]] = std::make_pair(extguid_x32[extensions[i]], extguid_x64[extensions[i]]);

			expanded_msg += L"  • There are " + boost::lexical_cast<std::wstring>(nonmatching.size()) + 
				L" extensions with differing x32 and x64 handlers" +
				(nonmatching.empty() ? L"." : L":") +
				L"\n";

			for (guidext_t::const_iterator it = nonmatching.begin(); it != nonmatching.end(); ++it)
				expanded_msg += L"    ◦ " + it->first + L" is associated with " + guidtoname(it->second.first, false) +
					L" (32) and " + guidtoname(it->second.second, true) + L" (64)\n";
#endif
		}

		struct NameCache
		{
			bool isx64;
			NameCache(bool isx64) : isx64(isx64) {}

			typedef std::map<std::wstring, std::wstring> guidname_t;
			
			std::wstring name(const std::wstring& guid) const
			{
				guidname_t::const_iterator it;
				if ((it = names.find(guid)) != names.end())
					return it->second;
				return names[guid] = guidtoname(guid, isx64);
			}
		private:
			mutable guidname_t names;
		} namecache32(false), namecache64(true);
		
		typedef std::map<std::pair<std::wstring, std::wstring>, std::set<std::wstring> > namesext_t;
		namesext_t nem;

		for (size_t i=0; i<ARRAYSIZE(extensions); ++i)
			nem[std::make_pair(
				namecache32.name(extguid_x32[extensions[i]]),(onx64 ? 
				namecache64.name(extguid_x64[extensions[i]]) : L""))].insert(extensions[i]);

		for (namesext_t::const_iterator it = nem.begin(); it != nem.end(); ++it)
			expanded_msg += L"    ◦ " + hrlist(it->second) + L" currently use" + (it->second.size() == 1 ? L"s" : L"") + L" " + 
			it->first.first + (onx64 ? (it->first.first == it->first.second ? 
			L" (both 32 and 64)" : L" (32) and " + it->first.second + L" (64)") : L"") + L".\n";

		const std::wstring def32 = namecache32.name(default_guid), def64 = namecache64.name(default_guid);

		expanded_msg = expanded_msg + L"  • By default, only ";

		for (size_t i=0; i<ARRAYSIZE(default_assoc); ++i)
			expanded_msg = expanded_msg + default_assoc[i] +
				(i == ARRAYSIZE(default_assoc) - 2 ? L" and " : 
				(i != ARRAYSIZE(default_assoc) - 1 ? L", " : L"")); 

		expanded_msg = expanded_msg + 
			L" have handlers, th" + (onx64 ? L"ese are" : L"is is") + L" called '" + 
			def32 + L"'" + (onx64 ? (def32 == def64 ? L" (both 32 and 64)" : L" and '" + def64 + 
			L"' (64)") : L"") + L".";

		SetExpandedInformationText(expanded_msg.c_str());

        static TASKDIALOG_BUTTON buttons[] =
        {
            { Button_DefaultInstall, L"Default Install (recommended)\nRegister with the system and for supported extensions not already claimed" },
            { Button_AllExts, L"Install For All Extensions\nRegister with the system and for all supported extensions" },
			{ Button_RestoreDefault, L"Uninstall\nRestore the system to the Windows default settings" },
        };

        SetButtons(buttons,
                   _countof(buttons));

        ModifyFlags(0, TDF_ALLOW_DIALOG_CANCELLATION |
                       TDF_USE_COMMAND_LINKS |
                       TDF_EXPAND_FOOTER_AREA |
					   TDF_ENABLE_HYPERLINKS);
    }

	void OnDialogConstructed()
	{
		msgbox = m_hWnd;
	}

    void OnHyperlinkClicked(PCWSTR url)
    {
		if (!url)
			return;

        int result = (int)ShellExecute(m_hWnd, NULL, url, 0, 0, SW_SHOWNORMAL);
		if (result > 32)
			return;

		throw win32_error(std::wstring(L"Could not open URL '") + url + L"'", result);
    }

	void deletes(bool isx64)
	{
		HRESULT hr;
		for (size_t i=0; i<ARRAYSIZE(extensions); ++i)
		{
			hr = RegDeleteKeyTransacted(HKEY_LOCAL_MACHINE, (ps_path + std::wstring(L".") + extensions[i]).c_str(),
				(isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY), 0, transaction, NULL);
			if (hr != ERROR_SUCCESS && hr != ERROR_FILE_NOT_FOUND)
				throw win32_error(L"Couldn't delete entry", hr);
		}
	}

	void createDefaults(bool isx64)
	{
		for (size_t i=0; i<ARRAYSIZE(default_assoc); ++i)
		{
			HKEY keh;

			HRESULT hr = RegCreateKeyTransacted(HKEY_LOCAL_MACHINE, (ps_path + std::wstring(L".") + default_assoc[i]).c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
				KEY_ALL_ACCESS | (isx64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY), NULL, &keh, NULL, transaction, NULL); 
			if (hr != ERROR_SUCCESS)
				throw win32_error(L"Couldn't create key", hr);
			makeGuard(RegCloseKey, keh);
			throw_if_fail(RegSetValueExW(keh, NULL, 0, REG_SZ, (BYTE*)(&default_guid[0]), ARRAYSIZE(default_guid) * sizeof(wchar_t)));
		}

	}

    bool OnButtonClicked(int buttonId)
    {
        bool closeDialog = false;

        switch (buttonId)
        {
			case Button_RestoreDefault:
				{
					deletes(false);
					createDefaults(false);
					if (onx64)
					{
						deletes(true);
						createDefaults(true);
					}
					int mb = MessageBox(m_hWnd, L"Are you sure you want to return the system to it's original state? This will lose any changes ever made to the relevant property handlers.", 
						L"Really uninstall Taglib Handler?", MB_YESNO | MB_ICONWARNING);

					if (mb == IDYES)
					{
						if (!CommitTransaction(transaction))
							throw win32_error(L"Couldn't commit uninstall");
						SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
					}
					closeDialog = true;
				}
			break;
			case Button_DefaultInstall:
            case Button_AllExts:
			{
				ProgressDialog pd(installloc_x86 + suffix, installloc_x64 + suffix,
					extguid_x32, extguid_x64, static_cast<MainButtons>(buttonId));
				pd.DoModal();
			}
			// No break;

            case IDCANCEL:
            {
                closeDialog = true;
                break;
            }
        }

        return !closeDialog;
    }
};

int __stdcall wWinMain(HINSTANCE instance,
                       HINSTANCE,
                       LPWSTR /*cmdLine*/,
                       int /*cmdShow*/)
{
	{
		BOOL isx64;
		IsWow64Process(GetCurrentProcess(), &isx64);
		onx64 = isx64 == TRUE;
	}

	try
	{
		transaction = CreateTransaction(NULL, NULL, NULL, NULL, NULL, NULL, L"Taglib Handler Setup");
		if (transaction == INVALID_HANDLE_VALUE)
			throw win32_error(L"Couldn't create a transaction");
		makeGuard(CloseHandle, transaction);

		HRESULT hr;
		if (FAILED(hr = _Module.Init(0, instance)))
			throw std::exception("Initialisation failed", hr);
		makeGuard(&CAppModule::Term, _Module);

		InstallDialog dialog;
		return dialog.DoModal();
	}
	catch (std::exception& e)
	{
		MessageBoxA(msgbox, e.what(), "Fatal error, exiting", MB_ICONERROR | MB_OK);
		return -1;
	}
	catch (win32_error& e)
	{
		MessageBox(msgbox, e.what().c_str(), L"Unexpected Win32 error, exiting", MB_ICONERROR | MB_OK);
		return -2;
	}
}
