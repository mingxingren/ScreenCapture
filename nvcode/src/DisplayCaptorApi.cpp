#include "DXGICaptor.h"
#include "Utils/Logger.h"


DISPLAY_CAPTOR_API dc_ptr dc_encoder_init(void* device, void* context, const encoder_config_t* config) {
	LOG(INFO) << "dc_encoder_init device=0x" << std::hex << device << ", context=0x" << std::hex << context << " config=0x" << std::hex << config;;
	if (device == NULL || context == NULL || config == NULL) {
		return NULL;
	}

	VideoDXGICaptor* pScreen = new VideoDXGICaptor();
	if (pScreen->Init(device, context, config)) {
		LOG(ERROR) << "dc_encoder_init suc inst=0x" << std::hex << pScreen;
		return (dc_ptr)pScreen;
	}
	pScreen->UnInit();
	delete pScreen;
	return NULL;
}

DISPLAY_CAPTOR_API int dc_encoder_reset(dc_ptr inst, const encoder_config_t* config) {
	LOG(INFO) << "dc_encoder_reset inst=0x" << std::hex << inst;
	VideoDXGICaptor* pScreen = (VideoDXGICaptor*)inst;
	if (pScreen) {
		return pScreen->Reset(config);
	}
	LOG(ERROR) << "dc_encoder_reset inst has free";
	return FALSE;
}

DISPLAY_CAPTOR_API void dc_encoder_free(dc_ptr inst) {
	LOG(INFO) << "dc_encoder_free inst=0x" << std::hex << inst;
	VideoDXGICaptor* pScreen = (VideoDXGICaptor*)inst;
	if (pScreen) {
		pScreen->UnInit();
		delete pScreen;
	}
	else {
		LOG(ERROR) << "dc_encoder_free inst has free";
	}
}

DISPLAY_CAPTOR_API int dc_encode_send(dc_ptr inst, void* dx_frame) {
	VideoDXGICaptor* pScreen = (VideoDXGICaptor*)inst;
	if (pScreen && pScreen->m_pEncoder) {
		/* GPU copy to GPU */
		//auto t0 = std::chrono::system_clock::now();
		pScreen->m_pEncoder->OnSurfaceUpdate(dx_frame);
		//pScreen->WaitCopyComplete();
		//pScreen->m_nFrameId++;
		//auto t1 = std::chrono::system_clock::now();
		LOG(INFO) << "dc_encode_send success";
		return TRUE;
	}
	return FALSE;
}

DISPLAY_CAPTOR_API int dc_encode_recieve(dc_ptr inst, void** buf, int* buf_len) {
	if (buf == NULL || buf_len == NULL) {
		LOG(ERROR) << "dc_encode_recieve param invalid buf=0x" << std::hex << buf << ", buf_len=0x" << std::hex << buf_len;
		return FALSE;
	}
	VideoDXGICaptor* pScreen = (VideoDXGICaptor*)inst;
	if (pScreen && pScreen->m_pEncoder) {
		int nDataLen = pScreen->m_pEncoder->Encode(pScreen->m_pStreamBuffer, pScreen->m_uStreamBufferLen);
		if (nDataLen < 0) {
			return E_FAIL;
		}
		*buf = pScreen->m_pStreamBuffer;
		*buf_len = nDataLen;

		//auto t2 = std::chrono::system_clock::now();
		//auto copy_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
		//auto encode_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
		//LOG(INFO) << "gpu copy:" << copy_ms.count() << " ms, encode: " << encode_ms.count() << " ms";
		return TRUE;
	}
	return FALSE;
}

DISPLAY_CAPTOR_API int dc_copy(dc_ptr inst, void* dst_resource, void* src_resource) {
	VideoDXGICaptor* pScreen = (VideoDXGICaptor*)inst;
	if (pScreen && pScreen->m_pEncoder) {
		/* GPU copy to GPU */
		//auto t0 = std::chrono::system_clock::now();
		pScreen->Context()->CopyResource((ID3D11Resource*)dst_resource, (ID3D11Resource*)src_resource);
		pScreen->WaitCopyComplete();
		//pScreen->m_nFrameId++;
		//auto t1 = std::chrono::system_clock::now();
		return TRUE;
	}
	return FALSE;
}
