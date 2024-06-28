/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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

#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "C2RKChipCapDef"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <cutils/properties.h>

#include "C2RKChipCapDef.h"
#include "C2RKLog.h"
#include "mpp_platform.h"
#include "mpp_dev_defs.h"

#define MAX_SOC_NAME_LENGTH     1024

static C2FbcCaps fbcCaps_rk356x[] = {
    { MPP_VIDEO_CodingAVC,  C2_COMPRESS_AFBC_16x16, 0, 4 },
    { MPP_VIDEO_CodingHEVC, C2_COMPRESS_AFBC_16x16, 0, 4 },
    { MPP_VIDEO_CodingVP9,  C2_COMPRESS_AFBC_16x16, 0, 0 },
};

static C2FbcCaps fbcCaps_rk3588[] = {
    { MPP_VIDEO_CodingAVC,  C2_COMPRESS_AFBC_16x16, 0, 4 },
    { MPP_VIDEO_CodingHEVC, C2_COMPRESS_AFBC_16x16, 0, 4 },
    { MPP_VIDEO_CodingVP9,  C2_COMPRESS_AFBC_16x16, 0, 0 },
    { MPP_VIDEO_CodingAVS2, C2_COMPRESS_AFBC_16x16, 0, 8 },
};

static C2FbcCaps fbcCaps_rk3576[] = {
    { MPP_VIDEO_CodingAVC,  C2_COMPRESS_RFBC_64x4, 0, 0 },
    { MPP_VIDEO_CodingHEVC, C2_COMPRESS_RFBC_64x4, 0, 0 },
    { MPP_VIDEO_CodingVP9,  C2_COMPRESS_RFBC_64x4, 0, 0 },
    { MPP_VIDEO_CodingAVS2, C2_COMPRESS_RFBC_64x4, 0, 0 },
    { MPP_VIDEO_CodingAV1,  C2_COMPRESS_RFBC_64x4, 0, 0 },
};

static C2ChipCapInfo sChipCapDefault = {
    .chipName       = "unknown",
    .chipType       = RK_CHIP_UNKOWN,
    .fbcCapNum      = 0,
    .fbcCaps        = nullptr,
    .scaleMode      = 0,
    .cap10bit       = C2_CAP_10BIT_NONE,
    .grallocVersion = 3,
    .reserved       = 0,
};

static C2ChipCapInfo sChipCapInfos[] = {
    {
        .chipName       = "rk3288",
        .chipType       = RK_CHIP_3288,
        .fbcCapNum      = 0,
        .fbcCaps        = nullptr,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_NONE,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3328",
        .chipType       = RK_CHIP_3328,
        .fbcCapNum      = 0,
        .fbcCaps        = nullptr,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC | C2_CAP_10BIT_VP9,
        .grallocVersion = 3,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3399",
        .chipType       = RK_CHIP_3399,
        .fbcCapNum      = 0,
        .fbcCaps        = nullptr,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3368",
        .chipType       = RK_CHIP_3368,
        .fbcCapNum      = 0,
        .fbcCaps        = nullptr,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_HEVC,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3326",
        .chipType       = RK_CHIP_3326,
        .fbcCapNum      = 0,
        .fbcCaps        = nullptr,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_NONE,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "px30",
        .chipType       = RK_CHIP_3326,
        .fbcCapNum      = 0,
        .fbcCaps        = nullptr,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_NONE,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3566",
        .chipType       = RK_CHIP_356X,
        .fbcCapNum      = 3,
        .fbcCaps        = fbcCaps_rk356x,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC | C2_CAP_10BIT_VP9,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3567",
        .chipType       = RK_CHIP_356X,
        .fbcCapNum      = 3,
        .fbcCaps        = fbcCaps_rk356x,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC | C2_CAP_10BIT_VP9,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3568",
        .chipType       = RK_CHIP_356X,
        .fbcCapNum      = 3,
        .fbcCaps        = fbcCaps_rk356x,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC | C2_CAP_10BIT_VP9,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3528",
        .chipType       = RK_CHIP_3528,
        .fbcCapNum      = 4,
        .fbcCaps        = fbcCaps_rk3588,
        .hdrMetaCap     = 1,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC | C2_CAP_10BIT_VP9,
        .grallocVersion = 3,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3588",
        .chipType       = RK_CHIP_3588,
        .fbcCapNum      = 4,
        .fbcCaps        = fbcCaps_rk3588,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC | C2_CAP_10BIT_VP9,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3562",
        .chipType       = RK_CHIP_3562,
        .fbcCapNum      = 0,
        .fbcCaps        = nullptr,
        .hdrMetaCap     = 0,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_NONE,
        .grallocVersion = 4,
        .reserved       = 0,
    },
    {
        .chipName       = "rk3576",
        .chipType       = RK_CHIP_3576,
        .fbcCapNum      = 5,
        .fbcCaps        = fbcCaps_rk3576,
        .hdrMetaCap     = 1,
        .scaleMode      = 0,
        .cap10bit       = C2_CAP_10BIT_AVC | C2_CAP_10BIT_HEVC | C2_CAP_10BIT_VP9,
        .grallocVersion = 4,
        .reserved       = 0,
    },
};


static void readChipName(char *name) {
    const char *path = "/proc/device-tree/compatible";
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        c2_err("open %s error", path);
        return;
    }

    char* ptr = nullptr;

    int length = read(fd, name, MAX_SOC_NAME_LENGTH - 1);
    if (length > 0) {
        /* replacing the termination character to space */
        for (ptr = name;; ptr = name) {
            ptr += strnlen(name, MAX_SOC_NAME_LENGTH);
            *ptr = ' ';
            if (ptr >= name + length - 1)
                break;
        }
        c2_info("read chip name: %s", name);
    }

    close(fd);
}

static C2ChipCapInfo *checkChipInfo(const char *chipName) {
    if (chipName == nullptr)
        return nullptr;

    size_t size = sizeof(sChipCapInfos) / sizeof(sChipCapInfos[0]);

    for (int i = 0; i < size; i++) {
        const char *compatible = sChipCapInfos[i].chipName;

        if (strstr(chipName, compatible)) {
            c2_info("match chip %s", compatible);
            return &sChipCapInfos[i];
        }
    }

    return nullptr;
}


C2RKChipCapDef::C2RKChipCapDef() {
    char name[MAX_SOC_NAME_LENGTH] = { 0 };

    readChipName(name);
    mChipCapInfo = checkChipInfo(name);
    if (mChipCapInfo == nullptr) {
        c2_info("use default chip info");
        mChipCapInfo = &sChipCapDefault;
    }
}

const char* C2RKChipCapDef::getChipName() {
    return mChipCapInfo->chipName;
}

C2ChipType C2RKChipCapDef::getChipType() {
    return mChipCapInfo->chipType;
}

uint32_t C2RKChipCapDef::getHdrMetaCap() {
    uint32_t hdrMeta = mChipCapInfo->hdrMetaCap;
    if (hdrMeta > 0 && property_get_int32("codec2_hdr_meta_disable", 0)) {
        c2_info("property match, disable hdr meta");
        hdrMeta = 0;
    }

    return hdrMeta;
}

uint32_t C2RKChipCapDef::getScaleMode() {
    uint32_t scaleMode = mChipCapInfo->scaleMode;
    if (scaleMode > 0 && property_get_int32("codec2_scale_disable", 0)) {
        c2_info("property match, disable scale mode");
        scaleMode = 0;
    }

    return scaleMode;
}

uint32_t C2RKChipCapDef::getGrallocVersion() {
    return mChipCapInfo->grallocVersion;
}

uint32_t C2RKChipCapDef::getFbcOutputMode(MppCodingType codecId) {
    uint32_t fbcMode = 0;

    for (int i = 0; i < mChipCapInfo->fbcCapNum; i++) {
        if (mChipCapInfo->fbcCaps[i].codecId == codecId) {
            fbcMode = mChipCapInfo->fbcCaps[i].fbcMode;
            break;
        }
    }

    if (fbcMode > 0 && property_get_int32("codec2_fbc_disable", 0)) {
        c2_info("property match, disable fbc output mode");
        fbcMode = 0;
    }

    c2_trace("[%s] codec-0x%08x fbcMode-%d", mChipCapInfo->chipName, codecId, fbcMode);

    return fbcMode;
}

uint32_t C2RKChipCapDef::getFbcMinStride(uint32_t fbcMode) {
    uint32_t minStride = property_get_int32("codec2_fbc_min_stride", 0);
    if (minStride == 0) {
        if (fbcMode == C2_COMPRESS_RFBC_64x4) {
            minStride = 4096;
        } else {
            minStride = 1920;
        }
    }

    return minStride;
}

uint32_t C2RKChipCapDef::getFbcOutputOffset(
        MppCodingType codecId, uint32_t *offsetX, uint32_t *offsetY) {
    *offsetX = *offsetY = 0;

    for (int i = 0; i < mChipCapInfo->fbcCapNum; i++) {
        if (mChipCapInfo->fbcCaps[i].codecId == codecId) {
            *offsetX = mChipCapInfo->fbcCaps[i].offsetX;
            *offsetY = mChipCapInfo->fbcCaps[i].offsetY;
            break;
        }
    }

    return 0;
}

bool C2RKChipCapDef::is10bitSupport(MppCodingType codecId) {
    bool ret = false;

    switch (codecId) {
        case MPP_VIDEO_CodingAVC: {
            if (mChipCapInfo->cap10bit & C2_CAP_10BIT_AVC) {
                ret = true;
            }
            break;
        }
        case MPP_VIDEO_CodingHEVC: {
            if (mChipCapInfo->cap10bit & C2_CAP_10BIT_HEVC) {
                ret = true;
            }
            break;
        }
        case MPP_VIDEO_CodingVP9: {
            if (mChipCapInfo->cap10bit & C2_CAP_10BIT_VP9) {
                ret = true;
            }
            break;
        }
        default: {
            c2_err("Unknown cap10bit for codec: 0x%08x", codecId);
            break;
        }
    }

    return ret;
}

bool C2RKChipCapDef::hasRkVenc() {
    bool ret = false;
    uint32_t vcodec_type = mpp_get_vcodec_type();
    if (vcodec_type & HAVE_RKVENC)
        ret = true;
    return ret;
}

