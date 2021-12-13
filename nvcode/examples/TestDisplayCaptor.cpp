
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <wrl.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include "display_captor.h"
#include "../src/Utils/Logger.h"

#pragma comment(lib, "DisplayCaptor.lib")
//#pragma comment(lib, "nvencodeapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "msvcrtd.lib")
//#pragma comment(lib, "dxgi.lib")
//#pragma comment(lib, "SetupAPI.lib")
//#pragma comment(lib, "Wtsapi32.lib")

Microsoft::WRL::ComPtr<ID3D11Device>		   m_pDevice;
Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_pContext;
Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_pDeskDupl;

UINT                    m_OutputNumber = 0;
DXGI_OUTPUT_DESC        m_dxgiOutDesc;

dc_ptr g_dc_pgr;
void init() {

	HRESULT hr = S_OK;
	// Driver types supported
	D3D_DRIVER_TYPE DriverTypes[] =
	{
		//D3D_DRIVER_TYPE_UNKNOWN,
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

	// Feature levels supported
	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_1
	};
	UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

	D3D_FEATURE_LEVEL FeatureLevel;
	for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex) {

		hr = D3D11CreateDevice(nullptr,
			DriverTypes[DriverTypeIndex],
			nullptr,
			0,
			FeatureLevels,
			NumFeatureLevels,
			D3D11_SDK_VERSION,
			m_pDevice.GetAddressOf(),
			&FeatureLevel,
			m_pContext.GetAddressOf());

		if (SUCCEEDED(hr)) {
			break;
		}
	}

	encoder_config_t config;
	config.codec = NV_CODEC_H264;
	config.width = 1920;
	config.height = 1080;
	config.framerate = 30 * 1000 * 1000;
	config.bitrate = 20;
	//config.idr_period = config.framerate;
	config.chroma_type = NV_CHROMA_YUV420;
	g_dc_pgr = dc_encoder_init(m_pDevice.Get(), m_pContext.Get(), &config);
}

HRESULT init_desk() {
	HRESULT hr = S_OK;

	Microsoft::WRL::ComPtr <IDXGIDevice> pDxgiDevice;
	hr = m_pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(pDxgiDevice.GetAddressOf()));
	if (FAILED(hr)) {
		LOG(ERROR) << "QueryInterface IDXGIDevice error hr=0x" << std::hex << hr;
		return hr;
	}

	//
	// Get DXGI adapter
	//
	Microsoft::WRL::ComPtr<IDXGIAdapter> pDxgiAdapter = nullptr;
	hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(pDxgiAdapter.GetAddressOf()));
	if (FAILED(hr)) {
		LOG(ERROR) << "hDxgiDevice->GetParent error hr=0x" << std::hex << hr;
		return hr;
	}

	//
	// Get output
	//
	Microsoft::WRL::ComPtr <IDXGIOutput> pDxgiOutput;
	hr = pDxgiAdapter->EnumOutputs(m_OutputNumber, pDxgiOutput.GetAddressOf());/* Get the first output data */
	if (FAILED(hr)) {
		LOG(ERROR) << "hDxgiAdapter->EnumOutputs error hr=0x" << std::hex << hr;
		return hr;
	}

	//
	// get output description struct
	//
	hr = pDxgiOutput->GetDesc(&m_dxgiOutDesc);
	if (FAILED(hr)) {
		LOG(ERROR) << "hDxgiOutput->GetDesc error hr=0x" << std::hex << hr;
		return hr;
	}

	//
	// QI for Output 1
	//
	Microsoft::WRL::ComPtr <IDXGIOutput1> pDxgiOutput1 = nullptr;
	hr = pDxgiOutput->QueryInterface(__uuidof(pDxgiOutput1), reinterpret_cast<void**>(pDxgiOutput1.GetAddressOf()));
	if (FAILED(hr)) {
		LOG(ERROR) << "hDxgiOutput->QueryInterface error hr=0x" << std::hex << hr;
		return hr;
	}

	//
	// Create desktop duplication
	//
	hr = pDxgiOutput1->DuplicateOutput(m_pDevice.Get(), m_pDeskDupl.GetAddressOf());
	if (FAILED(hr)) {
		LOG(ERROR) << "hDxgiOutput1->DuplicateOutput error hr=0x" << std::hex << hr;
		return hr;
	}

	return hr;
}

HRESULT deal() {
	Microsoft::WRL::ComPtr<IDXGIResource> pDesktopResource;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pAcquiredDesktopImage;
	char* buf;
	int buf_len;

	DXGI_OUTDUPL_FRAME_INFO frameData;
	memset(&frameData, 0, sizeof(DXGI_OUTDUPL_FRAME_INFO));
	std::cout << "#########################running capture 3" << std::endl;
	HRESULT hr = m_pDeskDupl->AcquireNextFrame(500, &frameData, pDesktopResource.GetAddressOf());
	if (hr == DXGI_ERROR_ACCESS_LOST
		|| hr == DXGI_ERROR_INVALID_CALL) {/* 在winlogon session和用户session切换会出现DXGI_ERROR_INVALID_CALL错误 */
		LOG(ERROR) << "AcquireNextFrame DXGI_ERROR_ACCESS_LOST hr=0x" << std::hex << hr;
		std::cout << "#########################running capture 4" << std::endl;
		return DXGI_ERROR_ACCESS_LOST;
	}

	if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
		LOG(ERROR) << "AcquireNextFrame DXGI_ERROR_WAIT_TIMEOUT hr=0x" << std::hex << hr;
		std::cout << "#########################running capture 5" << std::endl;
		return S_OK;
	}
	else if (FAILED(hr)) {
		std::cout << "#########################running capture 6" << std::endl;
		LOG(ERROR) << "AcquireNextFrame error hr=0x" << std::hex << hr;
		return E_FAIL;
	}

	hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pAcquiredDesktopImage.GetAddressOf()));
	if (FAILED(hr)) {
		LOG(ERROR) << "QueryInterface ID3D11Texture2D error hr=0x" << std::hex << hr;
		return E_FAIL;
	}
	std::cout << "#########################running capture" << std::endl;

	if (dc_encode_send(g_dc_pgr, pAcquiredDesktopImage.Get())) {
		if (dc_encode_recieve(g_dc_pgr, (void**)&buf, &buf_len)) {
			static int i = 0;
				char file_name[256];
				sprintf(file_name, "h264_%d.dat", i);
				FILE* fp = fopen(file_name, "wb");
				if (fp) {
					fwrite(buf, 1, buf_len, fp);
						fclose(fp);
				}
				else {
					LOG(ERROR) << "fopen fail: " << file_name;
				}
		}
	}
	std::cout << "#########################running capture 2" << std::endl;
	return S_OK;
}

int main(int argc, char* args[])
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR     lpCmdLine, int       nCmdShow)
{
	init();
	HRESULT hr = init_desk();
	if (FAILED(hr)) {
		return hr;
	}
	while (1) {
		deal();
	}
}