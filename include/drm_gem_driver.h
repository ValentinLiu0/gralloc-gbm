#ifndef DRM_GEM_DRIVER_H
#define DRM_GEM_DRIVER_H

#include <fcntl.h>
#include <linux/string.h>
#include <stdint.h>
#include <stdio.h>
#include <xf86drm.h>

/* Gralloc DRM GEM flags */
#define GRALLOC_DRM_GEM_FLAG_CPU_MMAP (1 << 0)

/* Gralloc DRM GEM driver ABI versions */
#define DRM_GEM_DRIVER_ABI_VERSION_0_0_1 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef const char *(*drm_gem_drv_get_name)(void);
typedef int (*drm_gem_drv_init)(int dri_fd);
typedef int (*drm_gem_drv_bo_alloc)(uint64_t size, uint32_t flags, int *out_prime_fd);
typedef int (*drm_gem_drv_bo_free)(int prime_fd);

typedef struct drm_gem_driver_ops {
	uint32_t			abi_version;
	drm_gem_drv_get_name		get_name;
	drm_gem_drv_init		init;
	drm_gem_drv_bo_alloc		bo_alloc;
	drm_gem_drv_bo_free		bo_free;
} drm_gem_driver_ops_t;

#define GRALLOC_DRM_GEM_DRIVER_OPS drm_gem_driver_ops
// All drivers must export this symbol with the name GRALLOC_DRM_GEM_DRIVER_OPS
extern const drm_gem_driver_ops_t GRALLOC_DRM_GEM_DRIVER_OPS;

#ifdef __cplusplus
}
#endif

#endif /* DRM_GEM_DRIVER_H */