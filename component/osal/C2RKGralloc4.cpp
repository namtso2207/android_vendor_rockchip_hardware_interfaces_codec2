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
#define ROCKCHIP_LOG_TAG    "C2RKGralloc4"

#include "C2RKGralloc4.h"
#include "C2RKLog.h"

#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include <gralloctypes/Gralloc4.h>
#include <utils/Errors.h>

using android::status_t;

using android::hardware::graphics::mapper::V4_0::Error;
using android::hardware::graphics::mapper::V4_0::IMapper;
using android::hardware::hidl_vec;

using android::gralloc4::MetadataType_Width;
using android::gralloc4::decodeWidth;
using android::gralloc4::MetadataType_Height;
using android::gralloc4::decodeHeight;
using android::gralloc4::MetadataType_PixelFormatRequested;
using android::gralloc4::decodePixelFormatRequested;
using android::gralloc4::MetadataType_AllocationSize;
using android::gralloc4::decodeAllocationSize;
using android::gralloc4::MetadataType_PlaneLayouts;
using android::gralloc4::decodePlaneLayouts;
using android::gralloc4::MetadataType_Usage;
using android::gralloc4::decodeUsage;
using android::gralloc4::MetadataType_BufferId;
using android::gralloc4::decodeBufferId;

using android::hardware::graphics::common::V1_2::PixelFormat;
using aidl::android::hardware::graphics::common::Dataspace;
using aidl::android::hardware::graphics::common::PlaneLayout;

//#ifdef USE_HARDWARE_ROCKCHIP
//#include <hardware/hardware_rockchip.h>
//#endif

#define HAL_PIXEL_FORMAT_YCrCb_NV12_10      0x17

#define GRALLOC_ARM_METADATA_TYPE_NAME "arm.graphics.ArmMetadataType"
const static IMapper::MetadataType ArmMetadataType_PLANE_FDS {
    GRALLOC_ARM_METADATA_TYPE_NAME,
    1
};

#define OFFSET_OF_DYNAMIC_HDR_METADATA      (1)
#define GRALLOC_RK_METADATA_TYPE_NAME       "rk.graphics.RkMetadataType"
const static IMapper::MetadataType RkMetadataType_OFFSET_OF_DYNAMIC_HDR_METADATA {
    GRALLOC_RK_METADATA_TYPE_NAME,
    OFFSET_OF_DYNAMIC_HDR_METADATA
};

static IMapper &get_service()
{
    static android::sp<IMapper> cached_service = IMapper::getService();
    return *cached_service;
}

static android::status_t decodeRkOffsetOfVideoMetadata(
        const hidl_vec<uint8_t>& input, int64_t* offset_of_metadata)
{
    int64_t offset = 0;

    memcpy(&offset, input.data(), sizeof(offset));

    *offset_of_metadata = offset;

    return android::NO_ERROR;
}

static android::status_t encodeRkOffsetOfVideoMetadata(
        const int64_t offset, hidl_vec<uint8_t>* output)
{
    output->resize(1 * sizeof(int64_t));

    memcpy(output->data(), &offset, sizeof(offset));

    return android::OK;
}

template <typename T>
static int get_metadata(IMapper &mapper, buffer_handle_t handle, IMapper::MetadataType type,
                        status_t (*decode)(const hidl_vec<uint8_t> &, T *), T *value)
{
    void *handle_arg = const_cast<native_handle_t *>(handle);
    assert(handle_arg);
    assert(value);
    assert(decode);

    int err = 0;
    mapper.get(handle_arg, type, [&err, value, decode](Error error, const hidl_vec<uint8_t> &metadata)
                {
                    if (error != Error::NONE) {
                        err = android::BAD_VALUE;
                        return;
                    }
                    err = decode(metadata, value);
                });
    return err;
}

status_t static decodeArmPlaneFds(const hidl_vec<uint8_t>& input, std::vector<int64_t>* fds)
{
    assert (fds != nullptr);
    int64_t size = 0;

    memcpy(&size, input.data(), sizeof(int64_t));
    if (size < 0) {
        return android::BAD_VALUE;
    }

    fds->resize(size);

    const uint8_t *tmp = input.data() + sizeof(int64_t);
    memcpy(fds->data(), tmp, sizeof(int64_t) * size);

    return android::NO_ERROR;
}

/* ---------------------------------------------------------------------------------------------------------
 * Static Functions Implementation
 * ---------------------------------------------------------------------------------------------------------
 */

int32_t C2RKGralloc4::getShareFd(buffer_handle_t handle) {
    auto &mapper = get_service();
    std::vector<int64_t> fds;

    int err = get_metadata(mapper, handle, ArmMetadataType_PLANE_FDS, decodeArmPlaneFds, &fds);
    if (err != android::OK) {
        c2_err("Failed to get plane_fds. err : %d", err);
        return -1;
    }

    return (int32_t)(fds[0]);
}

int32_t C2RKGralloc4::getWidth(buffer_handle_t handle) {
    auto &mapper = get_service();
    uint64_t width = 0;

    int err = get_metadata(mapper, handle, MetadataType_Width, decodeWidth, &width);
    if (err != android::OK) {
        c2_err("Failed to get width. err : %d", err);
        return -1;
    }

    return (uint32_t)width;
}

int32_t C2RKGralloc4::getHeight(buffer_handle_t handle) {
    auto &mapper = get_service();
    uint64_t height = 0;

    int err = get_metadata(mapper, handle, MetadataType_Height, decodeHeight, &height);
    if (err != android::OK) {
        c2_err("Failed to get height. err : %d", err);
        return -1;
    }

    return (int32_t)height;
}

int32_t C2RKGralloc4::getFormatRequested(buffer_handle_t handle) {
    auto &mapper = get_service();
    PixelFormat format;

    int err = get_metadata(mapper, handle,
            MetadataType_PixelFormatRequested, decodePixelFormatRequested, &format);
    if (err != android::OK) {
        c2_err("Failed to get pixel_format_requested. err : %d", err);
        return -1;
    }

    return (int32_t)format;
}

int32_t C2RKGralloc4::getAllocationSize(buffer_handle_t handle) {
    auto &mapper = get_service();
    uint64_t size = 0;

    int err = get_metadata(mapper, handle, MetadataType_AllocationSize, decodeAllocationSize, &size);
    if (err != android::OK) {
        c2_err("Failed to get allocation_size. err : %d", err);
        return -1;
    }

    return (int32_t)size;
}

int32_t C2RKGralloc4::getPixelStride(buffer_handle_t handle) {
    auto &mapper = get_service();
    std::vector<PlaneLayout> layouts;
    int32_t formatRequested = 0;

    formatRequested = getFormatRequested(handle);
    if (formatRequested < 0) {
        c2_err("err formatRequested: %d", formatRequested);
        return -1;
    }

    if (formatRequested != HAL_PIXEL_FORMAT_YCrCb_NV12_10) {
        int err = get_metadata(mapper, handle,
                MetadataType_PlaneLayouts, decodePlaneLayouts, &layouts);
        if (err != android::OK || layouts.size() < 1) {
            c2_err("Failed to get plane layouts. err : %d", err);
            return err;
        }

        if (layouts.size() > 1) {
            c2_warn("it's not reasonable to get global pixel_stride with planes more than 1.");
        }

        return (int32_t)(layouts[0].widthInSamples);
    } else {
        int32_t width = getWidth(handle);
        if (width <= 0) {
            c2_err("err width : %d", width);
            return -1;
        }

        return width;
    }
}

int32_t C2RKGralloc4::getByteStride(buffer_handle_t handle) {
    auto &mapper = get_service();
    std::vector<PlaneLayout> layouts;
    int32_t formatRequested = 0;

    formatRequested = getFormatRequested(handle);
    if (formatRequested < 0) {
        c2_err("err formatRequested: %d", formatRequested);
        return -1;
    }

    if (formatRequested != HAL_PIXEL_FORMAT_YCrCb_NV12_10) {
        int err = get_metadata(mapper, handle,
                MetadataType_PlaneLayouts, decodePlaneLayouts, &layouts);
        if (err != android::OK || layouts.size() < 1) {
            c2_err("Failed to get plane layouts. err : %d", err);
            return err;
        }

        if (layouts.size() > 1) {
            c2_warn("it's not reasonable to get global pixel_stride with planes more than 1.");
        }

        return (int32_t)(layouts[0].strideInBytes);
    } else {
        int32_t width = getWidth(handle);
        if (width <= 0) {
            c2_err("err width : %d", width);
            return -1;
        }

        return width;
    }
}

uint64_t C2RKGralloc4::getUsage(buffer_handle_t handle) {
    auto &mapper = get_service();
    uint64_t usage = 0;

    int err = get_metadata(mapper, handle, MetadataType_Usage, decodeUsage, &usage);
    if (err != android::OK) {
        c2_err("Failed to get usage. err : %d", err);
        return 0;
    }

    return usage;
}

uint64_t C2RKGralloc4::getBufferId(buffer_handle_t handle) {
    auto &mapper = get_service();
    uint64_t id = 0;

    int err = get_metadata(mapper, handle, MetadataType_BufferId, decodeBufferId, &id);
    if (err != android::OK) {
        c2_err("Failed to get buffer id. err : %d", err);
        return 0;
    }

    return id;
}

int32_t C2RKGralloc4::setDynamicHdrMeta(buffer_handle_t handle, int64_t offset) {
    int32_t err = android::OK;
    auto &mapper = get_service();
    hidl_vec<uint8_t> encodedOffset;

    err = encodeRkOffsetOfVideoMetadata(offset, &encodedOffset);
    if (err != android::OK) {
        c2_err("Failed to encode offset_of_dynamic_hdr_metadata. err : %d", err);
        return err;
    }

    auto ret = mapper.set(const_cast<native_handle_t*>(handle),
                          RkMetadataType_OFFSET_OF_DYNAMIC_HDR_METADATA,
                          encodedOffset);
    const Error error = ret.withDefault(Error::NO_RESOURCES);
    switch (error) {
        case Error::BAD_DESCRIPTOR:
        case Error::BAD_BUFFER:
        case Error::BAD_VALUE:
        case Error::NO_RESOURCES:
            c2_err("set(%s, %lld, ...) failed with %d",
                RkMetadataType_OFFSET_OF_DYNAMIC_HDR_METADATA.name.c_str(),
                (long long)RkMetadataType_OFFSET_OF_DYNAMIC_HDR_METADATA.value,
                error);
            err = -1;
            break;
        // It is not an error to attempt to set metadata that a particular gralloc implementation
        // happens to not support.
        case Error::UNSUPPORTED:
        case Error::NONE:
            break;
    }

    return -1;
}

int64_t C2RKGralloc4::getDynamicHdrMeta(buffer_handle_t handle) {
    auto &mapper = get_service();
    int64_t offset = 0;

    int err = get_metadata(mapper, handle, RkMetadataType_OFFSET_OF_DYNAMIC_HDR_METADATA,
                           decodeRkOffsetOfVideoMetadata, &offset);
    if (err != android::OK) {
        c2_err("Failed to get buffer id. err : %d", err);
        return 0;
    }

    return -1;
}

int32_t C2RKGralloc4::mapScaleMeta(
        buffer_handle_t handle, metadata_for_rkvdec_scaling_t** metadata) {
    (void)handle;
    (void)metadata;
    c2_err("not implement");
    return -1;
}

int32_t C2RKGralloc4::unmapScaleMeta(buffer_handle_t handle) {
    (void)handle;
    c2_err("not implement");
    return -1;
}

