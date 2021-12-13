#include "DXGICaptor.h"
#include "Utils/Logger.h"


VideoDXGICaptor::VideoDXGICaptor()
{
}

VideoDXGICaptor::~VideoDXGICaptor()
{
}

BOOL VideoDXGICaptor::Init(void* device, void* context, const encoder_config_t* pConfig)
{
	m_pDevice = (ID3D11Device*)device;
	m_pContext = (ID3D11DeviceContext*)context;
	if (!InitDx11Fence()) {
		LOG(INFO) << "InitDx11Fence failed";
		return FALSE;
	}

	m_pEncoder = new CStreamEncoder();
	if (!m_pEncoder->Init(m_pDevice, m_pContext, (encoder_config_t*)pConfig)) {
		return FALSE;
	}
	m_uStreamBufferLen = pConfig->width * pConfig->height * 4;
	m_pStreamBuffer = new BYTE[m_uStreamBufferLen];
	return TRUE;
}

BOOL VideoDXGICaptor::Reset(const encoder_config_t* pConfig) {
	if (m_pEncoder) {
		return m_pEncoder->ResetEncoder(pConfig);
	}
	return FALSE;
}

VOID VideoDXGICaptor::UnInit()
{
	if (m_hFenceHandle) {
		CloseHandle(m_hFenceHandle);
		m_hFenceHandle = nullptr;
	}

	if (m_pEncoder) {
		m_pEncoder->UnInit();
		delete m_pEncoder;
		m_pEncoder = nullptr;
	}
}

bool VideoDXGICaptor::InitDx11Fence()
{
	HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device5), (void**)m_pD3D11Device5.GetAddressOf());
	if (SUCCEEDED(hr)) {
		hr = m_pContext->QueryInterface(__uuidof(ID3D11DeviceContext4), (void**)&m_pContext4);
		if (FAILED(hr)) {
			LOG(ERROR) << "CDXGICaptureToDx QueryInterface ID3D11DeviceContext4 faield 0x" << std::hex << hr;
			return false;
		}

		hr = m_pD3D11Device5->CreateFence(0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
		if (FAILED(hr)) {
			LOG(ERROR) << "CDXGICaptureToDx CreateFence faield 0x" << std::hex << hr;
			return false;
		}

		m_hFenceHandle = CreateEvent(nullptr, FALSE, 0, nullptr);
		if (m_hFenceHandle == nullptr) {
			LOG(ERROR) << "CDXGICaptureToDx CreateFence faield " << GetLastError();
			return false;
		}
		return true;
	}
	return false;
}

void VideoDXGICaptor::WaitCopyComplete()
{
	HRESULT hr;
	m_pContext->Flush();

	if (m_pD3D11Device5) {
		hr = m_pContext4->Signal(m_Fence.Get(), m_nFrameId);
		if (FAILED(hr)) {
			LOG(ERROR) << "m_pContext4->Signal failed hr=0x" << std::hex << hr;
			return;
		}
		hr = m_Fence->SetEventOnCompletion(m_nFrameId, m_hFenceHandle);
		if (FAILED(hr)) {
			LOG(ERROR) << "m_Fence->SetEventOnCompletion failed hr=0x" << std::hex << hr;
			return;
		}
		WaitForSingleObject(m_hFenceHandle, INFINITE);
	}
}
