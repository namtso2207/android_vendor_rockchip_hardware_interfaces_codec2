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

#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "C2VdecExtendFeature"

#include "C2RKVdecExtendFeature.h"
#include "C2RKLog.h"
#include "C2RKGrallocOps.h"

#include <errno.h>
#include <inttypes.h>

namespace android {

int C2RKVdecExtendFeature::configFrameHdrDynamicMeta(buffer_handle_t hnd, int64_t offset)
{
    int ret = 0;
    int64_t dynamicHdrOffset = offset;

    ret = C2RKGrallocOps::get()->setDynamicHdrMeta(hnd, dynamicHdrOffset);
    if (ret)
        return ret;

    return ret;
}

int C2RKVdecExtendFeature::checkNeedScale(buffer_handle_t hnd) {
    int ret = 0;
    int need = 0;
    uint64_t bufId = 0;
    uint64_t usage = 0;

    metadata_for_rkvdec_scaling_t* metadata = NULL;
    bufId = C2RKGrallocOps::get()->getBufferId(hnd);
    usage = C2RKGrallocOps::get()->getUsage(hnd);
    ret = C2RKGrallocOps::get()->mapScaleMeta(hnd, &metadata);
    if (!ret) {
        /*
         * NOTE: After info change realloc buf, buf has not processed by hwc,
         * metadata->requestMask is default value 0. So we define:
         * requestMask = 1 : need scale
         * requestMask = 2 : no need scale
         * other : keep same as before
         */
        switch (metadata->requestMask) {
        case 1:
            need = 1;
            c2_info("bufId:0x%" PRIx64" hwc need scale", bufId);
            break;
        case 2:
            need = 0;
            c2_info("bufId:0x%" PRIx64" hwc no need scale", bufId);
            break;
        default:
            need = -1;
            break;
        }
        C2RKGrallocOps::get()->unmapScaleMeta(hnd);
    }

    return need;
}

int C2RKVdecExtendFeature::configFrameScaleMeta(
        buffer_handle_t hnd, C2PreScaleParam *scaleParam) {
    int ret = 0;
    metadata_for_rkvdec_scaling_t* metadata = NULL;

    ret = C2RKGrallocOps::get()->mapScaleMeta(hnd, &metadata);
    if (!ret) {
        int32_t thumbWidth     = scaleParam->thumbWidth;
        int32_t thumbHeight    = scaleParam->thumbHeight;
        int32_t thumbHorStride = scaleParam->thumbHorStride;
        uint64_t usage         = 0;

        metadata->replyMask     = 1;
        /*
         * NOTE: keep same with gralloc
         * width = stride, crop real size
         */
        metadata->width         = thumbHorStride;
        metadata->height        = thumbHeight;
        metadata->pixel_stride  = thumbHorStride;
        metadata->format        = scaleParam->format;

        // NV12 8/10 bit nfbc, modifier = 0
        metadata->modifier      = 0;

        metadata->srcLeft       = 0;
        metadata->srcTop        = 0;
        metadata->srcRight      = thumbWidth;
        metadata->srcBottom     = thumbHeight;
        metadata->offset[0]     = scaleParam->yOffset;
        metadata->offset[1]     = scaleParam->uvOffset;
        metadata->byteStride[0] = thumbHorStride;
        metadata->byteStride[1] = thumbHorStride;

        usage = C2RKGrallocOps::get()->getUsage(hnd);
        metadata->usage = (uint32_t)usage;
    }

    ret = C2RKGrallocOps::get()->unmapScaleMeta(hnd);

    return ret;
}

}
