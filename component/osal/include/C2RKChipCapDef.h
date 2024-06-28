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

#ifndef C2_RK_CHIP_CAP_DEF_H_
#define C2_RK_CHIP_CAP_DEF_H_

#include <stdio.h>
#include "rk_type.h"

typedef enum _C2ChipType {
    RK_CHIP_UNKOWN = 0,

    // 2928 and 3036 no iep
    RK_CHIP_2928,
    RK_CHIP_3036,

    RK_CHIP_3066,
    RK_CHIP_3188,

    // iep
    RK_CHIP_3368H,
    RK_CHIP_3368A,
    RK_CHIP_3128H,
    RK_CHIP_3128M,
    RK_CHIP_312X,
    RK_CHIP_3326,

    // support 10bit chips
    RK_CHIP_10BIT_SUPPORT_BEGIN,

    // 3288 support max width to 3840
    RK_CHIP_3288,

    // support 4k chips
    RK_CHIP_4K_SUPPORT_BEGIN,
    RK_CHIP_322X_SUPPORT_BEGIN,
    RK_CHIP_3228A,
    RK_CHIP_3228B,
    RK_CHIP_3228H,
    RK_CHIP_3328,
    RK_CHIP_3229,
    RK_CHIP_322X_SUPPORT_END,
    RK_CHIP_3399,
    RK_CHIP_1126,
    RK_CHIP_3562,
    // support 8k chips
    RK_CHIP_8K_SUPPORT_BEGIN,
    RK_CHIP_356X,
    RK_CHIP_3528,
    RK_CHIP_3576,
    RK_CHIP_3588,
    RK_CHIP_8K_SUPPORT_END,

    RK_CHIP_10BIT_SUPPORT_END,

    RK_CHIP_3368,
    RK_CHIP_4K_SUPPORT_END,
} C2ChipType;

typedef enum _C2Cap10bit {
    C2_CAP_10BIT_NONE = 0,   /* unsupport 10bit */
    C2_CAP_10BIT_AVC  = 0x1,
    C2_CAP_10BIT_HEVC = 0x2,
    C2_CAP_10BIT_VP9  = 0x4,
} C2Cap10bit;

typedef enum _C2CompressMode {
    C2_COMPRESS_MODE_NONE = 0,   /* no compress */
    C2_COMPRESS_AFBC_16x16 = 1,
    C2_COMPRESS_RFBC_64x4 = 2,
    C2_COMPRESS_MODE_BUTT
} C2CompressMode;

typedef struct {
    MppCodingType  codecId;
    C2CompressMode fbcMode;

    /* output padding, for setcrop before display */
    uint32_t       offsetX;
    uint32_t       offsetY;
} C2FbcCaps;

typedef enum _C2ScaleMode {
    C2_SCALE_MODE_NONE       = 0,
    C2_SCALE_MODE_META       = 1,      /* output scale meta to the display */
    C2_SCALE_MODE_DOWN_SCALE = 2,      /* output down scale stream directly */
} C2ScaleMode;

typedef struct {
    const char   *chipName;
    C2ChipType    chipType;
    int32_t       fbcCapNum;
    C2FbcCaps    *fbcCaps;
    uint32_t      scaleMode        : 2;
    uint32_t      cap10bit         : 3;
    uint32_t      grallocVersion   : 4;
    uint32_t      hdrMetaCap       : 1;
    uint32_t      reserved         : 22;
} C2ChipCapInfo;

class C2RKChipCapDef {
public:
    static C2RKChipCapDef *get() {
        static C2RKChipCapDef instance;
        return &instance;
    }

    const char* getChipName();
    C2ChipType  getChipType();
    uint32_t    getHdrMetaCap();
    uint32_t    getScaleMode();
    uint32_t    getGrallocVersion();

    uint32_t getFbcOutputMode(MppCodingType codecId);
    uint32_t getFbcMinStride(uint32_t fbcMode);
    uint32_t getFbcOutputOffset(MppCodingType codecId, uint32_t *offsetX, uint32_t *offsetY);

    bool is10bitSupport(MppCodingType codecId);
    bool hasRkVenc();

private:
    C2RKChipCapDef();
    ~C2RKChipCapDef() {};

    C2ChipCapInfo *mChipCapInfo;
};

#endif  // C2_RK_CHIP_CAP_DEF_H_
