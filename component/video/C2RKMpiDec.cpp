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
#define ROCKCHIP_LOG_TAG    "C2RKMpiDec"

#include <C2PlatformSupport.h>
#include <C2AllocatorGralloc.h>
#include <Codec2Mapper.h>
#include <media/stagefright/foundation/ALookup.h>

#include "C2RKMpiDec.h"
#include "C2RKPlatformSupport.h"
#include "C2RKLog.h"
#include "C2RKMediaUtils.h"
#include "C2RKRgaDef.h"
#include "C2RKChipCapDef.h"
#include "C2RKColorAspects.h"
#include "C2RKNaluParser.h"
#include "C2RKVdecExtendFeature.h"
#include "C2RKCodecMapper.h"
#include "C2RKMlvecLegacy.h"
#include "C2RKExtendParameters.h"
#include "C2RKGrallocOps.h"
#include "C2RKMemTrace.h"
#include "C2RKVersion.h"

namespace android {

/* max support video resolution */
constexpr uint32_t kMaxVideoWidth = 8192;
constexpr uint32_t kMaxVideoHeight = 4320;

constexpr size_t kMinInputBufferSize = 2 * 1024 * 1024;

struct MlvecParams {
    std::shared_ptr<C2DriverVersion::output> driverInfo;
    std::shared_ptr<C2LowLatencyMode::output> lowLatencyMode;
};

c2_status_t importGraphicBuffer(const C2Handle *const c2Handle, buffer_handle_t *outHandle) {
    uint32_t bqSlot, width, height, format, stride, generation;
    uint64_t usage, bqId;

    native_handle_t *gHandle = UnwrapNativeCodec2GrallocHandle(c2Handle);

    android::_UnwrapNativeCodec2GrallocMetadata(
            c2Handle, &width, &height, &format, &usage,
            &stride, &generation, &bqId, &bqSlot);

    status_t err = GraphicBufferMapper::get().importBuffer(
            gHandle, width, height, 1, format, usage,
            stride, outHandle);
    if (err != OK) {
        c2_err("failed to import buffer %p", gHandle);
    }

    native_handle_delete(gHandle);
    return (c2_status_t)err;
}

void freeGraphicBuffer(buffer_handle_t outHandle) {
    if (outHandle) {
        GraphicBufferMapper::get().freeBuffer(outHandle);
    }
}

class C2RKMpiDec::IntfImpl : public C2RKInterface<void>::BaseParams {
public:
    explicit IntfImpl(
            const std::shared_ptr<C2ReflectorHelper> &helper,
            C2String name,
            C2Component::kind_t kind,
            C2Component::domain_t domain,
            C2String mediaType)
        : C2RKInterface<void>::BaseParams(helper, name, kind, domain, mediaType) {
        mMlvecParams = std::make_shared<MlvecParams>();

        addParameter(
                DefineParam(mActualOutputDelay, C2_PARAMKEY_OUTPUT_DELAY)
                .withDefault(new C2PortActualDelayTuning::output(C2_MAX_REF_FRAME_COUNT))
                .withFields({C2F(mActualOutputDelay, value).inRange(0, C2_MAX_REF_FRAME_COUNT)})
                .withSetter(Setter<decltype(*mActualOutputDelay)>::StrictValueWithNoDeps)
                .build());

        addParameter(
                DefineParam(mAttrib, C2_PARAMKEY_COMPONENT_ATTRIBUTES)
                .withConstValue(new C2ComponentAttributesSetting(C2Component::ATTRIB_IS_TEMPORAL))
                .build());

        // input picture frame size
        addParameter(
                DefineParam(mSize, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(0u, 320, 240))
                .withFields({
                    C2F(mSize, width).inRange(2, kMaxVideoWidth, 2),
                    C2F(mSize, height).inRange(2, kMaxVideoWidth, 2),
                })
                .withSetter(SizeSetter)
                .build());

        addParameter(
                DefineParam(mMaxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(0u, 320, 240))
                .withFields({
                    C2F(mSize, width).inRange(2, kMaxVideoWidth, 2),
                    C2F(mSize, height).inRange(2, kMaxVideoWidth, 2),
                })
                .withSetter(MaxPictureSizeSetter, mSize)
                .build());

        addParameter(
                DefineParam(mFrameRate, C2_PARAMKEY_FRAME_RATE)
                .withDefault(new C2StreamFrameRateInfo::output(0u, 1.))
                // TODO: More restriction?
                .withFields({C2F(mFrameRate, value).greaterThan(0.)})
                .withSetter(Setter<decltype(*mFrameRate)>::StrictValueWithNoDeps)
                .build());

        addParameter(
                DefineParam(mBlockSize, C2_PARAMKEY_BLOCK_SIZE)
                .withDefault(new C2StreamBlockSizeInfo::output(0u, 320, 240))
                .withFields({
                    C2F(mBlockSize, width).inRange(2, kMaxVideoWidth, 2),
                    C2F(mBlockSize, height).inRange(2, kMaxVideoWidth, 2),
                })
                .withSetter(BlockSizeSetter)
                .build());

        std::vector<uint32_t> pixelFormats = {HAL_PIXEL_FORMAT_YCBCR_420_888};
        if (C2RKMediaUtils::isP010Allowed()) {
            pixelFormats.push_back(HAL_PIXEL_FORMAT_YCBCR_P010);
        }

        // TODO: support more formats?
        addParameter(
                DefineParam(mPixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
                .withDefault(new C2StreamPixelFormatInfo::output(
                                    0u, HAL_PIXEL_FORMAT_YCBCR_420_888))
                .withFields({C2F(mPixelFormat, value).oneOf(pixelFormats)})
                .withSetter((Setter<decltype(*mPixelFormat)>::StrictValueWithNoDeps))
                .build());

        // profile and level
        if (mediaType == MEDIA_MIMETYPE_VIDEO_AVC) {
            std::vector<uint32_t> avcProfiles ={
                                C2Config::PROFILE_AVC_CONSTRAINED_BASELINE,
                                C2Config::PROFILE_AVC_BASELINE,
                                C2Config::PROFILE_AVC_MAIN,
                                C2Config::PROFILE_AVC_CONSTRAINED_HIGH,
                                C2Config::PROFILE_AVC_PROGRESSIVE_HIGH,
                                C2Config::PROFILE_AVC_HIGH};
            if (C2RKChipCapDef::get()->is10bitSupport(MPP_VIDEO_CodingAVC)) {
                avcProfiles.push_back(C2Config::PROFILE_AVC_HIGH_10);
                avcProfiles.push_back(C2Config::PROFILE_AVC_PROGRESSIVE_HIGH_10);
            }
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_AVC_BASELINE, C2Config::LEVEL_AVC_5_1))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf(avcProfiles),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_AVC_1, C2Config::LEVEL_AVC_1B, C2Config::LEVEL_AVC_1_1,
                                C2Config::LEVEL_AVC_1_2, C2Config::LEVEL_AVC_1_3,
                                C2Config::LEVEL_AVC_2, C2Config::LEVEL_AVC_2_1, C2Config::LEVEL_AVC_2_2,
                                C2Config::LEVEL_AVC_3, C2Config::LEVEL_AVC_3_1, C2Config::LEVEL_AVC_3_2,
                                C2Config::LEVEL_AVC_4, C2Config::LEVEL_AVC_4_1, C2Config::LEVEL_AVC_4_2,
                                C2Config::LEVEL_AVC_5, C2Config::LEVEL_AVC_5_1, C2Config::LEVEL_AVC_5_2,
                                C2Config::LEVEL_AVC_6, C2Config::LEVEL_AVC_6_1, C2Config::LEVEL_AVC_6_2})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mediaType == MEDIA_MIMETYPE_VIDEO_HEVC) {
            std::vector<uint32_t> hevcProfiles ={C2Config::PROFILE_HEVC_MAIN};
            if (C2RKChipCapDef::get()->is10bitSupport(MPP_VIDEO_CodingHEVC)) {
                hevcProfiles.push_back(C2Config::PROFILE_HEVC_MAIN_10);
            }
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_HEVC_MAIN, C2Config::LEVEL_HEVC_MAIN_5_1))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf(hevcProfiles),
                        C2F(mProfileLevel, level).oneOf({
                               C2Config::LEVEL_HEVC_MAIN_1,
                               C2Config::LEVEL_HEVC_MAIN_2, C2Config::LEVEL_HEVC_MAIN_2_1,
                               C2Config::LEVEL_HEVC_MAIN_3, C2Config::LEVEL_HEVC_MAIN_3_1,
                               C2Config::LEVEL_HEVC_MAIN_4, C2Config::LEVEL_HEVC_MAIN_4_1,
                               C2Config::LEVEL_HEVC_MAIN_5, C2Config::LEVEL_HEVC_MAIN_5_1,
                               C2Config::LEVEL_HEVC_MAIN_5_2, C2Config::LEVEL_HEVC_MAIN_6,
                               C2Config::LEVEL_HEVC_MAIN_6_1, C2Config::LEVEL_HEVC_MAIN_6_2,
                               C2Config::LEVEL_HEVC_HIGH_4, C2Config::LEVEL_HEVC_HIGH_4_1,
                               C2Config::LEVEL_HEVC_HIGH_5, C2Config::LEVEL_HEVC_HIGH_5_1,
                               C2Config::LEVEL_HEVC_HIGH_5_2, C2Config::LEVEL_HEVC_HIGH_6,
                               C2Config::LEVEL_HEVC_HIGH_6_1, C2Config::LEVEL_HEVC_HIGH_6_2})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mediaType == MEDIA_MIMETYPE_VIDEO_MPEG2) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_MP2V_SIMPLE, C2Config::LEVEL_MP2V_HIGH))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_MP2V_SIMPLE,
                                C2Config::PROFILE_MP2V_MAIN}),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_MP2V_LOW,
                                C2Config::LEVEL_MP2V_MAIN,
                                C2Config::LEVEL_MP2V_HIGH_1440,
                                C2Config::LEVEL_MP2V_HIGH})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mediaType == MEDIA_MIMETYPE_VIDEO_MPEG4) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_MP4V_SIMPLE, C2Config::LEVEL_MP4V_3))
                   .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_MP4V_SIMPLE}),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_MP4V_0,
                                C2Config::LEVEL_MP4V_0B,
                                C2Config::LEVEL_MP4V_1,
                                C2Config::LEVEL_MP4V_2,
                                C2Config::LEVEL_MP4V_3})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mediaType == MEDIA_MIMETYPE_VIDEO_H263) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_H263_BASELINE, C2Config::LEVEL_H263_30))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_H263_BASELINE,
                                C2Config::PROFILE_H263_ISWV2}),
                       C2F(mProfileLevel, level).oneOf({
                               C2Config::LEVEL_H263_10,
                               C2Config::LEVEL_H263_20,
                               C2Config::LEVEL_H263_30,
                               C2Config::LEVEL_H263_40,
                               C2Config::LEVEL_H263_45})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mediaType == MEDIA_MIMETYPE_VIDEO_VP9) {
            std::vector<uint32_t> vp9Profiles ={C2Config::PROFILE_VP9_0};
            if (C2RKChipCapDef::get()->is10bitSupport(MPP_VIDEO_CodingVP9)) {
                vp9Profiles.push_back(C2Config::PROFILE_VP9_2);
            }
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_VP9_0, C2Config::LEVEL_VP9_5))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf(vp9Profiles),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_VP9_1,
                                C2Config::LEVEL_VP9_1_1,
                                C2Config::LEVEL_VP9_2,
                                C2Config::LEVEL_VP9_2_1,
                                C2Config::LEVEL_VP9_3,
                                C2Config::LEVEL_VP9_3_1,
                                C2Config::LEVEL_VP9_4,
                                C2Config::LEVEL_VP9_4_1,
                                C2Config::LEVEL_VP9_5,
                                C2Config::LEVEL_VP9_5_1,
                                C2Config::LEVEL_VP9_5_2,
                                C2Config::LEVEL_VP9_6,
                                C2Config::LEVEL_VP9_6_1,
                                C2Config::LEVEL_VP9_6_2})
                     })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mediaType == MEDIA_MIMETYPE_VIDEO_AV1) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_AV1_0, C2Config::LEVEL_AV1_6_3))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_AV1_0,
                                C2Config::PROFILE_AV1_1}),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_AV1_2, C2Config::LEVEL_AV1_2_1, C2Config::LEVEL_AV1_2_2,
                                C2Config::LEVEL_AV1_2_3, C2Config::LEVEL_AV1_3, C2Config::LEVEL_AV1_3_1,
                                C2Config::LEVEL_AV1_3_2, C2Config::LEVEL_AV1_3_3, C2Config::LEVEL_AV1_4,
                                C2Config::LEVEL_AV1_4_1, C2Config::LEVEL_AV1_4_2, C2Config::LEVEL_AV1_4_3,
                                C2Config::LEVEL_AV1_5, C2Config::LEVEL_AV1_5_1, C2Config::LEVEL_AV1_5_2,
                                C2Config::LEVEL_AV1_5_3, C2Config::LEVEL_AV1_6, C2Config::LEVEL_AV1_6_1,
                                C2Config::LEVEL_AV1_6_2, C2Config::LEVEL_AV1_6_3})
                     })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        }

        // max input buffer size
        addParameter(
                DefineParam(mMaxInputSize, C2_PARAMKEY_INPUT_MAX_BUFFER_SIZE)
                .withDefault(new C2StreamMaxBufferSizeInfo::input(0u, kMinInputBufferSize))
                .withFields({
                    C2F(mMaxInputSize, value).any(),
                })
                .calculatedAs(MaxInputSizeSetter, mMaxSize)
                .build());

        // ColorInfo
        C2ChromaOffsetStruct locations[1] = { C2ChromaOffsetStruct::ITU_YUV_420_0() };
        std::shared_ptr<C2StreamColorInfo::output> defaultColorInfo =
            C2StreamColorInfo::output::AllocShared(
                    1u, 0u, 8u /* bitDepth */, C2Color::YUV_420);
        memcpy(defaultColorInfo->m.locations, locations, sizeof(locations));

        defaultColorInfo =
            C2StreamColorInfo::output::AllocShared(
                   { C2ChromaOffsetStruct::ITU_YUV_420_0() },
                   0u, 8u /* bitDepth */, C2Color::YUV_420);
        helper->addStructDescriptors<C2ChromaOffsetStruct>();

        addParameter(
                DefineParam(mColorInfo, C2_PARAMKEY_CODED_COLOR_INFO)
                .withConstValue(defaultColorInfo)
                .build());

        // colorAspects
        addParameter(
                DefineParam(mDefaultColorAspects, C2_PARAMKEY_DEFAULT_COLOR_ASPECTS)
                .withDefault(new C2StreamColorAspectsTuning::output(
                        0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                        C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                .withFields({
                    C2F(mDefaultColorAspects, range).inRange(
                            C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                    C2F(mDefaultColorAspects, primaries).inRange(
                            C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                    C2F(mDefaultColorAspects, transfer).inRange(
                            C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                    C2F(mDefaultColorAspects, matrix).inRange(
                            C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                })
                .withSetter(DefaultColorAspectsSetter)
                .build());

        // vui colorAspects
        if (mediaType == MEDIA_MIMETYPE_VIDEO_AVC ||
            mediaType == MEDIA_MIMETYPE_VIDEO_HEVC ||
            mediaType == MEDIA_MIMETYPE_VIDEO_AV1 ||
            mediaType == MEDIA_MIMETYPE_VIDEO_MPEG2) {
            addParameter(
                    DefineParam(mCodedColorAspects, C2_PARAMKEY_VUI_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsInfo::input(
                            0u, C2Color::RANGE_LIMITED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields({
                        C2F(mCodedColorAspects, range).inRange(
                                C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                        C2F(mCodedColorAspects, primaries).inRange(
                                C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                        C2F(mCodedColorAspects, transfer).inRange(
                                C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                        C2F(mCodedColorAspects, matrix).inRange(
                                C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                    })
                    .withSetter(CodedColorAspectsSetter)
                    .build());

           addParameter(
                    DefineParam(mColorAspects, C2_PARAMKEY_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsInfo::output(
                            0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields({
                        C2F(mColorAspects, range).inRange(
                                C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                        C2F(mColorAspects, primaries).inRange(
                                C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                        C2F(mColorAspects, transfer).inRange(
                                C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                        C2F(mColorAspects, matrix).inRange(
                                C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                    })
                    .withSetter(ColorAspectsSetter, mDefaultColorAspects, mCodedColorAspects)
                    .build());

            addParameter(
                    DefineParam(mLowLatency, C2_PARAMKEY_LOW_LATENCY_MODE)
                    .withDefault(new C2GlobalLowLatencyModeTuning(false))
                    .withFields({C2F(mLowLatency, value)})
                    .withSetter(Setter<decltype(*mLowLatency)>::NonStrictValueWithNoDeps)
                    .build());

            /* extend parameter definition */
            addParameter(
                    DefineParam(mMlvecParams->driverInfo, C2_PARAMKEY_MLVEC_DEC_DRI_VERSION)
                    .withConstValue(new C2DriverVersion::output(MLVEC_DRIVER_VERSION))
                    .build());

            addParameter(
                    DefineParam(mMlvecParams->lowLatencyMode, C2_PARAMKEY_MLVEC_DEC_LOW_LATENCY_MODE)
                    .withDefault(new C2LowLatencyMode::output(0))
                    .withFields({
                        C2F(mMlvecParams->lowLatencyMode, enable).any(),
                    })
                    .withSetter(MLowLatenctyModeSetter)
                    .build());
        }
    }

    static C2R SizeSetter(bool mayBlock, const C2P<C2StreamPictureSizeInfo::output> &oldMe,
                          C2P<C2StreamPictureSizeInfo::output> &me) {
        (void)mayBlock;
        C2R res = C2R::Ok();
        if (!me.F(me.v.width).supportsAtAll(me.v.width)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width)));
            me.set().width = oldMe.v.width;
        }
        if (!me.F(me.v.height).supportsAtAll(me.v.height)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height)));
            me.set().height = oldMe.v.height;
        }
        if (me.set().width * me.set().height > kMaxVideoWidth * kMaxVideoHeight) {
            c2_warn("max support video resolution %dx%d, cur %dx%d",
                    kMaxVideoWidth, kMaxVideoHeight, me.set().width, me.set().height);
        }
        return res;
    }

    static C2R MaxPictureSizeSetter(bool mayBlock, C2P<C2StreamMaxPictureSizeTuning::output> &me,
                                    const C2P<C2StreamPictureSizeInfo::output> &size) {
        (void)mayBlock;
        // TODO: get max width/height from the size's field helpers vs. hardcoding
        me.set().width = c2_min(c2_max(me.v.width, size.v.width), kMaxVideoWidth);
        me.set().height = c2_min(c2_max(me.v.height, size.v.height), kMaxVideoWidth);
        if (me.set().width * me.set().height > kMaxVideoWidth * kMaxVideoHeight) {
            c2_warn("max support video resolution %dx%d, cur %dx%d",
                    kMaxVideoWidth, kMaxVideoHeight, me.set().width, me.set().height);
        }
        return C2R::Ok();
    }

    static C2R BlockSizeSetter(bool mayBlock, const C2P<C2StreamBlockSizeInfo::output> &oldMe,
                          C2P<C2StreamBlockSizeInfo::output> &me) {
        (void)mayBlock;
        C2R res = C2R::Ok();
        if (!me.F(me.v.width).supportsAtAll(me.v.width)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width)));
            me.set().width = oldMe.v.width;
        }
        if (!me.F(me.v.height).supportsAtAll(me.v.height)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height)));
            me.set().height = oldMe.v.height;
        }
        return res;
    }

    static C2R ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::input> &me,
                                  const C2P<C2StreamPictureSizeInfo::output> &size) {
        (void)mayBlock;
        (void)size;
        (void)me;  // TODO: validate
        return C2R::Ok();
    }

    static C2R MaxInputSizeSetter(bool mayBlock, C2P<C2StreamMaxBufferSizeInfo::input> &me,
                                const C2P<C2StreamMaxPictureSizeTuning::output> &maxSize) {
        (void)mayBlock;
        // assume compression ratio of 2
        me.set().value = c2_max((((maxSize.v.width + 63) / 64)
                * ((maxSize.v.height + 63) / 64) * 3072), kMinInputBufferSize);
        return C2R::Ok();
    }


    static C2R DefaultColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsTuning::output> &me) {
        (void)mayBlock;
        if (me.v.range > C2Color::RANGE_OTHER) {
            me.set().range = C2Color::RANGE_OTHER;
        }
        if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
            me.set().primaries = C2Color::PRIMARIES_OTHER;
        }
        if (me.v.transfer > C2Color::TRANSFER_OTHER) {
            me.set().transfer = C2Color::TRANSFER_OTHER;
        }
        if (me.v.matrix > C2Color::MATRIX_OTHER) {
            me.set().matrix = C2Color::MATRIX_OTHER;
        }
        return C2R::Ok();
    }

    static C2R CodedColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::input> &me) {
        (void)mayBlock;
        if (me.v.range > C2Color::RANGE_OTHER) {
            me.set().range = C2Color::RANGE_OTHER;
        }
        if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
            me.set().primaries = C2Color::PRIMARIES_OTHER;
        }
        if (me.v.transfer > C2Color::TRANSFER_OTHER) {
            me.set().transfer = C2Color::TRANSFER_OTHER;
        }
        if (me.v.matrix > C2Color::MATRIX_OTHER) {
            me.set().matrix = C2Color::MATRIX_OTHER;
        }
        return C2R::Ok();
    }

    static C2R ColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::output> &me,
                                  const C2P<C2StreamColorAspectsTuning::output> &def,
                                  const C2P<C2StreamColorAspectsInfo::input> &coded) {
        (void)mayBlock;
        // take default values for all unspecified fields, and coded values for specified ones
        me.set().range = coded.v.range == RANGE_UNSPECIFIED ? def.v.range : coded.v.range;
        me.set().primaries = coded.v.primaries == PRIMARIES_UNSPECIFIED
                ? def.v.primaries : coded.v.primaries;
        me.set().transfer = coded.v.transfer == TRANSFER_UNSPECIFIED
                ? def.v.transfer : coded.v.transfer;
        me.set().matrix = coded.v.matrix == MATRIX_UNSPECIFIED ? def.v.matrix : coded.v.matrix;
        return C2R::Ok();
    }

    static C2R MLowLatenctyModeSetter(
        bool mayBlock, C2P<C2LowLatencyMode::output> &me) {
        (void)mayBlock;
        (void)me;
        return C2R::Ok();
    }

    std::shared_ptr<C2StreamPictureSizeInfo::output> getSize_l() {
        return mSize;
    }

    std::shared_ptr<C2StreamFrameRateInfo::output> getFrameRate_l() {
        return mFrameRate;
    }

    std::shared_ptr<C2StreamColorAspectsInfo::output> getColorAspects_l() {
        return mColorAspects;
    }

    std::shared_ptr<C2StreamColorAspectsTuning::output> getDefaultColorAspects_l() {
        return mDefaultColorAspects;
    }

    std::shared_ptr<C2GlobalLowLatencyModeTuning> getLowLatency_l() {
        return mLowLatency;
    }

    std::shared_ptr<C2StreamProfileLevelInfo::input> getProfileLevel_l() {
        return mProfileLevel;
    }

    std::shared_ptr<C2StreamPixelFormatInfo::output> getPixelFormat_l() const {
        return mPixelFormat;
    }

    bool getIsLowLatencyMode() {
        if (mLowLatency && mLowLatency->value > 0) {
            return true;
        }
        if (mMlvecParams->lowLatencyMode
                && mMlvecParams->lowLatencyMode->enable != 0) {
            return true;
        }
        return false;
    }

    bool getIs10bitVideo() {
        uint32_t profile = mProfileLevel ? mProfileLevel->profile : 0;
        if (profile == PROFILE_AVC_HIGH_10 ||
            profile == PROFILE_HEVC_MAIN_10 ||
            profile == PROFILE_VP9_2) {
            return true;
        }
        if (mDefaultColorAspects->transfer == 6 /* SMPTEST2084 */) {
            return true;
        }
        return false;
    }

private:
    std::shared_ptr<C2StreamPictureSizeInfo::output> mSize;
    std::shared_ptr<C2StreamMaxPictureSizeTuning::output> mMaxSize;
    std::shared_ptr<C2StreamFrameRateInfo::output> mFrameRate;
    std::shared_ptr<C2StreamBlockSizeInfo::output> mBlockSize;
    std::shared_ptr<C2StreamPixelFormatInfo::output> mPixelFormat;
    std::shared_ptr<C2StreamProfileLevelInfo::input> mProfileLevel;
    std::shared_ptr<C2StreamMaxBufferSizeInfo::input> mMaxInputSize;
    std::shared_ptr<C2StreamColorInfo::output> mColorInfo;
    std::shared_ptr<C2StreamColorAspectsTuning::output> mDefaultColorAspects;
    std::shared_ptr<C2StreamColorAspectsInfo::input> mCodedColorAspects;
    std::shared_ptr<C2StreamColorAspectsInfo::output> mColorAspects;
    std::shared_ptr<C2GlobalLowLatencyModeTuning> mLowLatency;
    std::shared_ptr<MlvecParams> mMlvecParams;
};

static int32_t frameReadyCb(void *ctx, void *mppCtx, int32_t cmd, void *frame) {
    (void)mppCtx;
    (void)cmd;
    (void)frame;
    C2RKMpiDec *decoder = (C2RKMpiDec *)ctx;
    decoder->postFrameReady();
    return 0;
}

void C2RKMpiDec::WorkHandler::waitAllMsgFlushed() {
    sp<AMessage> msg = new AMessage(WorkHandler::kWhatFlushMessage, this);

    sp<AMessage> response;
    msg->postAndAwaitResponse(&response);
}

void C2RKMpiDec::WorkHandler::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatFrameReady: {
            C2RKMpiDec *thiz = nullptr;
            if (msg->findPointer("thiz", (void **)(&thiz)) && thiz) {
                thiz->onFrameReady();
            }
        } break;
        case kWhatFlushMessage: {
            sp<AReplyToken> replyID;
            msg->senderAwaitsResponse(&replyID);

            sp<AMessage> response = new AMessage;
            response->postReply(replyID);
        } break;
        default: {
            c2_err("Unrecognized msg: %d", msg->what());
        } break;
    }
}

C2RKMpiDec::C2RKMpiDec(
        const char *name,
        const char *mime,
        c2_node_id_t id,
        const std::shared_ptr<IntfImpl> &intfImpl)
    : C2RKComponent(std::make_shared<C2RKInterface<IntfImpl>>(name, id, intfImpl)),
      mName(name),
      mMime(mime),
      mIntf(intfImpl),
      mDump(new C2RKDump),
      mLooper(nullptr),
      mHandler(nullptr),
      mMppCtx(nullptr),
      mMppMpi(nullptr),
      mCodingType(MPP_VIDEO_CodingUnused),
      mColorFormat(MPP_FMT_YUV420SP),
      mFrmGrp(nullptr),
      mWidth(0),
      mHeight(0),
      mHorStride(0),
      mVerStride(0),
      mGrallocVersion(C2RKChipCapDef::get()->getGrallocVersion()),
      mScaleMode(0),
      mStarted(false),
      mFlushed(true),
      mSignalledInputEos(false),
      mOutputEos(false),
      mSignalledError(false),
      mLowLatencyMode(false),
      mIsGBSource(false),
      mHdrMetaEnabled(false),
      mBufferMode(false) {
    c2_info("[%s] version %s", name, C2_COMPONENT_FULL_VERSION);
    mCodingType = (MppCodingType)GetMppCodingFromComponentName(name);
    if (mCodingType == MPP_VIDEO_CodingUnused) {
        c2_err("failed to get coding from name %s", name);
    }
}

C2RKMpiDec::~C2RKMpiDec() {
    onRelease();

    C2RKMemTrace::get()->removeVideoNode(this);
    C2RKMemTrace::get()->dumpAllNode();
}

c2_status_t C2RKMpiDec::onInit() {
    c2_log_func_enter();

    C2RKMemTrace::C2NodeInfo node {
        .client = this,
        .name   = mName,
        .mime   = mMime,
        .type   = C2RKMemTrace::C2_TRACE_DECODER,
        .width  = mIntf->getSize_l()->width,
        .height = mIntf->getSize_l()->height,
        .frameRate = mIntf->getFrameRate_l()->value
    };
    if (!C2RKMemTrace::get()->tryAddVideoNode(node)) {
        C2RKMemTrace::get()->dumpAllNode();
        return C2_NO_MEMORY;
    }

    c2_status_t err = updateOutputDelay();
    if (err != C2_OK) {
        c2_err("failed to update output delay");
    }

    err = setupAndStartLooper();
    if (err != C2_OK) {
        c2_err("failed to start looper therad");
    }

    return err;
}

c2_status_t C2RKMpiDec::onStop() {
    c2_log_func_enter();
    if (!mFlushed) {
        return onFlush_sm();
    }

    return C2_OK;
}

void C2RKMpiDec::onReset() {
    c2_log_func_enter();
    onStop();
}

void C2RKMpiDec::onRelease() {
    c2_log_func_enter();

    if (!mStarted)
        return;

    if (!mFlushed) {
        onFlush_sm();
    }

    if (mBlockPool) {
        mBlockPool.reset();
    }

    if (mOutBlock) {
        mOutBlock.reset();
    }

    if (mFrmGrp != nullptr) {
        mpp_buffer_group_put(mFrmGrp);
        mFrmGrp = nullptr;
    }

    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = nullptr;
    }

    if (mDump != nullptr) {
        delete mDump;
        mDump = nullptr;
    }

    stopAndReleaseLooper();

    mStarted = false;
}

c2_status_t C2RKMpiDec::onFlush_sm() {
    if (!mFlushed) {
        c2_log_func_enter();
        mSignalledInputEos = false;
        mOutputEos = false;
        mSignalledError = false;

        if (mMppMpi) {
            mMppMpi->reset(mMppCtx);
        }

        if (mHandler) {
            mHandler->waitAllMsgFlushed();
        }

        Mutex::Autolock autoLock(mBufferLock);
        clearOutBuffers();
        mpp_buffer_group_clear(mFrmGrp);

        mFlushed = true;
    }

    return C2_OK;
}

c2_status_t C2RKMpiDec::setupAndStartLooper() {
    status_t err = OK;
    if (mLooper == nullptr) {
        status_t err = OK;
        mLooper = new ALooper;
        mHandler = new WorkHandler;

        mLooper->setName("C2DecLooper");
        err = mLooper->start();
        if (err == OK) {
            mLooper->registerHandler(mHandler);
        }
    }
    return (c2_status_t)err;
}

void C2RKMpiDec::stopAndReleaseLooper() {
    if (mLooper != nullptr) {
        if (mHandler != nullptr) {
            mLooper->unregisterHandler(mHandler->id());
            mHandler.clear();
        }
        mLooper->stop();
        mLooper.clear();
    }
}

uint32_t C2RKMpiDec::getFbcOutputMode(const std::unique_ptr<C2Work> &work) {
    uint32_t fbcMode = C2RKChipCapDef::get()->getFbcOutputMode(mCodingType);

    if (!fbcMode || mIsGBSource || mBufferMode) {
        return 0;
    }

    if (fbcMode == C2_COMPRESS_AFBC_16x16) {
        if (MPP_FRAME_FMT_IS_YUV_10BIT(mColorFormat)) {
            c2_info("10bit video source, perfer afbc output mode");
            return fbcMode;
        }

        /*
         * For 10bit flim source, we agreed that force fbc rendering.
         * Some apps(Kodi/Photos/Files) does not transmit profile info, so do extra
         * detection from spspps to search bitInfo in this case.
         */
        if (work != nullptr && work->input.flags & C2FrameData::FLAG_CODEC_CONFIG) {
            if (!work->input.buffers.empty()) {
                C2ReadView rView = mDummyReadView;
                rView = work->input.buffers[0]->data().linearBlocks().front().map().get();
                int32_t depth = C2RKNaluParser::detectBitDepth(
                        const_cast<uint8_t *>(rView.data()), rView.capacity(), mCodingType);
                if (depth == 10) {
                    c2_info("10bit video profile detached, prefer afbc output mode");
                    return fbcMode;
                }
            }
        }
    }

    uint32_t minStride = C2RKChipCapDef::get()->getFbcMinStride(fbcMode);

    if (mWidth <= minStride && mHeight <= minStride) {
        c2_info("within fbc min stirde %d, disable fbc otuput mode", minStride);
        return 0;
    }

    return fbcMode;
}

c2_status_t C2RKMpiDec::updateOutputDelay() {
    c2_status_t err = C2_OK;
    uint32_t width = 0, height = 0;
    uint32_t level = 0, refCnt = 0;

    {
        IntfImpl::Lock lock = mIntf->lock();
        width  = mIntf->getSize_l()->width;
        height = mIntf->getSize_l()->height;
        level  = mIntf->getProfileLevel_l() ? mIntf->getProfileLevel_l()->level : 0;
    }

    refCnt = C2RKMediaUtils::calculateVideoRefCount(mCodingType, width, height, level);

    c2_info("Codec(%s %dx%d) level(%d) needs %d reference frames",
            toStr_Coding(mCodingType), width, height, level, refCnt);

    C2PortActualDelayTuning::output delayTuning(refCnt);
    std::vector<std::unique_ptr<C2SettingResult>> failures;

    err = mIntf->config({&delayTuning}, C2_MAY_BLOCK, &failures);
    if (err != C2_OK) {
        c2_err("failed to config delay tuning, err %d", err);
    }

    return err;
}

c2_status_t C2RKMpiDec::updateSurfaceConfig(const std::shared_ptr<C2BlockPool> &pool) {
    c2_status_t err = C2_OK;
    std::shared_ptr<C2GraphicBlock> block;

    // alloc a temporary graphicBuffer to get surface features.
    err = pool->fetchGraphicBlock(
            176, 144, HAL_PIXEL_FORMAT_YCrCb_NV12,
            {C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE}, &block);
    if (err != C2_OK) {
        c2_err("failed to fetchGraphicBlock, err %d", err);
        return err;
    }

    uint64_t usage = 0;
    buffer_handle_t handle = nullptr;
    auto c2Handle = block->handle();

    err = importGraphicBuffer(c2Handle, &handle);
    if (err != C2_OK) {
        c2_err("failed to import graphic buffer");
        return err;
    }

    usage = C2RKGrallocOps::get()->getUsage(handle);
    if (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
        mIsGBSource = true;
        updateFbcModeIfNeeded();
    }

    MPP_RET ret = MPP_OK;
    MppDecCfg cfg = nullptr;
    uint32_t scaleMode = C2RKChipCapDef::get()->getScaleMode();

    // enable scale dec only in 8k
    if (!scaleMode || (mWidth <= 4096 && mHeight <= 4096)) {
        goto cleanUp;
    }

    if (scaleMode == C2_SCALE_MODE_META
            && C2RKVdecExtendFeature::checkNeedScale(handle) != 1) {
        goto cleanUp;
    }

    mpp_dec_cfg_init(&cfg);

    mMppMpi->control(mMppCtx, MPP_DEC_GET_CFG, cfg);
    mpp_dec_cfg_set_u32(cfg, "base:enable_thumbnail", scaleMode);
    ret = mMppMpi->control(mMppCtx, MPP_DEC_SET_CFG, cfg);
    if (ret != MPP_OK) {
        c2_warn("failed to set scale mode %d", scaleMode);
        goto cleanUp;
    }

    c2_info("enable scale dec, mode %d", scaleMode);

    // In down-scaling mode, it is necessary to update surface info using
    // down-scaling config, so request thumbnail info from decoder first.
    if (scaleMode == C2_SCALE_MODE_DOWN_SCALE) {
        MppFrame frame  = nullptr;

        mpp_frame_init(&frame);
        mpp_frame_set_width(frame, mWidth);
        mpp_frame_set_height(frame, mHeight);
        mpp_frame_set_hor_stride(frame, mHorStride);
        mpp_frame_set_ver_stride(frame, mVerStride);
        mpp_frame_set_fmt(frame, mColorFormat);

        ret = mMppMpi->control(
                mMppCtx, MPP_DEC_GET_THUMBNAIL_FRAME_INFO, (MppParam)frame);
        if (ret == MPP_OK) {
            mScaleInfo.width   = mpp_frame_get_width(frame);
            mScaleInfo.height  = mpp_frame_get_height(frame);
            mScaleInfo.hstride = mpp_frame_get_hor_stride(frame);
            mScaleInfo.vstride = mpp_frame_get_ver_stride(frame);
            mScaleInfo.format  = mpp_frame_get_fmt(frame);
            mFbcCfg.mode = MPP_FRAME_FMT_IS_FBC(mScaleInfo.format);
            mScaleMode = scaleMode;
            c2_info("update down-scaling config: w %d h %d hor %d ver %d fmt %x",
                    mScaleInfo.width, mScaleInfo.height, mScaleInfo.hstride,
                    mScaleInfo.vstride, mScaleInfo.format);
        } else {
            c2_err("failed to get down-scaling thumnail info, err %d", ret);
        }
        mpp_frame_deinit(&frame);
    } else {
        mScaleMode = scaleMode;
    }

cleanUp:
    if (cfg != nullptr) {
        mpp_dec_cfg_deinit(cfg);
    }

    freeGraphicBuffer(handle);

    return err;
}

c2_status_t C2RKMpiDec::initDecoder(const std::unique_ptr<C2Work> &work) {
    MPP_RET err = MPP_OK;

    c2_log_func_enter();

    {
        IntfImpl::Lock lock = mIntf->lock();
        mWidth = mIntf->getSize_l()->width;
        mHeight = mIntf->getSize_l()->height;
        mPixelFormat = mIntf->getPixelFormat_l()->value;
        mLowLatencyMode = mIntf->getIsLowLatencyMode();
        mColorFormat = mIntf->getIs10bitVideo() ? MPP_FMT_YUV420SP_10BIT : MPP_FMT_YUV420SP;
    }

    c2_info("azg test init: w %d h %d coding %s", mWidth, mHeight, toStr_Coding(mCodingType));

    err = mpp_create(&mMppCtx, &mMppMpi);
    if (err != MPP_OK) {
        c2_err("failed to mpp_create, ret %d", err);
        goto error;
    }

    // TODO: workround: CTS-CodecDecoderTest
    // testFlushNative[15(c2.rk.mpeg2.decoder_video/mpeg2)
    if (mCodingType == MPP_VIDEO_CodingMPEG2) {
        uint32_t mode = 0, split = 1;
        mMppMpi->control(mMppCtx, MPP_DEC_SET_ENABLE_DEINTERLACE, &mode);
        mMppMpi->control(mMppCtx, MPP_DEC_SET_PARSER_SPLIT_MODE, &split);
    } else {
        // enable deinterlace, but not decting
        uint32_t mode = 1;
        mMppMpi->control(mMppCtx, MPP_DEC_SET_ENABLE_DEINTERLACE, &mode);
    }

    {
        uint32_t fastParse = 1;
        mMppMpi->control(mMppCtx, MPP_DEC_SET_PARSER_FAST_MODE, &fastParse);

        uint32_t fastPlay = 1;
        mMppMpi->control(mMppCtx, MPP_DEC_SET_ENABLE_FAST_PLAY, &fastPlay);

        if (mLowLatencyMode) {
            uint32_t fastOut = 1;
            mMppMpi->control(mMppCtx, MPP_DEC_SET_IMMEDIATE_OUT, &fastOut);
            c2_info("enable lowLatency, enable mpp fast-out mode");
        }
    }

    err = mpp_init(mMppCtx, MPP_CTX_DEC, mCodingType);
    if (err != MPP_OK) {
        c2_err("failed to mpp_init, err %d", err);
        goto error;
    }

    {
        MppFrame frame  = nullptr;

        // av1 support convert to user-set format internally.
        if (mCodingType == MPP_VIDEO_CodingAV1
                && mPixelFormat == HAL_PIXEL_FORMAT_YCBCR_P010) {
            mColorFormat = MPP_FMT_YUV420SP_10BIT;
        }

        // P010 is different with decoding output compact 10bit, so reset to
        // output buffer mode and do one more extry copy to format P010.
        if (mColorFormat == MPP_FMT_YUV420SP_10BIT) {
            if (mPixelFormat == HAL_PIXEL_FORMAT_YCBCR_P010) {
                c2_warn("got p010 format request, use output buffer mode.");
                mBufferMode = true;
            }
            if (mWidth * mHeight <= 176 * 144) {
                mBufferMode = true;
            }
        }

        uint32_t mppFmt = mColorFormat;
        uint32_t fbcMode = getFbcOutputMode(work);
        if (fbcMode) {
            mppFmt |= MPP_FRAME_FBC_AFBC_V2;
            /* fbc decode output has padding inside, set crop before display */
            C2RKChipCapDef::get()->getFbcOutputOffset(
                    mCodingType, &mFbcCfg.paddingX, &mFbcCfg.paddingY);
            c2_info("use mpp fbc output mode, padding offset(%d, %d)",
                    mFbcCfg.paddingX, mFbcCfg.paddingY);
        }

        mMppMpi->control(mMppCtx, MPP_DEC_SET_OUTPUT_FORMAT, (MppParam)&mppFmt);

        mpp_frame_init(&frame);
        mpp_frame_set_width(frame, mWidth);
        mpp_frame_set_height(frame, mHeight);
        mpp_frame_set_fmt(frame, (MppFrameFormat)mppFmt);

        err = mMppMpi->control(mMppCtx, MPP_DEC_SET_FRAME_INFO, (MppParam)frame);
        if (err != MPP_OK) {
            c2_err("failed to set frame info, err %d", err);
            mpp_frame_deinit(&frame);
            goto error;
        }

        mHorStride   = mpp_frame_get_hor_stride(frame);
        mVerStride   = mpp_frame_get_ver_stride(frame);
        mColorFormat = mpp_frame_get_fmt(frame);
        mFbcCfg.mode = fbcMode;

        mpp_frame_deinit(&frame);

        c2_info("init: hor %d ver %d color 0x%08x", mHorStride, mVerStride, mColorFormat);
    }

    err = mpp_buffer_group_get_external(&mFrmGrp, MPP_BUFFER_TYPE_ION);
    if (err != MPP_OK) {
        c2_err("failed to get buffer_group, err %d", err);
        goto error;
    }

    mMppMpi->control(mMppCtx, MPP_DEC_SET_EXT_BUF_GROUP, mFrmGrp);
    if (err != MPP_OK) {
        c2_err("failed to set buffer group, err %d", err);
        goto error;
    }

    {
        /* set output frame callback  */
        MppDecCfg cfg;
        mpp_dec_cfg_init(&cfg);
        mpp_dec_cfg_set_ptr(cfg, "cb:frm_rdy_cb", (void *)frameReadyCb);
        mpp_dec_cfg_set_ptr(cfg, "cb:frm_rdy_ctx", this);

        /* check HDR Vivid support */
        if (!mBufferMode && C2RKChipCapDef::get()->getHdrMetaCap()) {
            mpp_dec_cfg_set_u32(cfg, "base:enable_hdr_meta", 1);
            mHdrMetaEnabled = true;
        }

        err = mMppMpi->control(mMppCtx, MPP_DEC_SET_CFG, cfg);
        if (err != MPP_OK) {
            c2_err("failed to set frame callback, err %d", err);
            goto error;
        }

        mpp_dec_cfg_deinit(cfg);
    }

    mDump->initDump(mHorStride, mVerStride, false);

    mStarted = true;

    return C2_OK;

error:
    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = nullptr;
    }

    return C2_CORRUPTED;
}

void C2RKMpiDec::finishWork(OutWorkEntry entry) {
    std::shared_ptr<C2Buffer> c2Buffer = nullptr;

    std::shared_ptr<C2GraphicBlock> outblock = entry.outblock;
    uint64_t timestamp = entry.timestamp;
    uint32_t flags = entry.flags;

    if (outblock) {
        uint32_t left = mFbcCfg.mode ? mFbcCfg.paddingX : 0;
        uint32_t top  = mFbcCfg.mode ? mFbcCfg.paddingY : 0;

        c2Buffer = createGraphicBuffer(
                std::move(outblock), C2Rect(mWidth, mHeight).at(left, top));

        mOutBlock = nullptr;

        if (mCodingType == MPP_VIDEO_CodingAVC ||
            mCodingType == MPP_VIDEO_CodingHEVC ||
            mCodingType == MPP_VIDEO_CodingAV1 ||
            mCodingType == MPP_VIDEO_CodingMPEG2) {
            IntfImpl::Lock lock = mIntf->lock();
            c2Buffer->setInfo(mIntf->getColorAspects_l());
        }
    }

    uint32_t inFlags = 0;
    // TODO: work flags set to incomplete to ignore frame index check
    uint32_t outFlags = C2FrameData::FLAG_INCOMPLETE;

    if ((flags & BUFFER_FLAGS_ERROR_FRAME) || isDropFrame(timestamp)) {
        inFlags = C2FrameData::FLAG_DROP_FRAME;
    }

    auto fillWork = [c2Buffer, inFlags, outFlags, timestamp]
            (const std::unique_ptr<C2Work> &work) {
        work->input.ordinal.timestamp = 0;
        work->input.ordinal.frameIndex = OUTPUT_WORK_INDEX;
        work->input.ordinal.customOrdinal = 0;
        work->input.flags = (C2FrameData::flags_t)inFlags;

        work->worklets.front()->output.flags = (C2FrameData::flags_t)outFlags;
        work->worklets.front()->output.ordinal = work->input.ordinal;
        work->worklets.front()->output.ordinal.timestamp = timestamp;
        work->worklets.front()->output.buffers.clear();
        if (c2Buffer) {
            work->worklets.front()->output.buffers.push_back(c2Buffer);
        }
        work->workletsProcessed = 1u;
        work->result = C2_OK;
    };

    std::unique_ptr<C2Work> work(new C2Work);
    work->worklets.clear();
    work->worklets.emplace_back(new C2Worklet);

    if (flags & BUFFER_FLAGS_INFO_CHANGE) {
        c2_info("update new size %dx%d config to framework.", mWidth, mHeight);
        C2StreamPictureSizeInfo::output size(0u, mWidth, mHeight);
        C2PortActualDelayTuning::output delay(mIntf->mActualOutputDelay->value);
        work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(size));
        work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(delay));
    }

    finish(work, fillWork);

    if (flags & BUFFER_FLAGS_EOS) {
        c2_info("signalling eos");
        Mutex::Autolock autoLock(mEosLock);
        mEosCondition.signal();
        mOutputEos = true;
    }
}

c2_status_t C2RKMpiDec::drain(
        uint32_t drainMode,
        const std::shared_ptr<C2BlockPool> &pool) {
    (void)drainMode;
    (void)pool;
    return C2_OK;
}

void C2RKMpiDec::process(
        const std::unique_ptr<C2Work> &work,
        const std::shared_ptr<C2BlockPool> &pool) {
    c2_status_t err = C2_OK;

    // Initialize output work
    work->result = C2_OK;
    work->workletsProcessed = 0u;
    work->worklets.front()->output.flags = work->input.flags;

    // Initialize decoder if not already initialized
    if (!mStarted) {
        mBlockPool = pool;
        mBufferMode = (pool->getLocalId() <= C2BlockPool::PLATFORM_START);
        err = initDecoder(work);
        if (err != C2_OK) {
            work->result = C2_BAD_VALUE;
            c2_info("failed to initialize, signalled Error");
            return;
        }
        err = updateSurfaceConfig(pool);
        if (err == C2_OK) {
            c2_info("surface config: surfaceMode %d graphicSource %d scaleMode %d",
                    !mBufferMode, mIsGBSource, mScaleMode);
        }
    }

    if (mSignalledInputEos || mSignalledError) {
        work->workletsProcessed = 1u;
        work->result = C2_CORRUPTED;
        return;
    }

    uint8_t *inData = nullptr;
    size_t inSize = 0u;
    C2ReadView rView = mDummyReadView;
    if (!work->input.buffers.empty()) {
        rView = work->input.buffers[0]->data().linearBlocks().front().map().get();
        inData = const_cast<uint8_t *>(rView.data());
        inSize = rView.capacity();
        if (inSize && rView.error()) {
            c2_err("failed to read rWiew, error %d", rView.error());
            work->result = rView.error();
            return;
        }
    }

    uint32_t flags = work->input.flags;
    uint64_t frameIndex = work->input.ordinal.frameIndex.peekull();
    uint64_t timestamp = work->input.ordinal.timestamp.peekll();

    c2_trace("in buffer attr. size %zu timestamp %lld frameindex %lld, flags %x",
             inSize, timestamp, frameIndex, flags);

    if (!(flags & C2FrameData::FLAG_CODEC_CONFIG)
            && (flags & C2FrameData::FLAG_DROP_FRAME)) {
        mDropFramesPts.push_back(timestamp);
    }

    err = ensureDecoderState();
    if (err != C2_OK) {
        mSignalledError = true;
        work->workletsProcessed = 1u;
        work->result = C2_CORRUPTED;
        return;
    }

    err = sendpacket(inData, inSize, timestamp, flags);
    if (err != C2_OK) {
        c2_err("failed to send packet, pts %lld", timestamp);
        mSignalledError = true;
        work->workletsProcessed = 1u;
        work->result = C2_CORRUPTED;
        return;
    }

    // fillEmpty for old worklet
    if (flags & C2FrameData::FLAG_END_OF_STREAM) {
        flags = C2FrameData::FLAG_END_OF_STREAM;
        mSignalledInputEos = true;
        // wait output eos stream
        Mutex::Autolock autoLock(mEosLock);
        if (!mOutputEos)
            mEosCondition.wait(mEosLock);
    } else {
        flags = 0;
    }

    work->worklets.front()->output.flags = (C2FrameData::flags_t)flags;
    work->worklets.front()->output.buffers.clear();
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->workletsProcessed = 1u;

    mFlushed = false;
}

void C2RKMpiDec::setDefaultCodecColorAspectsIfNeeded(ColorAspects &aspects) {
    typedef ColorAspects CA;
    CA::MatrixCoeffs matrix;

    static const ALookup<CA::Primaries, CA::MatrixCoeffs> sPMAspectMap = {
        {
            { CA::PrimariesUnspecified,   CA::MatrixUnspecified },
            { CA::PrimariesBT709_5,       CA::MatrixBT709_5 },
            { CA::PrimariesBT601_6_625,   CA::MatrixBT601_6 },
            { CA::PrimariesBT601_6_525,   CA::MatrixBT601_6 },
            { CA::PrimariesBT2020,        CA::MatrixBT2020 },
            { CA::PrimariesBT470_6M,      CA::MatrixBT470_6M },
        }
    };

    // dataspace supported lists: BT709 / BT601_6_625 / BT601_6_525 / BT2020.
    // so reset unsupport aspect here. For unassigned aspect, reassignment will
    // do later in frameworks.
    if (aspects.mMatrixCoeffs == CA::MatrixOther)
        aspects.mMatrixCoeffs = CA::MatrixUnspecified;
    if (!sPMAspectMap.map(aspects.mPrimaries, &matrix)) {
        c2_warn("reset unsupport primaries %s", asString(aspects.mPrimaries));
        aspects.mPrimaries = CA::PrimariesUnspecified;
    }

    if (aspects.mMatrixCoeffs == CA::MatrixUnspecified
            && aspects.mPrimaries != CA::PrimariesUnspecified) {
        sPMAspectMap.map(aspects.mPrimaries, &aspects.mMatrixCoeffs);
    } else if (aspects.mPrimaries == CA::PrimariesUnspecified
                   && aspects.mMatrixCoeffs != CA::MatrixUnspecified) {
        if (aspects.mMatrixCoeffs == CA::MatrixBT601_6) {
            if ((mWidth <= 720 && mHeight <= 480) || (mHeight <= 720 && mWidth <= 480)) {
                aspects.mPrimaries = CA::PrimariesBT601_6_525;
            } else {
                aspects.mPrimaries = CA::PrimariesBT601_6_625;
            }
        } else {
            if (!sPMAspectMap.map(aspects.mMatrixCoeffs, &aspects.mPrimaries)) {
                aspects.mMatrixCoeffs = CA::MatrixUnspecified;
            }
        }
    }
}

void C2RKMpiDec::getVuiParams(MppFrame frame) {
    VuiColorAspects aspects;

    aspects.primaries = mpp_frame_get_color_primaries(frame);
    aspects.transfer  = mpp_frame_get_color_trc(frame);
    aspects.coeffs    = mpp_frame_get_colorspace(frame);
    if (mCodingType == MPP_VIDEO_CodingMPEG2) {
        aspects.fullRange = 0;
    } else {
        aspects.fullRange =
            (mpp_frame_get_color_range(frame) == MPP_FRAME_RANGE_JPEG);
    }

    // convert vui aspects to C2 values if changed
    if (!(aspects == mBitstreamColorAspects)) {
        mBitstreamColorAspects = aspects;
        ColorAspects sfAspects;
        C2StreamColorAspectsInfo::input codedAspects = { 0u };

        c2_info("Got vui color aspects, P(%d) T(%d) M(%d) R(%d)",
                aspects.primaries, aspects.transfer,
                aspects.coeffs, aspects.fullRange);

        ColorUtils::convertIsoColorAspectsToCodecAspects(
                aspects.primaries, aspects.transfer, aspects.coeffs,
                aspects.fullRange, sfAspects);

        setDefaultCodecColorAspectsIfNeeded(sfAspects);

        if (!C2Mapper::map(sfAspects.mPrimaries, &codedAspects.primaries)) {
            codedAspects.primaries = C2Color::PRIMARIES_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mRange, &codedAspects.range)) {
            codedAspects.range = C2Color::RANGE_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mMatrixCoeffs, &codedAspects.matrix)) {
            codedAspects.matrix = C2Color::MATRIX_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mTransfer, &codedAspects.transfer)) {
            codedAspects.transfer = C2Color::TRANSFER_UNSPECIFIED;
        }

        std::vector<std::unique_ptr<C2SettingResult>> failures;
        mIntf->config({&codedAspects}, C2_MAY_BLOCK, &failures);

        c2_info("set colorAspects (R:%d(%s), P:%d(%s), M:%d(%s), T:%d(%s))",
                sfAspects.mRange, asString(sfAspects.mRange),
                sfAspects.mPrimaries, asString(sfAspects.mPrimaries),
                sfAspects.mMatrixCoeffs, asString(sfAspects.mMatrixCoeffs),
                sfAspects.mTransfer, asString(sfAspects.mTransfer));
    }
}

c2_status_t C2RKMpiDec::updateFbcModeIfNeeded() {
    bool needsUpdate = false;
    uint32_t format  = mColorFormat;
    uint32_t fbcMode = getFbcOutputMode();

    if (!MPP_FRAME_FMT_IS_FBC(format)) {
        if (fbcMode) {
            format |= MPP_FRAME_FBC_AFBC_V2;
            /* fbc decode output has padding inside, set crop before display */
            C2RKChipCapDef::get()->getFbcOutputOffset(
                    mCodingType, &mFbcCfg.paddingX, &mFbcCfg.paddingY);
            needsUpdate = true;
            c2_info("change use mpp fbc output mode, padding offset(%d, %d)",
                    mFbcCfg.paddingX, mFbcCfg.paddingY);
        }
    } else {
        if (!fbcMode) {
            format &= ~MPP_FRAME_FBC_AFBC_V2;
            memset(&mFbcCfg, 0, sizeof(mFbcCfg));
            needsUpdate = true;
            c2_info("change use mpp non-fbc output mode");
        }
    }

    if (needsUpdate) {
        MPP_RET err = MPP_OK;
        MppFrame frame = nullptr;

        mMppMpi->control(mMppCtx, MPP_DEC_SET_OUTPUT_FORMAT, (MppParam)&format);

        mpp_frame_init(&frame);
        mpp_frame_set_width(frame, mWidth);
        mpp_frame_set_height(frame, mHeight);
        mpp_frame_set_fmt(frame, (MppFrameFormat)format);

        err = mMppMpi->control(mMppCtx, MPP_DEC_SET_FRAME_INFO, (MppParam)frame);
        if (err != MPP_OK) {
            c2_err("failed to set frame info, err %d", err);
            mpp_frame_deinit(&frame);
            return C2_CORRUPTED;
        }

        mHorStride   = mpp_frame_get_hor_stride(frame);
        mVerStride   = mpp_frame_get_ver_stride(frame);
        mColorFormat = mpp_frame_get_fmt(frame);
        mFbcCfg.mode = fbcMode;

        mpp_frame_deinit(&frame);
    }

    return C2_OK;
}

c2_status_t C2RKMpiDec::commitBufferToMpp(std::shared_ptr<C2GraphicBlock> block) {
    c2_status_t err = C2_OK;
    auto c2Handle = block->handle();
    uint32_t fd = c2Handle->data[0];
    buffer_handle_t handle = nullptr;

    err = importGraphicBuffer(c2Handle, &handle);
    if (err != C2_OK) return err;

    uint32_t bufferId = C2RKGrallocOps::get()->getBufferId(handle);

    OutBuffer *buffer = findOutBuffer(bufferId);
    if (buffer) {
        /* commit this buffer back to mpp */
        MppBuffer mppBuffer = buffer->mppBuffer;
        if (mppBuffer) {
            mpp_buffer_put(mppBuffer);
        }
        buffer->block = block;
        buffer->site = BUFFER_SITE_BY_MPP;

        c2_trace("put this buffer, index %d mppBuf %p", bufferId, mppBuffer);
    } else {
        /* register this buffer to mpp group */
        MppBuffer mppBuffer;
        MppBufferInfo info;
        memset(&info, 0, sizeof(info));

        info.type  = MPP_BUFFER_TYPE_ION;
        info.fd    = fd;
        info.size  = C2RKGrallocOps::get()->getAllocationSize(handle);
        info.index = bufferId;

        mpp_buffer_import_with_tag(
                mFrmGrp, &info, &mppBuffer, "codec2", __FUNCTION__);

        OutBuffer *newBuffer = new OutBuffer;
        newBuffer->index     = bufferId;
        newBuffer->mppBuffer = mppBuffer;
        newBuffer->block     = block;
        newBuffer->site      = BUFFER_SITE_BY_MPP;

        // signal buffer available to decoder
        mpp_buffer_put(mppBuffer);

        mOutBuffers.push(newBuffer);

        c2_trace("import this buffer, index %d size %d mppBuf %p listSize %d",
                 bufferId, info.size, mppBuffer, mOutBuffers.size());
    }

    freeGraphicBuffer(handle);

    return err;
}

c2_status_t C2RKMpiDec::ensureDecoderState() {
    c2_status_t err = C2_OK;

    Mutex::Autolock autoLock(mBufferLock);

    uint32_t videoW = mWidth;
    uint32_t videoH = mHeight;
    uint32_t frameW = mHorStride;
    uint32_t frameH = mVerStride;
    uint32_t mppFmt = mColorFormat;

    uint32_t blockW = 0, blockH = 0, format = 0;
    uint64_t usage = RK_GRALLOC_USAGE_SPECIFY_STRIDE;

    if (mScaleMode == C2_SCALE_MODE_META) {
         usage |= GRALLOC_USAGE_RKVDEC_SCALING;
    } else if (mScaleMode == C2_SCALE_MODE_DOWN_SCALE) {
        // update scale thumbnail info in down scale mode
        videoW = mScaleInfo.width;
        videoH = mScaleInfo.height;
        frameW = mScaleInfo.hstride;
        frameH = mScaleInfo.vstride;
        mppFmt = mScaleInfo.format;
    }

    blockW = frameW;
    blockH = frameH;
    format = C2RKMediaUtils::getAndroidColorFmt(mppFmt, mFbcCfg.mode);

    // NOTE: private gralloc stride usage only support in 4.0.
    // Update use stride usage if we are able config available stride.
    if (mGrallocVersion == 4 && !mFbcCfg.mode && !mIsGBSource) {
        uint64_t horUsage = 0, verUsage = 0;

        // 10bit video calculate stride base on (width * 10 / 8)
        if (MPP_FRAME_FMT_IS_YUV_10BIT(mppFmt)) {
            horUsage = C2RKMediaUtils::getStrideUsage(videoW * 10 / 8, frameW);
        } else {
            horUsage = C2RKMediaUtils::getStrideUsage(videoW, frameW);
        }
        verUsage = C2RKMediaUtils::getHStrideUsage(videoH, frameH);

        if (horUsage > 0 && verUsage > 0) {
            blockW = videoW;
            blockH = videoH;
            usage &= ~RK_GRALLOC_USAGE_SPECIFY_STRIDE;
            usage |= (horUsage | verUsage);
        }
    }

    if (mFbcCfg.mode) {
        // NOTE: FBC case may have offset y on top and vertical stride
        // should aligned to 16.
        blockH = C2_ALIGN(frameH + mFbcCfg.paddingY, 16);

        // In fbc 10bit mode, surfaceCB treat width as pixel stride.
        if (format == HAL_PIXEL_FORMAT_YUV420_10BIT_I ||
            format == HAL_PIXEL_FORMAT_Y210 ||
            format == HAL_PIXEL_FORMAT_YUV420_10BIT_RFBC ||
            format == HAL_PIXEL_FORMAT_YUV422_10BIT_RFBC) {
            blockW = C2_ALIGN(videoW, 64);
        }
    } else if (mCodingType == MPP_VIDEO_CodingVP9 && mGrallocVersion < 4) {
        blockW = C2_ALIGN_ODD(videoW, 256);
    }

    {
        IntfImpl::Lock lock = mIntf->lock();
        std::shared_ptr<C2StreamColorAspectsTuning::output> colorAspects
                = mIntf->getDefaultColorAspects_l();

        switch(colorAspects->transfer) {
            case ColorTransfer::kColorTransferST2084:
                usage |= ((GRALLOC_NV12_10_HDR_10 << 24) & GRALLOC_COLOR_SPACE_MASK);  // hdr10;
                break;
            case ColorTransfer::kColorTransferHLG:
                usage |= ((GRALLOC_NV12_10_HDR_HLG << 24) & GRALLOC_COLOR_SPACE_MASK);  // hdr-hlg
                break;
            default:
                break;
        }

        switch (colorAspects->primaries) {
            case C2Color::PRIMARIES_BT601_525:
                usage |= MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT601;
                break;
            case C2Color::PRIMARIES_BT709:
                usage |= MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT709;
                break;
            default:
                break;
        }
        switch (colorAspects->range) {
            case C2Color::RANGE_FULL:
                usage |= MALI_GRALLOC_USAGE_RANGE_WIDE;
                break;
            default:
                usage |= MALI_GRALLOC_USAGE_RANGE_NARROW;
                break;
        }
    }

    // only large than gralloc 4 can support int64 usage.
    // otherwise, gralloc 3 will check high 32bit is empty,
    // if not empty, will alloc buffer failed and return
    // error. So we need clear high 32 bit.
    if (mGrallocVersion < 4) {
        usage &= 0xffffffff;
    }
    if (mHdrMetaEnabled) {
        usage |= GRALLOC_USAGE_DYNAMIC_HDR;
    }

    if (mBufferMode) {
        uint32_t bFormat = (MPP_FRAME_FMT_IS_YUV_10BIT(mppFmt)) ? mPixelFormat : format;
        uint64_t bUsage  = (usage | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);

        // allocate buffer within 4G to avoid rga2 error.
        if (C2RKChipCapDef::get()->getChipType() == RK_CHIP_3588 ||
            C2RKChipCapDef::get()->getChipType() == RK_CHIP_356X) {
            bUsage |= RK_GRALLOC_USAGE_WITHIN_4G;
        }
        /*
         * For buffer mode, since we don't konw when the last buffer will use
         * up by user, so we chose to copy output to another dst block.
         */
        if (mOutBlock &&
                (mOutBlock->width() != blockW || mOutBlock->height() != blockH)) {
            mOutBlock.reset();
        }
        if (!mOutBlock) {
            err = mBlockPool->fetchGraphicBlock(blockW, blockH, bFormat,
                                                C2AndroidMemoryUsage::FromGrallocUsage(bUsage),
                                                &mOutBlock);
            if (err != C2_OK) {
                c2_err("failed to fetchGraphicBlock, err %d usage 0x%llx", err, bUsage);
                return err;
            }
        }
    }

    std::shared_ptr<C2GraphicBlock> outblock;
    uint32_t count = mIntf->mActualOutputDelay->value - getOutBufferCountOwnByMpi();

    uint32_t i = 0;
    for (i = 0; i < count; i++) {
        err = mBlockPool->fetchGraphicBlock(blockW, blockH, format,
                                            C2AndroidMemoryUsage::FromGrallocUsage(usage),
                                            &outblock);
        if (err != C2_OK) {
            c2_err("failed to fetchGraphicBlock, err %d", err);
            break;
        }

        err = commitBufferToMpp(outblock);
        if (err != C2_OK) {
            c2_err("failed to commit buffer");
            break;
        }
    }

    c2_trace("required (%dx%d) usage 0x%llx format 0x%x, fetch %d/%d",
             blockW, blockH, usage, format, i, count);

    return err;
}

void C2RKMpiDec::postFrameReady() {
    sp<AMessage> msg = new AMessage(WorkHandler::kWhatFrameReady, mHandler);
    msg->setPointer("thiz", this);
    msg->post();
}

c2_status_t C2RKMpiDec::onFrameReady() {
    c2_status_t err = C2_OK;

outframe:
    OutWorkEntry entry;
    memset(&entry, 0, sizeof(entry));

    err = getoutframe(&entry);
    if (err == C2_OK) {
        finishWork(entry);
        /* Avoid stock frame, continue to search available output */
        ensureDecoderState();
        goto outframe;
    } else if (err == C2_CORRUPTED) {
        c2_err("signalling error");
        mSignalledError = true;
    }

    return err;
}

c2_status_t C2RKMpiDec::sendpacket(uint8_t *data, size_t size, uint64_t pts, uint32_t flags) {
    c2_status_t ret = C2_OK;
    MppPacket packet = nullptr;

    mpp_packet_init(&packet, data, size);
    mpp_packet_set_pts(packet, pts);
    mpp_packet_set_pos(packet, data);
    mpp_packet_set_length(packet, size);

    if (flags & C2FrameData::FLAG_END_OF_STREAM) {
        c2_info("send input eos");
        mpp_packet_set_eos(packet);
    }

    if (flags & C2FrameData::FLAG_CODEC_CONFIG) {
        mpp_packet_set_extra_data(packet);
    }

    MPP_RET err = MPP_OK;
    static uint32_t kMaxRetryCnt = 1000;
    uint32_t retry = 0;

    while (true) {
        err = mMppMpi->decode_put_packet(mMppCtx, packet);
        if (err == MPP_OK) {
            c2_trace("send packet pts %lld size %d", pts, size);
            /* dump input data if neccessary */
            mDump->recordInFile(data, size);
            mDump->showDebugFps(DUMP_ROLE_INPUT);
            break;
        }

        if ((++retry) > kMaxRetryCnt) {
            ret = C2_CORRUPTED;
            break;
        } else if (retry % 100 == 0) {
            c2_warn("try to resend packet, pts %lld", pts);
        }

        usleep(3 * 1000);
    }

    mpp_packet_deinit(&packet);

    return ret;
}

c2_status_t C2RKMpiDec::getoutframe(OutWorkEntry *entry) {
    c2_status_t ret = C2_OK;
    MPP_RET     err = MPP_OK;
    MppFrame  frame = nullptr;

    err = mMppMpi->decode_get_frame(mMppCtx, &frame);
    if (err != MPP_OK || frame == nullptr) {
        return C2_NOT_FOUND;
    }

    uint32_t width   = mpp_frame_get_width(frame);
    uint32_t height  = mpp_frame_get_height(frame);
    uint32_t hstride = mpp_frame_get_hor_stride(frame);
    uint32_t vstride = mpp_frame_get_ver_stride(frame);
    uint32_t error   = mpp_frame_get_errinfo(frame);
    uint32_t eos     = mpp_frame_get_eos(frame);
    uint64_t pts     = mpp_frame_get_pts(frame);
    uint32_t flags   = 0;

    MppFrameFormat format = mpp_frame_get_fmt(frame);
    MppBuffer   mppBuffer = mpp_frame_get_buffer(frame);
    OutBuffer  *outBuffer = nullptr;

    if (mpp_frame_get_info_change(frame)) {
        c2_info("info-change with old dimensions(%dx%d) stride(%dx%d) fmt %d", \
                mWidth, mHeight, mHorStride, mVerStride, mColorFormat);
        c2_info("info-change with new dimensions(%dx%d) stride(%dx%d) fmt %d", \
                width, height, hstride, vstride, format);

        if (width > kMaxVideoWidth || height > kMaxVideoWidth) {
            c2_err("unsupport video size %dx%d, signalled Error.", width, height);
            ret = C2_CORRUPTED;
            goto cleanUp;
        }

        Mutex::Autolock autoLock(mBufferLock);

        clearOutBuffers();
        mpp_buffer_group_clear(mFrmGrp);

        mWidth = width;
        mHeight = height;
        mHorStride = hstride;
        mVerStride = vstride;
        mColorFormat = format;

        // support fbc mode change on info change stage
        updateFbcModeIfNeeded();

        // All buffer group config done. Set info change ready to let
        // decoder continue decoding.
        mMppMpi->control(mMppCtx, MPP_DEC_SET_INFO_CHANGE_READY, nullptr);

        C2StreamPictureSizeInfo::output size(0u, mWidth, mHeight);
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        ret = mIntf->config({&size}, C2_MAY_BLOCK, &failures);
        if (ret != C2_OK) {
            c2_err("failed to set width and height");
            ret = C2_CORRUPTED;
            goto cleanUp;
        }

        ret = updateOutputDelay();
        if (ret != C2_OK) {
            c2_err("failed to update output delay");
            ret = C2_CORRUPTED;
            goto cleanUp;
        }

        // feekback config update to first output frame.
        flags |= BUFFER_FLAGS_INFO_CHANGE;

        goto cleanUp;
    }

    if (eos) {
        c2_info("get output eos.");
        flags |= BUFFER_FLAGS_EOS;
        // ignore null frame with eos
        if (!mppBuffer) goto cleanUp;
    }

    if (error) {
        c2_warn("skip error frame with pts %lld", pts);
        flags |= BUFFER_FLAGS_ERROR_FRAME;
    }

    if (isPendingFlushing()) {
        ret = C2_CANCELED;
        c2_trace("ignore frame output since pending flush");
        goto cleanUp;
    }

    outBuffer = findOutBuffer(mppBuffer);
    if (!outBuffer) {
        ret = C2_CORRUPTED;
        c2_err("get outdated mppBuffer %p", mppBuffer);
        goto cleanUp;
    }

    c2_trace("get one frame [%d:%d] stride [%d:%d] pts %lld err %d index %d",
             width, height, hstride, vstride, pts, error, outBuffer->index);

    if (mBufferMode) {
        if (MPP_FRAME_FMT_IS_YUV_10BIT(mColorFormat)) {
            C2GraphicView wView = mOutBlock->map().get();
            C2PlanarLayout layout = wView.layout();
            uint8_t *src = (uint8_t*)mpp_buffer_get_ptr(mppBuffer);
            uint8_t *dstY = const_cast<uint8_t *>(wView.data()[C2PlanarLayout::PLANE_Y]);
            uint8_t *dstUV = const_cast<uint8_t *>(wView.data()[C2PlanarLayout::PLANE_U]);
            size_t dstYStride = layout.planes[C2PlanarLayout::PLANE_Y].rowInc;
            size_t dstUVStride = layout.planes[C2PlanarLayout::PLANE_U].rowInc;

            C2RKMediaUtils::convert10BitNV12ToRequestFmt(
                    mPixelFormat, dstY, dstUV, dstYStride,
                    dstUVStride, src, hstride, vstride, width, height);
        } else {
            RgaInfo srcInfo, dstInfo;
            int32_t srcFd = 0, dstFd = 0;

            auto c2Handle = mOutBlock->handle();
            srcFd = mpp_buffer_get_fd(mppBuffer);
            dstFd = c2Handle->data[0];

            C2RKRgaDef::SetRgaInfo(
                    &srcInfo, srcFd, mWidth, mHeight, mHorStride, mVerStride);
            C2RKRgaDef::SetRgaInfo(
                    &dstInfo, dstFd, mWidth, mHeight, mHorStride, mVerStride);
            if (!C2RKRgaDef::NV12ToNV12(srcInfo, dstInfo)) {
                // fallback software copy
                uint8_t *srcPtr = (uint8_t*)mpp_buffer_get_ptr(mppBuffer);
                uint8_t *dstPtr = mOutBlock->map().get().data()[C2PlanarLayout::PLANE_Y];
                memcpy(dstPtr, srcPtr, mHorStride * mVerStride * 3 / 2);
            }
        }
        outBuffer->site = BUFFER_SITE_BY_MPP;
    } else {
        outBuffer->site = BUFFER_SITE_BY_US;
        // signal buffer occupied by users
        mpp_buffer_inc_ref(mppBuffer);

        if (mScaleMode == C2_SCALE_MODE_META) {
            configFrameScaleMeta(frame, outBuffer->block);
        }
        if (mHdrMetaEnabled) {
            configFrameHdrMeta(frame, outBuffer->block);
        }
    }

    if (mCodingType == MPP_VIDEO_CodingAVC ||
        mCodingType == MPP_VIDEO_CodingHEVC ||
        mCodingType == MPP_VIDEO_CodingAV1 ||
        mCodingType == MPP_VIDEO_CodingMPEG2) {
        getVuiParams(frame);
    }

    /* dump output data if neccessary */
    if (C2RKDump::getDumpFlag() & C2_DUMP_RECORD_DEC_OUT) {
        void *data = mpp_buffer_get_ptr(mppBuffer);
        mDump->recordOutFile(data, hstride, vstride, RAW_TYPE_YUV420SP);
    }

    /* show output process fps if neccessary */
    mDump->showDebugFps(DUMP_ROLE_OUTPUT);

    entry->outblock = mBufferMode ? mOutBlock : outBuffer->block;
    entry->timestamp = pts;

cleanUp:
    entry->flags = flags;

    if (frame) {
        mpp_frame_deinit(&frame);
        frame = nullptr;
    }

    return ret;
}

c2_status_t C2RKMpiDec::configFrameScaleMeta(
        MppFrame frame, std::shared_ptr<C2GraphicBlock> block) {
#ifdef mpp_frame_get_thumbnail_en
    if (block->handle()
            && mpp_frame_has_meta(frame) && mpp_frame_get_thumbnail_en(frame)) {
        MppMeta meta = nullptr;
        int32_t scaleYOffset = 0;
        int32_t scaleUVOffset = 0;
        int32_t width = 0, height = 0;
        MppFrameFormat format;
        C2PreScaleParam scaleParam;

        memset(&scaleParam, 0, sizeof(C2PreScaleParam));

        native_handle_t *nHandle = UnwrapNativeCodec2GrallocHandle(block->handle());

        width  = mpp_frame_get_width(frame);
        height = mpp_frame_get_height(frame);
        format = mpp_frame_get_fmt(frame);
        meta   = mpp_frame_get_meta(frame);

        mpp_meta_get_s32(meta, KEY_DEC_TBN_Y_OFFSET, &scaleYOffset);
        mpp_meta_get_s32(meta, KEY_DEC_TBN_UV_OFFSET, &scaleUVOffset);

        scaleParam.thumbWidth = width >> 1;
        scaleParam.thumbHeight = height >> 1;
        scaleParam.thumbHorStride = C2_ALIGN(mHorStride >> 1, 16);
        scaleParam.yOffset = scaleYOffset;
        scaleParam.uvOffset = scaleUVOffset;
        if (MPP_FRAME_FMT_IS_YUV_10BIT(format)) {
            scaleParam.format = HAL_PIXEL_FORMAT_YCrCb_NV12_10;
        } else {
            scaleParam.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
        }
        C2RKVdecExtendFeature::configFrameScaleMeta(nHandle, &scaleParam);
        memcpy((void *)&block->handle()->data,
               (void *)&nHandle->data,
               sizeof(int) * (nHandle->numFds + nHandle->numInts));

        native_handle_delete(nHandle);
    }
#else
    (void)frame;
    (void)block;
#endif

    return C2_OK;
}

c2_status_t C2RKMpiDec::configFrameHdrMeta(
        MppFrame frame, std::shared_ptr<C2GraphicBlock> block) {
    c2_status_t err = C2_OK;

    if (block->handle() && mpp_frame_has_meta(frame)
            && MPP_FRAME_FMT_IS_HDR(mpp_frame_get_fmt(frame))) {
        int32_t hdrMetaOffset = 0;
        int32_t hdrMetaSize = 0;

        MppMeta meta = mpp_frame_get_meta(frame);
        mpp_meta_get_s32(meta, KEY_HDR_META_OFFSET, &hdrMetaOffset);
        mpp_meta_get_s32(meta, KEY_HDR_META_SIZE, &hdrMetaSize);

        if (hdrMetaOffset && hdrMetaSize) {
            buffer_handle_t handle = nullptr;
            auto c2Handle = block->handle();

            err = importGraphicBuffer(c2Handle, &handle);
            if (err != C2_OK) {
                c2_err("failed to import graphic buffer");
                return err;
            }

            if (!C2RKVdecExtendFeature::configFrameHdrDynamicMeta(handle, hdrMetaOffset)) {
                c2_trace("failed to config HdrDynamicMeta");
            }

            freeGraphicBuffer(handle);
        }
    }

    return err;
}

class C2RKMpiDecFactory : public C2ComponentFactory {
public:
    explicit C2RKMpiDecFactory(std::string name)
            : mHelper(std::static_pointer_cast<C2ReflectorHelper>(
                  GetCodec2RKComponentStore()->getParamReflector())),
              mComponentName(name) {
        C2RKComponentEntry *entry = GetRKComponentEntry(name);
        if (entry != nullptr) {
            mKind = entry->kind;
            mMime = entry->mime;
            mDomain = C2Component::DOMAIN_VIDEO;
        } else {
            c2_err("failed to get component entry from name %s", name.c_str());
        }
    }

    virtual c2_status_t createComponent(
            c2_node_id_t id,
            std::shared_ptr<C2Component>* const component,
            std::function<void(C2Component*)> deleter) override {
        *component = std::shared_ptr<C2Component>(
                new C2RKMpiDec(
                        mComponentName.c_str(),
                        mMime.c_str(),
                        id,
                        std::make_shared<C2RKMpiDec::IntfImpl>
                            (mHelper, mComponentName, mKind, mDomain, mMime)),
                        deleter);
        return C2_OK;
    }

    virtual c2_status_t createInterface(
            c2_node_id_t id,
            std::shared_ptr<C2ComponentInterface>* const interface,
            std::function<void(C2ComponentInterface*)> deleter) override {
        *interface = std::shared_ptr<C2ComponentInterface>(
                new C2RKInterface<C2RKMpiDec::IntfImpl>(
                        mComponentName.c_str(),
                        id,
                        std::make_shared<C2RKMpiDec::IntfImpl>
                            (mHelper, mComponentName, mKind, mDomain, mMime)),
                        deleter);
        return C2_OK;
    }

    virtual ~C2RKMpiDecFactory() override = default;

private:
    std::shared_ptr<C2ReflectorHelper> mHelper;
    std::string mComponentName;
    std::string mMime;
    C2Component::kind_t mKind;
    C2Component::domain_t mDomain;
};

C2ComponentFactory* CreateRKMpiDecFactory(std::string componentName) {
    return new ::android::C2RKMpiDecFactory(componentName);
}

}  // namespace android
