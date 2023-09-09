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
 * @file        Exynos_OMX_H264enc.h
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#ifndef EXYNOS_OMX_H264_ENC_COMPONENT
#define EXYNOS_OMX_H264_ENC_COMPONENT

#include "Exynos_OMX_Def.h"
#include "OMX_Component.h"
#include "OMX_Video.h"

#include "ExynosVideoApi.h"
#include "library_register.h"


typedef struct _H264_VUI_SAR {
    OMX_BOOL SarEnable;
    OMX_U32  SarIndex;
    OMX_U32  SarWidth;
    OMX_U32  SarHeight;
} H264_VUI_SAR;

typedef struct _EXTRA_DATA
{
    OMX_PTR   pHeaderSPS;
    OMX_U32   SPSLen;
    OMX_PTR   pHeaderPPS;
    OMX_U32   PPSLen;
} EXTRA_DATA;

typedef struct _EXYNOS_MFC_H264ENC_HANDLE
{
    OMX_HANDLETYPE hMFCHandle;

    OMX_U32 indexTimestamp;
    OMX_U32 outputIndexTimestamp;

    OMX_BOOL bConfiguredMFCSrc;
    OMX_BOOL bConfiguredMFCDst;

    OMX_BOOL bPrependSpsPpsToIdr;
    OMX_BOOL bTemporalSVC;
    OMX_BOOL bRoiInfo;
    OMX_BOOL bWeightedPrediction;

    /* skypeHD */
    OMX_BOOL                            bEnableSkypeHD;
    OMX_S64                             nTriggerTS;
    OMX_U32                             nLTRFrames;
    OMX_BOOL                            bLowLatency;
    EXYNOS_OMX_HIERARCHICAL_CODING_TYPE eHierarchicalType;
    H264_VUI_SAR                        stSarParam;
    OMX_S32                             nBaseLayerPid;
    OMX_BOOL                            bSkypeBitrate;

    EXTRA_DATA headerData;

    ExynosVideoEncOps       *pEncOps;
    ExynosVideoEncBufferOps *pInbufOps;
    ExynosVideoEncBufferOps *pOutbufOps;
    ExynosVideoEncParam      encParam;
    ExynosVideoInstInfo      videoInstInfo;

    #define MAX_PROFILE_NUM 5
    OMX_VIDEO_AVCPROFILETYPE   profiles[MAX_PROFILE_NUM];
    OMX_S32                    nProfileCnt;
    OMX_VIDEO_AVCLEVELTYPE     maxLevel;
} EXYNOS_MFC_H264ENC_HANDLE;

typedef struct _EXYNOS_H264ENC_HANDLE
{
    /* OMX Codec specific */
    OMX_VIDEO_PARAM_AVCTYPE             AVCComponent[ALL_PORT_NUM];
    OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE errorCorrectionType[ALL_PORT_NUM];
    OMX_VIDEO_PARAM_AVCSLICEFMO         AVCSliceFmo;
    OMX_VIDEO_QPRANGE                   qpRangeI;
    OMX_VIDEO_QPRANGE                   qpRangeP;
    OMX_VIDEO_QPRANGE                   qpRangeB;
    OMX_VIDEO_PARAM_CHROMA_QP_OFFSET    chromaQPOffset;
    OMX_BOOL                            bDisableHBEncoding;

    /* Temporal SVC */
    OMX_U32  nMaxTemporalLayerCount;
    OMX_U32  nMaxTemporalLayerCountForB;
    OMX_U32  nTemporalLayerCount;
    OMX_U32  nTemporalLayerCountForB;
    OMX_BOOL bUseTemporalLayerBitrateRatio;
    OMX_U32  nTemporalLayerBitrateRatio[OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS];

    /* SEC MFC Codec specific */
    EXYNOS_MFC_H264ENC_HANDLE hMFCH264Handle;

    OMX_BOOL bSourceStart;
    OMX_BOOL bDestinationStart;
    OMX_HANDLETYPE hSourceStartEvent;
    OMX_HANDLETYPE hDestinationInStartEvent;
    OMX_HANDLETYPE hDestinationOutStartEvent;

    EXYNOS_QUEUE bypassBufferInfoQ;
} EXYNOS_H264ENC_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(OMX_HANDLETYPE hComponent, OMX_STRING componentName);
OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(OMX_HANDLETYPE hComponent);

#ifdef __cplusplus
};
#endif

#endif
