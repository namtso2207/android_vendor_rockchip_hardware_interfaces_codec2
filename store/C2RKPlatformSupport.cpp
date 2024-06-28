/*
 * Copyright (C) 2023 Rockchip Electronics Co. LTD
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

#include <string.h>
#include <media/stagefright/foundation/ALookup.h>

#include "C2RKPlatformSupport.h"
#include "rk_mpi.h"

namespace android {

int32_t getMppCodingFromMime(C2String mime) {
    static const ALookup<C2String, MppCodingType> sMimeCodingMaps = {
        {
            { MEDIA_MIMETYPE_VIDEO_AVC,   MPP_VIDEO_CodingAVC },
            { MEDIA_MIMETYPE_VIDEO_HEVC,  MPP_VIDEO_CodingHEVC },
            { MEDIA_MIMETYPE_VIDEO_VP9,   MPP_VIDEO_CodingVP9 },
            { MEDIA_MIMETYPE_VIDEO_VP8,   MPP_VIDEO_CodingVP8 },
            { MEDIA_MIMETYPE_VIDEO_MPEG2, MPP_VIDEO_CodingMPEG2 },
            { MEDIA_MIMETYPE_VIDEO_MPEG4, MPP_VIDEO_CodingMPEG4 },
            { MEDIA_MIMETYPE_VIDEO_H263,  MPP_VIDEO_CodingH263 },
            { MEDIA_MIMETYPE_VIDEO_AV1,   MPP_VIDEO_CodingAV1 },
        }
    };

    MppCodingType coding = MPP_VIDEO_CodingUnused;
    if (!sMimeCodingMaps.map(mime, &coding)) {
        coding = MPP_VIDEO_CodingUnused;
    }

    return coding;
}

C2RKComponentEntry* GetRKComponentEntry(C2String name) {
    for (int i = 0; i < sComponentMapsSize; ++i) {
        if (!strcasecmp(name.c_str(), sComponentMaps[i].name.c_str())) {
            return &sComponentMaps[i];
        }
    }
    return nullptr;
}

int32_t GetMppCodingFromComponentName(C2String name) {
    for (int i = 0; i < sComponentMapsSize; ++i) {
        if (!strcasecmp(name.c_str(), sComponentMaps[i].name.c_str())) {
            return getMppCodingFromMime(sComponentMaps[i].mime);
        }
    }
    return MPP_VIDEO_CodingUnused;
}

int32_t GetMppCtxTypeFromComponentName(C2String name) {
    for (int i = 0; i < sComponentMapsSize; ++i) {
        if (!strcasecmp(name.c_str(), sComponentMaps[i].name.c_str())) {
            if (sComponentMaps[i].kind == C2Component::KIND_DECODER) {
                return MPP_CTX_DEC;
            } else if (sComponentMaps[i].kind == C2Component::KIND_ENCODER) {
                return MPP_CTX_ENC;
            }
        }
    }
    return -1;
}

}
