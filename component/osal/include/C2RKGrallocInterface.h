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
 * limitations under the License.00
 *
 */

#ifndef ANDROID_C2_RK_GRALLOC_INTERFACE_H__
#define ANDROID_C2_RK_GRALLOC_INTERFACE_H__

#include <stdint.h>
#include <cutils/native_handle.h>

#ifndef GRALLOC_MODULE_PERFORM_LOCK_RKVDEC_SCALING_METADATA
typedef struct metadata_for_rkvdec_scaling_t
{
    uint64_t version;
    // mask
    uint64_t requestMask;
    uint64_t replyMask;

    // buffer info
    uint32_t width;   // pixel_w
    uint32_t height;  // pixel_h
    uint32_t format;  // drm_fourcc
    uint64_t modifier;// modifier
    uint32_t usage;   // usage
    uint32_t pixel_stride; // pixel_stride

    // image info
    uint32_t srcLeft;
    uint32_t srcTop;
    uint32_t srcRight;
    uint32_t srcBottom;

    // buffer layout
    uint32_t layer_cnt;
    uint32_t fd[4];
    uint32_t offset[4];
    uint32_t byteStride[4];
} metadata_for_rkvdec_scaling_t;
#endif

class C2RKGrallocInterface {
public:
    virtual ~C2RKGrallocInterface() {}

    virtual int32_t  getShareFd(buffer_handle_t handle) = 0;
    virtual int32_t  getWidth(buffer_handle_t handle) = 0;
    virtual int32_t  getHeight(buffer_handle_t handle) = 0;
    virtual int32_t  getFormatRequested(buffer_handle_t handle) = 0;
    virtual int32_t  getAllocationSize(buffer_handle_t handle) = 0;
    virtual int32_t  getPixelStride(buffer_handle_t handle) = 0;
    virtual int32_t  getByteStride(buffer_handle_t handle) = 0;
    virtual uint64_t getUsage(buffer_handle_t handle) = 0;
    virtual uint64_t getBufferId(buffer_handle_t handle) = 0;

    /* only support in gralloc 0.3 currently */
    virtual int32_t  setDynamicHdrMeta(buffer_handle_t handle, int64_t offset) = 0;
    virtual int64_t  getDynamicHdrMeta(buffer_handle_t handle) = 0;
    virtual int32_t  mapScaleMeta(buffer_handle_t handle, metadata_for_rkvdec_scaling_t** metadata) = 0;
    virtual int32_t  unmapScaleMeta(buffer_handle_t handle) = 0;

};

#endif /* ANDROID_C2_RK_GRALLOC_INTERFACE_H__ */
