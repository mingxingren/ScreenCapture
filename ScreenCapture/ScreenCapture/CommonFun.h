#pragma once

#include <stdio.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <string>

#define NVIDA_NAME L"NVIDIA GeForce"
void GetAdapter(IDXGIAdapter1** pAdapter) {
    IDXGIFactory1* pFactory = NULL;

    HRESULT res = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);     // 申请资源
    if (FAILED(res)) {
        printf_s("Create DXGIFactory fail: %d", res);
        return;
    }

    IDXGIAdapter1* pIndexAdapter;
    for (UINT i = 0; pFactory->EnumAdapters1(i, &pIndexAdapter) != DXGI_ERROR_NOT_FOUND; i++) {

        DXGI_ADAPTER_DESC oAdapterDesc;
        if (S_OK == pIndexAdapter->GetDesc(&oAdapterDesc)) {
            wprintf_s(L"Adapter Describe: %s\n", oAdapterDesc.Description);
            std::wstring description = oAdapterDesc.Description;
            if (std::string::npos != description.find(NVIDA_NAME)) {
                wprintf_s(L"find %s adaptor success\n", NVIDA_NAME);
                *pAdapter = pIndexAdapter;
            }
        }

    }

    if (pFactory) {
        pFactory->Release();    // 释放资源
    }
}
