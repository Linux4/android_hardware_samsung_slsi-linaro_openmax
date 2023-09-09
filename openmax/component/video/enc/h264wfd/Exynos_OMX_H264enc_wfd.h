/*
 *
 * Copyright 2017 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OMX_H264enc_wfd.h
 * @brief
 * @author      SeungBeom Kim  (sbcrux.kim@samsung.com)
 *              Taehwan Kim    (t_h.kim@samsung.com)
 *              ByungGwan Kang (bk0917.kang@samsung.com)
 * @version     2.0.0
 * @history
 *   2017.06.20 : Create
 */

#ifndef EXYNOS_OMX_H264_WFD_ENC_COMPONENT
#define EXYNOS_OMX_H264_WFD_ENC_COMPONENT

#include "Exynos_OMX_Def.h"
#include "OMX_Component.h"
#include "OMX_Video.h"

#include "ExynosVideoApi.h"
#include "library_register.h"

typedef struct _EXYNOS_MFC_H264WFDENC_HANDLE
{
    OMX_HANDLETYPE hMFCHandle;

    OMX_U32 indexTimestamp;
    OMX_U32 outputIndexTimestamp;
    OMX_BOOL bPrependSpsPpsToIdr;
    OMX_BOOL bTemporalSVC;
    OMX_BOOL bRoiInfo;
    OMX_U32  nLTRFrames;

    ExynosVideoEncOps       *pEncOps;
    ExynosVideoEncBufferOps *pInbufOps;
    ExynosVideoEncBufferOps *pOutbufOps;
    ExynosVideoEncParam      encParam;
    ExynosVideoInstInfo      videoInstInfo;

    #define MAX_PROFILE_NUM 5
    OMX_VIDEO_AVCPROFILETYPE   profiles[MAX_PROFILE_NUM];
    OMX_S32                    nProfileCnt;
    OMX_VIDEO_AVCLEVELTYPE     maxLevel;
} EXYNOS_MFC_H264WFDENC_HANDLE;

typedef struct _EXYNOS_H264WFDENC_HANDLE
{
    /* OMX Codec specific */
    OMX_VIDEO_PARAM_AVCTYPE             AVCComponent[ALL_PORT_NUM];
    OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE errorCorrectionType[ALL_PORT_NUM];
    OMX_VIDEO_PARAM_AVCSLICEFMO         AVCSliceFmo;
    OMX_VIDEO_QPRANGE                   qpRangeI;
    OMX_VIDEO_QPRANGE                   qpRangeP;
    OMX_VIDEO_QPRANGE                   qpRangeB;
    EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC TemporalSVC;    /* Temporal SVC */

    /* SEC MFC Codec specific */
    EXYNOS_MFC_H264WFDENC_HANDLE hMFCH264Handle;

    EXYNOS_QUEUE bypassBufferInfoQ;

    OMX_BOOL                            bLowLatency;
    EXYNOS_OMX_HIERARCHICAL_CODING_TYPE eHierarchicalType;

    OMX_S32                             nBaseLayerPid;
    OMX_U32                             nMaxTemporalLayerCount;
} EXYNOS_H264WFDENC_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(OMX_HANDLETYPE hComponent, OMX_STRING componentName);
OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE Exynos_OMX_Port_Constructor(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OMX_Port_Destructor(OMX_HANDLETYPE hComponent);
int Exynos_GetPlaneFromPort(EXYNOS_OMX_BASEPORT *pPort);
OMX_ERRORTYPE Exynos_SetPlaneToPort(EXYNOS_OMX_BASEPORT *pPort, int nPlaneNum);
OMX_ERRORTYPE Exynos_OMX_EmptyThisBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_BUFFERHEADERTYPE *pBuffer);
OMX_ERRORTYPE Exynos_OMX_FillThisBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_BUFFERHEADERTYPE *pBuffer);


OMX_ERRORTYPE Exynos_OMX_BaseComponent_Constructor(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OMX_BaseComponent_Destructor(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OMX_Check_SizeVersion(OMX_PTR header, OMX_U32 size);

void Exynos_UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent);
void Exynos_Input_SetSupportFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent);
OMX_COLOR_FORMATTYPE Exynos_Input_GetActualColorFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent);
OMX_ERRORTYPE Exynos_ResetAllPortConfig(OMX_COMPONENTTYPE *pOMXComponent);
OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentDeinit(OMX_IN OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE Exynos_OMX_UseBuffer(
        OMX_IN OMX_HANDLETYPE            hComponent,
        OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
        OMX_IN OMX_U32                   nPortIndex,
        OMX_IN OMX_PTR                   pAppPrivate,
        OMX_IN OMX_U32                   nSizeBytes,
        OMX_IN OMX_U8                   *pBuffer);
OMX_ERRORTYPE Exynos_OMX_AllocateBuffer(
        OMX_IN OMX_HANDLETYPE            hComponent,
        OMX_INOUT OMX_BUFFERHEADERTYPE **ppBuffer,
        OMX_IN OMX_U32                   nPortIndex,
        OMX_IN OMX_PTR                   pAppPrivate,
        OMX_IN OMX_U32                   nSizeBytes);
OMX_ERRORTYPE Exynos_OMX_FreeBuffer(
        OMX_IN OMX_HANDLETYPE        hComponent,
        OMX_IN OMX_U32               nPortIndex,
        OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr);

#ifdef __cplusplus
};
#endif

#endif
