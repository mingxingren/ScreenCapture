#ifndef __DXGI_SCREEN_CAPTURE_H__
#define __DXGI_SCREEN_CAPTURE_H__

#include "StreamEncoder.h"
#include <dxgi1_2.h>
#include <d3d11_4.h>

class VideoDXGICaptor
{
public:
	VideoDXGICaptor();
	~VideoDXGICaptor();
	BOOL			Init(void* device, void* context, const encoder_config_t* pConfig);
	BOOL			Reset(const encoder_config_t* pConfig);
	VOID			UnInit();

	void WaitCopyComplete();
private:
	bool InitDx11Fence();

	ID3D11Device*		   m_pDevice;
	ID3D11DeviceContext*    m_pContext;
	Microsoft::WRL::ComPtr<ID3D11Device5>			m_pD3D11Device5;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext4>	m_pContext4;
	Microsoft::WRL::ComPtr<ID3D11Fence>				m_Fence;
	HANDLE					m_hFenceHandle = nullptr;

public:
	UINT					m_nFrameId = 0;/* 记录帧数 */
	IStreamEncoder*			m_pEncoder = nullptr;
	PBYTE					m_pStreamBuffer = nullptr;
	UINT					m_uStreamBufferLen = 0;
};

#endif