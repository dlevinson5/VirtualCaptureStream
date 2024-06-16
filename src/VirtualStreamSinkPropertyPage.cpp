#include "StdAfx.h"
#include "VirtualStreamSinkPropertyPage.h"
#include "resource.h"
#include "VirtualStreamSink.h"

CVirtualStreamSinkPropertyPage::CVirtualStreamSinkPropertyPage(LPUNKNOWN lpUnk, HRESULT *phr) : CBasePropertyPage(NAME("Stream Buffer Sink Property Page\0"),lpUnk, IDD_SBSPROPS, IDS_TITLE)
{
}

CVirtualStreamSinkPropertyPage::~CVirtualStreamSinkPropertyPage(void)
{
}

CUnknown* WINAPI CVirtualStreamSinkPropertyPage::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	ATLASSERT(phr);
	return new CVirtualStreamSinkPropertyPage(pUnk, phr);
}

HRESULT CVirtualStreamSinkPropertyPage::OnConnect(IUnknown *pUnknown) 
{ 
	m_spStreamBuffer = pUnknown;
	return NOERROR;
}

HRESULT CVirtualStreamSinkPropertyPage::OnActivate() 
{ 
	return NOERROR; 
}