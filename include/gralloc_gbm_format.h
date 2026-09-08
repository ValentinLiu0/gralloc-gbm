/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GRALLOC_GBM_FORMAT_H
#define GRALLOC_GBM_FORMAT_H

#include <gbm.h>
#include <linux/errno.h>
#include <stdlib.h>
#include <system/graphics.h>

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define ALIGN(A, B) (((A) + (B)-1) & ~((B)-1))
#define IS_ALIGNED(A, B) (ALIGN((A), (B)) == (A))

#ifdef __cplusplus
extern "C" {
#endif


typedef struct gralloc_gbm_color_format {
	uint32_t android_format;
	const char *android_name;
	uint32_t gbm_format;
	const char *gbm_name;
} gralloc_gbm_color_format_t;
typedef gralloc_gbm_color_format_t color_fmt_t;

/**
 * Android color format is using Little-endian, but GBM color format is using Big-endian.
 * We store the map between them in this struct.
 * YCbCr = YUV
 */
static const color_fmt_t color_format_map[] = {
	{ HAL_PIXEL_FORMAT_RGBA_8888, "HAL_PIXEL_FORMAT_RGBA_8888", GBM_FORMAT_ABGR8888, "GBM_FORMAT_ABGR8888" },
	{ HAL_PIXEL_FORMAT_RGBX_8888, "HAL_PIXEL_FORMAT_RGBX_8888", GBM_FORMAT_XBGR8888, "GBM_FORMAT_XBGR8888" },
	{ HAL_PIXEL_FORMAT_BGRA_8888, "HAL_PIXEL_FORMAT_BGRA_8888", GBM_FORMAT_ARGB8888, "GBM_FORMAT_ARGB8888" },
	{ HAL_PIXEL_FORMAT_RGB_888, "HAL_PIXEL_FORMAT_RGB_888", GBM_FORMAT_BGR888, "GBM_FORMAT_BGR888" },
	{ HAL_PIXEL_FORMAT_RGB_565, "HAL_PIXEL_FORMAT_RGB_565", GBM_FORMAT_BGR565, "GBM_FORMAT_BGR565" },
	{ HAL_PIXEL_FORMAT_YCBCR_422_SP, "HAL_PIXEL_FORMAT_YCBCR_422_SP", GBM_FORMAT_YUV422, "GBM_FORMAT_YUV422" },
	{ HAL_PIXEL_FORMAT_YCRCB_420_SP, "HAL_PIXEL_FORMAT_YCRCB_420_SP", GBM_FORMAT_YVU420, "GBM_FORMAT_YVU420" },
	{ HAL_PIXEL_FORMAT_YCBCR_422_I, "HAL_PIXEL_FORMAT_YCBCR_422_I", GBM_FORMAT_VYUY, "GBM_FORMAT_VYUY" },
	{ HAL_PIXEL_FORMAT_YCBCR_420_888, "HAL_PIXEL_FORMAT_YCBCR_420_888", GBM_FORMAT_YUV420, "GBM_FORMAT_YUV420" },
	{ HAL_PIXEL_FORMAT_RGBA_1010102, "HAL_PIXEL_FORMAT_RGBA_1010102", GBM_FORMAT_ABGR2101010, "GBM_FORMAT_ABGR2101010" },
	{ HAL_PIXEL_FORMAT_Y8, "HAL_PIXEL_FORMAT_Y8", GBM_FORMAT_R8, "GBM_FORMAT_R8" },
	{ HAL_PIXEL_FORMAT_Y16, "HAL_PIXEL_FORMAT_Y16", GBM_FORMAT_R16, "GBM_FORMAT_R16" },
	/* YV12 is planar, but must be a single buffer so ask for GR88 */
	{ HAL_PIXEL_FORMAT_YV12, "HAL_PIXEL_FORMAT_YV12", GBM_FORMAT_GR88, "GBM_FORMAT_GR88" },
	{ HAL_PIXEL_FORMAT_RGBA_FP16, "HAL_PIXEL_FORMAT_RGBA_FP16", GBM_FORMAT_ABGR16161616F, "GBM_FORMAT_ABGR16161616F" },
	{ HAL_PIXEL_FORMAT_YCBCR_P010, "HAL_PIXEL_FORMAT_YCBCR_P010", __gbm_fourcc_code('P', '0', '1', '0'), "GBM_FOURCC_CODE_P010" },
#define HAL_PIXEL_FORMAT_UNKNOWN 0
#define GBM_FORMAT_UNKNOWN __gbm_fourcc_code('0', '0', '0', '0')
	{ HAL_PIXEL_FORMAT_UNKNOWN, "HAL_PIXEL_FORMAT_UNKNOWN", GBM_FORMAT_UNKNOWN, "GBM_FORMAT_UNKNOWN" },
};
#define COLOR_FMT_MAP_SIZE (sizeof(color_format_map) / sizeof(color_fmt_t))

static inline uint32_t color_fmt_a2g(const uint32_t android_format)
{
	for (uint8_t i = 0; i < COLOR_FMT_MAP_SIZE; i++) {
		if (color_format_map[i].android_format == android_format)
			return color_format_map[i].gbm_format;
	}
	return -EINVAL;
}

static inline uint32_t color_fmt_g2a(const uint32_t gbm_format)
{
	for (uint8_t i = 0; i < COLOR_FMT_MAP_SIZE; i++) {
		if (color_format_map[i].gbm_format == gbm_format)
			return color_format_map[i].android_format;

	}
	return -EINVAL;
}

static inline const char *color_fmt_getname(const uint32_t fmt, const bool is_gbm_format)
{
	for (uint8_t i = 0; i < COLOR_FMT_MAP_SIZE; i++) {
		bool match = false;
		if (is_gbm_format) {
			if (color_format_map[i].gbm_format == fmt)
				return color_format_map[i].gbm_name;
		} else {
			if (color_format_map[i].android_format == fmt)
				return color_format_map[i].android_name;
		}
	}
	return "(no matched format)";
}

typedef struct gralloc_gbm_gbm_color_bpp {
	const uint32_t gbm_format;
	/**
	 * bits per pixel
	 * 1 Byte = 8 bits.
	 * Taking GBM_FORMAT_ABGR8888 as an example:
	 * 1 Pixel has 4 Bytes (1 Channel is 8 bits), so the bit per pixel of it is 4*8=32.
	 * Taking GBM_FORMAT_BGR565 as an example:
	 * 1 Pixel has 16 (5+6+5) bits.
	 */
	const uint8_t bpp;
} gralloc_gbm_gbm_color_bpp_t;
typedef gralloc_gbm_gbm_color_bpp_t color_bpp_t;

static const color_bpp_t color_bpp_map[] = {
	{ GBM_FORMAT_ABGR8888, 32 },
	{ GBM_FORMAT_XBGR8888, 32 },
	{ GBM_FORMAT_ARGB8888, 32 },
	{ GBM_FORMAT_BGR888, 24 },
	{ GBM_FORMAT_BGR565, 16 },
	{ GBM_FORMAT_YUV422, 16 },
	{ GBM_FORMAT_YVU420, 12 },
	{ GBM_FORMAT_VYUY, 16 },
	{ GBM_FORMAT_YUV420, 12 },
	{ GBM_FORMAT_ABGR2101010, 32 },
	{ GBM_FORMAT_ABGR16161616F, 64 },
	{ GBM_FORMAT_UNKNOWN, 0 },
};
#define COLOR_BPP_MAP_SIZE (sizeof(color_bpp_map) / sizeof(color_bpp_t))

static inline uint8_t color_bpp(const uint32_t gbm_format)
{
	for (uint8_t i = 0; i < COLOR_BPP_MAP_SIZE; i++) {
		if (color_bpp_map[i].gbm_format == gbm_format)
			return color_bpp_map[i].bpp;
	}
	return -1;
}

static inline uint8_t color_bytes_per_pixel(const uint32_t gbm_format)
{
	for (uint8_t i = 0; i < COLOR_BPP_MAP_SIZE; i++) {
		if (color_bpp_map[i].gbm_format == gbm_format)
			return (color_bpp_map[i].bpp / 8);
	}
	return -1;
}

static inline uint8_t color_pitch(const int32_t width, const uint32_t gbm_format)
{
	return (color_bytes_per_pixel(gbm_format) * width);
}

static inline uint32_t color_stride(const uint32_t gbm_format)
{
	uint8_t bpp = color_bpp(gbm_format);
	return (bpp > 0) ? (bpp + 7)/8 : 4; // default: 4 bytes
}

#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_GBM_FORMAT_H */
