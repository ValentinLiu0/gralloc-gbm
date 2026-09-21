#ifndef ANDROID_ALLOCATOR_DESCRIPTOR_H
#define ANDROID_ALLOCATOR_DESCRIPTOR_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// android::hardware::graphics::mapper::V4_0::IMapper::BufferDescriptorInfo
typedef struct android_allocator_buffer_descriptor {
	unsigned char *name;
	int32_t width;
	int32_t height;
	int32_t layer_count;
	/* enum android_pixel_format_t */
	int32_t format;
	int64_t usage;
	int64_t reserved_size;
	// std::vector<::aidl::android::hardware::graphics::common::ExtendableType> additionalOptions;
} android_allocator_buffer_descriptor_t;
/**
 * This structure is used to pass the informations required to create
 * a new GBM BO. The Allocator will generate it and pass it to gralloc_gbm.
 */
typedef android_allocator_buffer_descriptor_t allocator_desc_t;

static inline const char *allocator_desc_to_string(const allocator_desc_t *desc)
{
	static char buf[256];

	if (desc == NULL) {
		return "(null)";
	}

	snprintf(buf, sizeof(buf),
		 "name=%s, width=%d, height=%d, layer_count=%d, "
		 "format=%d, usage=0x%llx, reserved_size=%lld",
		 desc->name ? (const char *)desc->name : "(null)",
		 desc->width,
		 desc->height,
		 desc->layer_count,
		 desc->format,
		 (unsigned long long)desc->usage,
		 (long long)desc->reserved_size);

	return buf;
}

#ifdef __cplusplus
}
#endif

#endif /* ANDROID_ALLOCATOR_DESCRIPTOR_H */