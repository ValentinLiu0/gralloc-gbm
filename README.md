# Gralloc GBM

Gralloc V5 implements for using GBM backends (Currently using libgbm from Mesa3d).

Tested working fine with Android Hardware Composer 3 (HWC3) DRM.

## Compile and Use
1. Download the source.
```bash
cd <path_to_aosp_root>
mkdir -p vendor/valentinliu0/
cd vendor/valentinliu0/
git clone https://github.com/ValentinLiu0/gralloc-gbm.git
```

2. Add product packages.
```Makefile
# Gralloc GBM
PRODUCT_PACKAGES += \
	android.hardware.graphics.allocator-service.gbm \
	mapper.gbm
```
Note: the file `/vendor/bin/hw/android.hardware.graphics.allocator-service.gbm` should be labeled with correct SELinux label to execute.
Like:
```sepolicy
/vendor/bin/hw/android\.hardware\.graphics\.allocator-service\.gbm  u:object_r:vendor_services_exec:s0
```

3. Build AOSP and product image.

## Development
### Projects
| Project                     | Main build artifacts                              | Path              | Usage                           |
|-----------------------------|---------------------------------------------------|-------------------|---------------------------------|
| gralloc_gbm                 | `libgralloc_gbm.so`                               | `common/`         | Library provided by gralloc_gbm |
| Allocator V2                | `android.hardware.graphics.allocator-service.gbm` | `allocator/`      | Allocator V2 AIDL service       |
| Mapper Stable-C (Mapper V5) | `mapper.gbm.so`                                   | `mapper_stablec/` | IMapper Stable-C implement      |

### Dependencies
- libgbm (Provided by Mesa3d alias `libgbm_mesa`)
- uthash (`include/uthash/`)

## License
This project is licensed under the Apache License, Version 2.0.

Copyright (c) 2026 Valentin Liu (LIU, YUANCHEN)

You may obtain a copy of the Apache License, Version 2.0 at:

[Apache License 2.0](/LICENSE)

Unless required by applicable law or agreed to in writing, software distributed under the Apache License, Version 2.0 is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

### Third-Party Libraries
| Project | License           | Path                                                                            |
|---------|-------------------|---------------------------------------------------------------------------------|
| UTHASH  | BSD-style License | [include/uthash/](/include/uthash/LICENSE)                                      |
| Mesa3D  | Multiple Licenses | [Mesa3D Project](https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/licenses) |