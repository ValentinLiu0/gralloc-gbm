#ifndef GRALLOC_DRM_GEM_H
#define GRALLOC_DRM_GEM_H

#include <hardware/gralloc.h>

#include <android_allocator_descriptor.h>

/* Gralloc DRM GEM functions */

int gralloc_drm_gem_init(void);
bool gralloc_drm_gem_is_allocator_desc_supported(const allocator_desc_t *desc);
int gralloc_drm_gem_bo_create(allocator_desc_t *desc, uint32_t *out_stride, native_handle_t **out_buffer_handle);
int gralloc_drm_gem_bo_destory(int prime_fd);
int32_t gralloc_drm_gem_caculate_android_pixel_stride(const int32_t android_format, const uint32_t stride /*pitch*/);

#endif /* GRALLOC_DRM_GEM_H */