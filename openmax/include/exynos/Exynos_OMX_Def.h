/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
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

/*
 * @file    Exynos_OMX_Def.h
 * @brief   Exynos_OMX specific define
 * @author  SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version    2.0.0
 * @history
 *   2012.02.20 : Create
 */

#ifndef EXYNOS_OMX_DEF
#define EXYNOS_OMX_DEF

//#define PERFORMANCE_DEBUG

#include "OMX_Types.h"
#include "OMX_IVCommon.h"
#include "OMX_Video.h"
#include "OMX_VideoExt.h"
#include "OMX_IndexExt.h"
#include "OMX_Component.h"

#define VERSIONMAJOR_NUMBER                1
#define VERSIONMINOR_NUMBER                1
#define REVISION_NUMBER                    2
#define STEP_NUMBER                        0

#ifdef MAX_COMPONENT_NUM
#define RESOURCE_VIDEO_DEC MAX_COMPONENT_NUM
#define RESOURCE_VIDEO_ENC MAX_COMPONENT_NUM
#else
#define RESOURCE_VIDEO_DEC 16
#define RESOURCE_VIDEO_ENC 16
#endif
#define RESOURCE_AUDIO_DEC 10

#define MAX_OMX_COMPONENT_NUM              40
#define MAX_OMX_COMPONENT_ROLE_NUM         10
#define MAX_OMX_COMPONENT_NAME_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_ROLE_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_LIBNAME_SIZE     OMX_MAX_STRINGNAME_SIZE * 2
#define MAX_OMX_MIMETYPE_SIZE              OMX_MAX_STRINGNAME_SIZE

#define MAX_BUFFER_REF       40
#define MAX_TIMESTAMP        MAX_BUFFER_REF
#define MAX_FLAGS            MAX_BUFFER_REF

#define MAX_BUFFER_PLANE     3

#define INDEX_AFTER_EOS      0xE05
#define INDEX_AFTER_DRC      0xD5C
#define INDEX_HEADER_DATA    0xC5D

#define DEFAULT_TIMESTAMP_VAL (-1010101010)
#define RESET_TIMESTAMP_VAL   (-1001001001)

// The largest metadata buffer size advertised
// when metadata buffer mode is used
#define  MAX_METADATA_BUFFER_SIZE (64)

#define  MAX_VENDOR_EXT_NUM (32)

#define  PREFIX_COMPONENT_NAME "OMX.Exynos."
#define  IS_CUSTOM_COMPONENT(name) (((char)(name[((int)sizeof(PREFIX_COMPONENT_NAME))-1]) >= 0x61)? OMX_TRUE:OMX_FALSE)

#define IS_64BIT_OS                 (((sizeof(int) != sizeof(void *))? OMX_TRUE:OMX_FALSE))

/* for image converter(MSRND) */
#define OMX_BUFFERFLAG_CONVERTEDIMAGE 0x00000100

typedef enum _EXYNOS_CODEC_TYPE
{
    SW_CODEC,
    HW_VIDEO_DEC_CODEC,
    HW_VIDEO_ENC_CODEC,
    HW_VIDEO_DEC_SECURE_CODEC,
    HW_VIDEO_ENC_SECURE_CODEC,
    HW_AUDIO_DEC_CODEC,
    HW_AUDIO_ENC_CODEC
} EXYNOS_CODEC_TYPE;

#define PLANE_MAX_NUM 3
typedef enum _PLANE_TYPE {
    PLANE_MULTIPLE      = 0x00,
    PLANE_SINGLE        = 0x11,
    PLANE_SINGLE_USER   = 0x12,
} PLANE_TYPE;

typedef enum _EXYNOS_OMX_INDEXTYPE
{
#define EXYNOS_INDEX_CONFIG_VIDEO_INTRAPERIOD "OMX.SEC.index.VideoIntraPeriod"
    OMX_IndexConfigVideoIntraPeriod             = 0x7F000002,
#ifdef USE_S3D_SUPPORT
#define EXYNOS_INDEX_PARAM_GET_S3D "OMX.SEC.index.S3DMode"
    OMX_IndexVendorS3DMode                      = 0x7F000003,
#endif
#define EXYNOS_INDEX_PARAM_NEED_CONTIG_MEMORY "OMX.SEC.index.NeedContigMemory"  /* for HDCP */
    OMX_IndexVendorNeedContigMemory             = 0x7F000004,
#define EXYNOS_INDEX_CONFIG_GET_BUFFER_FD "OMX.SEC.index.GetBufferFD"           /* for HDCP */
    OMX_IndexVendorGetBufferFD                  = 0x7F000005,
#define EXYNOS_INDEX_PARAM_SET_DTS_MODE "OMX.SEC.index.SetDTSMode"              /* for decoding DTS contents */
    OMX_IndexVendorSetDTSMode                   = 0x7F000006,
#define EXYNOS_INDEX_PARAM_ENABLE_THUMBNAIL "OMX.SEC.index.enableThumbnailMode"
    OMX_IndexParamEnableThumbnailMode           = 0x7F000007,
#define EXYNOS_INDEX_PARAM_VIDEO_QPRANGE_TYPE "OMX.SEC.indexParam.VideoQPRange"
    OMX_IndexParamVideoQPRange                  = 0x7F000008,
#define EXYNOS_INDEX_CONFIG_VIDEO_QPRANGE_TYPE "OMX.SEC.indexConfig.VideoQPRange"
    OMX_IndexConfigVideoQPRange                 = 0x7F000009,
#define EXYNOS_INDEX_CONFIG_VIDEO_TEMPORALSVC "OMX.SEC.index.TemporalSVC"
    OMX_IndexConfigVideoTemporalSVC             = 0x7F000010,
#define EXYNOS_INDEX_PARAM_VIDEO_AVC_ENABLE_TEMPORALSVC "OMX.SEC.index.AVC.enableTemporalSVC"
    OMX_IndexParamVideoAVCEnableTemporalSVC     = 0x7F000011,
#define EXYNOS_INDEX_PARAM_ENABLE_BLUR_FILTER "OMX.SEC.indexParam.enableBlurFilter"
    OMX_IndexParamEnableBlurFilter              = 0x7F000014,
#define EXYNOS_INDEX_CONFIG_BLUR_INFO "OMX.SEC.indexConfig.BlurInfo"
    OMX_IndexConfigBlurInfo                     = 0x7F000015,
#define EXYNOS_INDEX_PARAM_VIDEO_HEVC_ENABLE_TEMPORALSVC "OMX.SEC.index.Hevc.enableTemporalSVC"
    OMX_IndexParamVideoHevcEnableTemporalSVC    = 0x7F000016,
#define EXYNOS_INDEX_CONFIG_VIDEO_ROIINFO "OMX.SEC.index.RoiInfo"
    OMX_IndexConfigVideoRoiInfo                 = 0x7F000017,
#define EXYNOS_INDEX_PARAM_VIDEO_ENABLE_ROIINFO "OMX.SEC.index.enableRoiInfo"
    OMX_IndexParamVideoEnableRoiInfo            = 0x7F000018,
#define EXYNOS_INDEX_PARAM_ROATION_INFO "OMX.SEC.indexParam.RotationInfo"
    OMX_IndexParamRotationInfo                  = 0x7F000019,
#define EXYNOS_INDEX_PARAM_ENABLE_PVC "OMX.SEC.indexParam.enablePVC"
    OMX_IndexParamVideoEnablePVC                = 0x7F000020,
#define EXYNOS_INDEX_CONFIG_BLACK_BAR_CROP_INFO "OMX.SEC.index.BlackBarCrop"
    OMX_IndexConfigBlackBarCrop                 = 0x7F000021,
#define EXYNOS_INDEX_CONFIG_SEARCH_BLACK_BAR "OMX.SEC.index.SearchBlackBar"
    OMX_IndexConfigSearchBlackBar               = 0x7F000022,
#define EXYNOS_CUSTOM_INDEX_PARAM_REORDER_MODE "OMX.SEC.CUSTOM.index.ReorderMode"
    OMX_IndexExynosParamReorderMode             = 0x7F000023,
#define EXYNOS_INDEX_CONFIG_IFRAME_RATIO "OMX.SEC.index.IFrameRatio"
    OMX_IndexConfigIFrameRatio                  = 0x7F000024,
#define EXYNOS_INDEX_PARAM_REF_NUM_PFRAMES "OMX.SEC.index.NumberRefPframes"
    OMX_IndexParamNumberRefPframes              = 0x7F000025,
#define EXYNOS_INDEX_PARAM_VIDEO_ENABLE_GPB "OMX.SEC.index.enableGPB"
    OMX_IndexParamVideoEnableGPB                = 0x7F000026,
#define EXYNOS_INDEX_PARAM_VIDEO_DROP_CONTROL "OMX.SEC.index.enableDropControl"
    OMX_IndexParamVideoDropControl              = 0x7F000027,
#define EXYNOS_INDEX_PARAM_VIDEO_COMP_COLOR_FORMAT "OMX.SEC.index.setCompressedColorFormat"
    OMX_IndexParamVideoCompressedColorFormat    = 0x7F000028,
#define EXYNOS_INDEX_PARAM_VIDEO_DISABLE_DFR "OMX.SEC.index.disableDFR"
    OMX_IndexParamVideoDisableDFR               = 0x7F000029,
#define EXYNOS_INDEX_PARAM_VIDEO_CHROMA_QP "OMX.SEC.index.chromaQP"
    OMX_IndexParamVideoChromaQP                 = 0x7F000030,
#define EXYNOS_INDEX_PARAM_VIDEO_DISABLE_HBENCODING "OMX.SEC.index.disableHBEncoding"
    OMX_IndexParamVideoDisableHBEncoding        = 0x7F000031,

////////////////////////////////////////////////////////////////////////////////////////////////
// for extension codec spec
////////////////////////////////////////////////////////////////////////////////////////////////
    OMX_IndexExtensionStartUnused               = 0x7F020000,
#ifdef USE_KHRONOS_OMX_HEADER
#define EXYNOS_INDEX_PARAM_VIDEO_HEVC_TYPE "OMX.SEC.index.VideoHevcType"
    OMX_IndexParamVideoHevc                     = 0x7F020000,
#define EXYNOS_INDEX_PARAM_VIDEO_VP9_TYPE "OMX.SEC.index.VideoVp9Type"
    OMX_IndexParamVideoVp9                      = 0x7F020001,
#endif
#ifndef USE_KHRONOS_OMX_1_2
#define EXYNOS_INDEX_PARAM_VIDEO_VC1_TYPE "OMX.SEC.index.VideoVc1Type"
    OMX_IndexParamVideoVC1                      = 0x7F021000,
#endif


////////////////////////////////////////////////////////////////////////////////////////////////
// for android platform
////////////////////////////////////////////////////////////////////////////////////////////////
    OMX_IndexAndroidStartUnused                     = 0x7F030000,
    /* query using get_extension_index */
#define EXYNOS_INDEX_PARAM_USE_ANB "OMX.google.android.index.useAndroidNativeBuffer"        /* no more used */
    OMX_IndexParamUseAndroidNativeBuffer            = 0x7F030001,
#define EXYNOS_INDEX_PARAM_USE_ANB2 "OMX.google.android.index.useAndroidNativeBuffer2"      /* no more used */
    OMX_IndexParamUseAndroidNativeBuffer2           = 0x7F030002,
#define EXYNOS_INDEX_PARAM_GET_ANB "OMX.google.android.index.getAndroidNativeBufferUsage"
    OMX_IndexParamGetAndroidNativeBuffer            = 0x7F030003,
#define EXYNOS_INDEX_PARAM_ENABLE_ANB "OMX.google.android.index.enableAndroidNativeBuffers"
    OMX_IndexParamEnableAndroidBuffers              = 0x7F030004,
#define EXYNOS_INDEX_PARAM_STORE_METADATA_BUFFER "OMX.google.android.index.storeMetaDataInBuffers"
    OMX_IndexParamStoreMetaDataBuffer               = 0x7F030005,
#define EXYNOS_INDEX_PARAM_PREPEND_SPSPPS_TO_IDR "OMX.google.android.index.prependSPSPPSToIDRFrames"
    OMX_IndexParamPrependSPSPPSToIDR                = 0x7F030006,
#define EXYNOS_INDEX_PARAM_ALLOCATE_NATIVE_HANDLE "OMX.google.android.index.allocateNativeHandle"
    OMX_IndexParamAllocateNativeHandle              = 0x7F030007,
#define EXYNOS_INDEX_PARAM_DESCRIBE_COLOR_FORMAT "OMX.google.android.index.describeColorFormat"
    OMX_IndexParamDescribeColorFormat               = 0x7F030008,
#define EXYNOS_INDEX_CONFIG_VIDEO_HDR_STATIC_INFO "OMX.google.android.index.describeHDRStaticInfo"
    OMX_IndexConfigVideoHdrStaticInfo               = 0x7F030009,
#define EXYNOS_INDEX_CONFIG_VIDEO_COLOR_ASPECTS_INFO "OMX.google.android.index.describeColorAspects"
    OMX_IndexConfigVideoColorAspects                = 0x7F030010,
#define EXYNOS_INDEX_CONFIG_VIDEO_HDR10_PLUS_INFO "OMX.google.android.index.describeHDR10PlusInfo"
    OMX_IndexConfigVideoHdr10PlusInfo               = 0x7F030011,
#define EXYNOS_INDEX_PARAM_SET_ANB_CONSUMER_USAGE "OMX.google.android.index.AndroidNativeBufferConsumerUsage"
    OMX_IndexParamAndroidNatvieBufferConsumerUsage  = 0x7F030012,

#ifdef USE_KHRONOS_OMX_HEADER
    /* not query, just uses index */
#define EXYNOS_INDEX_CONFIG_ANDROID_VENDOR_EXT "OMX.google.android.index.androidVendorExtension"
    OMX_IndexConfigAndroidVendorExtension           = 0x7F031001,


#define EXYNOS_INDEX_PARAM_VIDEO_ANDROID_VP8_ENCODER "OMX.SEC.index.VideoAndroidVP8Encoder"
    OMX_IndexParamVideoAndroidVp8Encoder            = 0x7F032001,
#define EXYNOS_INDEX_PARAM_SLICE_SEGMENTS "OMX.SEC.index.SliceSegments"                                 /* not supported yet */
    OMX_IndexParamSliceSegments                     = 0x7F032002,
#define EXYNOS_INDEX_CONFIG_ANDROID_INTRA_REFRESH "OMX.SEC.index.AndroidIntraRefresh"                   /* not supported yet */
    OMX_IndexConfigAndroidIntraRefresh              = 0x7F032003,
#define EXYNOS_INDEX_PARAM_ANDROID_VIDEO_TEMPORAL "OMX.SEC.index.param.TemporalLayering"
    OMX_IndexParamAndroidVideoTemporalLayering      = 0x7F032004,
#define EXYNOS_INDEX_CONFIG_ANDROID_VIDEO_TEMPORAL "OMX.SEC.index.config.TemporalLayering"
    OMX_IndexConfigAndroidVideoTemporalLayering     = 0x7F032005,
#define EXYNOS_INDEX_PARAM_MAX_FRAME_DURATION "OMX.SEC.index.maxDurationForBitratecontrol"              /* not supported yet */
    OMX_IndexParamMaxFrameDurationForBitrateControl = 0x7F032006,
#define EXYNOS_INDEX_PARAM_VIDEO_ANDROID_VP9_ENCODER "OMX.SEC.index.VideoAndroidVp9Encoder"             /* not supported yet */
    OMX_IndexParamVideoAndroidVp9Encoder            = 0x7F032007,


#define EXYNOS_INDEX_CONFIG_OPERATING_RATE "OMX.SEC.index.OperatingRate"
    OMX_IndexConfigOperatingRate                    = 0x7F033001,
#define EXYNOS_INDEX_PARAM_CONSUMER_USAGE_BITS "OMX.SEC.index.ConsumerUsageBits"
    OMX_IndexParamConsumerUsageBits                 = 0x7F033002,
#define EXYNOS_INDEX_CONFIG_PRIORTIY "OMX.SEC.index.Priority"
    OMX_IndexConfigPriority                         = 0x7F033003,
#endif  // USE_KHRONOS_OMX_HEADER


////////////////////////////////////////////////////////////////////////////////////////////////
// for linux platform
////////////////////////////////////////////////////////////////////////////////////////////////
    OMX_IndexLinuxStartUnused                = 0x7F040000,
#define EXYNOS_CUSTOM_INDEX_PARAM_IMAGE_CROP "OMX.SEC.CUSTOM.index.ImageCrop"
    OMX_IndexExynosParamImageCrop            = 0x7F040002,
////////////////////////////////////////////////////////////////////////////////////////////////
// for custom component
////////////////////////////////////////////////////////////////////////////////////////////////
    OMX_IndexCustomStartUnused              = 0x7F050000,
#define EXYNOS_CUSTOM_INDEX_CONFIG_PTS_MODE "OMX.SEC.CUSTOM.index.PTSMode"
    OMX_IndexExynosConfigPTSMode            = 0x7F050001,
#define EXYNOS_CUSTOM_INDEX_CONFIG_DISPLAY_DELAY "OMX.SEC.CUSTOM.index.DisplayDelay"
    OMX_IndexExynosConfigDisplayDelay       = 0x7F050002,
#define EXYNOS_CUSTOM_INDEX_PARAM_CORRUPTEDHEADER "OMX.SEC.CUSTOM.index.CorruptedHeader"
    OMX_IndexExynosParamCorruptedHeader     = 0x7F050003,
#define EXYNOS_CUSTOM_INDEX_PARAM_DISPLAY_DELAY "OMX.SEC.CUSTOM.index.DisplayDelay2"  /* Compatibility with OMX.SEC.CUSTOM.index.DisplayDelay */
    OMX_IndexExynosParamDisplayDelay        = 0x7F050004,
#define EXYNOS_CUSTOM_INDEX_PARAM_USE_BUFFERCOPY "OMX.SEC.CUSTOM.index.BufferCopy"
    OMX_IndexExynosParamBufferCopy          = 0x7F050005,
#define EXYNOS_CUSTOM_INDEX_PARAM_USE_IMG_CONV "OMX.SEC.CUSTOM.index.ImageConvert"
    OMX_IndexExynosParamImageConvert        = 0x7F050006,
#define EXYNOS_CUSTOM_INDEX_PARAM_USE_IMG_CONV_MODE "OMX.SEC.CUSTOM.index.ImageConvertMode"
    OMX_IndexExynosParamImageConvertMode    = 0x7F050007,


////////////////////////////////////////////////////////////////////////////////////////////////
// for Skype HD
////////////////////////////////////////////////////////////////////////////////////////////////
    OMX_IndexSkypeStartUnused               = 0x7F060000,
#define OMX_MS_SKYPE_PARAM_DRIVERVER "OMX.microsoft.skype.index.driverversion"
    OMX_IndexSkypeParamDriverVersion        = 0x7F060001,
#if 0
#define OMX_MS_SKYPE_PARAM_DECODERSETTING "OMX.microsoft.skype.index.decodersetting"
    OMX_IndexSkypeParamDecoderSetting       = 0x7F060002,
#define OMX_MS_SKYPE_PARAM_DECODERCAP "OMX.microsoft.skype.index.decodercapability"
    OMX_IndexSkypeParamDecoderCapability    = 0x7F060003,
#define OMX_MS_SKYPE_PARAM_ENCODERSETTING "OMX.microsoft.skype.index.encodersetting"
    OMX_IndexSkypeParamEncoderSetting       = 0x7F060004,
#define OMX_MS_SKYPE_PARAM_ENCODERCAP "OMX.microsoft.skype.index.encodercapability"
    OMX_IndexSkypeParamEncoderCapability    = 0x7F060005,
#endif
#define OMX_MS_SKYPE_CONFIG_MARKLTRFRAME "OMX.microsoft.skype.index.markltrframe"
    OMX_IndexSkypeConfigMarkLTRFrame        = 0x7F060006,
#define OMX_MS_SKYPE_CONFIG_USELTRFRAME "OMX.microsoft.skype.index.useltrframe"
    OMX_IndexSkypeConfigUseLTRFrame         = 0x7F060007,
#define OMX_MS_SKYPE_CONFIG_QP "OMX.microsoft.skype.index.qp"
    OMX_IndexSkypeConfigQP                  = 0x7F060008,
#if 0
#define OMX_MS_SKYPE_CONFIG_TEMPORALLAYERCOUNT "OMX.microsoft.skype.index.temporallayercount"
    OMX_IndexSkypeConfigTemporalLayerCount  = 0x7F060009,
#endif
#define OMX_MS_SKYPE_CONFIG_BASELAYERPID "OMX.microsoft.skype.index.basepid"
    OMX_IndexSkypeConfigBasePid             = 0x7F06000a,

    /* common */
    OMX_IndexSkypeParamLowLatency           = 0x7F060010,

    /* encoder */
    OMX_IndexSkypeParamEncoderMaxTemporalLayerCount = 0x7F061000,
    OMX_IndexSkypeParamEncoderMaxLTR                = 0x7F061001,
    OMX_IndexSkypeParamEncoderLTR                   = 0x7F061002,
    OMX_IndexSkypeParamEncoderPreprocess            = 0x7F061003,
    OMX_IndexSkypeParamEncoderSar                   = 0x7F061004,
    OMX_IndexSkypeParamEncoderInputControl          = 0x7F061005,
    OMX_IndexSkypeConfigEncoderLTR                  = 0x7F061006,
    OMX_IndexSkypeConfigEncoderInputTrigger         = 0x7F061007,
    OMX_IndexSkypeParamVideoBitrate                 = 0x7F061008,

    OMX_IndexExynosEndUnused                = 0x7F05FFFF,
} EXYNOS_OMX_INDEXTYPE;

typedef enum _EXYNOS_OMX_ERRORTYPE
{
    OMX_ErrorNoEOF              = (OMX_S32) 0x90000001,
    OMX_ErrorInputDataDecodeYet = (OMX_S32) 0x90000002,
    OMX_ErrorInputDataEncodeYet = (OMX_S32) 0x90000003,
    OMX_ErrorCodecInit          = (OMX_S32) 0x90000004,
    OMX_ErrorCodecDecode        = (OMX_S32) 0x90000005,
    OMX_ErrorCodecEncode        = (OMX_S32) 0x90000006,
    OMX_ErrorCodecFlush         = (OMX_S32) 0x90000007,
    OMX_ErrorOutputBufferUseYet = (OMX_S32) 0x90000008,
    OMX_ErrorCorruptedFrame     = (OMX_S32) 0x90000009,
    OMX_ErrorNeedNextHeaderInfo = (OMX_S32) 0x90000010,
    OMX_ErrorNoneSrcSetupFinish = (OMX_S32) 0x90000011,
    OMX_ErrorCorruptedHeader    = (OMX_S32) OMX_ErrorStreamCorrupt, /* 0x90000012, */
    OMX_ErrorNoneExpiration     = (OMX_S32) 0x90000013,
    OMX_ErrorNoneReuseBuffer    = (OMX_S32) 0x90000014,
} EXYNOS_OMX_ERRORTYPE;

typedef enum _EXYNOS_OMX_COMMANDTYPE
{
    EXYNOS_OMX_CommandComponentDeInit = 0x7F000001,
    EXYNOS_OMX_CommandEmptyBuffer,
    EXYNOS_OMX_CommandFillBuffer,
    EXYNOS_OMX_CommandFakeBuffer,
    Exynos_OMX_CommandSendEvent,
} EXYNOS_OMX_COMMANDTYPE;

typedef enum _EXYNOS_OMX_TRANS_STATETYPE {
    EXYNOS_OMX_TransStateInvalid,
    EXYNOS_OMX_TransStateLoadedToIdle,
    EXYNOS_OMX_TransStateIdleToExecuting,
    EXYNOS_OMX_TransStateExecutingToIdle,
    EXYNOS_OMX_TransStateIdleToLoaded,
    EXYNOS_OMX_TransStateMax = 0X7FFFFFFF
} EXYNOS_OMX_TRANS_STATETYPE;

typedef enum _EXYNOS_OMX_PORT_STATETYPE {
    EXYNOS_OMX_PortStateInvalid,
    EXYNOS_OMX_PortStateFlushing,
    EXYNOS_OMX_PortStateFlushingForDisable,
    EXYNOS_OMX_PortStateDisabling,
    EXYNOS_OMX_PortStateEnabling,
    EXYNOS_OMX_PortStateIdle,
    EXYNOS_OMX_PortStateLoaded,
    EXYNOS_OMX_PortStateMax = 0X7FFFFFFF
} EXYNOS_OMX_PORT_STATETYPE;

typedef enum _EXYNOS_OMX_COLOR_FORMATTYPE {
    OMX_SEC_COLOR_FormatNV12TPhysicalAddress          = 0x7F000001, /* unused */
    OMX_SEC_COLOR_FormatNV12LPhysicalAddress          = 0x7F000002, /* unused */
    OMX_SEC_COLOR_FormatNV12LVirtualAddress           = 0x7F000003, /* unused */

    OMX_SEC_COLOR_FormatNV21LPhysicalAddress          = 0x7F000010, /* unused */
    OMX_SEC_COLOR_FormatNV21Linear                    = 0x7F000011,
    OMX_SEC_COLOR_FormatYVU420Planar                  = 0x7F000012,
    OMX_SEC_COLOR_Format32bitABGR8888                 = 0x7F000013, /* unused */
    OMX_SEC_COLOR_FormatYUV420SemiPlanarInterlace     = 0x7F000014, /* unused */
    OMX_SEC_COLOR_Format10bitYUV420SemiPlanar         = 0x7F000015, /* unused */
    OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar        = 0x7F000016, /* S10B : Y/CbCr */
    OMX_SEC_COLOR_Format10bitYVU420SemiPlanar         = 0x7F000017, /* unused */
    OMX_SEC_COLOR_FormatS10bitYVU420SemiPlanar        = 0x7F000018, /* S10B : Y/CrCb */
    OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC          = 0x7F000019, /* SBWC : Y/CbCr */
    OMX_SEC_COLOR_FormatYVU420SemiPlanarSBWC          = 0x7F000020, /* SBWC : Y/CrCb */
    OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC     = 0x7F000021, /* SBWC 10B : Y/CbCr */
    OMX_SEC_COLOR_Format10bitYVU420SemiPlanarSBWC     = 0x7F000022, /* SBWC 10B : Y/CrCb */
    OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L50      = 0x7F000023, /* SBWC L50 : Y/CbCr */
    OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L75      = 0x7F000024, /* SBWC L75 : Y/CbCr */
    OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L40 = 0x7F000025, /* SBWC 10B L40 : Y/CbCr */
    OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L60 = 0x7F000026, /* SBWC 10B L60 : Y/CbCr */
    OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L80 = 0x7F000027, /* SBWC 10B L80 : Y/CbCr */

    /* to copy a encoded data for drm component using gsc or fimc */
    OMX_SEC_COLOR_FormatEncodedData                 = OMX_COLOR_FormatYCbYCr,
#ifdef USE_KHRONOS_OMX_HEADER
    OMX_COLOR_FormatAndroidOpaque                   = 0x7F000789,
    OMX_COLOR_Format32BitRGBA8888                   = 0x7F00A000,
    OMX_SEC_COLOR_FormatNV12Tiled                   = 0x7FC00002, /* unused */
    OMX_COLOR_FormatYUV420Flexible                  = 0x7F420888,

    // 10-bit or 12-bit YUV format, LSB-justified (0's on higher bits)
    OMX_COLOR_FormatYUV420Planar16                  = 0x7F42016B, /* P010 */
#endif
}EXYNOS_OMX_COLOR_FORMATTYPE;

typedef enum _EXYNOS_OMX_SUPPORTFORMAT_TYPE
{
    supportFormat_0 = 0x00,
    supportFormat_1,
    supportFormat_2,
    supportFormat_3,
    supportFormat_4,
    supportFormat_5,
    supportFormat_6,
    supportFormat_7,
} EXYNOS_OMX_SUPPORTFORMAT_TYPE;

typedef enum _EXYNOS_OMX_BUFFERPROCESS_TYPE
{
    BUFFER_DEFAULT    = 0x00,
    BUFFER_COPY       = 0x01,
    BUFFER_SHARE      = 0x02,
    BUFFER_COPY_FORCE = 0x10,
} EXYNOS_OMX_BUFFERPROCESS_TYPE;

#ifdef USE_S3D_SUPPORT
typedef enum _EXYNOS_OMX_FPARGMT_TYPE
{
    OMX_SEC_FPARGMT_INVALID           = -1,
    OMX_SEC_FPARGMT_CHECKERBRD_INTERL = 0x00,
    OMX_SEC_FPARGMT_COLUMN_INTERL     = 0x01,
    OMX_SEC_FPARGMT_ROW_INTERL        = 0x02,
    OMX_SEC_FPARGMT_SIDE_BY_SIDE      = 0x03,
    OMX_SEC_FPARGMT_TOP_BOTTOM        = 0x04,
    OMX_SEC_FPARGMT_TEMPORAL_INTERL   = 0x05,
    OMX_SEC_FPARGMT_NONE              = 0x0A
} EXYNOS_OMX_FPARGMT_TYPE;
#endif

typedef enum _EXYNOS_OMX_EVENTTYPE
{
   OMX_EventVendorStart    = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
#ifdef USE_KHRONOS_OMX_HEADER
   OMX_EventDataSpaceChanged,
#endif
#ifdef USE_S3D_SUPPORT
   OMX_EventS3DInformation,
#endif
} EXYNOS_OMX_EVENTTYPE;

typedef enum __EVENT_COMMAD_TYPE {
    EVENT_CMD_STATE_TO_LOAD_STATE,
    EVENT_CMD_STATE_TO_IDLE_STATE,
    EVENT_CMD_ENABLE_INPUT_PORT,
    EVENT_CMD_ENABLE_OUTPUT_PORT,
    EVENT_CMD_DISABLE_INPUT_PORT,
    EVENT_CMD_DISABLE_OUTPUT_PORT,
} EVENT_COMMAD_TYPE;

typedef enum _EXYNOS_METADATA_TYPE {
    METADATA_TYPE_DISABLED      = 0x0000,  /* 1. data buffer(ex: yuv, rgb) */
    METADATA_TYPE_DATA          = 0x0001,  /* 2. uses meta struct(type, [fd|handle|id]) */
    METADATA_TYPE_HANDLE        = 0x0002,  /* 3. naitve handle(fd) */

    METADATA_TYPE_BUFFER_LOCK   = 0x0100,  /* need to lock to get va */
    METADATA_TYPE_BUFFER_ID     = 0x1000,  /* need to get fd from id */

    METADATA_TYPE_GRAPHIC           = (METADATA_TYPE_DATA | METADATA_TYPE_BUFFER_LOCK),                            /* 4. uses meta struct(type, handle) */
    METADATA_TYPE_GRAPHIC_HANDLE    = (METADATA_TYPE_HANDLE | METADATA_TYPE_BUFFER_LOCK),                          /* 5. graphic buffer handle(fd) */
    METADATA_TYPE_UBM_BUFFER        = (METADATA_TYPE_DATA | METADATA_TYPE_BUFFER_LOCK | METADATA_TYPE_BUFFER_ID),  /* 6. uses meta struct(type, id) */
} EXYNOS_METADATA_TYPE;

typedef enum _EXYNOS_OMX_VIDEO_CONTROLRATETYPE {
    OMX_Video_ControlRateVendorStart    = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_Video_ControlRateConstantVTCall = 0x7F000001,
} EXYNOS_OMX_VIDEO_CONTROLRATETYPE;

typedef struct _EXYNOS_OMX_PRIORITYMGMTTYPE
{
    OMX_U32 nGroupPriority; /* the value 0 represents the highest priority */
                            /* for a group of components                   */
    OMX_U32 nGroupID;
} EXYNOS_OMX_PRIORITYMGMTTYPE;

typedef struct _EXYNOS_OMX_VIDEO_PROFILELEVEL
{
    OMX_S32  profile;
    OMX_S32  level;
} EXYNOS_OMX_VIDEO_PROFILELEVEL;

typedef struct _EXYNOS_OMX_LOCK_RANGE
{
    OMX_U32              nWidth;
    OMX_U32              nHeight;
    OMX_COLOR_FORMATTYPE eColorFormat;
} EXYNOS_OMX_LOCK_RANGE;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bNeedContigMem;
} EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE;

typedef struct _EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_PTR OMX_IN  pVirAddr;
    OMX_S32 OMX_OUT fd;
} EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_DTSMODE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL        bDTSMode;
} EXYNOS_OMX_VIDEO_PARAM_DTSMODE;

typedef struct _EXYNOS_OMX_VIDEO_THUMBNAILMODE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnable;
} EXYNOS_OMX_VIDEO_THUMBNAILMODE;

typedef struct _OMX_VIDEO_QPRANGE {
    OMX_U32 nMinQP;
    OMX_U32 nMaxQP;
} OMX_VIDEO_QPRANGE;

typedef struct _OMX_VIDEO_QPRANGETYPE {
    OMX_U32             nSize;
    OMX_VERSIONTYPE     nVersion;
    OMX_U32             nPortIndex;
    OMX_VIDEO_QPRANGE   qpRangeI;
    OMX_VIDEO_QPRANGE   qpRangeP;
    OMX_VIDEO_QPRANGE   qpRangeB;  /* H.264, HEVC, MPEG4 */
} OMX_VIDEO_QPRANGETYPE;

typedef struct __OMX_VIDEO_PARAM_CHROMA_QP_OFFSET { /* H.264, HEVC */
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_S32         nCr;
    OMX_S32         nCb;
} OMX_VIDEO_PARAM_CHROMA_QP_OFFSET;

/* Temporal SVC */
/* Maximum number of temporal layers */
#define OMX_VIDEO_MAX_TEMPORAL_LAYERS 7
#define OMX_VIDEO_MAX_TEMPORAL_LAYERS_WITH_LTR 3
#define OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS 7
#define OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS_FOR_B (OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS - 1)
#define OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS 7
#define OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS_FOR_B (OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS - 1)

typedef enum _EXYNOS_OMX_HIERARCHICAL_CODING_TYPE
{
    EXYNOS_OMX_Hierarchical_P = 0x00,
    EXYNOS_OMX_Hierarchical_B,
} EXYNOS_OMX_HIERARCHICAL_CODING_TYPE;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnableTemporalSVC;
} EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC;

typedef struct _EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_U32         nKeyFrameInterval;
    OMX_U32         nTemporalLayerCount;
    OMX_U32         nTemporalLayerBitrateRatio[OMX_VIDEO_MAX_TEMPORAL_LAYERS];
    OMX_U32         nMinQuantizer;
    OMX_U32         nMaxQuantizer;
} EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC;

typedef enum _EXYNOS_OMX_BLUR_MODE
{
    BLUR_MODE_NONE          = 0x00,
    BLUR_MODE_DOWNUP        = 0x01,
    BLUR_MODE_COEFFICIENT   = 0x02,
} EXYNOS_OMX_BLUR_MODE;

typedef enum _EXYNOS_OMX_BLUR_RESOL
{
    BLUR_RESOL_240     = 426 * 240,   /* 426 x 240 */
    BLUR_RESOL_480     = 854 * 480,   /* 854 x 480 */
    BLUR_RESOL_720     = 1280 * 720,  /* 1280 x 720 */
    BLUR_RESOL_960     = 1920 * 960,  /* 1920 x 960 */
    BLUR_RESOL_1080    = 1920 * 1080, /* 1920 x 1080 */
} EXYNOS_OMX_BLUR_RESOL;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER {
    OMX_U32                 nSize;
    OMX_VERSIONTYPE         nVersion;
    OMX_U32                 nPortIndex;
    OMX_BOOL                bUseBlurFilter;
} EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER;

typedef struct _EXYNOS_OMX_VIDEO_CONFIG_BLURINFO {
    OMX_U32                 nSize;
    OMX_VERSIONTYPE         nVersion;
    OMX_U32                 nPortIndex;
    EXYNOS_OMX_BLUR_MODE    eBlurMode;
    EXYNOS_OMX_BLUR_RESOL   eTargetResol;
} EXYNOS_OMX_VIDEO_CONFIG_BLURINFO;
/* ROI Information */

typedef struct _EXYNOS_OMX_VIDEO_CONFIG_ROIINFO {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_S32         nUpperQpOffset;
    OMX_S32         nLowerQpOffset;
    OMX_BOOL        bUseRoiInfo;
    OMX_S32         nRoiMBInfoSize;
    OMX_PTR         pRoiMBInfo;
} EXYNOS_OMX_VIDEO_CONFIG_ROIINFO;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnableRoiInfo;
} EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO;

typedef enum _EXYNOS_OMX_ROTATION_TYPE
{
    ROTATE_0        = 0,
    ROTATE_90       = 90,
    ROTATE_180      = 180,
    ROTATE_270      = 270,
} EXYNOS_OMX_ROTATION_TYPE;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO {
    OMX_U32                     nSize;
    OMX_VERSIONTYPE             nVersion;
    OMX_U32                     nPortIndex;
    EXYNOS_OMX_ROTATION_TYPE    eRotationType;
} EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_REORDERMODE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL        bReorderMode;
} EXYNOS_OMX_VIDEO_PARAM_REORDERMODE;

typedef enum _EXYNOS_OMX_IMG_CROP_PORT
{
    IMG_CROP_INPUT_PORT   = 0x00, //OMX_IndexConfigCommonInputCrop
    IMG_CROP_OUTPUT_PORT,         //OMX_IndexConfigCommonOutputCrop
    IMG_CROP_PORT_MAX,
} EXYNOS_OMX_IMG_CROP_PORT;

/* EXYNOS_OMX_IMG_INFO */
/*                           nStride
 *            ------------------------------------
 *              |    nImageWidth     | padding |
 *              |                    | xxxxxxx |
 *              | (top.left)---------| xxxxxxx |
 *              |           | width  | xxxxxxx |
 * nImageHeight |           | cccccc | xxxxxxx |
 *              |    height | cccccc | xxxxxxx | nSliceHeight
 *              |           | cccccc | xxxxxxx |
 *              |           ---------| xxxxxxx |
 *            --|--------------------| xxxxxxx |
 *              | xxxxxxxxxxxxxxxxxxxxxxxxxxxx |
 *              ----------------------------------
 */
typedef struct __EXYNOS_OMX_IMG_INFO {
    OMX_S32 nStride;
    OMX_S32 nSliceHeight;
    OMX_S32 nImageWidth;
    OMX_S32 nImageHeight;

    /* crop */
    OMX_S32 nLeft;
    OMX_S32 nTop;
    OMX_S32 nWidth;
    OMX_S32 nHeight;
} EXYNOS_OMX_IMG_INFO;

typedef OMX_CONFIG_BOOLEANTYPE EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP;

// codec specific
typedef enum _EXYNOS_OMX_VIDEO_CODINGTYPE {
#ifdef USE_KHRONOS_OMX_HEADER
    OMX_VIDEO_CodingVP9 = OMX_VIDEO_CodingVP8 + 1,  /**< Google VP9 */
    OMX_VIDEO_CodingHEVC,                           /**< ITU H.265/HEVC */
#endif
    OMX_VIDEO_VendorCodingMAX  = 0x7FFFFFFF,
} EXYNOS_OMX_VIDEO_CODINGTYPE;

/* for AVC */
#ifdef USE_KHRONOS_OMX_HEADER
typedef enum _EXYNOS_OMX_VIDEO_AVCPROFILETYPE {
    OMX_VIDEO_AVCProfileConstrainedBaseline = 0x10000,
    OMX_VIDEO_AVCProfileConstrainedHigh     = 0x80000,
} OMX_VIDEO_AVCPROFILEEXTTYPE;

typedef enum _EXYNOS_OMX_VIDEO_AVCLEVELTYPE {
    OMX_VIDEO_AVCLevel52  = 0x10000,  /**< Level 5.2 */
} EXYNOS_OMX_VIDEO_AVCLEVELTYPE;
#endif
// AVC end

/* for Mpeg4 */
#ifdef USE_KHRONOS_OMX_HEADER
typedef enum _EXYNOS_OMX_VIDEO_MPEG4LEVELTYPE {
    OMX_VIDEO_MPEG4Level3b  = 0x18,     /**< Level 3a */
    OMX_VIDEO_MPEG4Level6   = 0x100,   /**< Level 6 */
} EXYNOS_OMX_VIDEO_MPEG4LEVELTYPE;
#endif
// Mpeg4 end

/* for Mpeg2 */
#ifdef USE_KHRONOS_OMX_HEADER
typedef enum _EXYNOS_OMX_VIDEO_MPEG2LEVELTYPE {
    OMX_VIDEO_MPEG2LevelHP      = OMX_VIDEO_MPEG2LevelHL + 1,      /**< HighP Level */
} EXYNOS_OMX_VIDEO_MPEG2LEVELTYPE;
#endif
// Mpeg2 end

/* for HEVC */
#ifdef USE_KHRONOS_OMX_HEADER
typedef enum _OMX_VIDEO_HEVCPROFILETYPE {
    OMX_VIDEO_HEVCProfileUnknown           = 0x0,
    OMX_VIDEO_HEVCProfileMain              = 0x1,          /**< Main profile */
    OMX_VIDEO_HEVCProfileMain10            = 0x2,          /**< Main 10 profile */
    OMX_VIDEO_HEVCProfileMainStillPicture  = 0x4,          /**< Main Still Picture */
    // Main10 profile with HDR SEI support.
    OMX_VIDEO_HEVCProfileMain10HDR10       = 0x1000,        /**< Main10 profile with HDR SEI support */
    OMX_VIDEO_HEVCProfileKhronosExtensions = 0x6F000000,    /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_HEVCProfileVendorStartUnused = 0x7F000000,    /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_HEVCProfileMax               = 0x7FFFFFFF
} OMX_VIDEO_HEVCPROFILETYPE;

/** HEVC Level enum type */
typedef enum _OMX_VIDEO_HEVCLEVELTYPE {
    OMX_VIDEO_HEVCLevelUnknown              = 0x00000000,
    OMX_VIDEO_HEVCMainTierLevel1            = 0x00000001,   /**< Level 1 */
    OMX_VIDEO_HEVCHighTierLevel1            = 0x00000002,
    OMX_VIDEO_HEVCMainTierLevel2            = 0x00000004,   /**< Level 2 */
    OMX_VIDEO_HEVCHighTierLevel2            = 0x00000008,
    OMX_VIDEO_HEVCMainTierLevel21           = 0x00000010,   /**< Level 2.1 */
    OMX_VIDEO_HEVCHighTierLevel21           = 0x00000020,
    OMX_VIDEO_HEVCMainTierLevel3            = 0x00000040,   /**< Level 3 */
    OMX_VIDEO_HEVCHighTierLevel3            = 0x00000080,
    OMX_VIDEO_HEVCMainTierLevel31           = 0x00000100,   /**< Level 3.1 */
    OMX_VIDEO_HEVCHighTierLevel31           = 0x00000200,
    OMX_VIDEO_HEVCMainTierLevel4            = 0x00000400,   /**< Level 4 */
    OMX_VIDEO_HEVCHighTierLevel4            = 0x00000800,
    OMX_VIDEO_HEVCMainTierLevel41           = 0x00001000,   /**< Level 4.1 */
    OMX_VIDEO_HEVCHighTierLevel41           = 0x00002000,
    OMX_VIDEO_HEVCMainTierLevel5            = 0x00004000,   /**< Level 5 */
    OMX_VIDEO_HEVCHighTierLevel5            = 0x00008000,
    OMX_VIDEO_HEVCMainTierLevel51           = 0x00010000,   /**< Level 5.1 */
    OMX_VIDEO_HEVCHighTierLevel51           = 0x00020000,
    OMX_VIDEO_HEVCMainTierLevel52           = 0x00040000,   /**< Level 5.2 */
    OMX_VIDEO_HEVCHighTierLevel52           = 0x00080000,
    OMX_VIDEO_HEVCMainTierLevel6            = 0x00100000,   /**< Level 6 */
    OMX_VIDEO_HEVCHighTierLevel6            = 0x00200000,
    OMX_VIDEO_HEVCMainTierLevel61           = 0x00400000,   /**< Level 6.1 */
    OMX_VIDEO_HEVCHighTierLevel61           = 0x00800000,
    OMX_VIDEO_HEVCMainTierLevel62           = 0x01000000,   /**< Level 6.2 */
    OMX_VIDEO_HEVCHighTierLevel62           = 0x02000000,
    OMX_VIDEO_HEVCLevelKhronosExtensions    = 0x6F000000,   /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_HEVCLevelVendorStartUnused    = 0x7F000000,   /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_HEVCHighMAX                   = 0x7FFFFFFF
} OMX_VIDEO_HEVCLEVELTYPE;

/** Structure for controlling HEVC video encoding and decoding */
typedef struct _OMX_VIDEO_PARAM_HEVCTYPE {
    OMX_U32                     nSize;
    OMX_VERSIONTYPE             nVersion;
    OMX_U32                     nPortIndex;
    OMX_VIDEO_HEVCPROFILETYPE   eProfile;
    OMX_VIDEO_HEVCLEVELTYPE     eLevel;
    OMX_U32                     nKeyFrameInterval;  // distance between consecutive I-frames (including one
                                                    // of the I frames). 0 means interval is unspecified and
                                                    // can be freely chosen by the codec. 1 means a stream of
                                                    // only I frames.
} OMX_VIDEO_PARAM_HEVCTYPE;
#endif // hevc end

/* for VP9 */
#ifdef USE_KHRONOS_OMX_HEADER
typedef enum _OMX_VIDEO_VP9PROFILETYPE {
    OMX_VIDEO_VP9Profile0                   = 0x0,
    OMX_VIDEO_VP9Profile1                   = 0x1,
    OMX_VIDEO_VP9Profile2                   = 0x2,
    OMX_VIDEO_VP9Profile3                   = 0x4,
    // HDR profiles also support passing HDR metadata
    OMX_VIDEO_VP9Profile2HDR                = 0x1000,
    OMX_VIDEO_VP9Profile3HDR                = 0x2000,
    OMX_VIDEO_VP9Profile2HDR10Plus          = 0x4000,
    OMX_VIDEO_VP9Profile3HDR10Plus          = 0x8000,
    OMX_VIDEO_VP9ProfileUnknown             = 0x6EFFFFFF,
    OMX_VIDEO_VP9ProfileKhronosExtensions   = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_VP9ProfileVendorStartUnused   = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VP9ProfileMax                 = 0x7FFFFFFF
} OMX_VIDEO_VP9PROFILETYPE;

    /* VP9 levels */
typedef enum _OMX_VIDEO_VP9LEVELTYPE {
    OMX_VIDEO_VP9Level1                     = 0x0,
    OMX_VIDEO_VP9Level11                    = 0x1,
    OMX_VIDEO_VP9Level2                     = 0x2,
    OMX_VIDEO_VP9Level21                    = 0x4,
    OMX_VIDEO_VP9Level3                     = 0x8,
    OMX_VIDEO_VP9Level31                    = 0x10,
    OMX_VIDEO_VP9Level4                     = 0x20,
    OMX_VIDEO_VP9Level41                    = 0x40,
    OMX_VIDEO_VP9Level5                     = 0x80,
    OMX_VIDEO_VP9Level51                    = 0x100,
    OMX_VIDEO_VP9Level52                    = 0x200,
    OMX_VIDEO_VP9Level6                     = 0x400,
    OMX_VIDEO_VP9Level61                    = 0x800,
    OMX_VIDEO_VP9Level62                    = 0x1000,
    OMX_VIDEO_VP9LevelUnknown               = 0x6EFFFFFF,
    OMX_VIDEO_VP9LevelKhronosExtensions     = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_VP9LevelVendorStartUnused     = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VP9LevelMax                   = 0x7FFFFFFF
} OMX_VIDEO_VP9LEVELTYPE;

typedef struct _OMX_VIDEO_PARAM_VP9TYPE {
    OMX_U32                     nSize;
    OMX_VERSIONTYPE             nVersion;
    OMX_U32                     nPortIndex;
    OMX_VIDEO_VP9PROFILETYPE    eProfile;
    OMX_VIDEO_VP9LEVELTYPE      eLevel;
    OMX_BOOL                    bErrorResilientMode;
} OMX_VIDEO_PARAM_VP9TYPE;
#endif // vp9 end
#ifndef USE_KHRONOS_OMX_1_2
/* WMV codec */
/** WMV Profile enum type */
typedef enum _OMX_VIDEO_WMVPROFILETYPE {
    OMX_VIDEO_WMVProfileSimple = 0,
    OMX_VIDEO_WMVProfileMain,
    OMX_VIDEO_WMVProfileAdvanced,
    OMX_VIDEO_WMVProfileUnknown           = 0x6EFFFFFF,
    OMX_VIDEO_WMVProfileKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_WMVProfileVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
} OMX_VIDEO_WMVPROFILETYPE;

/** WMV Level enum type */
typedef enum _OMX_VIDEO_WMVLEVELTYPE {
    OMX_VIDEO_WMVLevelLow = 0,
    OMX_VIDEO_WMVLevelMedium,
    OMX_VIDEO_WMVLevelHigh,
    OMX_VIDEO_WMVLevelL0,
    OMX_VIDEO_WMVLevelL1,
    OMX_VIDEO_WMVLevelL2,
    OMX_VIDEO_WMVLevelL3,
    OMX_VIDEO_WMVLevelL4,
    OMX_VIDEO_WMVLevelUnknown           = 0x6EFFFFFF,
    OMX_VIDEO_WMVLevelKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_WMVLevelVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
} OMX_VIDEO_WMVLEVELTYPE;

/* VC1 codec */
/** VC1 Profile enum type */
typedef enum _OMX_VIDEO_VC1PROFILETYPE {
    OMX_VIDEO_VC1ProfileUnused = 0,
    OMX_VIDEO_VC1ProfileSimple,
    OMX_VIDEO_VC1ProfileMain,
    OMX_VIDEO_VC1ProfileAdvanced,
    OMX_VIDEO_VC1ProfileUnknown           = 0x6EFFFFFF,
    OMX_VIDEO_VC1ProfileKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_VC1ProfileVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VC1ProfileMax
} OMX_VIDEO_VC1PROFILETYPE;

/** VC1 Level enum type */
typedef enum _OMX_VIDEO_VC1LEVELTYPE {
    OMX_VIDEO_VC1LevelUnused = 0,
    OMX_VIDEO_VC1LevelLow,
    OMX_VIDEO_VC1LevelMedium,
    OMX_VIDEO_VC1LevelHigh,
    OMX_VIDEO_VC1Level0,
    OMX_VIDEO_VC1Level1,
    OMX_VIDEO_VC1Level2,
    OMX_VIDEO_VC1Level3,
    OMX_VIDEO_VC1Level4,
    OMX_VIDEO_VC1LevelUnknown           = 0x6EFFFFFF,
    OMX_VIDEO_VC1LevelKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_VC1LevelVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VC1LevelMax
} OMX_VIDEO_VC1LEVELTYPE;

/** Structure for controlling VC1 video encoding and decoding */
typedef struct _OMX_VIDEO_PARAM_VC1TYPE {
    OMX_U32                     nSize;
    OMX_VERSIONTYPE             nVersion;
    OMX_U32                     nPortIndex;
    OMX_VIDEO_VC1PROFILETYPE    eProfile;
    OMX_VIDEO_VC1LEVELTYPE      eLevel;
} OMX_VIDEO_PARAM_VC1TYPE;
#endif  // USE_KHRONOS_OMX_1_2
// codec specific end



// for android
typedef struct _EXYNOS_OMX_VIDEO_PRIMARIES {
    OMX_U16     x;
    OMX_U16     y;
} EXYNOS_OMX_VIDEO_PRIMARIES;

typedef struct _EXYNOS_OMX_VIDEO_HDRSTATICINFO {
    OMX_U32                      nMaxPicAverageLight;
    OMX_U32                      nMaxContentLight;
    OMX_U32                      nMaxDisplayLuminance;
    OMX_U32                      nMinDisplayLuminance;

    EXYNOS_OMX_VIDEO_PRIMARIES   red;
    EXYNOS_OMX_VIDEO_PRIMARIES   green;
    EXYNOS_OMX_VIDEO_PRIMARIES   blue;
    EXYNOS_OMX_VIDEO_PRIMARIES   white;
} EXYNOS_OMX_VIDEO_HDRSTATICINFO;

typedef struct _EXYNOS_OMX_VIDEO_COLORASPECTS {
    OMX_U32 nRangeType;
    OMX_U32 nPrimaryType;
    OMX_U32 nTransferType;
    OMX_U32 nCoeffType;
    OMX_U32 nDataSpace;
} EXYNOS_OMX_VIDEO_COLORASPECTS;

#define MAX_HDR10PLUS_SIZE 1024
typedef struct _EXYNOS_OMX_VIDEO_HDR10PLUS_INFO {
    OMX_BOOL bOccupied;
    OMX_U32  nTag;
    OMX_PTR  pHDR10PlusInfo;
} EXYNOS_OMX_VIDEO_HDR10PLUS_INFO;

#ifdef USE_KHRONOS_OMX_HEADER
/**
 * Structure for configuring video compression intra refresh period
 *
 * STRUCT MEMBERS:
 *  nSize               : Size of the structure in bytes
 *  nVersion            : OMX specification version information
 *  nPortIndex          : Port that this structure applies to
 *  nRefreshPeriod      : Intra refreh period in frames. Value 0 means disable intra refresh
*/
typedef struct _OMX_VIDEO_CONFIG_ANDROID_INTRAREFRESHTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_U32         nRefreshPeriod;
} OMX_VIDEO_CONFIG_ANDROID_INTRAREFRESHTYPE;

/** Maximum number of VP8 temporal layers */
#define OMX_VIDEO_ANDROID_MAXVP8TEMPORALLAYERS 3

/** VP8 temporal layer patterns */
typedef enum _OMX_VIDEO_ANDROID_VPXTEMPORALLAYERPATTERNTYPE {
    OMX_VIDEO_VPXTemporalLayerPatternNone   = 0,
    OMX_VIDEO_VPXTemporalLayerPatternWebRTC = 1,
    OMX_VIDEO_VPXTemporalLayerPatternMax    = 0x7FFFFFFF
} OMX_VIDEO_ANDROID_VPXTEMPORALLAYERPATTERNTYPE;

/**
 * Android specific VP8/VP9 encoder params
 *
 * STRUCT MEMBERS:
 *  nSize                      : Size of the structure in bytes
 *  nVersion                   : OMX specification version information
 *  nPortIndex                 : Port that this structure applies to
 *  nKeyFrameInterval          : Key frame interval in frames
 *  eTemporalPattern           : Type of temporal layer pattern
 *  nTemporalLayerCount        : Number of temporal coding layers
 *  nTemporalLayerBitrateRatio : Bitrate ratio allocation between temporal
 *                               streams in percentage
 *  nMinQuantizer              : Minimum (best quality) quantizer
 *  nMaxQuantizer              : Maximum (worst quality) quantizer
 */
typedef struct _OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_U32         nKeyFrameInterval;      // distance between consecutive key_frames (including one
                                            // of the key_frames). 0 means interval is unspecified and
                                            // can be freely chosen by the codec. 1 means a stream of
                                            // only key_frames.
    OMX_VIDEO_ANDROID_VPXTEMPORALLAYERPATTERNTYPE eTemporalPattern;
    OMX_U32         nTemporalLayerCount;
    OMX_U32         nTemporalLayerBitrateRatio[OMX_VIDEO_ANDROID_MAXVP8TEMPORALLAYERS];
    OMX_U32         nMinQuantizer;
    OMX_U32         nMaxQuantizer;
} OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE;

/** Structure to define if dependent slice segments should be used */
typedef struct _OMX_VIDEO_SLICESEGMENTSTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bDepedentSegments;
    OMX_BOOL        bEnableLoopFilterAcrossSlices;
} OMX_VIDEO_SLICESEGMENTSTYPE;

#define OMX_MAX_STRINGVALUE_SIZE OMX_MAX_STRINGNAME_SIZE
#define OMX_MAX_ANDROID_VENDOR_PARAMCOUNT 32

typedef enum _OMX_ANDROID_VENDOR_VALUETYPE {
    OMX_AndroidVendorValueInt32 = 0,   /*<< int32_t value */
    OMX_AndroidVendorValueInt64,       /*<< int64_t value */
    OMX_AndroidVendorValueString,      /*<< string value */
    OMX_AndroidVendorValueEndUnused,
} OMX_ANDROID_VENDOR_VALUETYPE;

/**
 * Structure describing a single value of an Android vendor extension.
 *
 * STRUCTURE MEMBERS:
 *  cKey        : parameter value name.
 *  eValueType  : parameter value type
 *  bSet        : if false, the parameter is not set (for OMX_GetConfig) or is unset (OMX_SetConfig)
 *                if true, the parameter is set to the corresponding value below
 *  nInt64      : int64 value
 *  cString     : string value
 */
typedef struct _OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE {
    OMX_U8 cKey[OMX_MAX_STRINGNAME_SIZE];
    OMX_ANDROID_VENDOR_VALUETYPE eValueType;
    OMX_BOOL bSet;
    union {
        OMX_S32 nInt32;
        OMX_S64 nInt64;
        OMX_U8 cString[OMX_MAX_STRINGVALUE_SIZE];
    };
} OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE;

/**
 * OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE is the structure for an Android vendor extension
 * supported by the component. This structure enumerates the various extension parameters and their
 * values.
 *
 * Android vendor extensions have a name and one or more parameter values - each with a string key -
 * that are set together. The values are exposed to Android applications via a string key that is
 * the concatenation of 'vendor', the extension name and the parameter key, each separated by dot
 * (.), with any trailing '.value' suffix(es) removed (though optionally allowed).
 *
 * Extension names and parameter keys are subject to the following rules:
 *   - Each SHALL contain a set of lowercase alphanumeric (underscore allowed) tags separated by
 *     dot (.) or dash (-).
 *   - The first character of the first tag, and any tag following a dot SHALL not start with a
 *     digit.
 *   - Tags 'value', 'vendor', 'omx' and 'android' (even if trailed and/or followed by any number
 *     of underscores) are prohibited in the extension name.
 *   - Tags 'vendor', 'omx' and 'android' (even if trailed and/or followed by any number
 *     of underscores) are prohibited in parameter keys.
 *   - The tag 'value' (even if trailed and/or followed by any number
 *     of underscores) is prohibited in parameter keys with the following exception:
 *     the parameter key may be exactly 'value'
 *   - The parameter key for extensions with a single parameter value SHALL be 'value'
 *   - No two extensions SHALL have the same name
 *   - No extension's name SHALL start with another extension's NAME followed by a dot (.)
 *   - No two parameters of an extension SHALL have the same key
 *
 * This config can be used with both OMX_GetConfig and OMX_SetConfig. In the OMX_GetConfig
 * case, the caller specifies nIndex and nParamSizeUsed. The component fills in cName,
 * eDir and nParamCount. Additionally, if nParamSizeUsed is not less than nParamCount, the
 * component fills out the parameter values (nParam) with the current values for each parameter
 * of the vendor extension.
 *
 * The value of nIndex goes from 0 to N-1, where N is the number of Android vendor extensions
 * supported by the component. The component does not need to report N as the caller can determine
 * N by enumerating all extensions supported by the component. The component may not support any
 * extensions. If there are no more extensions, OMX_GetParameter returns OMX_ErrorNoMore. The
 * component supplies extensions in the order it wants clients to set them.
 *
 * The component SHALL return OMX_ErrorNone for all cases where nIndex is less than N (specifically
 * even in the case of where nParamCount is greater than nParamSizeUsed).
 *
 * In the OMX_SetConfig case the field nIndex is ignored. If the component supports an Android
 * vendor extension with the name in cName, it SHALL configure the parameter values for that
 * extension according to the parameters in nParam. nParamCount is the number of valid parameters
 * in the nParam array, and nParamSizeUsed is the size of the nParam array. (nParamSizeUsed
 * SHALL be at least nParamCount) Parameters that are part of a vendor extension but are not
 * in the nParam array are assumed to be unset (this is different from not changed).
 * All parameter values SHALL have distinct keys in nParam (the component can assume that this
 * is the case. Otherwise, the actual value for the parameters that are multiply defined can
 * be any of the set values.)
 *
 * Return values in case of OMX_SetConfig:
 *   OMX_ErrorUnsupportedIndex: the component does not support the extension specified by cName
 *   OMX_ErrorUnsupportedSetting: the component does not support some or any of the parameters
 *       (names) specified in nParam
 *   OMX_ErrorBadParameter: the parameter is invalid (e.g. nParamCount is greater than
 *       nParamSizeUsed, or some parameter value has invalid type)
 *
 * STRUCTURE MEMBERS:
 *  nSize       : size of the structure in bytes
 *  nVersion    : OMX specification version information
 *  cName       : name of vendor extension
 *  nParamCount : the number of parameter values that are part of this vendor extension
 *  nParamSizeUsed : the size of nParam
 *                (must be at least 1 and at most OMX_MAX_ANDROID_VENDOR_PARAMCOUNT)
 *  param       : the parameter values
 */
typedef struct _OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nIndex;
    OMX_U8  cName[OMX_MAX_STRINGNAME_SIZE];
    OMX_DIRTYPE eDir;
    OMX_U32 nParamCount;
    OMX_U32 nParamSizeUsed;
    OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE param[1];
} OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE;

/** Maximum number of temporal layers supported by AVC/HEVC */
#define OMX_VIDEO_ANDROID_MAXTEMPORALLAYERS 8

/** temporal layer patterns */
typedef enum OMX_VIDEO_ANDROID_TEMPORALLAYERINGPATTERNTYPE {
    OMX_VIDEO_AndroidTemporalLayeringPatternNone = 0,
    // pattern as defined by WebRTC
    OMX_VIDEO_AndroidTemporalLayeringPatternWebRTC = 1 << 0,
    // pattern where frames in any layer other than the base layer only depend on at most the very
    // last frame from each preceding layer (other than the base layer.)
    OMX_VIDEO_AndroidTemporalLayeringPatternAndroid = 1 << 1,
} OMX_VIDEO_ANDROID_TEMPORALLAYERINGPATTERNTYPE;

/**
 * Android specific param for configuration of temporal layering.
 * Android only supports temporal layering where successive layers each double the
 * previous layer's framerate.
 * NOTE: Reading this parameter at run-time SHALL return actual run-time values.
 *
 *  nSize                      : Size of the structure in bytes
 *  nVersion                   : OMX specification version information
 *  nPortIndex                 : Port that this structure applies to (output port for encoders)
 *  eSupportedPatterns         : A bitmask of supported layering patterns
 *  nLayerCountMax             : Max number of temporal coding layers supported
 *                               by the encoder (must be at least 1, 1 meaning temporal layering
 *                               is NOT supported)
 *  nBLayerCountMax            : Max number of layers that can contain B frames
 *                               (0) to (nLayerCountMax - 1)
 *  ePattern                   : Layering pattern.
 *  nPLayerCountActual         : Number of temporal layers to be coded with non-B frames,
 *                               starting from and including the base-layer.
 *                               (1 to nLayerCountMax - nBLayerCountActual)
 *                               If nPLayerCountActual is 1 and nBLayerCountActual is 0, temporal
 *                               layering is disabled. Otherwise, it is enabled.
 *  nBLayerCountActual         : Number of temporal layers to be coded with B frames,
 *                               starting after non-B layers.
 *                               (0 to nBLayerCountMax)
 *  bBitrateRatiosSpecified    : Flag to indicate if layer-wise bitrate
 *                               distribution is specified.
 *  nBitrateRatios             : Bitrate ratio (100 based) per layer (index 0 is base layer).
 *                               Honored if bBitrateRatiosSpecified is set.
 *                               i.e for 4 layers with desired distribution (25% 25% 25% 25%),
 *                               nBitrateRatio = {25, 50, 75, 100, ... }
 *                               Values in indices not less than 'the actual number of layers
 *                               minus 1' MAY be ignored and assumed to be 100.
 */
typedef struct OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_ANDROID_TEMPORALLAYERINGPATTERNTYPE eSupportedPatterns;
    OMX_U32 nLayerCountMax;
    OMX_U32 nBLayerCountMax;
    OMX_VIDEO_ANDROID_TEMPORALLAYERINGPATTERNTYPE ePattern;
    OMX_U32 nPLayerCountActual;
    OMX_U32 nBLayerCountActual;
    OMX_BOOL bBitrateRatiosSpecified;
    OMX_U32 nBitrateRatios[OMX_VIDEO_ANDROID_MAXTEMPORALLAYERS];
} OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE;

/**
 * Android specific config for changing the temporal-layer count or
 * bitrate-distribution at run-time.
 *
 *  nSize                      : Size of the structure in bytes
 *  nVersion                   : OMX specification version information
 *  nPortIndex                 : Port that this structure applies to (output port for encoders)
 *  ePattern                   : Layering pattern.
 *  nPLayerCountActual         : Number of temporal layers to be coded with non-B frames.
 *                               (same OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE limits apply.)
 *  nBLayerCountActual         : Number of temporal layers to be coded with B frames.
 *                               (same OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE limits apply.)
 *  bBitrateRatiosSpecified    : Flag to indicate if layer-wise bitrate
 *                               distribution is specified.
 *  nBitrateRatios             : Bitrate ratio (100 based, Q16 values) per layer (0 is base layer).
 *                               Honored if bBitrateRatiosSpecified is set.
 *                               (same OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE limits apply.)
 */
typedef struct OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_ANDROID_TEMPORALLAYERINGPATTERNTYPE ePattern;
    OMX_U32 nPLayerCountActual;
    OMX_U32 nBLayerCountActual;
    OMX_BOOL bBitrateRatiosSpecified;
    OMX_U32 nBitrateRatios[OMX_VIDEO_ANDROID_MAXTEMPORALLAYERS];
} OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE;

#endif
// android end



// for custom component
typedef struct _EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL        bDiscardEvent;
} EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER;

typedef struct _OMX_VIDEO_PARAM_IMG_CONV {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnable;
    OMX_U32         nWidth;
    OMX_U32         nHeight;
} OMX_VIDEO_PARAM_IMG_CONV;

// custom component end



// for Skype HD
typedef struct OMX_VIDEO_PARAM_DRIVERVER {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U64 nDriverVersion;
} OMX_VIDEO_PARAM_DRIVERVER;

typedef struct OMX_VIDEO_CONFIG_MARKLTRFRAME {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nLongTermFrmIdx;
} OMX_VIDEO_CONFIG_MARKLTRFRAME;

typedef struct OMX_VIDEO_CONFIG_USELTRFRAME {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U16 nUsedLTRFrameBM;
} OMX_VIDEO_CONFIG_USELTRFRAME;

typedef struct OMX_VIDEO_CONFIG_QP {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nQP;
} OMX_VIDEO_CONFIG_QP;

typedef struct OMX_VIDEO_CONFIG_BASELAYERPID{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nPID;
} OMX_VIDEO_CONFIG_BASELAYERPID;

typedef struct __OMX_PARAM_ENC_MAX_TEMPORALLAYER_COUNT {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nMaxCountP;
    OMX_U32 nMaxCountB;
} OMX_PARAM_ENC_MAX_TEMPORALLAYER_COUNT;

typedef struct __OMX_PARAM_ENC_PREPROCESS {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nResize;
    OMX_U32 nRotation;
} OMX_PARAM_ENC_PREPROCESS;

typedef struct __OMX_PARAM_ENC_SAR {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nWidth;
    OMX_U32 nHeight;
} OMX_PARAM_ENC_SAR;

typedef struct __OMX_CONFIG_ENC_TRIGGER_TS {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_S64 nTimestamp;
} OMX_CONFIG_ENC_TRIGGER_TS;

#ifndef __OMX_EXPORTS
#define __OMX_EXPORTS
#define EXYNOS_EXPORT_REF __attribute__((visibility("default")))
#define EXYNOS_IMPORT_REF __attribute__((visibility("default")))
#endif

#endif
