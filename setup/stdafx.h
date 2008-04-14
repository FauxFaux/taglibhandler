#pragma once

#include <stdexcept>
#include <string>
#include <map>
#include <set>

// Use explicit constructors for CString
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

// Use non-restrictive notify handlers
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS

// Don't bother with supporting older versions of Windows
// #define _WTL_TASKDIALOG_DIRECT

#define NTDDI_VERSION NTDDI_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 1

#include <windows.h>
#include <ktmw32.h>

// From ATL
#include <atlbase.h>
#include <atlcoll.h>
#include <atlstr.h>

// From WTL
#include <atlapp.h>
#include <atldlgs.h>

#include <boost/preprocessor.hpp>
#include <boost/preprocessor/wstringize.hpp>
#include <boost/lexical_cast.hpp>

#include <loki/scopeguard.h>
#define makeGuard(...) Loki::ScopeGuard BOOST_PP_CAT(guard,__LINE__) = Loki::MakeGuard(__VA_ARGS__); BOOST_PP_CAT(guard,__LINE__);

extern CAppModule _Module;

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
