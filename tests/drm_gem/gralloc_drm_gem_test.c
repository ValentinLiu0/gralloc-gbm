#include <drm-uapi/panthor_drm.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include <gralloc_drm_gem.h>

int main()
{
	printf("---gralloc_drm_gem_test BEGIN---\n");

	int ret = 0;
	ret = gralloc_drm_gem_init();
	printf("Init DRM GEM ret=%d\n", ret);
	int buf_fd;
	// Fake desc
	allocator_desc_t desc = {
		.width = 32768,
		.height = 1,
		.usage = 0,
	};
	buffer_handle_t handle;
	ret = gralloc_drm_gem_bo_create(&desc, &buf_fd, &handle);
	printf("Create BO size=32768 flags=0 ret=%d, buf_fd=%d\n", ret, buf_fd);
	if (ret == 0 && buf_fd > 0)
		printf("PASSED bo_create\n");
	else
		return ret;
	ret = gralloc_drm_gem_bo_destory(buf_fd);
	printf("Destory BO ret=%d, buf_fd=%d\n", ret, buf_fd);
	if (ret == 0)
		printf("PASSED bo_destory\n");
	else
		return ret;
	
	printf("---gralloc_drm_gem_test END---\n");

	return 0;
}