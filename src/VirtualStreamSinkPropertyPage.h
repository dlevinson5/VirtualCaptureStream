#pragma once

#include "stdafx.h"
#include "VirtualStreamSink.h"

// {2F92F774-6088-464C-B23D-7FF3E6490DD8}
DEFINE_GUID(CLSID_VirtualStreamSinkPropertyPage, 0x2f92f774, 0x6088, 0x464c, 0xb2, 0x3d, 0x7f, 0xf3, 0xe6, 0x49, 0xd, 0xd8);

class CVirtualStreamSinkPropertyPage : public CBasePropertyPage
{
	private:
		CComQIPtr<IVirtualStreamSink> m_spStreamBuffer;

	public:
		CVirtualStreamSinkPropertyPage(LPUNKNOWN lpUnk, HRESULT *phr);
		~CVirtualStreamSinkPropertyPage(void);

		static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);
		virtual HRESULT OnConnect(IUnknown *pUnknown);
		virtual HRESULT OnActivate();
};
