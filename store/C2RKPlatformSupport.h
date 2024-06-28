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

#ifndef ANDROID_C2_RK_PLATFORM_SUPPORT_H__
#define ANDROID_C2_RK_PLATFORM_SUPPORT_H__

#include <C2Config.h>
#include <media/stagefright/foundation/MediaDefs.h>

namespace android {

struct C2RKComponentEntry {
    C2String              name;
    C2String              mime;
    C2Component::kind_t   kind;
};

static C2RKComponentEntry sComponentMaps[] = {
    /* Hardware decoder list */
    { "c2.rk.avc.decoder",          MEDIA_MIMETYPE_VIDEO_AVC,   C2Component::KIND_DECODER },
    { "c2.rk.vp9.decoder",          MEDIA_MIMETYPE_VIDEO_VP9,   C2Component::KIND_DECODER },
    { "c2.rk.hevc.decoder",         MEDIA_MIMETYPE_VIDEO_HEVC,  C2Component::KIND_DECODER },
    { "c2.rk.vp8.decoder",          MEDIA_MIMETYPE_VIDEO_VP8,   C2Component::KIND_DECODER },
    { "c2.rk.mpeg2.decoder",        MEDIA_MIMETYPE_VIDEO_MPEG2, C2Component::KIND_DECODER },
    { "c2.rk.m4v.decoder",          MEDIA_MIMETYPE_VIDEO_MPEG4, C2Component::KIND_DECODER },
    { "c2.rk.h263.decoder",         MEDIA_MIMETYPE_VIDEO_H263,  C2Component::KIND_DECODER },
    { "c2.rk.av1.decoder",          MEDIA_MIMETYPE_VIDEO_AV1,   C2Component::KIND_DECODER },
    { "c2.rk.avc.decoder.secure",   MEDIA_MIMETYPE_VIDEO_AVC,   C2Component::KIND_DECODER },
    { "c2.rk.vp9.decoder.secure",   MEDIA_MIMETYPE_VIDEO_VP9,   C2Component::KIND_DECODER },
    { "c2.rk.hevc.decoder.secure",  MEDIA_MIMETYPE_VIDEO_HEVC,  C2Component::KIND_DECODER },
    { "c2.rk.vp8.decoder.secure",   MEDIA_MIMETYPE_VIDEO_VP8,   C2Component::KIND_DECODER },
    { "c2.rk.mpeg2.decoder.secure", MEDIA_MIMETYPE_VIDEO_MPEG2, C2Component::KIND_DECODER },
    { "c2.rk.m4v.decoder.secure",   MEDIA_MIMETYPE_VIDEO_MPEG4, C2Component::KIND_DECODER },
    /* Hardware encoder list */
    { "c2.rk.avc.encoder",          MEDIA_MIMETYPE_VIDEO_AVC,   C2Component::KIND_ENCODER },
    { "c2.rk.hevc.encoder",         MEDIA_MIMETYPE_VIDEO_HEVC,  C2Component::KIND_ENCODER },
    { "c2.rk.vp8.encoder",          MEDIA_MIMETYPE_VIDEO_VP8,   C2Component::KIND_ENCODER },
};

static size_t sComponentMapsSize = sizeof(sComponentMaps) / sizeof(sComponentMaps[0]);

/* C2RKComponent Getter */
C2RKComponentEntry* GetRKComponentEntry(C2String name);

int32_t GetMppCodingFromComponentName(C2String name);
int32_t GetMppCtxTypeFromComponentName(C2String name);

/* Get Rockchip componentStore */
std::shared_ptr<C2ComponentStore> GetCodec2RKComponentStore();

} // namespace android

#endif // ANDROID_C2_RK_PLATFORM_SUPPORT_H__

