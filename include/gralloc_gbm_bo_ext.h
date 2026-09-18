/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GRALLOC_GBM_BO_EXT_H
#define GRALLOC_GBM_BO_EXT_H

#include <stdlib.h>

#define GBM_BO_USE_NONE			0
#define GBM_BO_USE_EXT_RESERVED1	(1 << 11)
#define GBM_BO_USE_EXT_RESERVED2	(1 << 12)
#define GBM_BO_USE_EXT_RESERVED3	(1 << 13)

#ifdef __cplusplus
extern "C" {
#endif

// Smpte2086.h
typedef struct gralloc_gbm_smpte2086 {
	float primary_red_x;
	float primary_red_y;
	float primary_green_x;
	float primary_green_y;
	float primary_blue_x;
	float primary_blue_y;
	float white_point_x;
	float white_point_y;
	float max_luminance;
	float min_luminance;
} gralloc_gbm_smpte2086_t, smpte2086_t;

// Cta861_3.h
typedef struct gralloc_gbm_cta861_3 {
	float max_content_light_level;
	float max_frame_average_light_level;
} gralloc_gbm_cta861_3_t, cta861_3_t;

typedef struct gralloc_gbm_bo_data {
	void *map_data;
	int lock_count;
	int locked_for;

	/* Android Graphics Metadata Support */

	int32_t dataspace;
	int32_t blend_mode;
	smpte2086_t *smpte2086;
	cta861_3_t *cta861_3;
} gralloc_gbm_bo_data_t, gbm_bo_data_t;


#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_GBM_BO_EXT_H */