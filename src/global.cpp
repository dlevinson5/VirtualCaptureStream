
#include <stdafx.h>
#include "global.h"
#include "VirtualStreamSink.h"
#include "VirtualStreamSource.h"
#include "VirtualStreamSinkPropertyPage.h"
 
const AMOVIESETUP_MEDIATYPE sudVideoOpPinTypes =
{
    &MEDIATYPE_Video,       // Major type
    &MEDIASUBTYPE_NULL      // Minor type
};
const AMOVIESETUP_MEDIATYPE sudAudioOpPinTypes =
{
    &MEDIATYPE_Audio,       // Major type
    &MEDIASUBTYPE_NULL      // Minor type
};
const AMOVIESETUP_MEDIATYPE sudLine21OpPinTypes =
{
    &MEDIATYPE_AUXLine21Data, // Major type
    &MEDIASUBTYPE_NULL        // Minor type
};

const AMOVIESETUP_PIN psudSinkPinSet[] =
{
	{
		L"Video",      // Obsolete, not used.
		FALSE,          // Is this pin rendered?
		FALSE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		L"Input",           // Obsolete.
		1,              // Number of media types.
		&sudVideoOpPinTypes  // Pointer to media types.
	},
	{
		L"Audio",      // Obsolete, not used.
		FALSE,          // Is this pin rendered?
		FALSE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		L"Input",           // Obsolete.
		1,              // Number of media types.
		&sudAudioOpPinTypes
	},
	{ 
		L"Line21",      // Obsolete, not used.
		FALSE,          // Is this pin rendered?
		FALSE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		L"Input",           // Obsolete.
		1,              // Number of media types.
		&sudLine21OpPinTypes
	}
};

const AMOVIESETUP_PIN psudSourcePinSet[] =
{
	{
		L"Video Output",      // Obsolete, not used.
		FALSE,          // Is this pin rendered?
		TRUE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		L"Input",           // Obsolete.
		1,              // Number of media types.
		&sudVideoOpPinTypes  // Pointer to media types.
	},
	{
		L"Audio Output",      // Obsolete, not used.
		FALSE,          // Is this pin rendered?
		TRUE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		L"Input",           // Obsolete.
		1,              // Number of media types.
		&sudAudioOpPinTypes
	},
	{ 
		L"Line21 Output",      // Obsolete, not used.
		FALSE,          // Is this pin rendered?
		TRUE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		L"Input",           // Obsolete.
		1,              // Number of media types.
		&sudLine21OpPinTypes
	}
};

const AMOVIESETUP_FILTER sudVirtualStreamSink =
{
    &CLSID_VirtualStreamSink,   // CLSID of filter
    L"Virtual Stream Sink",     // Filter's name
    MERIT_DO_NOT_USE,			// Filter merit
    3,							// Number of pins
    psudSinkPinSet				// Pin information
};

const AMOVIESETUP_FILTER sudVirtualStreamSource =
{
    &CLSID_VirtualStreamSource,	// CLSID of filter
    L"Virtual Stream Source",	// Filter's name
    MERIT_DO_NOT_USE,			// Filter merit
    3,							// Number of pins
    psudSourcePinSet			// Pin information
};

CFactoryTemplate g_Templates [] = 
{
    { L"Virtual Stream Sink"
    , &CLSID_VirtualStreamSink
    , CVirtualStreamSink::CreateInstance
    , NULL
    , &sudVirtualStreamSink },
    { L"Virtual Stream Sink Property Page"
    , &CLSID_VirtualStreamSinkPropertyPage
	, CVirtualStreamSinkPropertyPage::CreateInstance },
    { L"Virtual Stream Source"
    , &CLSID_VirtualStreamSource
	, CVirtualStreamSource::CreateInstance
	, NULL
	, &sudVirtualStreamSource}
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

STDAPI RegisterFilters( BOOL bRegister )
{
	HRESULT hr = NOERROR;
	ASSERT(g_hInst != 0);
  
	hr = CoInitialize(0);
	
	if(SUCCEEDED(hr) && bRegister)
		hr = AMovieDllRegisterServer2(TRUE);
   
	if( SUCCEEDED(hr) )
	{
		CComPtr<IFilterMapper2> spFilterMapper;
		hr = spFilterMapper.CoCreateInstance(CLSID_FilterMapper2);
 
		if( SUCCEEDED(hr) )
		{
			if(bRegister)
			{
				IMoniker *pMoniker = 0;
				REGFILTER2 rf2;
				rf2.dwVersion = 1;
				rf2.dwMerit = MERIT_DO_NOT_USE;
				rf2.cPins = 2;
				rf2.rgPins = psudSourcePinSet;
				hr = spFilterMapper->RegisterFilter(CLSID_VirtualStreamSource, L"Virtual Capture Source", &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
				  
				/*
				IMoniker *pMoniker2 = 0;
				rf2.dwVersion = 1;
				rf2.dwMerit = MERIT_DO_NOT_USE;
				rf2.cPins = 2;
				rf2.rgPins = psudSourcePinSet;
				hr = spFilterMapper->RegisterFilter(CLSID_VirtualStreamSource, L"Virtual Capture Source", &pMoniker2, CLSID_AudioInputDeviceCategory, NULL, &rf2);
				*/
			}
			else 
			{
				hr = spFilterMapper->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, CLSID_VirtualStreamSource);
				hr = spFilterMapper->UnregisterFilter(&CLSID_AudioInputDeviceCategory, 0, CLSID_VirtualStreamSource);
			}
		}
	}

	if( SUCCEEDED(hr) && !bRegister )
		hr = AMovieDllRegisterServer2( FALSE );
 
	CoFreeUnusedLibraries();
	CoUninitialize();
	return hr;
}

//
// DllRegisterServer
//
STDAPI DllRegisterServer()
{
	RegisterFilters( FALSE );
	return RegisterFilters( TRUE );
}


//
// DllUnregisterServer
//
STDAPI DllUnregisterServer()
{
	return RegisterFilters( FALSE );
}


//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

CCritSec g_lock;
static TCHAR g_szTraceFile[MAX_PATH] = { NULL };

void FilterTrace(LPCSTR pszFormat, ...)
{
#ifndef _DEBUG
	if (g_szTraceFile[0] == NULL)
		return;
#endif _DEBUG

	CAutoLock lock(&g_lock);

 	va_list ptr;
	va_start(ptr, pszFormat);

	char buffer[1024], buffer2[1024];
	memset(buffer, 0, 1024);
	memset(buffer2, 0, 1024);
	vsprintf_s(buffer, sizeof(buffer), pszFormat, ptr);

	char tmptime[1024];
	char tmpdate[1024];
	_strtime( tmptime );
	_strdate( tmpdate );

	sprintf_s(buffer2, sizeof(buffer), "[%s %s] %s", tmpdate, tmptime, buffer);

#ifdef _DEBUG
	ATLTRACE(buffer2);

	if (g_szTraceFile[0] == NULL)
		return;
#endif _DEBUG
	  
	FILE* pF = NULL;
	_tfopen_s(&pF, g_szTraceFile, L"a");
	
	if (pF != NULL)
	{
		fwrite(buffer2, sizeof(char), strlen(buffer2), pF);
		fclose(pF);
	}
   
	va_end(ptr);
}
 
void SetTraceFile(LPCTSTR szFilePath)
{
	if (szFilePath == NULL)
	{
		g_szTraceFile[0] = NULL;
		return;
	}

	::DeleteFile(szFilePath);
	_tcscpy(g_szTraceFile, szFilePath);

	USES_CONVERSION;
	FilterTrace("Trace [%s]\n", T2A(szFilePath));
}
 