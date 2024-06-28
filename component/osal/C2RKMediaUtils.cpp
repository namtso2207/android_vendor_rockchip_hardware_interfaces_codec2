/*
 * Copyright (C) 2020 Rockchip Electronics Co. LTD
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
#define ROCKCHIP_LOG_TAG    "C2RKMediaUtils"

#include <string.h>
#include <C2Config.h>
#include <cutils/properties.h>

#include "C2RKMediaUtils.h"
#include "C2RKLog.h"

using namespace android;

typedef struct {
    int32_t      level;
    uint32_t     maxDpbPixs;     /* Max dpb picture total pixels */
    const char  *name;
} C2LevelInfo;

typedef struct {
    uint32_t c2Format;
    uint32_t androidFormat[3];
} C2FormatMap;

static const C2FormatMap gFormatList[] = {
    {
        MPP_FMT_YUV420SP,
        {
            HAL_PIXEL_FORMAT_YCrCb_NV12,       // RT_COMPRESS_MODE_NONE
            HAL_PIXEL_FORMAT_YUV420_8BIT_I,    // RT_COMPRESS_AFBC_16x16
            HAL_PIXEL_FORMAT_YUV420_8BIT_RFBC  // RT_COMPRESS_RFBC_64x4
        }
    },
    {
        MPP_FMT_YUV420P,
        {
            HAL_PIXEL_FORMAT_YCrCb_NV12,
            HAL_PIXEL_FORMAT_YUV420_8BIT_I,
            HAL_PIXEL_FORMAT_YUV420_8BIT_RFBC
        }
    },
    {
        MPP_FMT_YUV420SP_10BIT,
        {
            HAL_PIXEL_FORMAT_YCrCb_NV12_10,
            HAL_PIXEL_FORMAT_YUV420_10BIT_I,
            HAL_PIXEL_FORMAT_YUV420_10BIT_RFBC
        }
    },
    {
        MPP_FMT_YUV422SP,
        {
            HAL_PIXEL_FORMAT_YCbCr_422_SP,
            HAL_PIXEL_FORMAT_YCbCr_422_I,
            HAL_PIXEL_FORMAT_YUV422_8BIT_RFBC,
        }
    },
    {
        MPP_FMT_YUV422P,
        {
            HAL_PIXEL_FORMAT_YCbCr_422_SP,
            HAL_PIXEL_FORMAT_YCbCr_422_I,
            HAL_PIXEL_FORMAT_YUV422_8BIT_RFBC,
        }
    },
    {
        MPP_FMT_YUV422SP_10BIT,
        {
            HAL_PIXEL_FORMAT_YCbCr_422_SP_10,
            HAL_PIXEL_FORMAT_Y210,
            HAL_PIXEL_FORMAT_YUV422_10BIT_RFBC,
        }
    },
    {
        MPP_FMT_YUV444SP,
        {
            HAL_PIXEL_FORMAT_YCBCR_444_888,
            0,
            HAL_PIXEL_FORMAT_YUV444_8BIT_RFBC,
        }
    },
    {
        MPP_FMT_YUV444P,
        {
            HAL_PIXEL_FORMAT_YCBCR_444_888,
            0,
            HAL_PIXEL_FORMAT_YUV444_8BIT_RFBC,
        }
    },
};

static const size_t gNumFormatList =
        sizeof(gFormatList) / sizeof(gFormatList[0]);


static C2LevelInfo h264LevelInfos[] = {
    /*  level            maxDpbPixs(maxDpbMbs * 256) name    */
    {   C2Config::LEVEL_AVC_5,    110400 * 256,    "h264 level 5"   },
    {   C2Config::LEVEL_AVC_5_1,  184320 * 256,    "h264 level 5.1" },
    {   C2Config::LEVEL_AVC_5_2,  184320 * 256,    "h264 level 5.2" },
    {   C2Config::LEVEL_AVC_6,    696320 * 256,    "h264 level 6"   },
    {   C2Config::LEVEL_AVC_6_1,  696320 * 256,    "h264 level 6.1" },
    {   C2Config::LEVEL_AVC_6_2,  696320 * 256,    "h264 level 6.2" },
};

static C2LevelInfo h265LevelInfos[] = {
    /*  level                     maxDpbMBs(maxPicSize * 6) name    */
    {   C2Config::LEVEL_HEVC_MAIN_5,     8912896 * 6,    "h265 level 5"   },
    {   C2Config::LEVEL_HEVC_MAIN_5_1,   8912896 * 6,    "h265 level 5.1" },
    {   C2Config::LEVEL_HEVC_MAIN_5_2,   8912896 * 6,    "h265 level 5.2" },
    {   C2Config::LEVEL_HEVC_MAIN_6,    35651584 * 6,    "h265 level 6"   },
    {   C2Config::LEVEL_HEVC_MAIN_6_1,  35651584 * 6,    "h265 level 6.1" },
    {   C2Config::LEVEL_HEVC_MAIN_6_2,  35651584 * 6,    "h265 level 6.2" },
    {   C2Config::LEVEL_HEVC_HIGH_5,     8912896 * 6,    "h265 level 5"   },
    {   C2Config::LEVEL_HEVC_HIGH_5_1,   8912896 * 6,    "h265 level 5.1" },
    {   C2Config::LEVEL_HEVC_HIGH_5_2,   8912896 * 6,    "h265 level 5.2" },
    {   C2Config::LEVEL_HEVC_HIGH_6,    35651584 * 6,    "h265 level 6"   },
    {   C2Config::LEVEL_HEVC_HIGH_6_1,  35651584 * 6,    "h265 level 6.1" },
    {   C2Config::LEVEL_HEVC_HIGH_6_2,  35651584 * 6,    "h265 level 6.2" },
};

static C2LevelInfo vp9LevelInfos[] = {
    /*  level                     maxDpbMBs(maxPicSize * 4) name    */
    {   C2Config::LEVEL_VP9_5,      8912896 * 4,    "vp9 level 5"   },
    {   C2Config::LEVEL_VP9_5_1,    8912896 * 4,    "vp9 level 5.1" },
    {   C2Config::LEVEL_VP9_5_2,    8912896 * 4,    "vp9 level 5.2" },
    {   C2Config::LEVEL_VP9_6,     35651584 * 4,    "vp9 level 6"   },
    {   C2Config::LEVEL_VP9_6_1,   35651584 * 4,    "vp9 level 6.1" },
    {   C2Config::LEVEL_VP9_6_2,   35651584 * 4,    "vp9 level 6.2" },
};

uint32_t C2RKMediaUtils::getAndroidColorFmt(uint32_t format, uint32_t fbcMode) {
    uint32_t androidFormat = HAL_PIXEL_FORMAT_YCrCb_NV12;

    int32_t i = 0;
    for (i = 0; i < gNumFormatList; i++) {
        if (gFormatList[i].c2Format == (format & MPP_FRAME_FMT_MASK)) {
            if (gFormatList[i].androidFormat[fbcMode] > 0) {
                androidFormat = gFormatList[i].androidFormat[fbcMode];
            } else {
                c2_err("unable to get available fmt from fbcMode %d", fbcMode);
            }
            break;
        }
    }

    if (i == gNumFormatList) {
        c2_err("unsupport c2Format 0x%x fbcMode %d", format, fbcMode);
    }

    return androidFormat;
}

uint64_t C2RKMediaUtils::getStrideUsage(int32_t width, int32_t stride) {
#ifdef RK_GRALLOC_USAGE_STRIDE_ALIGN_256_ODD_TIMES
    if (stride == C2_ALIGN_ODD(width, 256)) {
        return RK_GRALLOC_USAGE_STRIDE_ALIGN_256_ODD_TIMES;
#ifdef RK_GRALLOC_USAGE_STRIDE_ALIGN_128_ODD_TIMES_PLUS_64
    } else if (stride == (C2_ALIGN_ODD(width, 128) + 64)) {
        return RK_GRALLOC_USAGE_STRIDE_ALIGN_128_ODD_TIMES_PLUS_64;
#endif
    } else if (stride == C2_ALIGN(width, 128)) {
        return  RK_GRALLOC_USAGE_STRIDE_ALIGN_128;
    } else if (stride == C2_ALIGN(width, 64)) {
        return RK_GRALLOC_USAGE_STRIDE_ALIGN_64;
    } else if (stride == C2_ALIGN(width, 16)) {
        return RK_GRALLOC_USAGE_STRIDE_ALIGN_16;
    }
#endif
    return 0;
}

uint64_t C2RKMediaUtils::getHStrideUsage(int32_t height, int32_t hstride) {
#ifdef RK_GRALLOC_USAGE_ALLOC_HEIGHT_ALIGN_64
    if (hstride == C2_ALIGN(height, 64)) {
        return RK_GRALLOC_USAGE_ALLOC_HEIGHT_ALIGN_64;
    } else if (hstride == C2_ALIGN(height, 16)) {
        return  RK_GRALLOC_USAGE_ALLOC_HEIGHT_ALIGN_16;
    } else if (hstride == C2_ALIGN(height, 8)) {
        return RK_GRALLOC_USAGE_ALLOC_HEIGHT_ALIGN_8;
    }
#endif
    return 0;
}

uint32_t C2RKMediaUtils::calculateVideoRefCount(
        MppCodingType type, int32_t width, int32_t height, int32_t level) {
    static const int32_t g264MinRefCount = 4;
    static const int32_t g264MaxRefCount = 16;
    static const int32_t g265MinRefCount = 6;
    static const int32_t g265MaxRefCount = 16;
    static const int32_t gVP9MinRefCount = 5;
    static const int32_t gVP9MaxRefCount = 16;
    static const int32_t gAV1DefRefCount = 10;
    static const int32_t gIepDefRefCount = 5;

    uint32_t maxDpbPixs = 0;
    uint32_t refCount = 0;

    switch (type) {
      case MPP_VIDEO_CodingAVC: {
        // default max Dpb Mbs is level 5.1
        maxDpbPixs = h264LevelInfos[1].maxDpbPixs;
        for (int idx = 0; idx < C2_ARRAY_ELEMS(h264LevelInfos); idx++) {
            if (h264LevelInfos[idx].level == level) {
                maxDpbPixs = h264LevelInfos[idx].maxDpbPixs;
            }
        }
        refCount = maxDpbPixs / (width * height);
        refCount = C2_CLIP(refCount, g264MinRefCount, g264MaxRefCount);
        if (width <= 1920 || height <= 1920) {
            // reserved for deinterlace
            refCount += gIepDefRefCount;
        }
      } break;
      case MPP_VIDEO_CodingHEVC: {
        // default max Dpb Mbs is level 5.1
        maxDpbPixs = h265LevelInfos[1].maxDpbPixs;
        for (int idx = 0; idx < C2_ARRAY_ELEMS(h265LevelInfos); idx++) {
            if (h265LevelInfos[idx].level == level) {
                maxDpbPixs = h265LevelInfos[idx].maxDpbPixs;
            }
        }
        refCount = maxDpbPixs / (width * height);
        refCount = C2_CLIP(refCount, g265MinRefCount, g265MaxRefCount);
      } break;
      case MPP_VIDEO_CodingVP9: {
        // default max Dpb Mbs is level 5.1
        maxDpbPixs = vp9LevelInfos[1].maxDpbPixs;
        for (int idx = 0; idx < C2_ARRAY_ELEMS(vp9LevelInfos); idx++) {
            if (vp9LevelInfos[idx].level == level) {
                maxDpbPixs = vp9LevelInfos[idx].maxDpbPixs;
            }
        }
        refCount = maxDpbPixs / (width * height);
        refCount = C2_CLIP(refCount, gVP9MinRefCount, gVP9MaxRefCount);
      } break;
      case MPP_VIDEO_CodingAV1:
        refCount = gAV1DefRefCount;
        break;
      default: {
        c2_err("use default ref frame count(%d)", C2_DEFAULT_REF_FRAME_COUNT);
        refCount = C2_DEFAULT_REF_FRAME_COUNT;
      }
    }

    return refCount;
}

bool C2RKMediaUtils::isP010Allowed() {
    // The first SDK the device shipped with.
    int32_t productFirstApiLevel = property_get_int32("ro.product.first_api_level", 0);

    // GRF devices (introduced in Android 11) list the first and possibly the current api levels
    // to signal which VSR requirements they conform to even if the first device SDK was higher.
    int32_t boardFirstApiLevel = property_get_int32("ro.board.first_api_level", 0);

    // Some devices that launched prior to Android S may not support P010 correctly, even
    // though they may advertise it as supported.
    if (productFirstApiLevel != 0 && productFirstApiLevel < 31) {
        return false;
    }

    if (boardFirstApiLevel != 0 && boardFirstApiLevel < 31) {
        return false;
    }

    int32_t boardApiLevel = property_get_int32("ro.board.api_level", 0);;
    // For non-GRF devices, use the first SDK version by the product.
    int32_t kFirstApiLevel =
        boardApiLevel != 0 ? boardApiLevel :
        boardFirstApiLevel != 0 ? boardFirstApiLevel :
        productFirstApiLevel;

    return kFirstApiLevel >= 33;
}

void C2RKMediaUtils::convert10BitNV12ToRequestFmt(
        uint32_t dstFormat, uint8_t *dstY, uint8_t *dstUV,
        size_t dstYStride, size_t dstUVStride, uint8_t *src,
        size_t hstride, size_t vstride, size_t width, size_t height) {
    if (dstFormat == HAL_PIXEL_FORMAT_YCBCR_P010) {
        C2RKMediaUtils::convert10BitNV12ToP010(
                dstY, dstUV, dstYStride, dstUVStride,
                src, hstride, vstride, width, height);
    } else {
        C2RKMediaUtils::convert10BitNV12ToNV12(
                dstY, dstUV, dstYStride, dstUVStride,
                src, hstride, vstride, width, height);
    }
}

void C2RKMediaUtils::convert10BitNV12ToP010(
        uint8_t *dstY, uint8_t *dstUV, size_t dstYStride,
        size_t dstUVStride, uint8_t *src, size_t hstride,
        size_t vstride, size_t width, size_t height) {
    uint32_t i, k;
    uint8_t *base_y = src;
    uint8_t *base_uv = src + hstride * vstride;
    for (i = 0; i < height; i++, base_y += hstride, dstY += dstYStride) {
        for (k = 0; k < (width + 7) / 8; k++) {
            uint16_t *pix = (uint16_t *)(dstY + k * 16);
            uint16_t *base_u16 = (uint16_t *)(base_y + k * 10);

            pix[0] =  (base_u16[0] & 0x03FF) << 6;
            pix[1] = ((base_u16[0] & 0xFC00) >> 10 | (base_u16[1] & 0x000F) << 6) << 6;
            pix[2] = ((base_u16[1] & 0x3FF0) >> 4) << 6;
            pix[3] = ((base_u16[1] & 0xC000) >> 14 | (base_u16[2] & 0x00FF) << 2) << 6;
            pix[4] = ((base_u16[2] & 0xFF00) >> 8  | (base_u16[3] & 0x0003) << 8) << 6;
            pix[5] = ((base_u16[3] & 0x0FFC) >> 2) << 6;
            pix[6] = ((base_u16[3] & 0xF000) >> 12 | (base_u16[4] & 0x003F) << 4) << 6;
            pix[7] = ((base_u16[4] & 0xFFC0) >> 6) << 6;
        }
    }
    for (i = 0; i < height / 2; i++, base_uv += hstride, dstUV += dstUVStride) {
        for (k = 0; k < (width + 7) / 8; k++) {
            uint16_t *pix = (uint16_t *)(dstUV + k * 16);
            uint16_t *base_u16 = (uint16_t *)(base_uv + k * 10);

            pix[0] =  (base_u16[0] & 0x03FF) << 6;
            pix[1] = ((base_u16[0] & 0xFC00) >> 10 | (base_u16[1] & 0x000F) << 6) << 6;
            pix[2] = ((base_u16[1] & 0x3FF0) >> 4) << 6;
            pix[3] = ((base_u16[1] & 0xC000) >> 14 | (base_u16[2] & 0x00FF) << 2) << 6;
            pix[4] = ((base_u16[2] & 0xFF00) >> 8  | (base_u16[3] & 0x0003) << 8) << 6;
            pix[5] = ((base_u16[3] & 0x0FFC) >> 2) << 6;
            pix[6] = ((base_u16[3] & 0xF000) >> 12 | (base_u16[4] & 0x003F) << 4) << 6;
            pix[7] = ((base_u16[4] & 0xFFC0) >> 6) << 6;
        }
    }
}

void C2RKMediaUtils::convert10BitNV12ToNV12(
        uint8_t *dstY, uint8_t *dstUV, size_t dstYStride,
        size_t dstUVStride, uint8_t *src, size_t hstride,
        size_t vstride, size_t width, size_t height) {
    uint32_t i, k;
    uint8_t *base_y = src;
    uint8_t *base_uv = src + hstride * vstride;
    for (i = 0; i < height; i++, base_y += hstride, dstY += dstYStride) {
        for (k = 0; k < (width + 7) / 8; k++) {
            uint8_t *pix = (uint8_t *)(dstY + k * 8);
            uint16_t *base_u16 = (uint16_t *)(base_y + k * 10);

            pix[0] = (uint8_t)((base_u16[0] & 0x03FF) >> 2);
            pix[1] = (uint8_t)(((base_u16[0] & 0xFC00) >> 10
                | (base_u16[1] & 0x000F) << 6) >> 2);
            pix[2] = (uint8_t)(((base_u16[1] & 0x3FF0) >> 4) >> 2);
            pix[3] = (uint8_t)(((base_u16[1] & 0xC000) >> 14
                | (base_u16[2] & 0x00FF) << 2) >> 2);
            pix[4] = (uint8_t)(((base_u16[2] & 0xFF00) >> 8
                | (base_u16[3] & 0x0003) << 8) >> 2);
            pix[5] = (uint8_t)(((base_u16[3] & 0x0FFC) >> 2) >> 2);
            pix[6] = (uint8_t)(((base_u16[3] & 0xF000) >> 12
                | (base_u16[4] & 0x003F) << 4) >> 2);
            pix[7] = (uint8_t)(((base_u16[4] & 0xFFC0) >> 6) >> 2);
        }
    }
    for (i = 0; i < height / 2; i++, base_uv += hstride, dstUV += dstUVStride) {
        for (k = 0; k < (width + 7) / 8; k++) {
            uint8_t *pix = (uint8_t *)(dstUV + k * 8);
            uint16_t *base_u16 = (uint16_t *)(base_uv + k * 10);

            pix[0] = (uint8_t)((base_u16[0] & 0x03FF) >> 2);
            pix[1] = (uint8_t)(((base_u16[0] & 0xFC00) >> 10
                | (base_u16[1] & 0x000F) << 6) >> 2);
            pix[2] = (uint8_t)(((base_u16[1] & 0x3FF0) >> 4) >> 2);
            pix[3] = (uint8_t)(((base_u16[1] & 0xC000) >> 14
                | (base_u16[2] & 0x00FF) << 2) >> 2);
            pix[4] = (uint8_t)(((base_u16[2] & 0xFF00) >> 8
                | (base_u16[3] & 0x0003) << 8) >> 2);
            pix[5] = (uint8_t)(((base_u16[3] & 0x0FFC) >> 2) >> 2);
            pix[6] = (uint8_t)(((base_u16[3] & 0xF000) >> 12
                | (base_u16[4] & 0x003F) << 4) >> 2);
            pix[7] = (uint8_t)(((base_u16[4] & 0xFFC0) >> 6) >> 2);
        }
    }
}
