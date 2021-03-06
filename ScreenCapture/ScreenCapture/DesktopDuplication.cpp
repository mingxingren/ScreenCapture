
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
    ID3D11DeviceContext* pImmContext;
    ID3D11Multithread* pMultithread;
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
            &pD3DDevice, &featureLevel, &pImmContext
        );

        if (res == S_OK) {
            printf_s("ID3D11Device init success, type: %d\n", index);
            pImmContext->QueryInterface(IID_PPV_ARGS(&pMultithread));
            pMultithread->SetMultithreadProtected(true);
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
    encode_config.param = "-preset p1";
    encode_config.param_len = 0;

    if (!oEncoder.encode_init_width(pD3DDevice, pImmContext, &encode_config)) {
        printf_s("encode init fail!\n");
    }
    else {
        printf_s("encode init success\n");
    }

    // multithread code
    oEncoder.start_encode();

    ID3D11Texture2D* copy_texture = nullptr;

    Timer clock;
    int64_t frame_num = 0;
    while (true) {
        DXGI_OUTDUPL_FRAME_INFO frame_info;
        IDXGIResource* frame_resource = nullptr;

        res = pCaptureOut->ReleaseFrame();
        if (res != S_OK) {
            printf_s("IDXGIOutputDuplication Release Frame\n");
        }

        res = pCaptureOut->AcquireNextFrame(8, &frame_info, &frame_resource);
        if (res != S_OK) {
            switch (res) {
            case DXGI_ERROR_ACCESS_LOST:
                printf_s("DXGI_ERROR_ACCESS_LOST\n");
                break;
            case DXGI_ERROR_WAIT_TIMEOUT:
                printf_s("DXGI_ERROR_WAIT_TIMEOUT\n");
                //std::this_thread::sleep_for(std::chrono::milliseconds(2));
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

            int64_t start_time = clock.elapsed();
            ID3D11Texture2D* texture = nullptr;
            res = frame_resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
            if (res != S_OK) {
                printf_s("Get ID3D11Texture2D fail: %d\n", res);
            }
            else {
                //if (copy_texture == nullptr) {
                //    D3D11_TEXTURE2D_DESC texture_desc, org_texture_desc;
                //    texture->GetDesc(&org_texture_desc);

                //    ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE2D_DESC));
                //    texture_desc.Width = org_texture_desc.Width;
                //    texture_desc.Height = org_texture_desc.Height;
                //    texture_desc.MipLevels = 1;
                //    texture_desc.ArraySize = 1;
                //    texture_desc.Format = org_texture_desc.Format;
                //    texture_desc.SampleDesc.Count = 1;
                //    texture_desc.Usage = D3D11_USAGE_DEFAULT;
                //    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
                //    texture_desc.CPUAccessFlags = 0;

                //    printf_s("#######################texture_desc.width: %d  height: %d  format:%d \n",
                //        texture_desc.Width, texture_desc.Height, texture_desc.Format);
                //    res = pD3DDevice->CreateTexture2D(&texture_desc, NULL, &copy_texture);
                //    if (res != S_OK) {
                //        printf_s("Create a texture ID3D11Texture2D fail: 0x%x\n", res);
                //    }
                //}

                //printf_s("#################Texture address:0x%p  copy texture address:0x%p\n", texture, copy_texture);
                //int64_t copy_start_time = clock.elapsed();

                //ID3D11Resource* texture_resource = nullptr;
                //texture->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<void**>(&texture_resource));
                //
                //ID3D11Resource* copy_resource = nullptr;
                //res = copy_texture->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<void**>(&copy_resource));

                //if (0 != ::dc_copy(oEncoder.get_ptr(), copy_texture, texture_resource)) {
                //    // multithread code
                //    oEncoder.notify_encode(copy_texture);
                //}

                std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp
                    = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
                std::time_t timestamp = tp.time_since_epoch().count();
                frame_num += 1;
                printf_s("#######capture frame num: %d,  timestamp: %d\n", frame_num, timestamp);

                int64_t encode_start_time = clock.elapsed();
                oEncoder.notify_encode(texture);
                int64_t encode_end_time = clock.elapsed();
                printf_s("#######call oEncoder.notify_encode time: %d\n", encode_end_time - encode_start_time);
            }
            frame_resource->Release();
            int64_t end_time = clock.elapsed();
            int64_t diff_time = end_time - start_time;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return 0;
}