
#include <array>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "timer.h"
#include "CommonType.h"
#include "Encoder.hpp"
#include "CommonFun.h"

int main(int argc, char** argv) {
    printf_s("start program\n");

    IDXGIAdapter1* pNvidaAdapter = nullptr;
    GetAdapter(&pNvidaAdapter);

    std::array<int, 4> arrDriverType = { D3D_DRIVER_TYPE_HARDWARE,  D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_UNKNOWN };
    ID3D11Device* pD3DDevice = nullptr;
    ID3D11DeviceContext* pContext;
    D3D_FEATURE_LEVEL featureLevel;

    // 创建屏幕设备 device默认为空
    for (auto &index : arrDriverType) {
        HRESULT res = D3D11CreateDevice(pNvidaAdapter,
            static_cast<D3D_DRIVER_TYPE>(index),
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &pD3DDevice, &featureLevel, &pContext
        );

        if (res == S_OK) {
            printf_s("ID3D11Device init success, type: %d\n", index);
            break;
        }
    }

    // 遍历屏幕
    std::vector<IDXGIOutput*> vtOutputs;
    {
        IDXGIDevice *dxgiDevice = nullptr;
        HRESULT res = pD3DDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
        if (res != S_OK) {
            printf_s("Get dxgiDevice faild: %d\n", res);
        }

        IDXGIAdapter* dxgiAdapter = nullptr;
        res = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));
        dxgiDevice->Release();
        dxgiDevice = nullptr;
        if (res != S_OK) {
            printf_s("Get IDXGIAdapter faild: %d\n", res);
        }
        else {
            printf_s("Get IDXGIAdapter success\n");
        }

        UINT index = 0;
        while (true) {
            IDXGIOutput* pOutput = nullptr;
            res = dxgiAdapter->EnumOutputs(index, &pOutput);
            if (res != DXGI_ERROR_NOT_FOUND) {
                DXGI_OUTPUT_DESC OutputDesc;
                if (S_OK != pOutput->GetDesc(&OutputDesc)) {
                    return -1;
                }
                wprintf_s(L"Output Describe: %s\n", OutputDesc.DeviceName);
                vtOutputs.push_back(pOutput);
                index++;
            }
            else {
                printf_s("cant find IDXGIOutput, index:%d\n", index);
                break;
            }
        }

        dxgiAdapter->Release();
        dxgiAdapter = nullptr;
    }

    // 使用当前的屏幕输出对象
    IDXGIOutput* pCurrentOutput = vtOutputs[0];
    DXGI_OUTPUT_DESC sreen_desc;
    pCurrentOutput->GetDesc(&sreen_desc);
    printf_s("#############sreen left:%d top:%d right:%d bottom: %d \n",
        sreen_desc.DesktopCoordinates.left,
        sreen_desc.DesktopCoordinates.top,
        sreen_desc.DesktopCoordinates.right,
        sreen_desc.DesktopCoordinates.bottom);


    IDXGIOutput1* pOutput1;
    HRESULT res = pCurrentOutput->QueryInterface(__uuidof(IDXGIOutput), reinterpret_cast<void**>(&pOutput1));
    if (res != S_OK) {
        printf_s("Get IDXGIOutput1 fail! res: %d\n", res);
        return -1;
    }

    IDXGIOutputDuplication* pCaptureOut;
    res = pOutput1->DuplicateOutput(pD3DDevice, &pCaptureOut);
    if (res != S_OK) {
        printf_s("Get IDXGIOutputDuplication fail: %d\n", res);
    }

    
    // 创建编码器
    Encoder oEncoder;
    encoder_config encode_config;
    encode_config.codec = NV_CODEC_H264;
    encode_config.width = sreen_desc.DesktopCoordinates.right;
    encode_config.height = sreen_desc.DesktopCoordinates.bottom;
    int bitrate = 10 * 1000 * 1000;
    encode_config.framerate = 60;
    encode_config.bitrate = bitrate;
    encode_config.chroma_type = NV_CHROMA_YUV420;
    encode_config.param = "";
    encode_config.param_len = 0;

    //if (!oEncoder.encode_init(&encode_config)) {
    //    printf_s("encode init fail!\n");
    //}
    //else {
    //    printf_s("encode init success\n");
    //}
    
    ID3D11DeviceContext* deferred_context;
    res = pD3DDevice->CreateDeferredContext(0, &deferred_context);
    if (res != S_OK) {
        printf_s("CreateDeferredContext return error: %d", res);
        return -1;
    }

    if (!oEncoder.encode_init_width(pD3DDevice, deferred_context, &encode_config)) {
        printf_s("encode init fail!\n");
    }
    else {
        printf_s("encode init success\n");
    }

    oEncoder.start_encode();

    Timer clock;
    while (true) {
        DXGI_OUTDUPL_FRAME_INFO frame_info;
        IDXGIResource* frame_resource;

        res = pCaptureOut->ReleaseFrame();
        if (res != S_OK) {
            printf_s("IDXGIOutputDuplication Release Frame\n");
        }

        printf("###################pCaptureOut->AcquireNextFrame wait start \n");
        res = pCaptureOut->AcquireNextFrame(17, &frame_info, &frame_resource);
        printf("###################pCaptureOut->AcquireNextFrame wait end \n");
        if (res != S_OK) {
            switch (res) {
            case DXGI_ERROR_ACCESS_LOST:
                printf_s("DXGI_ERROR_ACCESS_LOST\n");
                break;
            case DXGI_ERROR_WAIT_TIMEOUT:
                printf_s("DXGI_ERROR_WAIT_TIMEOUT\n");
                break;
            case DXGI_ERROR_INVALID_CALL:
                printf_s("DXGI_ERROR_INVALID_CALL\n");
                break;
            case E_INVALIDARG:
                printf_s("E_INVALIDARG\n");
                break;
            default:
                printf_s("Get IDXGIOutputDuplication fail: %d\n", res);
            }
        }
        else {
            if (frame_info.LastPresentTime.QuadPart == 0) {
                continue;
            }

            ID3D11Texture2D* texture;
            res = frame_resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
            if (res != S_OK) {
                printf_s("Get ID3D11Texture2D fail: %d\n", res);
            }
            else {
                void* buf = nullptr;
                int buf_len = 0;

                if (oEncoder.copy_mutex(texture)) {
                    printf_s("Get a Frame success, sub cost time: %d\n", clock.elapsed());
                    oEncoder.notify_encode();
                }
                else {
                    printf("encode_send fail!\n");
                }
            }
        }
    }

    return 0;
}