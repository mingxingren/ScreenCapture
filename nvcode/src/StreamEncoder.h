#pragma once
#include <Windows.h>
#include <wrl.h>
#include <d3d11.h>
#include "NvEncoder.h"
#include "display_captor.h"


class IStreamEncoder 
{
public:
	virtual bool Init(ID3D11Device* pD3D11Device, ID3D11DeviceContext* pD3D11DeviceContext, const encoder_config_t* pConfig) = 0;
	virtual bool OnSurfaceUpdate(void* pDxFrame) = 0;
	virtual bool ResetEncoder(const encoder_config_t* pConfig) = 0;
	virtual int  Encode(PBYTE pBuffer, UINT uBufferLen, bool bForceIDR=false) = 0;
	virtual void UnInit() = 0;
};

class CStreamEncoder : public IStreamEncoder
{
public:
	virtual bool Init(ID3D11Device* pD3D11Device, ID3D11DeviceContext* pD3D11DeviceContext, const encoder_config_t* pConfig) override;
	virtual bool OnSurfaceUpdate(void* pDxFrame) override;
	virtual bool ResetEncoder(const encoder_config_t* pConfig) override;
	virtual int  Encode(PBYTE pBuffer, UINT uBufferLen, bool bForceIDR=false) override;
	virtual void UnInit() override;

private:
	Microsoft::WRL::ComPtr<ID3D11Device>        m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pContext;
	NvEncoder*									m_pNvEncoder = nullptr;
	std::mutex									m_encodeMutex;
};