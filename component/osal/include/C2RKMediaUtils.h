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

#ifndef ANDROID_C2_RK_MEDIA_UTILS_H_
#define ANDROID_C2_RK_MEDIA_UTILS_H_

#include "rk_mpi.h"
#include "hardware/hardware_rockchip.h"
#include "hardware/gralloc_rockchip.h"

using namespace android;

#ifndef HAL_PIXEL_FORMAT_YUV420_8BIT_RFBC
#define HAL_PIXEL_FORMAT_YUV420_8BIT_RFBC   0x200
#endif

#ifndef HAL_PIXEL_FORMAT_YUV422_8BIT_RFBC
#define HAL_PIXEL_FORMAT_YUV422_8BIT_RFBC   0x202
#endif

#ifndef HAL_PIXEL_FORMAT_YUV420_10BIT_RFBC
#define HAL_PIXEL_FORMAT_YUV420_10BIT_RFBC  0x201
#endif

#ifndef HAL_PIXEL_FORMAT_YUV422_10BIT_RFBC
#define HAL_PIXEL_FORMAT_YUV422_10BIT_RFBC  0x203
#endif

#ifndef HAL_PIXEL_FORMAT_YUV444_8BIT_RFBC
#define HAL_PIXEL_FORMAT_YUV444_8BIT_RFBC   0x204
#endif

#define C2_DEFAULT_REF_FRAME_COUNT  12
#define C2_MAX_REF_FRAME_COUNT      21

#define C2_ALIGN(x, a)              (((x)+(a)-1)&~((a)-1))
#define C2_IS_ALIGNED(x, a)         (!((x) & ((a)-1)))
#define C2_ALIGN_ODD(x, a)          (((x)+(a)-1)&~((a)-1) | a)
#define C2_CLIP(a, l, h)            ((a) < (l) ? (l) : ((a) > (h) ? (h) : (a)))
#define C2_ARRAY_ELEMS(a)           (sizeof(a) / sizeof((a)[0]))

class C2RKMediaUtils {
public:
    static uint32_t getAndroidColorFmt(uint32_t format, uint32_t fbcMode);
    static uint64_t getStrideUsage(int32_t width, int32_t stride);
    static uint64_t getHStrideUsage(int32_t height, int32_t hstride);
    static uint32_t calculateVideoRefCount(
                MppCodingType type, int32_t width, int32_t height, int32_t level);
    static bool isP010Allowed();
    static void convert10BitNV12ToRequestFmt(
                uint32_t dstFormat, uint8_t *dstY, uint8_t *dstUV,
                size_t dstYStride, size_t dstUVStride, uint8_t *src,
                size_t hstride, size_t vstride, size_t width, size_t height);

private:
    static void convert10BitNV12ToP010(
                uint8_t *dstY, uint8_t *dstUV, size_t dstYStride,
                size_t dstUVStride, uint8_t *src, size_t hstride,
                size_t vstride, size_t width, size_t height);
    static void convert10BitNV12ToNV12(
                uint8_t *dstY, uint8_t *dstUV, size_t dstYStride,
                size_t dstUVStride, uint8_t *src, size_t hstride,
                size_t vstride, size_t width, size_t height);
};

#endif  // ANDROID_C2_RK_MEDIA_UTILS_H_

