#pragma once
#include <stdio.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <sstream>
#include <string>
#include <queue>
#include "time.h"
#include "CommonFun.h"
#include "CommonType.h"
#include "display_captor.h"
class Encoder {
public:
	using texture_type = Microsoft::WRL::ComPtr<ID3D11Texture2D>;

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

		if (test_file) {
			fclose(test_file);
			test_file = nullptr;
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
		m_iHasCount.store(0);
		m_pThread = new std::thread(&Encoder::run, this);
	}

	void notify_encode(void* dx_frame) {
		bool need_notify = false;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			ID3D11Texture2D* pTexture = reinterpret_cast<ID3D11Texture2D*>(dx_frame);
			texture_type pTexture_com = pTexture;
			if (m_iHasCount == 0) {
				need_notify = true;
			}
			this->encode_send(dx_frame);
			m_iHasCount += 1;
		}

		if (need_notify) {
			m_cond.notify_one();
		}
	}

	dc_ptr get_ptr() {
		return m_pEncoder;
	}

protected:
	void run() {
		Timer clock;
		int frame_num = 0;
		int64_t frame_count = 0;
		int64_t frame_cal_start = 0;
		int64_t wait_frame_start = clock.elapsed();

		test_file = fopen("test_file.h264", "wb");

		while (!m_bQuitFlag) {
			if (frame_num == 0) {
				frame_cal_start = clock.elapsed();
			}

			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_iHasCount == 0)
			{
				m_cond.wait(lock, [this]() {
					return m_iHasCount > 0;
				});
			}

			printf_s("#############wait time: %d \n", clock.elapsed() - wait_frame_start);

			void* buf;
			int buf_len;
			int64_t encode_start_time = clock.elapsed();
			if (this->encode_recieve(&buf, &buf_len)) {
				m_iHasCount -= 1;
				frame_num += 1;
				if (frame_num == 100) {
					frame_num = 0;
					int64_t cost_time = clock.elapsed() - frame_cal_start;
					int64_t fps = 100 * 1000 / cost_time;
					printf_s("fps: %d\n", fps);
				}
				wait_frame_start = clock.elapsed();
				printf_s("Get a Frame success, buf_len: %d, time cost: %d\n", buf_len, clock.elapsed() - encode_start_time);
				std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp
					= std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
				std::time_t timestamp = tp.time_since_epoch().count();
				frame_count += 1;
				printf_s("#######encoder frame num: %d,  timestamp: %d\n", frame_count, timestamp);

				//fwrite(buf, 1, buf_len, test_file);
			}
		}
	}

private:
	dc_ptr m_pEncoder = nullptr;

	IDXGIAdapter1* m_pNividaAdapter1 = nullptr;	// 用于初始化 D3DDevice 设备的适配器
	ID3D11Device* m_pD3DDevice = nullptr;			// 显卡设备对象
	ID3D11DeviceContext* m_pImmContext;				// 显卡设备及时上下文
	atomic_bool m_bQuitFlag;
	std::condition_variable m_cond;

	std::mutex m_mutex;
	std::thread* m_pThread = nullptr;
	atomic_int16_t m_iHasCount;

	FILE* test_file = nullptr;
};