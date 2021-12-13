#include "StreamEncoder.h"
#include "NvEncoderD3D11.h"

#include "Utils/NvEncoderCLIOptions.h"

std::string GetAppDir() {
	char szPath[MAX_PATH];
	GetModuleFileNameA(NULL, szPath, sizeof(szPath));
	std::string path = szPath;
	size_t pos = path.rfind('\\');
	return path.substr(0, pos + 1);
}

std::string ReadFile(const std::string& file) {
	std::string txt;
	std::ifstream fin(file);
	if (fin) {
		getline(fin, txt);
		fin.close();
	}
	else {
		LOG(ERROR) << "not exist file: " << file;
	}
	return txt;
}

simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateFileLogger(GetAppDir() + "DisplayCaptor64.log");;


bool  CStreamEncoder::Init(ID3D11Device* pD3D11Device, ID3D11DeviceContext* pD3D11DeviceContext, const encoder_config_t* pConfig)
{
	std::lock_guard<std::mutex> locker(m_encodeMutex);

	m_pDevice = pD3D11Device;
	//m_pDevice->GetImmediateContext(m_pContext.GetAddressOf());
	*m_pContext.GetAddressOf() = pD3D11DeviceContext;

	NV_ENC_BUFFER_FORMAT bufferFmt = NV_ENC_BUFFER_FORMAT::NV_ENC_BUFFER_FORMAT_ARGB;
	try {
		m_pNvEncoder = new NvEncoderD3D11(m_pDevice.Get(), pD3D11DeviceContext, pConfig->width, pConfig->height, bufferFmt, 0);

		NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
		NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
		initializeParams.encodeConfig = &encodeConfig;

		// https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-preset-migration-guide/index.html
		// In CreateDefaultEncoderParams, it will get the default configuration according to presetGuid and tuningInfo
		// https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/#recommended-nvenc-settings
		// Recommended settings for optimal quality and performance
		std::string param;
		if (pConfig->param) {
			param = pConfig->param;
		}
		if (pConfig->codec == NV_CODEC_HEVC) {
			param += " -codec hevc";
		}
		if (pConfig->chroma_type == NV_CHROMA_YUV444) {
			param += " -444";
		}
		LOG(INFO) << "init param: " << param;
		NvEncoderInitParam initParam(param.c_str());
		{
			static bool printHelp = true;
			if (printHelp) {
				std::string help = initParam.GetHelpMessage();
				LOG(INFO) << help;
			}
		}

		m_pNvEncoder->CreateDefaultEncoderParams(&initializeParams, 
			initParam.GetEncodeGUID(), initParam.GetPresetGUID(), initParam.GetTuningInfo());
		{
			std::string fullParam = initParam.FullParamToString(&initializeParams);
			LOG(INFO) << fullParam;
		}
		initializeParams.frameRateNum = pConfig->framerate;
		initializeParams.frameRateDen = 1;
		initializeParams.encodeConfig->rcParams.averageBitRate = pConfig->bitrate;
		initializeParams.encodeConfig->rcParams.vbvBufferSize = encodeConfig.rcParams.averageBitRate * initializeParams.frameRateDen / initializeParams.frameRateNum;
		initParam.SetInitParams(&initializeParams, bufferFmt);
		
		/*
		oDisplay's config
		m_pNvEncoder->CreateDefaultEncoderParams(&initializeParams, codecId, NV_ENC_PRESET_LOW_LATENCY_HP_GUID);
		initializeParams.encodeConfig->rcParams.averageBitRate = pConfig->bitrate;
		initializeParams.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
		initializeParams.encodeConfig->rcParams.disableBadapt = 1;
		initializeParams.encodeConfig->rcParams.vbvBufferSize = encodeConfig.rcParams.averageBitRate * initializeParams.frameRateDen / initializeParams.frameRateNum;
		*/

		{
			std::string fullParam = initParam.FullParamToString(&initializeParams);
			LOG(INFO) << fullParam;
		}
		m_pNvEncoder->CreateEncoder(&initializeParams);
	}
	catch (NVENCException& e) {
		LOG(ERROR) << "[nvenc] Init Error[" << e.getErrorCode() << "]: " << e.getErrorString();
		return false;
	}
	catch (std::exception& e) {
		LOG(ERROR) << "CStreamEncoder Init Error: " << e.what();
		return false;
	}
	catch (...) {
		LOG(ERROR) << "CStreamEncoder Init unkown error: ";
		return false;
	}
	return true;
}

bool  CStreamEncoder::OnSurfaceUpdate(void* pDxFrame)
{
	std::lock_guard<std::mutex> locker(m_encodeMutex);

	ID3D11Resource* pTexture = (ID3D11Resource*)pDxFrame;
	const NvEncInputFrame* encoderInputFrame = m_pNvEncoder->GetNextInputFrame();

	ID3D11Texture2D* pTexBgra = reinterpret_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);
	m_pContext->CopyResource(pTexBgra, pTexture);
	return true;
}

bool  CStreamEncoder::ResetEncoder(const encoder_config_t* pConfig)
{
	NV_ENC_RECONFIGURE_PARAMS reconfigureParams;
	memset(&reconfigureParams, 0, sizeof(NV_ENC_RECONFIGURE_PARAMS));
	reconfigureParams.version = NV_ENC_RECONFIGURE_PARAMS_VER;

	NV_ENC_INITIALIZE_PARAMS InitializeParams;
	NV_ENC_CONFIG encodeConfig;
	InitializeParams.encodeConfig = &encodeConfig;

	m_pNvEncoder->GetInitializeParams(&InitializeParams);
	memcpy(&reconfigureParams.reInitEncodeParams, &InitializeParams, sizeof(NV_ENC_INITIALIZE_PARAMS));

	std::string param;
	NV_ENC_BUFFER_FORMAT bufferFmt = NV_ENC_BUFFER_FORMAT::NV_ENC_BUFFER_FORMAT_ARGB;
	if (pConfig->param) {
		param = std::string(pConfig->param, pConfig->param_len);
	}
	if (pConfig->codec == NV_CODEC_HEVC) {
		param += " -codec hevc";
	}
	if (pConfig->chroma_type > 0) {
		if (pConfig->chroma_type == NV_CHROMA_YUV444) {
			param += " -444";
		}
		else {
			param += " -420";
		}
	}
	LOG(INFO) << "reset param: " << param;
	NvEncoderInitParam initParam(param.c_str());
	initParam.SetInitParams(&reconfigureParams.reInitEncodeParams, bufferFmt);

	reconfigureParams.reInitEncodeParams.encodeWidth = pConfig->width;
	reconfigureParams.reInitEncodeParams.encodeHeight = pConfig->height;
	reconfigureParams.reInitEncodeParams.frameRateNum = pConfig->framerate;
	reconfigureParams.reInitEncodeParams.frameRateDen = 1;
	reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.averageBitRate = pConfig->bitrate;
	reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.vbvBufferSize = encodeConfig.rcParams.averageBitRate * reconfigureParams.reInitEncodeParams.frameRateDen / reconfigureParams.reInitEncodeParams.frameRateNum;
	reconfigureParams.forceIDR = 1;
	reconfigureParams.resetEncoder = 1;
	return m_pNvEncoder->Reconfigure(&reconfigureParams);
}

int  CStreamEncoder::Encode(PBYTE pBuffer, UINT uBufferLen, bool bForceIDR)
{
	std::lock_guard<std::mutex> locker(m_encodeMutex);
	if (m_pNvEncoder == nullptr) {
		return 0;
	}

	std::vector<std::vector<uint8_t>> vPacket;
	NV_ENC_PIC_PARAMS picParams;
	memset(&picParams, 0, sizeof(NV_ENC_PIC_PARAMS));
	if (bForceIDR) {
		picParams.encodePicFlags = NV_ENC_PIC_FLAG_OUTPUT_SPSPPS | NV_ENC_PIC_FLAG_FORCEIDR;
	}
	//picParams.codecPicParams.h264PicParams.ltrMarkFrame = 1;
	//picParams.codecPicParams.hevcPicParams.ltrMarkFrame = 1;

	try {
		m_pNvEncoder->EncodeFrame(vPacket, &picParams);
	}
	catch (NVENCException& e) {
		LOG(ERROR) << "[nvenc] Encode Error[" << e.getErrorCode() << "]: %s" << e.getErrorString();
		return -1;
	}

	int frameSize = 0;
	for (std::vector<uint8_t>& packet : vPacket) {
		if (frameSize + packet.size() < uBufferLen) {
			memcpy(pBuffer + frameSize, packet.data(), packet.size());
			frameSize += (int)packet.size();
		}
		else {
			break;
		}
	}
	return frameSize;
}

void CStreamEncoder::UnInit()
{
	std::lock_guard<std::mutex> locker(m_encodeMutex);
	if (m_pNvEncoder != nullptr) {
		try {
			std::vector<std::vector<uint8_t>> vPacket;
			m_pNvEncoder->EndEncode(vPacket);
			m_pNvEncoder->DestroyEncoder();
			delete m_pNvEncoder;
			m_pNvEncoder = nullptr;
		}
		catch (NVENCException &e) {
			LOG(ERROR) << "[nvenc] UnInit Error[" << e.getErrorCode() << "]: %s" << e.getErrorString();
		}
	}
}
