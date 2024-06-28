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
 *
 */

#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "C2RKGrallocOps"

#include "C2RKGrallocOps.h"
#include "C2RKLog.h"
#include "C2RKGrallocOrigin.h"
#include "C2RKGralloc4.h"
#include "C2RKChipCapDef.h"

namespace android {

C2RKGrallocOps::C2RKGrallocOps() {
    if (C2RKChipCapDef::get()->getGrallocVersion() == 4) {
        mGrallocOps = C2RKGralloc4::getInstance();
    } else {
        mGrallocOps = C2RKGrallocOrigin::getInstance();
    }
}

C2RKGrallocOps::~C2RKGrallocOps() {
}

int32_t C2RKGrallocOps::getShareFd(buffer_handle_t handle) {
    return mGrallocOps->getShareFd(handle);
}

int32_t C2RKGrallocOps::getWidth(buffer_handle_t handle) {
    return mGrallocOps->getWidth(handle);
}

int32_t C2RKGrallocOps::getHeight(buffer_handle_t handle) {
    return mGrallocOps->getHeight(handle);
}

int32_t C2RKGrallocOps::getFormatRequested(buffer_handle_t handle) {
    return mGrallocOps->getFormatRequested(handle);
}

int32_t C2RKGrallocOps::getAllocationSize(buffer_handle_t handle) {
    return mGrallocOps->getAllocationSize(handle);
}

int32_t C2RKGrallocOps::getPixelStride(buffer_handle_t handle) {
    return mGrallocOps->getPixelStride(handle);
}

int32_t C2RKGrallocOps::getByteStride(buffer_handle_t handle) {
    return mGrallocOps->getByteStride(handle);
}

uint64_t C2RKGrallocOps::getUsage(buffer_handle_t handle) {
    return mGrallocOps->getUsage(handle);
}

uint64_t C2RKGrallocOps::getBufferId(buffer_handle_t handle) {
    return mGrallocOps->getBufferId(handle);
}

int32_t C2RKGrallocOps::setDynamicHdrMeta(buffer_handle_t handle, int64_t offset) {
    return mGrallocOps->setDynamicHdrMeta(handle, offset);
}

int64_t C2RKGrallocOps::getDynamicHdrMeta(buffer_handle_t handle) {
    return mGrallocOps->getDynamicHdrMeta(handle);
}

int32_t C2RKGrallocOps::mapScaleMeta(
        buffer_handle_t handle, metadata_for_rkvdec_scaling_t** metadata) {
    return mGrallocOps->mapScaleMeta(handle, metadata);
}

int32_t C2RKGrallocOps::unmapScaleMeta(buffer_handle_t handle) {
    return mGrallocOps->unmapScaleMeta(handle);
}

}
