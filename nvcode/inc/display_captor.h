
#ifndef DISPLAY_CAPTOR_H_
#define DISPLAY_CAPTOR_H_

#define DISPLAY_CAPTOR_API

typedef void* dc_ptr;

enum NV_CODEC_TYPE {
	NV_CODEC_H264 = 0,
	NV_CODEC_HEVC,
	NV_CODEC_MAX,
};

enum NV_CHROMA_TYPE {
	NV_CHROMA_NOSET = 0,
	NV_CHROMA_YUV420,
	NV_CHROMA_YUV422,
	NV_CHROMA_YUV444,
	NV_CHROMA_MAX,
};

typedef struct encoder_config {
	int codec;
	int width;
	int height;
	int framerate;
	int bitrate;
	int chroma_type;
	const char* param;
	int param_len;
}encoder_config_t;

#ifdef __cplusplus
extern "C" {
#endif

DISPLAY_CAPTOR_API dc_ptr dc_encoder_init(void* device, void* context, const encoder_config_t* config);
DISPLAY_CAPTOR_API int dc_encoder_reset(dc_ptr inst, const encoder_config_t* config);
DISPLAY_CAPTOR_API void dc_encoder_free(dc_ptr inst);
DISPLAY_CAPTOR_API int dc_encode_send(dc_ptr inst, void* dx_frame);
DISPLAY_CAPTOR_API int dc_encode_recieve(dc_ptr inst, void** buf, int* buf_len);
DISPLAY_CAPTOR_API int dc_copy(dc_ptr inst, void* dst_resource, void* src_resource);

#ifdef __cplusplus
}
#endif
#endif // DISPLAY_CAPTOR_H_
