/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAPPER_DMA_BUF_HPP
#define MAPPER_DMA_BUF_HPP

#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/hardware/graphics/mapper/utils/IMapperProvider.h>

#include "../MapperBackendImpl.hpp"

/**
 * The FourCC format codes are taken from the drm_fourcc.h definition, and
 * re-namespaced. New GBM formats must not be added, unless they are
 * identical ports from drm_fourcc.
 */
#define __gbm_fourcc_code(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
			      ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define DRM_FORMAT_R8		__gbm_fourcc_code('R', '8', ' ', ' ') /* [7:0] R */

using gralloc_generic::MapperBackendImpl;

class MapperBackendDmaBuf final : public MapperBackendImpl {
public:
	explicit MapperBackendDmaBuf() = default;
	virtual ~MapperBackendDmaBuf() = default;

	bool isReady() const override;
	int init() override;

	int importBuffer(const native_handle_t* _Nonnull handle) override;
	int freeBuffer(buffer_handle_t _Nonnull buffer) override;

	int lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
		 int acquireFence, void* _Nullable* _Nonnull outData) override;
	int unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence) override;

	int flushLockedBuffer(buffer_handle_t _Nonnull buffer) override;
	int rereadLockedBuffer(buffer_handle_t _Nonnull buffer) override;

	int getStandardMetadata(buffer_handle_t _Nonnull buffer,
				int64_t standardMetadataType,
				void* _Nullable destBuffer, size_t destBufferSize) override;
	int setStandardMetadata(buffer_handle_t _Nonnull buffer,
				int64_t standardMetadataType,
				buffer_metadata_t* _Nonnull metadata) override;
};

#endif /* MAPPER_DMA_BUF_HPP */