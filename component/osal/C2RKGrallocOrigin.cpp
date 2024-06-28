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

#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "C2RKGrallocOrigin"

#include "C2RKGrallocOrigin.h"
#include "C2RKLog.h"

#define PERFORM_SET_OFFSET_OF_DYNAMIC_HDR_METADATA           0x08100017
#define PERFORM_GET_OFFSET_OF_DYNAMIC_HDR_METADATA           0x08100018
#define PERFORM_LOCK_RKVDEC_SCALING_METADATA                 0x08100019
#define PERFORM_UNLOCK_RKVDEC_SCALING_METADATA               0x0810001A

#define PERFORM_GET_HADNLE_PRIME_FD                          0x08100002
#define PERFORM_GET_HADNLE_WIDTH                             0x08100008
#define PERFORM_GET_HADNLE_HEIGHT                            0x0810000A
#define PERFORM_GET_HADNLE_STRIDE                            0x0810000C
#define PERFORM_GET_HADNLE_BYTE_STRIDE                       0x0810000E
#define PERFORM_GET_HADNLE_FORMAT                            0x08100010
#define PERFORM_GET_SIZE                                     0x08100012
#define PERFORM_GET_BUFFER_ID                                0x0810001B
#define PERFORM_GET_USAGE                                    0x0feeff03

#define CHECK_RUNTIME() \
do{\
    if (!mGralloc || !mGralloc->perform) {\
        c2_err("Failed to %s in error state", __FUNCTION__); \
        return -1; \
}} while (0)

C2RKGrallocOrigin::C2RKGrallocOrigin() {
    const hw_module_t* module = NULL;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) != 0) {
        c2_err("Failed to open gralloc module");
        return;
    }
    mGralloc = reinterpret_cast<const gralloc_module_t*>(module);
}

C2RKGrallocOrigin::~C2RKGrallocOrigin(){
}

int32_t C2RKGrallocOrigin::getShareFd(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int32_t fd = -1;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_HADNLE_PRIME_FD, handle, &fd);
    if (err != 0) {
        c2_err("Failed to get fd. err %d", err);
        return -1;
    }

    return fd;
}

int32_t C2RKGrallocOrigin::getWidth(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int32_t width = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_HADNLE_WIDTH, handle, &width);
    if (err != 0) {
        c2_err("Failed to get width. err %d", err);
        return -1;
    }

    return width;
}

int32_t C2RKGrallocOrigin::getHeight(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int32_t height = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_HADNLE_HEIGHT, handle, &height);
    if (err != 0) {
        c2_err("Failed to get width. err %d", err);
        return -1;
    }

    return height;
}

int32_t C2RKGrallocOrigin::getFormatRequested(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int32_t format = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_HADNLE_FORMAT, handle, &format);
    if (err != 0) {
        c2_err("Failed to get format. err %d", err);
        return -1;
    }

    return format;
}

int32_t C2RKGrallocOrigin::getAllocationSize(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int32_t allocSize = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_SIZE, handle, &allocSize);
    if (err != 0) {
        c2_err("Failed to get size. err %d", err);
        return -1;
    }

    return allocSize;
}

int32_t C2RKGrallocOrigin::getPixelStride(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int32_t stride = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_HADNLE_STRIDE, handle, &stride);
    if (err != 0) {
        c2_err("Failed to get pixer stride. err %d", err);
        return -1;
    }

    return stride;
}

int32_t C2RKGrallocOrigin::getByteStride(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int32_t stride = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_HADNLE_BYTE_STRIDE, handle, &stride);
    if (err != 0) {
        c2_err("Failed to get byte stride. err %d", err);
        return -1;
    }

    return stride;
}

uint64_t C2RKGrallocOrigin::getUsage(buffer_handle_t handle) {
    CHECK_RUNTIME();

    uint64_t usage = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_USAGE, handle, &usage);
    if (err != 0) {
        c2_err("Failed to get usage. err %d", err);
        return 0;
    }

    return usage;
}

uint64_t C2RKGrallocOrigin::getBufferId(buffer_handle_t handle) {
    CHECK_RUNTIME();

    uint64_t id = 0;

    int err = mGralloc->perform(mGralloc, PERFORM_GET_BUFFER_ID, handle, &id);
    if (err != 0) {
        c2_err("Failed to get bufferId. err %d", err);
        return 0;
    }

    return id;
}

int32_t C2RKGrallocOrigin::setDynamicHdrMeta(buffer_handle_t handle, int64_t offset) {
    CHECK_RUNTIME();

    int err = mGralloc->perform(
            mGralloc, PERFORM_SET_OFFSET_OF_DYNAMIC_HDR_METADATA, handle, offset);
    if (err != 0) {
        c2_err("Failed to set dynamic hdr metadata, err %d", err);
    }

    return err;
}

int64_t C2RKGrallocOrigin::getDynamicHdrMeta(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int64_t offset = 0;

    int err = mGralloc->perform(
            mGralloc, PERFORM_GET_OFFSET_OF_DYNAMIC_HDR_METADATA, handle, &offset);
    if (err != 0) {
        c2_err("Failed to get dynamic hdr metadata, err %d", err);
        return -1;
    }

    return offset;
}

int32_t C2RKGrallocOrigin::mapScaleMeta(
        buffer_handle_t handle, metadata_for_rkvdec_scaling_t** metadata) {
    CHECK_RUNTIME();

    int err = mGralloc->perform(
            mGralloc, PERFORM_LOCK_RKVDEC_SCALING_METADATA, handle, metadata);
    if (err != 0) {
        c2_err("Failed to lock rkdevc_scaling_metadata, err %d", err);
    }

    return err;
}

int32_t C2RKGrallocOrigin::unmapScaleMeta(buffer_handle_t handle) {
    CHECK_RUNTIME();

    int err = mGralloc->perform(mGralloc, PERFORM_UNLOCK_RKVDEC_SCALING_METADATA, handle);
    if (err != 0) {
        c2_err("Failed to unlock rkdevc_scaling_metadata, err %d", err);
    }

    return err;
}

