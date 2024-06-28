/*

 * Copyright 2023 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_RK_C2_GRALLOC_OPS_H_
#define ANDROID_RK_C2_GRALLOC_OPS_H_

#include "C2RKGrallocInterface.h"

namespace android {

#ifndef GRALLOC_USAGE_RKVDEC_SCALING
#define GRALLOC_USAGE_RKVDEC_SCALING   0x01000000U
#endif

#ifndef GRALLOC_USAGE_DYNAMIC_HDR
#define GRALLOC_USAGE_DYNAMIC_HDR      0x02000000U
#endif

class C2RKGrallocOps {
public:
    static C2RKGrallocOps* get()
    {
        static C2RKGrallocOps _gInstance;
        return &_gInstance;
    }

    int32_t  getShareFd(buffer_handle_t handle);
    int32_t  getWidth(buffer_handle_t handle);
    int32_t  getHeight(buffer_handle_t handle);
    int32_t  getFormatRequested(buffer_handle_t handle);
    int32_t  getAllocationSize(buffer_handle_t handle);
    int32_t  getPixelStride(buffer_handle_t handle);
    int32_t  getByteStride(buffer_handle_t handle);
    uint64_t getUsage(buffer_handle_t handle);
    uint64_t getBufferId(buffer_handle_t handle);

    /* only support in gralloc 0.3 currently */
    int32_t  setDynamicHdrMeta(buffer_handle_t handle, int64_t offset);
    int64_t  getDynamicHdrMeta(buffer_handle_t handle);
    int32_t  mapScaleMeta(buffer_handle_t handle, metadata_for_rkvdec_scaling_t** metadata);
    int32_t  unmapScaleMeta(buffer_handle_t handle);

private:
    C2RKGrallocOps();
    virtual ~C2RKGrallocOps();

    C2RKGrallocInterface *mGrallocOps;
};

}

#endif // ANDROID_RK_C2_GRALLOC_OPS_H_

