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

#ifndef ANDROID_C2_RK_GRALLOC4_H__
#define ANDROID_C2_RK_GRALLOC4_H__

#include "C2RKGrallocInterface.h"

class C2RKGralloc4 : public C2RKGrallocInterface {
public:
    static C2RKGralloc4* getInstance() {
        static C2RKGralloc4 _gInstance;
        return &_gInstance;
    }

    virtual int32_t  getShareFd(buffer_handle_t handle);
    virtual int32_t  getWidth(buffer_handle_t handle);
    virtual int32_t  getHeight(buffer_handle_t handle);
    virtual int32_t  getFormatRequested(buffer_handle_t handle);
    virtual int32_t  getAllocationSize(buffer_handle_t handle);
    virtual int32_t  getPixelStride(buffer_handle_t handle);
    virtual int32_t  getByteStride(buffer_handle_t handle);
    virtual uint64_t getUsage(buffer_handle_t handle);
    virtual uint64_t getBufferId(buffer_handle_t handle);

    virtual int32_t  setDynamicHdrMeta(buffer_handle_t handle, int64_t offset);
    virtual int64_t  getDynamicHdrMeta(buffer_handle_t handle);
    virtual int32_t  mapScaleMeta(buffer_handle_t handle, metadata_for_rkvdec_scaling_t** metadata);
    virtual int32_t  unmapScaleMeta(buffer_handle_t handle);

private:
    C2RKGralloc4() {};
    virtual ~C2RKGralloc4() {};
};

#endif /* ANDROID_C2_RK_GRALLOC4_H__ */

