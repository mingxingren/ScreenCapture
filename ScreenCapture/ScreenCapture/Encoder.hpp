#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <sstream>
#include <string>
#include "time.h"
#include "CommonFun.h"
#include "CommonType.h"
#include "display_captor.h"
class Encoder {
public:
	Encoder() {

	}

	~Encoder() {
		m_bQuitFlag.store(true);
		m_pThread->join();

		if (m_pEncoder) {
			::dc_encoder_free(m_pEncoder);
			m_pEncoder = nullptr;
		}

		if (m_pImmContext) {
			m_pImmContext->Release();
			m_pImmContext = nullptr;
		}

		if (m_pD3DDevice) {
			m_pD3DDevice->Release();
			m_pD3DDevice = nullptr;
		}

		if (m_pNividaAdapter1) {
			m_pNividaAdapter1->Release();
			m_pNividaAdapter1 = nullptr;
		}
	}
	
	bool encode_init_width(ID3D11Device* pD3DDevice, ID3D11DeviceContext* pImmContext, const encoder_config_t *config) {
		if (pD3DDevice == nullptr || pImmContext ==nullptr ) {
			return false;
		}

		m_pD3DDevice = pD3DDevice;
		m_pImmContext = pImmContext;
		m_pEncoder = dc_encoder_init(static_cast<void*>(m_pD3DDevice), static_cast<void*>(m_pImmContext), config);
		return m_pEncoder != nullptr;
	}

	bool encode_init(const encoder_config_t* config) {
		// 寻找显卡适配器
		::GetAdapter(&m_pNividaAdapter1);
		if (m_pNividaAdapter1 == nullptr) {
			return false;
		}

		std::array<int, 4> arrDriverType = { D3D_DRIVER_TYPE_HARDWARE,  D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_UNKNOWN };
		D3D_FEATURE_LEVEL featureLevel;
		
		for (auto& index : arrDriverType) {
			HRESULT res = D3D11CreateDevice(m_pNividaAdapter1,
				static_cast<D3D_DRIVER_TYPE>(index),
				nullptr,
				0,
				nullptr,
				0,
				D3D11_SDK_VERSION,
				&m_pD3DDevice, &featureLevel, &m_pImmContext
			);

			if (res == S_OK) {
				printf_s("ID3D11Device init success, type: %d\n", index);
				break;
			}
		}

		if (m_pD3DDevice == nullptr) {
			return false;
		}

		m_pEncoder = dc_encoder_init(static_cast<void*>(m_pD3DDevice), static_cast<void*>(m_pImmContext), config);
		return m_pEncoder != nullptr;
	}

	bool encode_send(void* dx_frame) {
		if (m_pEncoder == nullptr) {
			return false;
		}
		int ret = ::dc_encode_send(m_pEncoder, dx_frame);
		return ret != 0;
	}

	bool encode_recieve(void** buf, int* buf_len) {
		if (m_pEncoder == nullptr) {
			return false;
		}
		int ret = ::dc_encode_recieve(m_pEncoder, buf, buf_len);
		return ret != 0;
	}

	void start_encode() {
		m_bQuitFlag.store(false);
		m_pThread = new std::thread(&Encoder::run, this);
	}

	bool notify_encode(void* dx_frame) {
		if (m_mutex.try_lock()) {
			ID3D11Texture2D* pTexture = reinterpret_cast<ID3D11Texture2D*>(dx_frame);
			m_pCurFrame = pTexture;
			m_mutex.unlock();
			m_cond.notify_one();
			return true;
		}
		else {
			return false;
		}
	}

protected:
	void run() {
		Timer clock;
		int frame_num = 0;
		int64_t frame_cal_start = 0;
		while (!m_bQuitFlag) {
			if (frame_num == 0) {
				frame_cal_start = clock.elapsed();
			}

			int64_t wait_frame_start = clock.elapsed();
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cond.wait(lock, [this]() {
				return this->m_pCurFrame != nullptr;
			});
			printf_s("#############wait time: %d \n", clock.elapsed() - wait_frame_start);

			void* buf;
			int buf_len;
			int64_t encode_start_time = clock.elapsed();
			if (encode_send(*(m_pCurFrame.GetAddressOf()))) {
				if (this->encode_recieve(&buf, &buf_len)) {
					frame_num += 1;
					if (frame_num == 100) {
						frame_num = 0;
						int64_t cost_time = clock.elapsed() - frame_cal_start;
						int64_t fps = 100 * 1000 / cost_time;
						printf_s("fps: %d\n", fps);
					}
					printf_s("Get a Frame success, buf_len: %d, time cost: %d\n", buf_len, clock.elapsed() - encode_start_time);
				}
			}

			this->m_pCurFrame = nullptr;
		}
	}

private:
	
	dc_ptr m_pEncoder = nullptr;

	IDXGIAdapter1* m_pNividaAdapter1 = nullptr;	// 用于初始化 D3DDevice 设备的适配器
	ID3D11Device* m_pD3DDevice = nullptr;			// 显卡设备对象
	ID3D11DeviceContext* m_pImmContext;				// 显卡设备及时上下文
	atomic_bool m_bQuitFlag;

	std::mutex m_mutex;
	std::condition_variable m_cond;
	std::thread* m_pThread = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pCurFrame = nullptr;
};