/*
 *
 * Copyright 2013 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OMX_HEVCdec.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Taehwan Kim (t_h.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2013.07.26 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Vdec.h"
#include "Exynos_OMX_VdecControl.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "library_register.h"
#include "Exynos_OMX_HEVCdec.h"
#include "ExynosVideoApi.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"

#include "Exynos_OSAL_Platform.h"

#include "VendorVideoAPI.h"

/* To use CSC_METHOD_HW in EXYNOS OMX */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_HEVC_DEC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


static OMX_ERRORTYPE SetProfileLevel(
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec       = NULL;

    int nProfileCnt = 0;

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain;
    pHevcDec->hMFCHevcHandle.nProfileCnt = nProfileCnt;

    switch (pHevcDec->hMFCHevcHandle.videoInstInfo.HwVersion) {
    case MFC_1501:
    case MFC_150:
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10;
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10;
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10Plus;
        pHevcDec->hMFCHevcHandle.nProfileCnt = nProfileCnt;

        pHevcDec->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel61;
        break;
    case MFC_1400:
    case MFC_1410:
    case MFC_140:
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10;
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10;
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10Plus;
        pHevcDec->hMFCHevcHandle.nProfileCnt = nProfileCnt;

        pHevcDec->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel6;
        break;
    case MFC_130:
    case MFC_120:
    case MFC_1220:
    case MFC_110:
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10;
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10;
        pHevcDec->hMFCHevcHandle.nProfileCnt = nProfileCnt;

        pHevcDec->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel6;
        break;
    case MFC_101:
    case MFC_100:
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10;
        pHevcDec->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10;
        pHevcDec->hMFCHevcHandle.nProfileCnt = nProfileCnt;

        pHevcDec->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel51;
        break;
    case HEVC_10:
    case MFC_90:
    case MFC_1010:
    case MFC_1120:
        pHevcDec->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel5;
        break;
    case MFC_1011:
    case MFC_1020:
    case MFC_1021:
        pHevcDec->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel41;
        break;
    case MFC_92:
    default:
        pHevcDec->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel4;
        break;
    }

EXIT:

    return ret;
}

static OMX_ERRORTYPE GetIndexToProfileLevel(
    EXYNOS_OMX_BASECOMPONENT         *pExynosComponent,
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevelType)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec       = NULL;

    int nLevelCnt = 0;
    OMX_U32 nMaxIndex = 0;

    FunctionIn();

    if ((pExynosComponent == NULL) ||
        (pProfileLevelType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (pHevcDec->hMFCHevcHandle.nProfileCnt <= (int)pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pHevcDec->hMFCHevcHandle.profiles[pProfileLevelType->nProfileIndex];
    pProfileLevelType->eLevel   = pHevcDec->hMFCHevcHandle.maxLevel;
#else
    while ((pHevcDec->hMFCHevcHandle.maxLevel >> nLevelCnt) > 0) {
        nLevelCnt++;
    }

    if ((pHevcDec->hMFCHevcHandle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] there is no any profile/level",
                                        pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    nMaxIndex = pHevcDec->hMFCHevcHandle.nProfileCnt * nLevelCnt;
    if (nMaxIndex <= pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pHevcDec->hMFCHevcHandle.profiles[pProfileLevelType->nProfileIndex / nLevelCnt];
    pProfileLevelType->eLevel = 0x1 << (pProfileLevelType->nProfileIndex % nLevelCnt);
#endif

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] supported profile(%x), level(%x)",
                            pExynosComponent, __FUNCTION__, pProfileLevelType->eProfile, pProfileLevelType->eLevel);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_BOOL CheckProfileLevelSupport(
    EXYNOS_OMX_BASECOMPONENT         *pExynosComponent,
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevelType)
{
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec  = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec   = NULL;

    OMX_BOOL bProfileSupport = OMX_FALSE;
    OMX_BOOL bLevelSupport   = OMX_FALSE;

    int nLevelCnt = 0;
    int i;

    FunctionIn();

    if ((pExynosComponent == NULL) ||
        (pProfileLevelType == NULL)) {
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL)
        goto EXIT;

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL)
        goto EXIT;

    while ((pHevcDec->hMFCHevcHandle.maxLevel >> nLevelCnt++) > 0);

    if ((pHevcDec->hMFCHevcHandle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pHevcDec->hMFCHevcHandle.nProfileCnt; i++) {
        if (pHevcDec->hMFCHevcHandle.profiles[i] == pProfileLevelType->eProfile) {
            bProfileSupport = OMX_TRUE;
            break;
        }
    }

    if (bProfileSupport != OMX_TRUE)
        goto EXIT;

    while (nLevelCnt >= 0) {
        if ((int)pProfileLevelType->eLevel == (0x1 << nLevelCnt)) {
            bLevelSupport = OMX_TRUE;
            break;
        }

        nLevelCnt--;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] profile(%x)/level(%x) is %ssupported", pExynosComponent, __FUNCTION__,
                                            pProfileLevelType->eProfile, pProfileLevelType->eLevel,
                                            (bProfileSupport && bLevelSupport)? "":"not ");

EXIT:
    FunctionOut();

    return (bProfileSupport && bLevelSupport);
}

static OMX_ERRORTYPE GetCodecOutputPrivateData(
    OMX_PTR     codecBuffer,
    OMX_PTR     addr[],
    OMX_U32     size[])
{
    OMX_ERRORTYPE       ret             = OMX_ErrorNone;
    ExynosVideoBuffer  *pCodecBuffer    = NULL;

    if (codecBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pCodecBuffer = (ExynosVideoBuffer *)codecBuffer;

    if (addr != NULL) {
        addr[0] = pCodecBuffer->planes[0].addr;
        addr[1] = pCodecBuffer->planes[1].addr;
        addr[2] = pCodecBuffer->planes[2].addr;
    }

    if (size != NULL) {
        size[0] = pCodecBuffer->planes[0].allocSize;
        size[1] = pCodecBuffer->planes[1].allocSize;
        size[2] = pCodecBuffer->planes[2].allocSize;
    }

EXIT:
    return ret;
}

static OMX_BOOL Check_HEVC_StartCode(
    OMX_U8     *pInputStream,
    OMX_U32     streamSize)
{
    OMX_BOOL ret = OMX_FALSE;

    FunctionIn();

    if (streamSize < 4) {
        ret = OMX_FALSE;
        goto EXIT;
    }

    if ((pInputStream[0] == 0x00) &&
        (pInputStream[1] == 0x00) &&
        (pInputStream[2] == 0x01))
        ret = OMX_TRUE;

    if ((pInputStream[0] == 0x00) &&
        (pInputStream[1] == 0x00) &&
        (pInputStream[2] == 0x00) &&
        (pInputStream[3] == 0x01))
        ret = OMX_TRUE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_BOOL CheckFormatHWSupport(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_COLOR_FORMATTYPE         eColorFormat)
{
    OMX_BOOL                         ret            = OMX_FALSE;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec       = NULL;
    EXYNOS_OMX_BASEPORT             *pOutputPort    = NULL;
    ExynosVideoColorFormatType       eVideoFormat   = VIDEO_COLORFORMAT_UNKNOWN;
    int i;

    if (pExynosComponent == NULL)
        goto EXIT;

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL)
        goto EXIT;

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL)
        goto EXIT;
    pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    eVideoFormat = (ExynosVideoColorFormatType)Exynos_OSAL_OMX2VideoFormat(eColorFormat, pOutputPort->ePlaneType);

    for (i = 0; i < VIDEO_COLORFORMAT_MAX; i++) {
        if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportFormat[i] == VIDEO_COLORFORMAT_UNKNOWN)
            break;

        if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportFormat[i] == eVideoFormat) {
            ret = OMX_TRUE;
            break;
        }
    }

EXIT:

    return ret;
}

OMX_ERRORTYPE HevcCodecOpen(
    EXYNOS_HEVCDEC_HANDLE *pHevcDec,
    ExynosVideoInstInfo   *pVideoInstInfo)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if ((pHevcDec == NULL) ||
        (pVideoInstInfo == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    /* alloc ops structure */
    pDecOps    = (ExynosVideoDecOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecOps));
    pInbufOps  = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));
    pOutbufOps = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));

    if ((pDecOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to allocate decoder ops buffer", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.pDecOps    = pDecOps;
    pHevcDec->hMFCHevcHandle.pInbufOps  = pInbufOps;
    pHevcDec->hMFCHevcHandle.pOutbufOps = pOutbufOps;

    /* function pointer mapping */
    pDecOps->nSize    = sizeof(ExynosVideoDecOps);
    pInbufOps->nSize  = sizeof(ExynosVideoDecBufferOps);
    pOutbufOps->nSize = sizeof(ExynosVideoDecBufferOps);

    if (Exynos_Video_Register_Decoder(pDecOps, pInbufOps, pOutbufOps) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to get decoder ops", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* check mandatory functions for decoder ops */
    if ((pDecOps->Init == NULL) || (pDecOps->Finalize == NULL) ||
        (pDecOps->Get_ActualBufferCount == NULL) ||
#ifdef USE_S3D_SUPPORT
        (pDecOps->Enable_SEIParsing == NULL) || (pDecOps->Get_FramePackingInfo == NULL) ||
#endif
        (pDecOps->Set_FrameTag == NULL) || (pDecOps->Get_FrameTag == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Mandatory functions must be supplied", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* check mandatory functions for buffer ops */
    if ((pInbufOps->Setup == NULL) || (pOutbufOps->Setup == NULL) ||
        (pInbufOps->Run == NULL) || (pOutbufOps->Run == NULL) ||
        (pInbufOps->Stop == NULL) || (pOutbufOps->Stop == NULL) ||
        (pInbufOps->Enqueue == NULL) || (pOutbufOps->Enqueue == NULL) ||
        (pInbufOps->Dequeue == NULL) || (pOutbufOps->Dequeue == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Mandatory functions must be supplied", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* alloc context, open, querycap */
#ifdef USE_DMA_BUF
    pVideoInstInfo->nMemoryType = VIDEO_MEMORY_DMABUF;
#else
    pVideoInstInfo->nMemoryType = VIDEO_MEMORY_USERPTR;
#endif
    pHevcDec->hMFCHevcHandle.hMFCHandle = pHevcDec->hMFCHevcHandle.pDecOps->Init(pVideoInstInfo);

    if (pHevcDec->hMFCHevcHandle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to init", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

#ifdef USE_S3D_SUPPORT
    /* S3D: Enable SEI parsing to check Frame Packing */
    if (pDecOps->Enable_SEIParsing(pHevcDec->hMFCHevcHandle.hMFCHandle) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Enable SEI Parsing", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
#endif

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pDecOps != NULL) {
            Exynos_OSAL_Free(pDecOps);
            pHevcDec->hMFCHevcHandle.pDecOps = NULL;
        }
        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pHevcDec->hMFCHevcHandle.pInbufOps = NULL;
        }
        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pHevcDec->hMFCHevcHandle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecClose(EXYNOS_HEVCDEC_HANDLE *pHevcDec)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcDec->hMFCHevcHandle.hMFCHandle;
    pDecOps    = pHevcDec->hMFCHevcHandle.pDecOps;
    pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pDecOps->Finalize(hMFCHandle);
        pHevcDec->hMFCHevcHandle.hMFCHandle = NULL;
        pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc = OMX_FALSE;
        pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_FALSE;
    }

    /* Unregister function pointers */
    Exynos_Video_Unregister_Decoder(pDecOps, pInbufOps, pOutbufOps);

    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pHevcDec->hMFCHevcHandle.pOutbufOps = NULL;
    }
    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pHevcDec->hMFCHevcHandle.pInbufOps = NULL;
    }
    if (pDecOps != NULL) {
        Exynos_OSAL_Free(pDecOps);
        pHevcDec->hMFCHevcHandle.pDecOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecStart(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;
    void                            *hMFCHandle         = NULL;
    ExynosVideoDecBufferOps         *pInbufOps          = NULL;
    ExynosVideoDecBufferOps         *pOutbufOps         = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcDec->hMFCHevcHandle.hMFCHandle;
    pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc == OMX_TRUE)) {
        pInbufOps->Run(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE)) {
        pOutbufOps->Run(hMFCHandle);
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecStop(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;
    void                            *hMFCHandle         = NULL;
    ExynosVideoDecBufferOps         *pInbufOps          = NULL;
    ExynosVideoDecBufferOps         *pOutbufOps         = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcDec->hMFCHevcHandle.hMFCHandle;
    pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) && (pInbufOps != NULL)) {
        pInbufOps->Stop(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) && (pOutbufOps != NULL)) {
        EXYNOS_OMX_BASEPORT *pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

        pOutbufOps->Stop(hMFCHandle);

        if (pOutputPort->bufferProcessType == BUFFER_SHARE)
            pOutbufOps->Clear_RegisteredBuffer(hMFCHandle);
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecOutputBufferProcessRun(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pHevcDec->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pHevcDec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pHevcDec->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        } else if (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecReconfigAllBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = &pExynosComponent->pExynosPort[nPortIndex];
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    ExynosVideoDecBufferOps         *pBufferOps         = NULL;

    FunctionIn();

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pHevcDec->bSourceStart == OMX_TRUE)) {
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pHevcDec->bDestinationStart == OMX_TRUE)) {
        pBufferOps  = pHevcDec->hMFCHevcHandle.pOutbufOps;

        if (pExynosPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
            /**********************************/
            /* Codec Buffer Free & Unregister */
            /**********************************/
            Exynos_Free_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX);
            Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);
            pBufferOps->Clear_RegisteredBuffer(hMFCHandle);
            pBufferOps->Cleanup_Buffer(hMFCHandle);

            pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_FALSE;

            /******************************************************/
            /* V4L2 Destnation Setup for DPB Buffer Number Change */
            /******************************************************/
            ret = HevcCodecDstSetup(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to HevcCodecDstSetup(0x%x)",
                                                    pExynosComponent, __FUNCTION__, ret);
                goto EXIT;
            }

            pVideoDec->bReconfigDPB = OMX_FALSE;
        } else if (pExynosPort->bufferProcessType == BUFFER_SHARE) {
            /***************************/
            /* Codec Buffer Unregister */
            /***************************/
            pBufferOps->Clear_RegisteredBuffer(hMFCHandle);
            pBufferOps->Cleanup_Buffer(hMFCHandle);

            pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_FALSE;
        }
    } else {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecEnQueueAllBuffer(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    int i;

    ExynosVideoDecBufferOps *pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    FunctionIn();

    if ((nPortIndex != INPUT_PORT_INDEX) && (nPortIndex != OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] CodecBuffer(input) [%d]: FD(0x%x), VA(0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                i, pVideoDec->pMFCDecInputBuffer[i]->fd[0], pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
        }

        pInbufOps->Clear_Queue(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);

        for (i = 0; i < pHevcDec->hMFCHevcHandle.maxDPBNum; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] CodecBuffer(output) [%d]: FD(0x%x), VA(0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                i, pVideoDec->pMFCDecOutputBuffer[i]->fd[0], pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnQueue(pExynosComponent, OUTPUT_PORT_INDEX, pVideoDec->pMFCDecOutputBuffer[i]);
        }
        pOutbufOps->Clear_Queue(hMFCHandle);
    }

EXIT:
    FunctionOut();

    return ret;
}

#ifdef USE_S3D_SUPPORT
OMX_BOOL HevcCodecCheckFramePacking(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    ExynosVideoDecOps             *pDecOps           = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoFramePacking        framePacking;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    OMX_BOOL                       ret               = OMX_FALSE;

    FunctionIn();

    /* Get Frame packing information*/
    if (pDecOps->Get_FramePackingInfo(pHevcDec->hMFCHevcHandle.hMFCHandle, &framePacking) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Get Frame Packing Information", pExynosComponent, __FUNCTION__);
        ret = OMX_FALSE;
        goto EXIT;
    }

    if (framePacking.available) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] arrangement ID: 0x%08x",
                                            pExynosComponent, __FUNCTION__, framePacking.arrangement_id);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] arrangement_type: %d",
                                            pExynosComponent, __FUNCTION__, framePacking.arrangement_type);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] content_interpretation_type: %d",
                                            pExynosComponent, __FUNCTION__, framePacking.content_interpretation_type);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] current_frame_is_frame0_flag: %d",
                                            pExynosComponent, __FUNCTION__, framePacking.current_frame_is_frame0_flag);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] spatial_flipping_flag: %d",
                                            pExynosComponent, __FUNCTION__, framePacking.spatial_flipping_flag);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] fr0X:%d fr0Y:%d fr0X:%d fr0Y:%d",
                                            pExynosComponent, __FUNCTION__, framePacking.frame0_grid_pos_x,
                                            framePacking.frame0_grid_pos_y, framePacking.frame1_grid_pos_x, framePacking.frame1_grid_pos_y);

        pHevcDec->hMFCHevcHandle.S3DFPArgmtType = (EXYNOS_OMX_FPARGMT_TYPE) framePacking.arrangement_type;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventS3DInformation)",
                                                pExynosComponent, __FUNCTION__);
        /** Send Port Settings changed call back - output color format change */
        (*(pExynosComponent->pCallbacks->EventHandler))
              (pOMXComponent,
               pExynosComponent->callbackData,
               (OMX_EVENTTYPE)OMX_EventS3DInformation,              /* The command was completed */
               OMX_TRUE,                                            /* S3D is enabled */
               (OMX_S32)pHevcDec->hMFCHevcHandle.S3DFPArgmtType,    /* S3D FPArgmtType */
               NULL);

        Exynos_OSAL_SleepMillisec(0);
    } else {
        pHevcDec->hMFCHevcHandle.S3DFPArgmtType = OMX_SEC_FPARGMT_NONE;
    }

    ret = OMX_TRUE;

EXIT:
    FunctionOut();

    return ret;
}
#endif

void HevcCodecUpdateHdrInfo(
    OMX_COMPONENTTYPE *pOMXComponent,
    DECODE_CODEC_EXTRA_BUFFERINFO *pBufferInfo)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort       = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pInputPort        = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;

    ExynosVideoDecOps  *pDecOps     = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoHdrInfo *pHdrInfo    = &(pHevcDec->hMFCHevcHandle.sHdrInfo);

    if (pDecOps->Get_HDRInfo(hMFCHandle, pHdrInfo) == VIDEO_ERROR_NONE) {
        /* update bitstream's info to input port */
        EXYNOS_OMX_VIDEO_HDRSTATICINFO   *pHDRStaticInfo = &(pOutputPort->HDRStaticInfo);
        EXYNOS_OMX_VIDEO_COLORASPECTS    *pColorAspects  = &(pInputPort->ColorAspects);

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] eType(0x%x) is changed", pExynosComponent, __FUNCTION__, pHdrInfo->eChangedType);

        /* color aspects */
        if (pHdrInfo->eValidType & (HDR_INFO_COLOR_ASPECTS | HDR_INFO_RANGE)) {
            /* HDR_INFO_COLOR_ASPECTS (8) */
            pColorAspects->nCoeffType      = pHdrInfo->sColorAspects.eCoeffType;
            pColorAspects->nPrimaryType    = pHdrInfo->sColorAspects.ePrimariesType;
            pColorAspects->nTransferType   = pHdrInfo->sColorAspects.eTransferType;

            /* HDR_INFO_RANGE (16) */
            pColorAspects->nRangeType      = pHdrInfo->sColorAspects.eRangeType;

            if (pOutputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) {
                /* update dataspace in output port from input port */
                Exynos_OSAL_UpdateDataSpaceFromBitstream(pExynosComponent);
            }
        }

        /* hdr static info */
        if (pHdrInfo->eValidType & (HDR_INFO_LIGHT | HDR_INFO_LUMINANCE)) {
            /* HDR_INFO_LIGHT (1) */
            pHDRStaticInfo->nMaxPicAverageLight  = pHdrInfo->sHdrStatic.max_pic_average_light;
            pHDRStaticInfo->nMaxContentLight     = pHdrInfo->sHdrStatic.max_content_light;

            /* HDR_INFO_LUMINANCE (2) */
            pHDRStaticInfo->nMaxDisplayLuminance = pHdrInfo->sHdrStatic.max_display_luminance;
            pHDRStaticInfo->nMinDisplayLuminance = pHdrInfo->sHdrStatic.min_display_luminance;

            pHDRStaticInfo->red.x  = pHdrInfo->sHdrStatic.red.x;
            pHDRStaticInfo->red.y  = pHdrInfo->sHdrStatic.red.y;

            pHDRStaticInfo->green.x = pHdrInfo->sHdrStatic.green.x;
            pHDRStaticInfo->green.y = pHdrInfo->sHdrStatic.green.y;

            pHDRStaticInfo->blue.x  = pHdrInfo->sHdrStatic.blue.x;
            pHDRStaticInfo->blue.y  = pHdrInfo->sHdrStatic.blue.y;

            pHDRStaticInfo->white.x = pHdrInfo->sHdrStatic.white.x;
            pHDRStaticInfo->white.y = pHdrInfo->sHdrStatic.white.y;
        }

        /* hdr dynamic info */
        if (pHdrInfo->eValidType & HDR_INFO_DYNAMIC_META) {
            ExynosHdrDynamicInfo    *pMetaHDRDynamic    = &(pBufferInfo->HDRDynamic); /* HDR Dynamic info(HDR10+) to extra info */
            ExynosVideoHdrDynamic   *pHDRDynamicInfo    = &(pHdrInfo->sHdrDynamic);

            /* actual validity in bitstream */
            pMetaHDRDynamic->valid = (pHdrInfo->eValidType & HDR_INFO_DYNAMIC_META)? 1:0;

            pMetaHDRDynamic->data.country_code              = pHDRDynamicInfo->itu_t_t35_country_code;
            pMetaHDRDynamic->data.provider_code             = pHDRDynamicInfo->itu_t_t35_terminal_provider_code;
            pMetaHDRDynamic->data.provider_oriented_code    = pHDRDynamicInfo->itu_t_t35_terminal_provider_oriented_code;
            pMetaHDRDynamic->data.application_identifier    = pHDRDynamicInfo->application_identifier;
            pMetaHDRDynamic->data.application_version       = pHDRDynamicInfo->application_version;
#ifdef USE_FULL_ST2094_40
            pMetaHDRDynamic->data.targeted_system_display_maximum_luminance = pHDRDynamicInfo->targeted_system_display_maximum_luminance;

            if (pHDRDynamicInfo->num_windows > 0) {
                /* save information on window-0 only */
                ExynosVideoHdrWindowInfo *pWindowInfo = &(pHDRDynamicInfo->window_info[0]);
                int i;

                /* maxscl */
                for (i = 0; i < (int)(sizeof(pMetaHDRDynamic->data.maxscl)/sizeof(pMetaHDRDynamic->data.maxscl[0])); i++)
                    pMetaHDRDynamic->data.maxscl[0][i] = pWindowInfo->maxscl[i];

                /* distribution maxrgb */
                pMetaHDRDynamic->data.num_maxrgb_percentiles[0] = pWindowInfo->num_distribution_maxrgb_percentiles;
                for (i = 0; i < pWindowInfo->num_distribution_maxrgb_percentiles; i++) {
                    pMetaHDRDynamic->data.maxrgb_percentages[0][i] = pWindowInfo->distribution_maxrgb_percentages[i];
                    pMetaHDRDynamic->data.maxrgb_percentiles[0][i] = pWindowInfo->distribution_maxrgb_percentiles[i];
                }

                /* tone mapping curve */
                pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag[0] = pWindowInfo->tone_mapping_flag;
                if (pWindowInfo->tone_mapping_flag != 0) {
                    pMetaHDRDynamic->data.tone_mapping.knee_point_x[0] = pWindowInfo->knee_point_x;
                    pMetaHDRDynamic->data.tone_mapping.knee_point_y[0] = pWindowInfo->knee_point_y;

                    pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors[0] = pWindowInfo->num_bezier_curve_anchors;
                    for (i = 0; i < pWindowInfo->num_bezier_curve_anchors; i++)
                        pMetaHDRDynamic->data.tone_mapping.bezier_curve_anchors[0][i] = pWindowInfo->bezier_curve_anchors[i];
                }
            }
#else // USE_FULL_ST2094_40
            pMetaHDRDynamic->data.display_maximum_luminance = pHDRDynamicInfo->targeted_system_display_maximum_luminance;

            if (pHDRDynamicInfo->num_windows > 0) {
                /* save information on window-0 only */
                ExynosVideoHdrWindowInfo *pWindowInfo = &(pHDRDynamicInfo->window_info[0]);
                int i;

                /* maxscl */
                for (i = 0; i < (int)(sizeof(pMetaHDRDynamic->data.maxscl)/sizeof(pMetaHDRDynamic->data.maxscl[0])); i++)
                    pMetaHDRDynamic->data.maxscl[i] = pWindowInfo->maxscl[i];

                /* distribution maxrgb */
                pMetaHDRDynamic->data.num_maxrgb_percentiles = pWindowInfo->num_distribution_maxrgb_percentiles;
                for (i = 0; i < pWindowInfo->num_distribution_maxrgb_percentiles; i++) {
                    pMetaHDRDynamic->data.maxrgb_percentages[i] = pWindowInfo->distribution_maxrgb_percentages[i];
                    pMetaHDRDynamic->data.maxrgb_percentiles[i] = pWindowInfo->distribution_maxrgb_percentiles[i];
                }

                /* tone mapping curve */
                pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag = pWindowInfo->tone_mapping_flag;
                if (pWindowInfo->tone_mapping_flag != 0) {
                    pMetaHDRDynamic->data.tone_mapping.knee_point_x = pWindowInfo->knee_point_x;
                    pMetaHDRDynamic->data.tone_mapping.knee_point_y = pWindowInfo->knee_point_y;

                    pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors = pWindowInfo->num_bezier_curve_anchors;
                    for (i = 0; i < pWindowInfo->num_bezier_curve_anchors; i++)
                        pMetaHDRDynamic->data.tone_mapping.bezier_curve_anchors[i] = pWindowInfo->bezier_curve_anchors[i];
                }
            }
#endif
        }

#ifdef USE_ANDROID
        /* if both have changed, should send an event once */
        if (pHdrInfo->eChangedType & (HDR_INFO_COLOR_ASPECTS | HDR_INFO_RANGE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_IndexConfigVideoColorAspects)",
                                                    pExynosComponent, __FUNCTION__);
            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 OMX_IndexConfigVideoColorAspects,
                 NULL);
        } else if (pHdrInfo->eChangedType & (HDR_INFO_LIGHT | HDR_INFO_LUMINANCE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_IndexConfigVideoHdrStaticInfo)",
                                                    pExynosComponent, __FUNCTION__);
            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 OMX_IndexConfigVideoHdrStaticInfo,
                 NULL);
        }
#endif
    }

EXIT:

    return;
}

OMX_ERRORTYPE HevcCodecUpdateExtraInfo(
    OMX_COMPONENTTYPE   *pOMXComponent,
    ExynosVideoMeta     *pMeta)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;

    if (pMeta == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pMeta->eType = VIDEO_INFO_TYPE_INVALID;

    /* HDR */
    {
        ExynosType1         *pMetaHDR = &(pMeta->sHdrStaticInfo.sType1);
        ExynosColorAspects  *pMetaCA  = &(pMeta->sColorAspects);

        if (pHevcDec->hMFCHevcHandle.sHdrInfo.eValidType & (HDR_INFO_COLOR_ASPECTS | HDR_INFO_RANGE)) {
            ExynosVideoColorAspects *pColorAspects = &(pHevcDec->hMFCHevcHandle.sHdrInfo.sColorAspects);

            pMetaCA->mMatrixCoeffs = pColorAspects->eCoeffType;
            pMetaCA->mPrimaries    = pColorAspects->ePrimariesType;
            pMetaCA->mTransfer     = pColorAspects->eTransferType;
            pMetaCA->mRange        = pColorAspects->eRangeType;

            //pMeta->eType |= VIDEO_INFO_TYPE_COLOR_ASPECTS;
        }

        if (pHevcDec->hMFCHevcHandle.sHdrInfo.eValidType & (HDR_INFO_LIGHT | HDR_INFO_LUMINANCE)) {
            ExynosVideoHdrStatic *pHDRStaticInfo = &(pHevcDec->hMFCHevcHandle.sHdrInfo.sHdrStatic);

            /* HDR_INFO_LIGHT (1) */
            pMetaHDR->mMaxFrameAverageLightLevel = pHDRStaticInfo->max_pic_average_light;
            pMetaHDR->mMaxContentLightLevel      = pHDRStaticInfo->max_content_light;

            /* HDR_INFO_LUMINANCE (2) */
            pMetaHDR->mMaxDisplayLuminance       = pHDRStaticInfo->max_display_luminance;
            pMetaHDR->mMinDisplayLuminance       = pHDRStaticInfo->min_display_luminance;

            pMetaHDR->mR.x = pHDRStaticInfo->red.x;
            pMetaHDR->mR.y = pHDRStaticInfo->red.y;

            pMetaHDR->mG.x = pHDRStaticInfo->green.x;
            pMetaHDR->mG.y = pHDRStaticInfo->green.y;

            pMetaHDR->mB.x = pHDRStaticInfo->blue.x;
            pMetaHDR->mB.y = pHDRStaticInfo->blue.y;

            pMetaHDR->mW.x = pHDRStaticInfo->white.x;
            pMetaHDR->mW.y = pHDRStaticInfo->white.y;

            pMeta->eType |= VIDEO_INFO_TYPE_HDR_STATIC;
        }

        /* hdr dynamic info */
        if (pHevcDec->hMFCHevcHandle.sHdrInfo.sHdrDynamic.valid != 0) {  /* once dynamic info is set, it will be always true */
            ExynosHdrDynamicInfo    *pMetaHDRDynamic    = &(pMeta->sHdrDynamicInfo); /* HDR Dynamic info(HDR10+) to private buffer */
            ExynosVideoHdrDynamic   *pHDRDynamicInfo    = &(pHevcDec->hMFCHevcHandle.sHdrInfo.sHdrDynamic);

            /* actual validity in bitstream */
            pMetaHDRDynamic->valid = (pHevcDec->hMFCHevcHandle.sHdrInfo.eValidType & HDR_INFO_DYNAMIC_META)? 1:0;

            pMetaHDRDynamic->data.country_code           = pHDRDynamicInfo->itu_t_t35_country_code;
            pMetaHDRDynamic->data.provider_code          = pHDRDynamicInfo->itu_t_t35_terminal_provider_code;
            pMetaHDRDynamic->data.provider_oriented_code = pHDRDynamicInfo->itu_t_t35_terminal_provider_oriented_code;
            pMetaHDRDynamic->data.application_identifier = pHDRDynamicInfo->application_identifier;
            pMetaHDRDynamic->data.application_version    = pHDRDynamicInfo->application_version;

#ifdef USE_FULL_ST2094_40
            pMetaHDRDynamic->data.targeted_system_display_maximum_luminance = pHDRDynamicInfo->targeted_system_display_maximum_luminance;

            pMetaHDRDynamic->data.num_windows = pHDRDynamicInfo->num_windows;

            if (pHDRDynamicInfo->num_windows > 0) {
                /* save information on window-0 only */
                ExynosVideoHdrWindowInfo *pWindowInfo = &(pHDRDynamicInfo->window_info[0]);
                int i;

                /* maxscl */
                for (i = 0; i < (int)(sizeof(pMetaHDRDynamic->data.maxscl)/sizeof(pMetaHDRDynamic->data.maxscl[0])); i++)
                    pMetaHDRDynamic->data.maxscl[0][i] = pWindowInfo->maxscl[i];

                /* average_maxrgb */
                pMetaHDRDynamic->data.average_maxrgb[0] = pWindowInfo->average_maxrgb;

                /* distribution maxrgb */
                pMetaHDRDynamic->data.num_maxrgb_percentiles[0] = pWindowInfo->num_distribution_maxrgb_percentiles;
                for (i = 0; i < pWindowInfo->num_distribution_maxrgb_percentiles; i++) {
                    pMetaHDRDynamic->data.maxrgb_percentages[0][i] = pWindowInfo->distribution_maxrgb_percentages[i];
                    pMetaHDRDynamic->data.maxrgb_percentiles[0][i] = pWindowInfo->distribution_maxrgb_percentiles[i];
                }

                /* fraction_bright_pixels */
                pMetaHDRDynamic->data.fraction_bright_pixels[0] = pWindowInfo->fraction_bright_pixels;

                /* tone mapping curve */
                pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag[0] = pWindowInfo->tone_mapping_flag;
                if (pWindowInfo->tone_mapping_flag != 0) {
                    pMetaHDRDynamic->data.tone_mapping.knee_point_x[0] = pWindowInfo->knee_point_x;
                    pMetaHDRDynamic->data.tone_mapping.knee_point_y[0] = pWindowInfo->knee_point_y;

                    pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors[0] = pWindowInfo->num_bezier_curve_anchors;
                    for (i = 0; i < pWindowInfo->num_bezier_curve_anchors; i++)
                        pMetaHDRDynamic->data.tone_mapping.bezier_curve_anchors[0][i] = pWindowInfo->bezier_curve_anchors[i];
                }

                /* color_saturation info */
                pMetaHDRDynamic->data.color_saturation_mapping_flag[0] = pWindowInfo->color_saturation_mapping_flag;
                if (pWindowInfo->color_saturation_mapping_flag != 0) {
                    pMetaHDRDynamic->data.color_saturation_weight[0] = pWindowInfo->color_saturation_weight;
                }
            }
#else // USE_FULL_ST2094_40
            pMetaHDRDynamic->data.display_maximum_luminance = pHDRDynamicInfo->targeted_system_display_maximum_luminance;

            if (pHDRDynamicInfo->num_windows > 0) {
                /* save information on window-0 only */
                ExynosVideoHdrWindowInfo *pWindowInfo = &(pHDRDynamicInfo->window_info[0]);
                int i;

                /* maxscl */
                for (i = 0; i < (int)(sizeof(pMetaHDRDynamic->data.maxscl)/sizeof(pMetaHDRDynamic->data.maxscl[0])); i++)
                    pMetaHDRDynamic->data.maxscl[i] = pWindowInfo->maxscl[i];

                /* distribution maxrgb */
                pMetaHDRDynamic->data.num_maxrgb_percentiles = pWindowInfo->num_distribution_maxrgb_percentiles;
                for (i = 0; i < pWindowInfo->num_distribution_maxrgb_percentiles; i++) {
                    pMetaHDRDynamic->data.maxrgb_percentages[i] = pWindowInfo->distribution_maxrgb_percentages[i];
                    pMetaHDRDynamic->data.maxrgb_percentiles[i] = pWindowInfo->distribution_maxrgb_percentiles[i];
                }

                /* tone mapping curve */
                pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag = pWindowInfo->tone_mapping_flag;
                if (pWindowInfo->tone_mapping_flag != 0) {
                    pMetaHDRDynamic->data.tone_mapping.knee_point_x = pWindowInfo->knee_point_x;
                    pMetaHDRDynamic->data.tone_mapping.knee_point_y = pWindowInfo->knee_point_y;

                    pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors = pWindowInfo->num_bezier_curve_anchors;
                    for (i = 0; i < pWindowInfo->num_bezier_curve_anchors; i++)
                        pMetaHDRDynamic->data.tone_mapping.bezier_curve_anchors[i] = pWindowInfo->bezier_curve_anchors[i];
                }
            }
#endif

            pMeta->eType |= VIDEO_INFO_TYPE_HDR_DYNAMIC;

#ifdef USE_ANDROID
            if (pHevcDec->hMFCHevcHandle.sHdrInfo.eChangedType & HDR_INFO_DYNAMIC_META) {
                ret = setHDR10PlusInfoForFramework(pOMXComponent, pMetaHDRDynamic);
                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Failed to set HDR10+ info for framework(ret: 0x%x)", pExynosComponent, __FUNCTION__, ret);
                }
            }
#endif
        }
    }

    /* Normal format for SBWC black bar */
    {
        if (pHevcDec->hMFCHevcHandle.nActualFormat != 0) {
            pMeta->nPixelFormat  = pHevcDec->hMFCHevcHandle.nActualFormat;
            pMeta->eType        |= VIDEO_INFO_TYPE_CHECK_PIXEL_FORMAT;

            pHevcDec->hMFCHevcHandle.nActualFormat = 0;
        } else {
            pMeta->nPixelFormat = 0;
        }
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE HevcCodecUpdateBlackBarCrop(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec            = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec             = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle           = pHevcDec->hMFCHevcHandle.hMFCHandle;
    OMX_CONFIG_RECTTYPE           *pBlackBarCropRect    = &pVideoDec->blackBarCropRect;

    ExynosVideoDecBufferOps  *pOutbufOps  = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoRect           CropRect;

    FunctionIn();

    Exynos_OSAL_Memset(&CropRect, 0, sizeof(ExynosVideoRect));
    if (pOutbufOps->Get_BlackBarCrop(hMFCHandle, &CropRect) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to get crop info", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorHardware;
        goto EXIT;
    }

    pBlackBarCropRect->nLeft   = CropRect.nLeft;
    pBlackBarCropRect->nTop    = CropRect.nTop;
    pBlackBarCropRect->nWidth  = CropRect.nWidth;
    pBlackBarCropRect->nHeight = CropRect.nHeight;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Black Bar Info: LEFT(%d) TOP(%d) WIDTH(%d) HEIGHT(%d)",
                                        pExynosComponent, __FUNCTION__,
                                        pBlackBarCropRect->nLeft, pBlackBarCropRect->nTop,
                                        pBlackBarCropRect->nWidth, pBlackBarCropRect->nHeight);

    /** Send Port Settings changed call back **/
    (*(pExynosComponent->pCallbacks->EventHandler))
        (pOMXComponent,
         pExynosComponent->callbackData,
         OMX_EventPortSettingsChanged, /* The command was completed */
         OMX_DirOutput, /* This is the port index */
         (OMX_INDEXTYPE)OMX_IndexConfigBlackBarCrop,
         NULL);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecCheckResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_EXCEPTION_STATE     eOutputExcepState  = pOutputPort->exceptionFlag;

    ExynosVideoDecOps             *pDecOps            = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps       *pOutbufOps         = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoGeometry            codecOutbufConf;

    OMX_CONFIG_RECTTYPE          *pCropRectangle        = &(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT]);
    OMX_PARAM_PORTDEFINITIONTYPE *pInputPortDefinition  = &(pInputPort->portDefinition);
    OMX_PARAM_PORTDEFINITIONTYPE *pOutputPortDefinition = &(pOutputPort->portDefinition);

    int maxDPBNum = 0;

    FunctionIn();

    /* get geometry for output */
    Exynos_OSAL_Memset(&codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    if (pOutbufOps->Get_Geometry(hMFCHandle, &codecOutbufConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to get geometry");
        ret = OMX_ErrorHardware;
        goto EXIT;
    }

    /* get dpb count */
    maxDPBNum = pDecOps->Get_ActualBufferCount(hMFCHandle);
    if (pVideoDec->bThumbnailMode == OMX_FALSE)
        maxDPBNum += EXTRA_DPB_NUM;

    if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
        pCropRectangle->nTop     = codecOutbufConf.cropRect.nTop;
        pCropRectangle->nLeft    = codecOutbufConf.cropRect.nLeft;
        pCropRectangle->nWidth   = codecOutbufConf.cropRect.nWidth;
        pCropRectangle->nHeight  = codecOutbufConf.cropRect.nHeight;
    }

    /* resolution is changed */
    if ((codecOutbufConf.nFrameWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth) ||
        (codecOutbufConf.nFrameHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight) ||
        (codecOutbufConf.nStride != pHevcDec->hMFCHevcHandle.codecOutbufConf.nStride) ||
#if 0  // TODO: check posibility
        (codecOutbufConf.eColorFormat != pHevcDec->hMFCHevcHandle.codecOutbufConf.eColorFormat) ||
        (codecOutbufConf.eFilledDataType != pHevcDec->hMFCHevcHandle.codecOutbufConf.eFilledDataType) ||
#endif
        (maxDPBNum != pHevcDec->hMFCHevcHandle.maxDPBNum)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][DRC] W(%d), H(%d) -> W(%d), H(%d)",
                            pExynosComponent, __FUNCTION__,
                            pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth,
                            pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight,
                            codecOutbufConf.nFrameWidth,
                            codecOutbufConf.nFrameHeight);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][DRC] DPB(%d), FORMAT(0x%x), TYPE(0x%x) -> DPB(%d), FORMAT(0x%x), TYPE(0x%x)",
                            pExynosComponent, __FUNCTION__,
                            pHevcDec->hMFCHevcHandle.maxDPBNum,
                            pHevcDec->hMFCHevcHandle.codecOutbufConf.eColorFormat,
                            pHevcDec->hMFCHevcHandle.codecOutbufConf.eFilledDataType,
                            maxDPBNum, codecOutbufConf.eColorFormat, codecOutbufConf.eFilledDataType);

        pInputPortDefinition->format.video.nFrameWidth     = codecOutbufConf.nFrameWidth;
        pInputPortDefinition->format.video.nFrameHeight    = codecOutbufConf.nFrameHeight;
        pInputPortDefinition->format.video.nStride         = codecOutbufConf.nFrameWidth;
        pInputPortDefinition->format.video.nSliceHeight    = codecOutbufConf.nFrameHeight;

        if (pOutputPort->bufferProcessType == BUFFER_SHARE) {
            pOutputPortDefinition->nBufferCountActual  = maxDPBNum;
            pOutputPortDefinition->nBufferCountMin     = maxDPBNum;
        }

        Exynos_UpdateFrameSize(pOMXComponent);

        if (eOutputExcepState == GENERAL_STATE) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
            pCropRectangle->nTop     = codecOutbufConf.cropRect.nTop;
            pCropRectangle->nLeft    = codecOutbufConf.cropRect.nLeft;
            pCropRectangle->nWidth   = codecOutbufConf.cropRect.nWidth;
            pCropRectangle->nHeight  = codecOutbufConf.cropRect.nHeight;

            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    }

    /* crop info of contents is changed */
    if ((codecOutbufConf.cropRect.nTop != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nTop) ||
        (codecOutbufConf.cropRect.nLeft != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nLeft) ||
        (codecOutbufConf.cropRect.nWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth) ||
        (codecOutbufConf.cropRect.nHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][DRC] CROP: W(%d), H(%d) -> W(%d), H(%d)",
                            pExynosComponent, __FUNCTION__,
                            pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth,
                            pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight,
                            codecOutbufConf.cropRect.nWidth,
                            codecOutbufConf.cropRect.nHeight);

        Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
        pCropRectangle->nTop     = codecOutbufConf.cropRect.nTop;
        pCropRectangle->nLeft    = codecOutbufConf.cropRect.nLeft;
        pCropRectangle->nWidth   = codecOutbufConf.cropRect.nWidth;
        pCropRectangle->nHeight  = codecOutbufConf.cropRect.nHeight;

        /** Send crop info call back **/
        (*(pExynosComponent->pCallbacks->EventHandler))
            (pOMXComponent,
             pExynosComponent->callbackData,
             OMX_EventPortSettingsChanged, /* The command was completed */
             OMX_DirOutput, /* This is the port index */
             OMX_IndexConfigCommonOutputCrop,
             NULL);
    }

    Exynos_OSAL_Memcpy(&pHevcDec->hMFCHevcHandle.codecOutbufConf, &codecOutbufConf, sizeof(codecOutbufConf));
    pHevcDec->hMFCHevcHandle.maxDPBNum = maxDPBNum;

    ret = OMX_ErrorNone;

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecUpdateResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    ExynosVideoDecOps             *pDecOps            = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps       *pOutbufOps         = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoErrorType           codecRet           = VIDEO_ERROR_NONE;

    OMX_CONFIG_RECTTYPE          *pCropRectangle        = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pInputPortDefinition  = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pOutputPortDefinition = NULL;

    OMX_BOOL bFormatChanged = OMX_FALSE;

    FunctionIn();

    /* get geometry for output */
    Exynos_OSAL_Memset(&pHevcDec->hMFCHevcHandle.codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    codecRet = pOutbufOps->Get_Geometry(hMFCHandle, &pHevcDec->hMFCHevcHandle.codecOutbufConf);
    if (codecRet ==  VIDEO_ERROR_HEADERINFO) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] parsed header info has only VPS", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorNeedNextHeaderInfo;
        goto EXIT;
    } else if (codecRet != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to get geometry about output", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorHardware;
        goto EXIT;
    }

    /* get dpb count */
    pHevcDec->hMFCHevcHandle.maxDPBNum = pDecOps->Get_ActualBufferCount(hMFCHandle);
    if (pVideoDec->bThumbnailMode == OMX_FALSE)
        pHevcDec->hMFCHevcHandle.maxDPBNum += EXTRA_DPB_NUM;
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] maxDPBNum: %d", pExynosComponent, __FUNCTION__, pHevcDec->hMFCHevcHandle.maxDPBNum);

    pCropRectangle          = &(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT]);
    pInputPortDefinition    = &(pInputPort->portDefinition);
    pOutputPortDefinition   = &(pOutputPort->portDefinition);

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] past info: width(%d) height(%d)",
                                            pExynosComponent, __FUNCTION__,
                                            pInputPortDefinition->format.video.nFrameWidth,
                                            pInputPortDefinition->format.video.nFrameHeight);

    /* output format is changed internally (8bit <> 10bit) */
    if (pHevcDec->hMFCHevcHandle.MFCOutputColorType != pHevcDec->hMFCHevcHandle.codecOutbufConf.eColorFormat) {
        OMX_COLOR_FORMATTYPE eOutputFormat = Exynos_OSAL_Video2OMXFormat(pHevcDec->hMFCHevcHandle.codecOutbufConf.eColorFormat);

        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] The format(%x) is changed to %x by H/W Codec",
                                    pExynosComponent, __FUNCTION__,
                                    pHevcDec->hMFCHevcHandle.MFCOutputColorType,
                                    pHevcDec->hMFCHevcHandle.codecOutbufConf.eColorFormat);

        pHevcDec->hMFCHevcHandle.MFCOutputColorType = pHevcDec->hMFCHevcHandle.codecOutbufConf.eColorFormat;
        Exynos_SetPlaneToPort(pOutputPort, Exynos_OSAL_GetPlaneCount(eOutputFormat, pOutputPort->ePlaneType));

        bFormatChanged = OMX_TRUE;

        if (!(pHevcDec->hMFCHevcHandle.codecOutbufConf.eFilledDataType & DATA_TYPE_8BIT_SBWC)) {
            if (pVideoDec->nCompColorFormat != OMX_COLOR_FormatUnused) {
                /* SBWC to non SBWC : must update eColorFormat */
                pOutputPort->portDefinition.format.video.eColorFormat = eOutputFormat;
            }

            pVideoDec->nCompColorFormat = OMX_COLOR_FormatUnused;
        } else {
            pVideoDec->nCompColorFormat = eOutputFormat;
        }
    }

    switch (pHevcDec->hMFCHevcHandle.codecOutbufConf.eFilledDataType) {
    case DATA_8BIT_WITH_2BIT:
        pVideoDec->eDataType = DATA_TYPE_8BIT_WITH_2BIT;
        break;
    case DATA_10BIT:
        pVideoDec->eDataType = DATA_TYPE_10BIT;
        break;
    case DATA_8BIT_SBWC:
        pVideoDec->eDataType = DATA_TYPE_8BIT_SBWC;
        break;
    case DATA_10BIT_SBWC:
        pVideoDec->eDataType = DATA_TYPE_10BIT_SBWC;
        break;
    default:
        pVideoDec->eDataType = DATA_TYPE_8BIT;
        break;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] resolution info: width(%d / %d), height(%d / %d)",
                                        pExynosComponent, __FUNCTION__,
                                        pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth,
                                        pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth,
                                        pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight,
                                        pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight);

    if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
        pCropRectangle->nTop     = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nTop;
        pCropRectangle->nLeft    = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nLeft;
        pCropRectangle->nWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth;
        pCropRectangle->nHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight;
    }

    if (pOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        if ((pVideoDec->bReconfigDPB) ||
            (bFormatChanged) ||
            (pInputPortDefinition->format.video.nFrameWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth) ||
            (pInputPortDefinition->format.video.nFrameHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pInputPortDefinition->format.video.nFrameWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nFrameHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;
            pInputPortDefinition->format.video.nStride       = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nSliceHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;
#if 0
            /* don't need to change */
            pOutputPortDefinition->nBufferCountActual  = pOutputPort->portDefinition.nBufferCountActual;
            pOutputPortDefinition->nBufferCountMin     = pOutputPort->portDefinition.nBufferCountMin;
#endif
            Exynos_UpdateFrameSize(pOMXComponent);

            Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
            pCropRectangle->nTop     = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nTop;
            pCropRectangle->nLeft    = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nLeft;
            pCropRectangle->nWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth;
            pCropRectangle->nHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventPortSettingsChanged)",
                                                    pExynosComponent, __FUNCTION__);
            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    } else if (pOutputPort->bufferProcessType == BUFFER_SHARE) {
        if ((pVideoDec->bReconfigDPB) ||
            (bFormatChanged) ||
            (pInputPortDefinition->format.video.nFrameWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth) ||
            (pInputPortDefinition->format.video.nFrameHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight) ||
            ((OMX_S32)pOutputPortDefinition->nBufferCountActual != pHevcDec->hMFCHevcHandle.maxDPBNum) ||
            ((OMX_S32)pOutputPortDefinition->nBufferCountMin < pHevcDec->hMFCHevcHandle.maxDPBNum)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pInputPortDefinition->format.video.nFrameWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nFrameHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;
            pInputPortDefinition->format.video.nStride       = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nSliceHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;

            pOutputPortDefinition->nBufferCountActual    = pHevcDec->hMFCHevcHandle.maxDPBNum;
            pOutputPortDefinition->nBufferCountMin       = pHevcDec->hMFCHevcHandle.maxDPBNum;

            Exynos_UpdateFrameSize(pOMXComponent);

            Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
            pCropRectangle->nTop     = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nTop;
            pCropRectangle->nLeft    = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nLeft;
            pCropRectangle->nWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth;
            pCropRectangle->nHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventPortSettingsChanged)",
                                                    pExynosComponent, __FUNCTION__);
            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    }

    /* contents has crop info */
    if ((pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth) ||
        (pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight)) {

        if ((pOutputPort->bufferProcessType & BUFFER_COPY) &&
            (pExynosComponent->bUseImgCrop == OMX_TRUE)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;
        }
        pInputPortDefinition->format.video.nFrameWidth     = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
        pInputPortDefinition->format.video.nFrameHeight    = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;
        pInputPortDefinition->format.video.nStride         = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
        pInputPortDefinition->format.video.nSliceHeight    = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;

        Exynos_UpdateFrameSize(pOMXComponent);

        Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
        pCropRectangle->nTop     = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nTop;
        pCropRectangle->nLeft    = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nLeft;
        pCropRectangle->nWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth;
        pCropRectangle->nHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventPortSettingsChanged) with crop",
                                                pExynosComponent, __FUNCTION__);
        /** Send crop info call back **/
        (*(pExynosComponent->pCallbacks->EventHandler))
            (pOMXComponent,
             pExynosComponent->callbackData,
             OMX_EventPortSettingsChanged, /* The command was completed */
             OMX_DirOutput, /* This is the port index */
             OMX_IndexConfigCommonOutputCrop,
             NULL);
    }

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);
        INIT_ARRAY_TO_VAL(pExynosComponent->timeStamp, DEFAULT_TIMESTAMP_VAL, MAX_TIMESTAMP);
        Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecHeaderDecoding(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize      = pSrcInputData->dataLen;

    ExynosVideoDecBufferOps *pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoErrorType     codecRet   = VIDEO_ERROR_NONE;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {oneFrameSize, 0, 0};

    FunctionIn();

    /* input buffer enqueue for header parsing */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Header Size: %d", pExynosComponent, __FUNCTION__, oneFrameSize);

    nAllocLen[0] = pSrcInputData->bufferHeader->nAllocLen;
    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        /* OMX buffer is not used directly : CODEC buffer */
        nAllocLen[0] = pSrcInputData->allocSize;
    }

    if (pInbufOps->ExtensionEnqueue(hMFCHandle,
                            (void **)pSrcInputData->buffer.addr,
                            (unsigned long *)pSrcInputData->buffer.fd,
                            nAllocLen,
                            nDataLen,
                            Exynos_GetPlaneFromPort(pExynosInputPort),
                            pSrcInputData->bufferHeader) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to enqueue input buffer for header parsing", pExynosComponent, __FUNCTION__);
//        ret = OMX_ErrorInsufficientResources;
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecInit;
        goto EXIT;
    }

    /* start header parsing */
    if (pInbufOps->Run(hMFCHandle) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to run input buffer for header parsing", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecInit;
        goto EXIT;
    }

    /* get geometry for output */
    Exynos_OSAL_Memset(&pHevcDec->hMFCHevcHandle.codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    codecRet = pOutbufOps->Get_Geometry(hMFCHandle, &pHevcDec->hMFCHevcHandle.codecOutbufConf);
    if (codecRet ==  VIDEO_ERROR_HEADERINFO) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] parsed header info has only VPS", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorNeedNextHeaderInfo;
        goto EXIT;
    } else if (codecRet != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to get geometry about output", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorCorruptedHeader;

        if ((pExynosComponent->codecType != HW_VIDEO_DEC_SECURE_CODEC) &&
            (oneFrameSize >= 8)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] CorruptedHeader Info : %02x %02x %02x %02x %02x %02x %02x %02x ...", pExynosComponent, __FUNCTION__,
                                        *((OMX_U8 *)pSrcInputData->buffer.addr[0])    , *((OMX_U8 *)pSrcInputData->buffer.addr[0] + 1),
                                        *((OMX_U8 *)pSrcInputData->buffer.addr[0] + 2), *((OMX_U8 *)pSrcInputData->buffer.addr[0] + 3),
                                        *((OMX_U8 *)pSrcInputData->buffer.addr[0] + 4), *((OMX_U8 *)pSrcInputData->buffer.addr[0] + 5),
                                        *((OMX_U8 *)pSrcInputData->buffer.addr[0] + 6), *((OMX_U8 *)pSrcInputData->buffer.addr[0] + 7));
        }

        goto EXIT;
    }

EXIT:
    FunctionOut();

    if (ret != OMX_ErrorNone) {
        if ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorNeedNextHeaderInfo) {
            if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
                EXYNOS_OMX_DATABUFFER directReturnUseBuffer;

                Exynos_Shared_DataToBuffer(pExynosInputPort, &directReturnUseBuffer, pSrcInputData);
                Exynos_InputBufferReturn(pOMXComponent, &directReturnUseBuffer);
            } else if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
                OMX_PTR codecBuffer = pSrcInputData->pPrivate;

                if (codecBuffer != NULL)
                    Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, codecBuffer);
            }
        }
        pInbufOps->Stop(hMFCHandle);
    }

    return ret;
}

OMX_ERRORTYPE HevcCodecSrcSetup(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                        allocFrameSize    = 0;
    OMX_COLOR_FORMATTYPE           eOutputFormat     = pExynosOutputPort->portDefinition.format.video.eColorFormat;

    EXYNOS_OMX_VIDEO_HDRSTATICINFO  *pHDRStaticInfo  = &(pExynosOutputPort->HDRStaticInfo);
    EXYNOS_OMX_VIDEO_COLORASPECTS   *pFWCA           = &(pExynosOutputPort->ColorAspects);
    EXYNOS_OMX_VIDEO_COLORASPECTS   *pBSCA           = &(pExynosInputPort->ColorAspects);

    ExynosVideoDecOps       *pDecOps    = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoGeometry      bufferConf;

    OMX_U32  nInBufferCnt                    = 0;
    OMX_BOOL bSupportFormat                  = OMX_FALSE;

    FunctionIn();

    if ((pSrcInputData->dataLen <= 0) && (pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] first frame has only EOS flag. EOS flag will be returned through FBD",
                                                pExynosComponent, __FUNCTION__);

        BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Malloc(sizeof(BYPASS_BUFFER_INFO));
        if (pBufferInfo == NULL) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        pBufferInfo->nFlags     = pSrcInputData->nFlags;
        pBufferInfo->timeStamp  = pSrcInputData->timeStamp;
        ret = Exynos_OSAL_Queue(&pHevcDec->bypassBufferInfoQ, (void *)pBufferInfo);

        if (pExynosOutputPort->bufferProcessType == BUFFER_SHARE) {
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationInStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        } else if (pExynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pVideoDec->bThumbnailMode == OMX_TRUE)
        pDecOps->Set_IFrameDecoding(hMFCHandle);
    else if ((IS_CUSTOM_COMPONENT(pExynosComponent->componentName) == OMX_TRUE) &&
             (pHevcDec->hMFCHevcHandle.nDisplayDelay <= MAX_HEVC_DISPLAYDELAY_VALIDNUM)) {
        pDecOps->Set_DisplayDelay(hMFCHandle, (int)pHevcDec->hMFCHevcHandle.nDisplayDelay);
    }

    if ((pDecOps->Enable_DTSMode != NULL) &&
        (pVideoDec->bDTSMode == OMX_TRUE))
        pDecOps->Enable_DTSMode(hMFCHandle);

    if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportInfo.dec.bPrioritySupport == VIDEO_TRUE)
        pDecOps->Set_Priority(hMFCHandle, pVideoDec->nPriority);

    /* input buffer info */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));
    bufferConf.eCompressionFormat = VIDEO_CODING_HEVC;
    pInbufOps->Set_Shareable(hMFCHandle);

    allocFrameSize = pSrcInputData->bufferHeader->nAllocLen;
    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        /* OMX buffer is not used directly : CODEC buffer */
        allocFrameSize = pSrcInputData->allocSize;
    }

    bufferConf.nSizeImage = allocFrameSize;
    bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pExynosInputPort);
    nInBufferCnt = MAX_INPUTBUFFER_NUM_DYNAMIC;

    /* should be done before prepare input buffer */
    if (pInbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* set input buffer geometry */
    if (pInbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set geometry about input", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* setup input buffer */
    if (pInbufOps->Setup(hMFCHandle, nInBufferCnt) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to setup input buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* set output geometry */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));

#ifdef USE_COMPRESSED_COLOR
    if ((pExynosOutputPort->eMetaDataType != METADATA_TYPE_GRAPHIC) ||
        (pExynosOutputPort->bForceUseNonCompFormat == OMX_TRUE)) {
        /* use SBWC format only ANB scenario */
        pVideoDec->nCompColorFormat = OMX_COLOR_FormatUnused;
    }
#endif

    if (pVideoDec->nCompColorFormat != OMX_COLOR_FormatUnused) {
        /* For SBWC format setting */
        bSupportFormat = CheckFormatHWSupport(pExynosComponent, pVideoDec->nCompColorFormat);
        if (bSupportFormat == OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] set compressed color format(0x%x)",
                    pExynosComponent, __FUNCTION__, (OMX_U32)pVideoDec->nCompColorFormat);

            eOutputFormat = pVideoDec->nCompColorFormat;
        }
    }

    bSupportFormat = CheckFormatHWSupport(pExynosComponent, eOutputFormat);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] omx format(0x%x) is %s by h/w",
                                            pExynosComponent, __FUNCTION__, eOutputFormat,
                                            (bSupportFormat == OMX_TRUE)? "supported":"not supported");
    if (bSupportFormat == OMX_TRUE) {  /* supported by H/W */
        bufferConf.eColorFormat = Exynos_OSAL_OMX2VideoFormat(eOutputFormat, pExynosOutputPort->ePlaneType);
        Exynos_SetPlaneToPort(pExynosOutputPort, Exynos_OSAL_GetPlaneCount(eOutputFormat, pExynosOutputPort->ePlaneType));
    } else {
        OMX_COLOR_FORMATTYPE eCheckFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        bSupportFormat = CheckFormatHWSupport(pExynosComponent, eCheckFormat);

        if (bSupportFormat == OMX_TRUE) {  /* supported by CSC(NV12 -> format) */
            bufferConf.eColorFormat = Exynos_OSAL_OMX2VideoFormat(eCheckFormat, pExynosOutputPort->ePlaneType);
            Exynos_SetPlaneToPort(pExynosOutputPort, Exynos_OSAL_GetPlaneCount(eCheckFormat, pExynosOutputPort->ePlaneType));
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Can not support this format (0x%x)", pExynosComponent, __FUNCTION__, eOutputFormat);
            ret = OMX_ErrorNotImplemented;
            pInbufOps->Cleanup_Buffer(hMFCHandle);
            goto EXIT;
        }
    }

    pHevcDec->hMFCHevcHandle.MFCOutputColorType = bufferConf.eColorFormat;
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output video format is 0x%x",
                                            pExynosComponent, __FUNCTION__, bufferConf.eColorFormat);

    bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    if (pOutbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set geometry about output", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        pInbufOps->Cleanup_Buffer(hMFCHandle);
        goto EXIT;
    }

    if (pVideoDec->bReorderMode == OMX_TRUE)
        Exynos_SetReorderTimestamp(pExynosComponent, &(pHevcDec->hMFCHevcHandle.indexTimestamp), pSrcInputData->timeStamp, pSrcInputData->nFlags);

    ret = HevcCodecHeaderDecoding(pOMXComponent, pSrcInputData);
    if (ret != OMX_ErrorNone) {
        pInbufOps->Cleanup_Buffer(hMFCHandle);
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc = OMX_TRUE;

    HevcCodecStart(pOMXComponent, INPUT_PORT_INDEX);

    ret = HevcCodecUpdateResolution(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc = OMX_FALSE;
        HevcCodecStop(pOMXComponent, INPUT_PORT_INDEX);
        pInbufOps->Cleanup_Buffer(hMFCHandle);
        goto EXIT;
    }

    Exynos_OSAL_SleepMillisec(0);
#if 1  /* enable re-input scheme */
    ret = (OMX_ERRORTYPE)OMX_ErrorInputDataDecodeYet;
    HevcCodecStop(pOMXComponent, INPUT_PORT_INDEX);
#else
    ret = (OMX_ERRORTYPE)OMX_ErrorNoneSrcSetupFinish;
#endif

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec            = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec             = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle           = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort    = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecOps           *pDecOps    = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps     *pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {0, 0, 0};
    int i, nOutbufs, nPlaneCnt;

    FunctionIn();

    nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    for (i = 0; i < nPlaneCnt; i++)
        nAllocLen[i] = pHevcDec->hMFCHevcHandle.codecOutbufConf.nAlignPlaneSize[i];

    HevcCodecStop(pOMXComponent, OUTPUT_PORT_INDEX);

    /* for adaptive playback */
    if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportInfo.dec.bDrvDPBManageSupport != VIDEO_TRUE) {
        if (pDecOps->Enable_DynamicDPB(hMFCHandle) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to enable Dynamic DPB", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorHardware;
            goto EXIT;
        }
    }

    pOutbufOps->Set_Shareable(hMFCHandle);

    if (pExynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        /* should be done before prepare output buffer */
        if (pOutbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        if (pOutbufOps->Setup(hMFCHandle, MAX_OUTPUTBUFFER_NUM_DYNAMIC) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to setup output buffer", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        /* get dpb count */
        nOutbufs = pHevcDec->hMFCHevcHandle.maxDPBNum;
        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX, nOutbufs, nAllocLen);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        /* Enqueue output buffer */
        for (i = 0; i < nOutbufs; i++) {
            pOutbufOps->ExtensionEnqueue(hMFCHandle,
                            (void **)pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr,
                            (unsigned long *)pVideoDec->pMFCDecOutputBuffer[i]->fd,
                            pVideoDec->pMFCDecOutputBuffer[i]->bufferSize,
                            nDataLen,
                            nPlaneCnt,
                            NULL);
        }

        pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_TRUE;
    } else if (pExynosOutputPort->bufferProcessType == BUFFER_SHARE) {
        /* get dpb count */
        nOutbufs = MAX_OUTPUTBUFFER_NUM_DYNAMIC;
        if (pOutbufOps->Setup(hMFCHandle, nOutbufs) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to setup output buffer", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        if (pExynosOutputPort->eMetaDataType == METADATA_TYPE_DISABLED) {
            /*************/
            /*    TBD    */
            /*************/
            /* data buffer : user buffer
             * H/W can't accept user buffer directly
             */
            ret = OMX_ErrorNotImplemented;
            goto EXIT;
        }

        pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_TRUE;
    }

    if (HevcCodecStart(pOMXComponent, OUTPUT_PORT_INDEX) != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to run output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec          = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentParameterStructure == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoDec->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoHevc:
    {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHevcComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHevcComponent = NULL;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstHevcComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstHevcComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcHevcComponent = &pHevcDec->HevcComponent[pDstHevcComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstHevcComponent) + nOffset,
                           ((char *)pSrcHevcComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_HEVCTYPE) - nOffset);
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_DEC_ROLE);
    }
        break;
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = GetIndexToProfileLevel(pExynosComponent, pDstProfileLevel);
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel  = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE         *pSrcHevcComponent = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcHevcComponent = &pHevcDec->HevcComponent[pDstProfileLevel->nPortIndex];

        pDstProfileLevel->eProfile = pSrcHevcComponent->eProfile;
        pDstProfileLevel->eLevel = pSrcHevcComponent->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcErrorCorrectionType = &pHevcDec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC                 = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync              = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing      = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning    = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC                = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    case OMX_IndexExynosParamDisplayDelay:  /* MSRND */
    {
        OMX_PARAM_U32TYPE       *pDisplayDelay  = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;
        EXYNOS_HEVCDEC_HANDLE   *pHevcDec       = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDisplayDelay, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDisplayDelay->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDisplayDelay->nU32 = pHevcDec->hMFCHevcHandle.nDisplayDelay;
    }
        break;
    case OMX_IndexExynosParamReorderMode:
    {
        EXYNOS_OMX_VIDEO_PARAM_REORDERMODE *pReorderParam = (EXYNOS_OMX_VIDEO_PARAM_REORDERMODE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pReorderParam, sizeof(EXYNOS_OMX_VIDEO_PARAM_REORDERMODE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pReorderParam->bReorderMode = pVideoDec->bReorderMode;
    }
        break;
    case OMX_IndexExynosParamBufferCopy:
    {
        OMX_PARAM_U32TYPE   *pBufferCopy = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;
        EXYNOS_OMX_BASEPORT *pExynosPort = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pBufferCopy, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pBufferCopy->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[pBufferCopy->nPortIndex];

        pBufferCopy->nU32 = (pExynosPort->bufferProcessType & BUFFER_COPY_FORCE)? 1:0;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexExynosParamImageConvertMode:
    {
        OMX_PARAM_U32TYPE *pImgConvMode = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pImgConvMode, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pImgConvMode->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pImgConvMode->nU32 = pVideoDec->nImageConvMode;
    }
        break;
#endif
    default:
        ret = Exynos_OMX_VideoDecodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec          = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentParameterStructure == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoDec->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamVideoHevc:
    {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHevcComponent = NULL;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHevcComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)pComponentParameterStructure;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcHevcComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcHevcComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstHevcComponent = &pHevcDec->HevcComponent[pSrcHevcComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstHevcComponent) + nOffset,
                           ((char *)pSrcHevcComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_HEVCTYPE) - nOffset);
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_DEC_ROLE)) {
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC;
        } else {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel  = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE         *pDstHevcComponent = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstHevcComponent = &pHevcDec->HevcComponent[pSrcProfileLevel->nPortIndex];

        if (OMX_FALSE == CheckProfileLevelSupport(pExynosComponent, pSrcProfileLevel)) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pDstHevcComponent->eProfile = pSrcProfileLevel->eProfile;
        pDstHevcComponent->eLevel   = pSrcProfileLevel->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstErrorCorrectionType = &pHevcDec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC                 = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync              = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing      = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning    = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC                = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    case OMX_IndexExynosParamDisplayDelay:  /* MSRND */
    {
        OMX_PARAM_U32TYPE       *pDisplayDelay  = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;
        EXYNOS_HEVCDEC_HANDLE   *pHevcDec       = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDisplayDelay, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDisplayDelay->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        if (pDisplayDelay->nU32 > MAX_HEVC_DISPLAYDELAY_VALIDNUM) {
            ret = OMX_ErrorBadParameter;
            break;
        }

        pHevcDec->hMFCHevcHandle.nDisplayDelay = pDisplayDelay->nU32;
    }
        break;
    case OMX_IndexExynosParamReorderMode:
    {
        EXYNOS_OMX_VIDEO_PARAM_REORDERMODE *pReorderParam = (EXYNOS_OMX_VIDEO_PARAM_REORDERMODE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pReorderParam, sizeof(EXYNOS_OMX_VIDEO_PARAM_REORDERMODE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoDec->bReorderMode = pReorderParam->bReorderMode;
    }
        break;
    case OMX_IndexExynosParamBufferCopy:
    {
        OMX_PARAM_U32TYPE   *pBufferCopy = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;
        EXYNOS_OMX_BASEPORT *pExynosPort = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pBufferCopy, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pBufferCopy->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can't change a buffer mode", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[pBufferCopy->nPortIndex];

        if (pBufferCopy->nU32 != 0) {
            pExynosPort->bufferProcessType |= BUFFER_COPY_FORCE;
        } else {
            pExynosPort->bufferProcessType &= (~BUFFER_COPY_FORCE);
        }
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexExynosParamImageConvert:
    {
        OMX_VIDEO_PARAM_IMG_CONV *pImgConv = (OMX_VIDEO_PARAM_IMG_CONV *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pImgConv, sizeof(OMX_VIDEO_PARAM_IMG_CONV));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pImgConv->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] secure component doesn't support image conversion", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can't change image conversion mode", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if (pVideoDec->hImgConv != NULL) {
            Exynos_OSAL_ImgConv_Terminate(pVideoDec->hImgConv);
            pVideoDec->hImgConv = NULL;
        }

        if (pImgConv->bEnable == OMX_TRUE) {
            pVideoDec->hImgConv = Exynos_OSAL_ImgConv_Create(pImgConv->nWidth, pImgConv->nHeight, pVideoDec->nImageConvMode);
            if (pVideoDec->hImgConv == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_ImgConv_Create()", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] ImageConverter is %s", pExynosComponent, __FUNCTION__,
                                            (pImgConv->bEnable == OMX_TRUE)? "enabled":"disabled");
    }
        break;
    case OMX_IndexExynosParamImageConvertMode:
    {
        OMX_PARAM_U32TYPE *pImgConvMode = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pImgConvMode, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pImgConvMode->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] secure component doesn't support image conversion", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if ((pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE) ||
            (pVideoDec->hImgConv != NULL))  {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can't change image conversion mode", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pVideoDec->nImageConvMode = pImgConvMode->nU32;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] ImageConverter mode(%d)", pExynosComponent, __FUNCTION__, pVideoDec->nImageConvMode);
    }
        break;
#endif
    default:
        ret = Exynos_OMX_VideoDecodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec          = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentConfigStructure == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoDec->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigCommonOutputCrop:
    {
        if (pExynosComponent->bUseImgCrop == OMX_TRUE) {
            ret = Exynos_OMX_VideoDecodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        } else {
            /* query crop information on bitstream */
            OMX_CONFIG_RECTTYPE *pDstRectType  = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
            OMX_CONFIG_RECTTYPE *pSrcRectType  = NULL;
            EXYNOS_OMX_BASEPORT *pOutputPort   = NULL;

            if (pDstRectType->nPortIndex != OUTPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }

            if (pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc == OMX_FALSE) {
                ret = OMX_ErrorNotReady;
                goto EXIT;
            }

            pOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            pSrcRectType = &(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT]);

            pDstRectType->nTop    = pSrcRectType->nTop;
            pDstRectType->nLeft   = pSrcRectType->nLeft;
            pDstRectType->nHeight = pSrcRectType->nHeight;
            pDstRectType->nWidth  = pSrcRectType->nWidth;
        }
    }
        break;
#ifdef USE_S3D_SUPPORT
    case OMX_IndexVendorS3DMode:
    {
        OMX_U32 *pS3DMode = NULL;

        pS3DMode = (OMX_U32 *)pComponentConfigStructure;
        *pS3DMode = (OMX_U32) pHevcDec->hMFCHevcHandle.S3DFPArgmtType;
    }
        break;
#endif
    case OMX_IndexExynosConfigDisplayDelay:  /* MSRND */
    {
        (*((OMX_U32 *)pComponentConfigStructure)) = pHevcDec->hMFCHevcHandle.nDisplayDelay;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoHdrStaticInfo:
    case OMX_IndexConfigVideoHdr10PlusInfo:
    {
        ret = Exynos_OSAL_GetConfig(hComponent, nIndex, pComponentConfigStructure);
    }
        break;
#endif
    default:
        ret = Exynos_OMX_VideoDecodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec          = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentConfigStructure == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoDec->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexExynosConfigDisplayDelay:  /* MSRND */
    {
        OMX_U32 nDisplayDelay = (*((OMX_U32 *)pComponentConfigStructure));

        if (pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc == OMX_TRUE) {
            ret = OMX_ErrorIncorrectStateOperation;
            break;
        }

        if (nDisplayDelay > MAX_HEVC_DISPLAYDELAY_VALIDNUM) {
            ret = OMX_ErrorBadParameter;
            break;
        }

        pHevcDec->hMFCHevcHandle.nDisplayDelay = nDisplayDelay;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoHdrStaticInfo:
    case OMX_IndexConfigVideoHdr10PlusInfo:
    {
        ret = Exynos_OSAL_SetConfig(hComponent, nIndex, pComponentConfigStructure);
    }
        break;
#endif
    default:
        ret = Exynos_OMX_VideoDecodeSetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_GetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE  hComponent,
    OMX_IN  OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE  *pIndexType)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (cParameterName == NULL) ||
        (pIndexType == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }


#ifdef USE_S3D_SUPPORT
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_GET_S3D) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorS3DMode;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_CUSTOM_INDEX_PARAM_REORDER_MODE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexExynosParamReorderMode;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_HDR_STATIC_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoHdrStaticInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_HDR10_PLUS_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoHdr10PlusInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    if (IS_CUSTOM_COMPONENT(pExynosComponent->componentName) == OMX_TRUE) {
        if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_CUSTOM_INDEX_CONFIG_DISPLAY_DELAY) == 0) {
            *pIndexType = (OMX_INDEXTYPE) OMX_IndexExynosConfigDisplayDelay;
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_CUSTOM_INDEX_PARAM_DISPLAY_DELAY) == 0) {
            *pIndexType = (OMX_INDEXTYPE) OMX_IndexExynosParamDisplayDelay;
            ret = OMX_ErrorNone;
            goto EXIT;
        }

#ifdef USE_ANDROID
        if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_CUSTOM_INDEX_PARAM_USE_IMG_CONV) == 0) {
            *pIndexType = (OMX_INDEXTYPE) OMX_IndexExynosParamImageConvert;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
#endif
    }

    ret = Exynos_OMX_VideoDecodeGetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (nIndex == (MAX_COMPONENT_ROLE_NUM-1)) {
        Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_HEVC_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_HevcDec_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

    ExynosVideoInstInfo *pVideoInstInfo = &(pHevcDec->hMFCHevcHandle.videoInstInfo);
    CSC_METHOD csc_method = CSC_METHOD_SW;
    int i;

    FunctionIn();

    pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc = OMX_FALSE;
    pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_FALSE;
    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] CodecOpen W:%d H:%d Bitrate:%d FPS:%d", pExynosComponent, __FUNCTION__,
                                                                                             pExynosInputPort->portDefinition.format.video.nFrameWidth,
                                                                                             pExynosInputPort->portDefinition.format.video.nFrameHeight,
                                                                                             pExynosInputPort->portDefinition.format.video.nBitrate,
                                                                                             pExynosInputPort->portDefinition.format.video.xFramerate);
    pVideoInstInfo->nSize         = sizeof(ExynosVideoInstInfo);
    pVideoInstInfo->nWidth        = pExynosInputPort->portDefinition.format.video.nFrameWidth;
    pVideoInstInfo->nHeight       = pExynosInputPort->portDefinition.format.video.nFrameHeight;
    pVideoInstInfo->nBitrate      = pExynosInputPort->portDefinition.format.video.nBitrate;
    pVideoInstInfo->xFramerate    = pExynosInputPort->portDefinition.format.video.xFramerate;

    /* HEVC Codec Open */
    ret = HevcCodecOpen(pHevcDec, pVideoInstInfo);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    Exynos_SetPlaneToPort(pExynosInputPort, MFC_DEFAULT_INPUT_BUFFER_PLANE);
    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
        nAllocLen[0] = ALIGN(pExynosInputPort->portDefinition.format.video.nFrameWidth *
                             pExynosInputPort->portDefinition.format.video.nFrameHeight * 3 / 2, 512);
        if (nAllocLen[0] < pVideoDec->nMinInBufSize)
            nAllocLen[0] = pVideoDec->nMinInBufSize;

        Exynos_OSAL_SemaphoreCreate(&pExynosInputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosInputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);

        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX, MFC_INPUT_BUFFER_NUM_MAX, nAllocLen);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)
            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    Exynos_SetPlaneToPort(pExynosOutputPort, MFC_DEFAULT_OUTPUT_BUFFER_PLANE);
    if (pExynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        Exynos_OSAL_SemaphoreCreate(&pExynosOutputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosOutputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);
    } else if (pExynosOutputPort->bufferProcessType == BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    pHevcDec->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pHevcDec->hSourceStartEvent);
    pHevcDec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pHevcDec->hDestinationInStartEvent);
    Exynos_OSAL_SignalCreate(&pHevcDec->hDestinationOutStartEvent);

    Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);
    INIT_ARRAY_TO_VAL(pExynosComponent->timeStamp, DEFAULT_TIMESTAMP_VAL, MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pHevcDec->hMFCHevcHandle.indexTimestamp = 0;
    pHevcDec->hMFCHevcHandle.outputIndexTimestamp = 0;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    Exynos_OSAL_QueueCreate(&pHevcDec->bypassBufferInfoQ, QUEUE_ELEMENTS);

#ifdef USE_CSC_HW
    csc_method = CSC_METHOD_HW;
#endif
    if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) {
        pVideoDec->csc_handle = csc_init(CSC_METHOD_HW);
        csc_set_hw_property(pVideoDec->csc_handle, CSC_HW_PROPERTY_FIXED_NODE, 2);
        csc_set_hw_property(pVideoDec->csc_handle, CSC_HW_PROPERTY_MODE_DRM, 1);
    } else {
        pVideoDec->csc_handle = csc_init(csc_method);
    }

    if (pVideoDec->csc_handle == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pVideoDec->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Terminate */
OMX_ERRORTYPE Exynos_HevcDec_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE               ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent  = NULL;

    FunctionIn();

    if (pOMXComponent == NULL)
        goto EXIT;

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent != NULL) {
        EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec   = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
        EXYNOS_OMX_BASEPORT     *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
        EXYNOS_OMX_BASEPORT     *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

        if (pVideoDec != NULL) {
            EXYNOS_HEVCDEC_HANDLE *pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

            if (pVideoDec->csc_handle != NULL) {
                csc_deinit(pVideoDec->csc_handle);
                pVideoDec->csc_handle = NULL;
            }

            if (pHevcDec != NULL) {
                Exynos_OSAL_QueueTerminate(&pHevcDec->bypassBufferInfoQ);

                Exynos_OSAL_SignalTerminate(pHevcDec->hDestinationInStartEvent);
                pHevcDec->hDestinationInStartEvent = NULL;
                Exynos_OSAL_SignalTerminate(pHevcDec->hDestinationOutStartEvent);
                pHevcDec->hDestinationOutStartEvent = NULL;
                pHevcDec->bDestinationStart = OMX_FALSE;

                Exynos_OSAL_SignalTerminate(pHevcDec->hSourceStartEvent);
                pHevcDec->hSourceStartEvent = NULL;
                pHevcDec->bSourceStart = OMX_FALSE;
            }

            if (pExynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
                Exynos_Free_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX);
                Exynos_OSAL_QueueTerminate(&pExynosOutputPort->codecBufferQ);
                Exynos_OSAL_SemaphoreTerminate(pExynosOutputPort->codecSemID);
                pExynosOutputPort->codecSemID = NULL;
            } else if (pExynosOutputPort->bufferProcessType == BUFFER_SHARE) {
                /*************/
                /*    TBD    */
                /*************/
                /* Does not require any actions. */
            }

            if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
                Exynos_Free_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX);
                Exynos_OSAL_QueueTerminate(&pExynosInputPort->codecBufferQ);
                Exynos_OSAL_SemaphoreTerminate(pExynosInputPort->codecSemID);
                pExynosInputPort->codecSemID = NULL;
            } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
                /*************/
                /*    TBD    */
                /*************/
                /* Does not require any actions. */
            }
            if (pHevcDec != NULL) {
                HevcCodecClose(pHevcDec);
            }
        }
    }

    Exynos_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SrcIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize      = pSrcInputData->dataLen;

    ExynosVideoDecOps       *pDecOps     = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps   = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    OMX_BUFFERHEADERTYPE tempBufferHeader;
    void *pPrivate = NULL;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {oneFrameSize, 0, 0};
    OMX_BOOL bInStartCode = OMX_FALSE;

    FunctionIn();

    if (pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = HevcCodecSrcSetup(pOMXComponent, pSrcInputData);
        goto EXIT;
    }

    if ((pVideoDec->bForceHeaderParsing == OMX_FALSE) &&
        (pHevcDec->bDestinationStart == OMX_FALSE) &&
        (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_FALSE)) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] do DstSetup", pExynosComponent, __FUNCTION__);
        ret = HevcCodecDstSetup(pOMXComponent);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to HevcCodecDstSetup(0x%x)",
                                            pExynosComponent, __FUNCTION__, ret);
            goto EXIT;
        }
    }

    if (((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) ||
           ((bInStartCode = Check_HEVC_StartCode(pSrcInputData->buffer.addr[0], oneFrameSize)) == OMX_TRUE)) ||
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        if (pVideoDec->bReorderMode == OMX_FALSE) {
            /* next slot will be used like as circular queue */
            pExynosComponent->timeStamp[pHevcDec->hMFCHevcHandle.indexTimestamp] = pSrcInputData->timeStamp;
            pExynosComponent->nFlags[pHevcDec->hMFCHevcHandle.indexTimestamp]    = pSrcInputData->nFlags;
        } else {  /* MSRND */
            Exynos_SetReorderTimestamp(pExynosComponent, &(pHevcDec->hMFCHevcHandle.indexTimestamp), pSrcInputData->timeStamp, pSrcInputData->nFlags);
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input / buffer header(%p), dataLen(%d), nFlags: 0x%x, timestamp %lld us (%.2f secs), tag: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        pSrcInputData->bufferHeader, oneFrameSize, pSrcInputData->nFlags,
                                                        pSrcInputData->timeStamp, (double)(pSrcInputData->timeStamp / 1E6),
                                                        pHevcDec->hMFCHevcHandle.indexTimestamp);

        pDecOps->Set_FrameTag(hMFCHandle, pHevcDec->hMFCHevcHandle.indexTimestamp);

        pHevcDec->hMFCHevcHandle.indexTimestamp++;
        pHevcDec->hMFCHevcHandle.indexTimestamp %= MAX_TIMESTAMP;

        if (pVideoDec->bQosChanged == OMX_TRUE) {
            if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportInfo.dec.bOperatingRateSupport == VIDEO_TRUE) {
                if (pDecOps->Set_OperatingRate != NULL)
                    pDecOps->Set_OperatingRate(hMFCHandle, pVideoDec->nOperatingRate);
            } else if (pDecOps->Set_QosRatio != NULL) {
                pDecOps->Set_QosRatio(hMFCHandle, pVideoDec->nQosRatio);
            }

            pVideoDec->bQosChanged = OMX_FALSE;
        }

         if (pVideoDec->bSearchBlackBarChanged == OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] BlackBar searching mode : %s",
                                            pExynosComponent, __FUNCTION__,
                                            (pVideoDec->bSearchBlackBar == OMX_TRUE) ? "enable" : "disable");
            pDecOps->Set_SearchBlackBar(hMFCHandle, (ExynosVideoBoolType)pVideoDec->bSearchBlackBar);
            pVideoDec->bSearchBlackBarChanged = OMX_FALSE;
        }

#ifdef PERFORMANCE_DEBUG
        Exynos_OSAL_V4L2CountIncrease(pExynosInputPort->hBufferCount, pSrcInputData->bufferHeader, INPUT_PORT_INDEX);
#endif

        /* queue work for input buffer */
        nAllocLen[0] = pSrcInputData->bufferHeader->nAllocLen;
        if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
            pPrivate = (void *)pSrcInputData->bufferHeader;
        } else {
            nAllocLen[0] = pSrcInputData->allocSize;

            tempBufferHeader.nFlags     = pSrcInputData->nFlags;
            tempBufferHeader.nTimeStamp = pSrcInputData->timeStamp;
            pPrivate = (void *)&tempBufferHeader;
        }

        codecReturn = pInbufOps->ExtensionEnqueue(hMFCHandle,
                                (void **)pSrcInputData->buffer.addr,
                                (unsigned long *)pSrcInputData->buffer.fd,
                                nAllocLen,
                                nDataLen,
                                Exynos_GetPlaneFromPort(pExynosInputPort),
                                pPrivate);
        if (codecReturn != VIDEO_ERROR_NONE) {
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to ExtensionEnqueue about input (0x%x)",
                                                pExynosComponent, __FUNCTION__, codecReturn);
            goto EXIT;
        }

        HevcCodecStart(pOMXComponent, INPUT_PORT_INDEX);
        if (pHevcDec->bSourceStart == OMX_FALSE) {
            pHevcDec->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pHevcDec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        if ((pHevcDec->bDestinationStart == OMX_FALSE) &&
            (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE)) {
            pHevcDec->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    } else if (bInStartCode == OMX_FALSE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] can't find a start code", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorCorruptedFrame;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SrcOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec        = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec         = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle       = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoDecBufferOps *pInbufOps      = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoBuffer       *pVideoBuffer   = NULL;
    ExynosVideoBuffer        videoBuffer;

    FunctionIn();

    if (pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc == OMX_TRUE) {
        if (pInbufOps->ExtensionDequeue(hMFCHandle, &videoBuffer) == VIDEO_ERROR_NONE)
            pVideoBuffer = &videoBuffer;
        else
            pVideoBuffer = NULL;
    } else {
        pVideoBuffer = NULL;
    }

    pSrcOutputData->dataLen       = 0;
    pSrcOutputData->usedDataLen   = 0;
    pSrcOutputData->remainDataLen = 0;
    pSrcOutputData->nFlags        = 0;
    pSrcOutputData->timeStamp     = 0;
    pSrcOutputData->bufferHeader  = NULL;

    if (pVideoBuffer == NULL) {
        pSrcOutputData->buffer.addr[0] = NULL;
        pSrcOutputData->allocSize  = 0;
        pSrcOutputData->pPrivate = NULL;
    } else {
        pSrcOutputData->buffer.addr[0] = pVideoBuffer->planes[0].addr;
        pSrcOutputData->buffer.fd[0] = pVideoBuffer->planes[0].fd;
        pSrcOutputData->allocSize  = pVideoBuffer->planes[0].allocSize;

        if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
            int i;
            for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
                if (pSrcOutputData->buffer.addr[0] ==
                        pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]) {
                    pVideoDec->pMFCDecInputBuffer[i]->dataSize = 0;
                    pSrcOutputData->pPrivate = pVideoDec->pMFCDecInputBuffer[i];
                    break;
                }
            }

            if (i >= MFC_INPUT_BUFFER_NUM_MAX) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Can not find a codec buffer", pExynosComponent, __FUNCTION__);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
                goto EXIT;
            }
        }

        /* For Share Buffer */
        if (pExynosInputPort->bufferProcessType == BUFFER_SHARE) {
            pSrcOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE*)pVideoBuffer->pPrivate;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input / buffer header(%p)",
                                                pExynosComponent, __FUNCTION__, pSrcOutputData->bufferHeader);
        }

#ifdef PERFORMANCE_DEBUG
        Exynos_OSAL_V4L2CountDecrease(pExynosInputPort->hBufferCount, pSrcOutputData->bufferHeader, INPUT_PORT_INDEX);
#endif
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_DstIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecBufferOps *pOutbufOps  = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {0, 0, 0};
    int i, nPlaneCnt;

    FunctionIn();

    if (pDstInputData->buffer.addr[0] == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to find output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    for (i = 0; i < nPlaneCnt; i++) {
        nAllocLen[i] = pHevcDec->hMFCHevcHandle.codecOutbufConf.nAlignPlaneSize[i];

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] ADDR[%d]: 0x%x, size[%d]: %d", pExynosComponent, __FUNCTION__,
                                        i, pDstInputData->buffer.addr[i], i, nAllocLen[i]);
    }

#ifdef PERFORMANCE_DEBUG
    Exynos_OSAL_V4L2CountIncrease(pExynosOutputPort->hBufferCount, pDstInputData->bufferHeader, OUTPUT_PORT_INDEX);
#endif

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output / buffer header(%p)",
                                        pExynosComponent, __FUNCTION__, pDstInputData->bufferHeader);

    codecReturn = pOutbufOps->ExtensionEnqueue(hMFCHandle,
                                (void **)pDstInputData->buffer.addr,
                                (unsigned long *)pDstInputData->buffer.fd,
                                nAllocLen,
                                nDataLen,
                                nPlaneCnt,
                                pDstInputData->bufferHeader);

    if (codecReturn != VIDEO_ERROR_NONE) {
        if (codecReturn != VIDEO_ERROR_WRONGBUFFERSIZE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to ExtensionEnqueue about output (0x%x)",
                                                pExynosComponent, __FUNCTION__, codecReturn);
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
        }

        goto EXIT;
    }

    HevcCodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_DstOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort       = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    DECODE_CODEC_EXTRA_BUFFERINFO *pBufferInfo       = NULL;

    ExynosVideoDecOps           *pDecOps        = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps     *pOutbufOps     = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoBuffer           *pVideoBuffer   = NULL;
    ExynosVideoBuffer            videoBuffer;
    ExynosVideoFrameStatusType   displayStatus  = VIDEO_FRAME_STATUS_UNKNOWN;
    ExynosVideoGeometry         *bufferGeometry = NULL;
    ExynosVideoErrorType         codecReturn    = VIDEO_ERROR_NONE;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {0, 0, 0};

    OMX_S32 indexTimestamp = 0;
    int plane, nPlaneCnt;

    ExynosVideoColorFormatType  nVideoFormat = VIDEO_COLORFORMAT_UNKNOWN;
    OMX_COLOR_FORMATTYPE        nOMXFormat   = OMX_COLOR_FormatUnused;
    OMX_U32                     nPixelFormat = 0;

    FunctionIn();

    if (pHevcDec->bDestinationStart == OMX_FALSE) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    while (1) {
        Exynos_OSAL_Memset(&videoBuffer, 0, sizeof(ExynosVideoBuffer));

        codecReturn = pOutbufOps->ExtensionDequeue(hMFCHandle, &videoBuffer);
        if (codecReturn == VIDEO_ERROR_NONE) {
            pVideoBuffer = &videoBuffer;
        } else if (codecReturn == VIDEO_ERROR_DQBUF_EIO) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] HW is not available(EIO) at ExtensionDequeue", pExynosComponent, __FUNCTION__);
            pVideoBuffer = NULL;
            ret = OMX_ErrorHardware;
            goto EXIT;
        } else {
            pVideoBuffer = NULL;
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        displayStatus = pVideoBuffer->displayStatus;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] displayStatus: 0x%x", pExynosComponent, __FUNCTION__, displayStatus);

        if ((displayStatus == VIDEO_FRAME_STATUS_DISPLAY_DECODING) ||
            (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_ONLY) ||
            (displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
            (displayStatus == VIDEO_FRAME_STATUS_DECODING_FINISHED) ||
            (displayStatus == VIDEO_FRAME_STATUS_ENABLED_S3D) ||
            (displayStatus == VIDEO_FRAME_STATUS_LAST_FRAME) ||
            (CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
            ret = OMX_ErrorNone;
            break;
        }
    }

#ifdef USE_S3D_SUPPORT
    /* Check Whether frame packing information is available */
    if ((pHevcDec->hMFCHevcHandle.S3DFPArgmtType == OMX_SEC_FPARGMT_INVALID) &&
        (pVideoDec->bThumbnailMode == OMX_FALSE) &&
        ((displayStatus == VIDEO_FRAME_STATUS_DISPLAY_ONLY) ||
         (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_DECODING) ||
         (displayStatus == VIDEO_FRAME_STATUS_ENABLED_S3D))) {
        if (HevcCodecCheckFramePacking(pOMXComponent) != OMX_TRUE) {
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            goto EXIT;
        }
    }
#endif

    if ((pVideoDec->bThumbnailMode == OMX_FALSE) &&
        ((displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
         (displayStatus == VIDEO_FRAME_STATUS_ENABLED_S3D))) {
        if (pVideoDec->bReconfigDPB != OMX_TRUE) {
            pOutputPort->exceptionFlag = NEED_PORT_FLUSH;
            pVideoDec->bReconfigDPB = OMX_TRUE;
            HevcCodecUpdateResolution(pOMXComponent);
            pVideoDec->csc_set_format = OMX_FALSE;
#ifdef USE_S3D_SUPPORT
            pHevcDec->hMFCHevcHandle.S3DFPArgmtType = OMX_SEC_FPARGMT_INVALID;
#endif
        }
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.outputIndexTimestamp++;
    pHevcDec->hMFCHevcHandle.outputIndexTimestamp %= MAX_TIMESTAMP;

    pDstOutputData->allocSize = pDstOutputData->dataLen = 0;
    nPlaneCnt = Exynos_GetPlaneFromPort(pOutputPort);
    for (plane = 0; plane < nPlaneCnt; plane++) {
        pDstOutputData->buffer.addr[plane]  = pVideoBuffer->planes[plane].addr;
        pDstOutputData->buffer.fd[plane]    = pVideoBuffer->planes[plane].fd;

        pDstOutputData->allocSize += pVideoBuffer->planes[plane].allocSize;
        pDstOutputData->dataLen   += pVideoBuffer->planes[plane].dataSize;
        nDataLen[plane]            = pVideoBuffer->planes[plane].dataSize;
    }
    pDstOutputData->usedDataLen = 0;
    pDstOutputData->pPrivate = pVideoBuffer;

    pBufferInfo     = (DECODE_CODEC_EXTRA_BUFFERINFO *)pDstOutputData->extInfo;
    bufferGeometry  = &pHevcDec->hMFCHevcHandle.codecOutbufConf;
    pBufferInfo->imageWidth       = bufferGeometry->nFrameWidth;
    pBufferInfo->imageHeight      = bufferGeometry->nFrameHeight;
    pBufferInfo->imageStride      = bufferGeometry->nStride;
    pBufferInfo->cropRect.nLeft   = bufferGeometry->cropRect.nLeft;
    pBufferInfo->cropRect.nTop    = bufferGeometry->cropRect.nTop;
    pBufferInfo->cropRect.nWidth  = bufferGeometry->cropRect.nWidth;
    pBufferInfo->cropRect.nHeight = bufferGeometry->cropRect.nHeight;
    pBufferInfo->colorFormat      = Exynos_OSAL_Video2OMXFormat((int)bufferGeometry->eColorFormat);
    Exynos_OSAL_Memcpy(&pBufferInfo->PDSB, &pVideoBuffer->PDSB, sizeof(PrivateDataShareBuffer));

    if (pOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        int i = 0;
        pDstOutputData->pPrivate = NULL;
        for (i = 0; i < MFC_OUTPUT_BUFFER_NUM_MAX; i++) {
            if (pDstOutputData->buffer.addr[0] ==
                pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[0]) {
                pDstOutputData->pPrivate = pVideoDec->pMFCDecOutputBuffer[i];
                break;
            }
        }

        if (pDstOutputData->pPrivate == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Can not find a codec buffer", pExynosComponent, __FUNCTION__);
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            goto EXIT;
        }

        /* calculate each plane info for the application */
        Exynos_OSAL_GetPlaneSize(pOutputPort->portDefinition.format.video.eColorFormat,
                                 PLANE_SINGLE, pOutputPort->portDefinition.format.video.nFrameWidth,
                                 pOutputPort->portDefinition.format.video.nFrameHeight,
                                 nDataLen, nAllocLen);

        pDstOutputData->allocSize = nAllocLen[0] + nAllocLen[1] + nAllocLen[2];
        pDstOutputData->dataLen   = nDataLen[0] + nDataLen[1] + nDataLen[2];
    }

    /* For Share Buffer */
    pDstOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE *)pVideoBuffer->pPrivate;

    /* update extra info */
    {
        /* HDR */
        if (pVideoBuffer->frameType & VIDEO_FRAME_WITH_HDR_INFO)
            HevcCodecUpdateHdrInfo(pOMXComponent, pBufferInfo);

        /* SBWC Normal format */
        if (pVideoBuffer->frameType & VIDEO_FRAME_NEED_ACTUAL_FORMAT) {
            nVideoFormat = pDecOps->Get_ActualFormat(hMFCHandle);

            if (nVideoFormat != VIDEO_COLORFORMAT_UNKNOWN) {
                nOMXFormat = Exynos_OSAL_Video2OMXFormat((int)nVideoFormat);

                if (nOMXFormat != OMX_COLOR_FormatUnused) {
                    nPixelFormat = Exynos_OSAL_OMX2HALPixelFormat(nOMXFormat, pOutputPort->ePlaneType);

                    if (nPixelFormat != 0) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Normal format at SBWC is 0x%x",
                                                            pExynosComponent, __FUNCTION__, nPixelFormat);
                        pHevcDec->hMFCHevcHandle.nActualFormat = nPixelFormat;
                    }
                }
            }
        }
    }

    /* update extra information to vendor path for renderer
     * if BUFFER_COPY_FORCE is used, it will be updated at Exynos_CSC_OutputData()
     */
    if ((pOutputPort->bufferProcessType == BUFFER_SHARE) &&
        (pVideoBuffer->planes[2].addr != NULL)) {
        HevcCodecUpdateExtraInfo(pOMXComponent, pVideoBuffer->planes[2].addr);
    }

    indexTimestamp = pDecOps->Get_FrameTag(hMFCHandle);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] out indexTimestamp: %d", pExynosComponent, __FUNCTION__, indexTimestamp);

    if (pVideoDec->bReorderMode == OMX_FALSE) {
        if ((indexTimestamp < 0) || (indexTimestamp >= MAX_TIMESTAMP)) {
            if ((pExynosComponent->checkTimeStamp.needSetStartTimeStamp != OMX_TRUE) &&
                (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp != OMX_TRUE)) {
                if (indexTimestamp == INDEX_AFTER_EOS) {
                    pDstOutputData->timeStamp = 0x00;
                    pDstOutputData->nFlags = 0x00;
                } else {
                    pDstOutputData->timeStamp = pExynosComponent->timeStamp[pHevcDec->hMFCHevcHandle.outputIndexTimestamp];
                    pDstOutputData->nFlags = pExynosComponent->nFlags[pHevcDec->hMFCHevcHandle.outputIndexTimestamp];
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] missing out indexTimestamp: %d", pExynosComponent, __FUNCTION__, indexTimestamp);
                }
            } else {
                pDstOutputData->timeStamp = 0x00;
                pDstOutputData->nFlags = 0x00;
            }
        } else {
            /* For timestamp correction. if mfc support frametype detect */
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] disp_pic_frame_type: %d", pExynosComponent, __FUNCTION__, pVideoBuffer->frameType);

            /* NEED TIMESTAMP REORDER */
            if (pVideoDec->bDTSMode == OMX_TRUE) {
                if ((pVideoBuffer->frameType & VIDEO_FRAME_I) ||
                    ((pVideoBuffer->frameType & VIDEO_FRAME_OTHERS) &&
                        ((pExynosComponent->nFlags[indexTimestamp] & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) ||
                    (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp == OMX_TRUE))
                   pHevcDec->hMFCHevcHandle.outputIndexTimestamp = indexTimestamp;
                else
                   indexTimestamp = pHevcDec->hMFCHevcHandle.outputIndexTimestamp;
            }

            pDstOutputData->timeStamp   = pExynosComponent->timeStamp[indexTimestamp];
            pDstOutputData->nFlags      = pExynosComponent->nFlags[indexTimestamp] | OMX_BUFFERFLAG_ENDOFFRAME;

            if (pVideoBuffer->frameType & VIDEO_FRAME_I)
                pDstOutputData->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output / buffer header(%p), nFlags: 0x%x, timestamp %lld us (%.2f secs), tag: %d",
                                                    pExynosComponent, __FUNCTION__,
                                                    pDstOutputData->bufferHeader, pDstOutputData->nFlags,
                                                    pDstOutputData->timeStamp, (double)(pDstOutputData->timeStamp / 1E6),
                                                    indexTimestamp);
    } else {  /* MSRND */
        EXYNOS_OMX_CURRENT_FRAME_TIMESTAMP sCurrentTimestamp;

        Exynos_GetReorderTimestamp(pExynosComponent, &sCurrentTimestamp, indexTimestamp, pVideoBuffer->frameType);

        pDstOutputData->timeStamp   = sCurrentTimestamp.timeStamp;
        pDstOutputData->nFlags      = sCurrentTimestamp.nFlags | OMX_BUFFERFLAG_ENDOFFRAME;

        pExynosComponent->nFlags[sCurrentTimestamp.nIndex]               = 0x00;
        pExynosComponent->bTimestampSlotUsed[sCurrentTimestamp.nIndex]   = OMX_FALSE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output / buffer header(%p), nFlags: 0x%x, timestamp %lld us (%.2f secs), reordered tag: %d, original tag: %d",
                                                    pExynosComponent, __FUNCTION__,
                                                    pDstOutputData->bufferHeader, pDstOutputData->nFlags,
                                                    pDstOutputData->timeStamp, (double)(pDstOutputData->timeStamp / 1E6),
                                                    sCurrentTimestamp.nIndex,
                                                    indexTimestamp);
    }

    if (pVideoBuffer->frameType & VIDEO_FRAME_WITH_BLACK_BAR) {
        if (HevcCodecUpdateBlackBarCrop(pOMXComponent) != OMX_ErrorNone)
            goto EXIT;
    }

    if (pVideoBuffer->frameType & VIDEO_FRAME_CONCEALMENT)
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_DATACORRUPT;

    if (pVideoBuffer->frameType & VIDEO_FRAME_CORRUPT) {
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_DATACORRUPT;
        ret = (OMX_ERRORTYPE)OMX_ErrorNoneReuseBuffer;
        goto EXIT;
    }

#ifdef PERFORMANCE_DEBUG
    if (pDstOutputData->bufferHeader != NULL) {
        pDstOutputData->bufferHeader->nTimeStamp = pDstOutputData->timeStamp;
        Exynos_OSAL_V4L2CountDecrease(pOutputPort->hBufferCount, pDstOutputData->bufferHeader, OUTPUT_PORT_INDEX);
    }
#endif

    if ((!(pVideoBuffer->frameType & VIDEO_FRAME_B)) &&
        (pExynosComponent->bSaveFlagEOS == OMX_TRUE)) {
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_EOS;
    }

    if (displayStatus == VIDEO_FRAME_STATUS_DECODING_FINISHED) {
        pDstOutputData->remainDataLen = 0;

        if ((indexTimestamp < 0) || (indexTimestamp >= MAX_TIMESTAMP)) {
            if (indexTimestamp != INDEX_AFTER_EOS)
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] tag(%d) is wrong", pExynosComponent, __FUNCTION__, indexTimestamp);

            pDstOutputData->timeStamp   = 0x00;
            pDstOutputData->nFlags      = 0x00;
            goto EXIT;
        }

        if ((pExynosComponent->nFlags[indexTimestamp] & OMX_BUFFERFLAG_EOS) ||
            (pExynosComponent->bSaveFlagEOS == OMX_TRUE)) {
            pDstOutputData->nFlags |= OMX_BUFFERFLAG_EOS;
            pExynosComponent->nFlags[indexTimestamp] &= (~OMX_BUFFERFLAG_EOS);
        }
    } else if ((pDstOutputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
        pDstOutputData->remainDataLen = 0;

        if (pExynosComponent->bBehaviorEOS == OMX_TRUE) {
            pDstOutputData->remainDataLen = nDataLen[0] + nDataLen[1] + nDataLen[2];

            if (!(pVideoBuffer->frameType & VIDEO_FRAME_B)) {
                pExynosComponent->bBehaviorEOS = OMX_FALSE;
            } else {
                pExynosComponent->bSaveFlagEOS = OMX_TRUE;
                pDstOutputData->nFlags &= (~OMX_BUFFERFLAG_EOS);
            }
        }
    } else {
        pDstOutputData->remainDataLen = nDataLen[0] + nDataLen[1] + nDataLen[2];
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_srcInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = (OMX_ERRORTYPE)OMX_ErrorInputDataDecodeYet;
        goto EXIT;
    }

    if ((pVideoDec->bForceHeaderParsing == OMX_FALSE) &&
        (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX))) {
        ret = (OMX_ERRORTYPE)OMX_ErrorInputDataDecodeYet;
        goto EXIT;
    }

    ret = Exynos_HevcDec_SrcIn(pOMXComponent, pSrcInputData);
    if ((ret != OMX_ErrorNone) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorInputDataDecodeYet) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorNeedNextHeaderInfo) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorNoneSrcSetupFinish) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorCorruptedFrame)) {

        if (((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorCorruptedHeader) &&
            (pVideoDec->bDiscardCSDError == OMX_TRUE)) {
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                pExynosComponent, __FUNCTION__);
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_srcOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pExynosInputPort   = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    if ((pHevcDec->bSourceStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pExynosInputPort))) {
        Exynos_OSAL_SignalWait(pHevcDec->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoDec->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get SourceStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pHevcDec->hSourceStartEvent);
    }

    ret = Exynos_HevcDec_SrcOut(pOMXComponent, pSrcOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                pExynosComponent, __FUNCTION__);
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_dstInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) ||
        (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        if (pExynosComponent->currentState == OMX_StatePause)
            ret = (OMX_ERRORTYPE)OMX_ErrorOutputBufferUseYet;
        else
            ret = OMX_ErrorNone;
        goto EXIT;
    }

    if ((pHevcDec->bDestinationStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
        Exynos_OSAL_SignalWait(pHevcDec->hDestinationInStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoDec->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationInStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pHevcDec->hDestinationInStartEvent);
    }

    if (pExynosOutputPort->bufferProcessType == BUFFER_SHARE) {
        if (Exynos_OSAL_GetElemNum(&pHevcDec->bypassBufferInfoQ) > 0) {
            BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Dequeue(&pHevcDec->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bypassBufferInfoQ has EOS buffer", pExynosComponent, __FUNCTION__);

            pDstInputData->bufferHeader->nFlags     = pBufferInfo->nFlags;
            pDstInputData->bufferHeader->nTimeStamp = pBufferInfo->timeStamp;
            Exynos_OMX_OutputBufferReturn(pOMXComponent, pDstInputData->bufferHeader);
            Exynos_OSAL_Free(pBufferInfo);

            ret = OMX_ErrorNone;
            goto EXIT;
        }

        if ((pVideoDec->bReconfigDPB == OMX_TRUE) &&
            (pExynosOutputPort->exceptionFlag == GENERAL_STATE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] do DstSetup", pExynosComponent, __FUNCTION__);
            ret = HevcCodecDstSetup(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to HevcCodecDstSetup(0x%x)", pExynosComponent, __FUNCTION__, ret);
                goto EXIT;
            }

            pVideoDec->bReconfigDPB = OMX_FALSE;
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationOutStartEvent);
        }
    }

    if (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_HevcDec_DstIn(pOMXComponent, pDstInputData);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                    pExynosComponent, __FUNCTION__);
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
        }
    } else {
        if ((pHevcDec->bDestinationStart == OMX_FALSE) &&
            (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            ret = (OMX_ERRORTYPE)OMX_ErrorNoneReuseBuffer;
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_dstOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) ||
        (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (((pHevcDec->bDestinationStart == OMX_FALSE) ||
         (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_FALSE)) &&
        (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
        Exynos_OSAL_SignalWait(pHevcDec->hDestinationOutStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoDec->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationOutStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pHevcDec->hDestinationOutStartEvent);
    }

    if (pExynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        if (Exynos_OSAL_GetElemNum(&pHevcDec->bypassBufferInfoQ) > 0) {
            EXYNOS_OMX_DATABUFFER *dstOutputUseBuffer   = &pExynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
            OMX_BUFFERHEADERTYPE  *pOMXBuffer           = NULL;
            BYPASS_BUFFER_INFO    *pBufferInfo          = NULL;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bypassBufferInfoQ has EOS buffer", pExynosComponent, __FUNCTION__);

            if (dstOutputUseBuffer->dataValid == OMX_FALSE) {
                pOMXBuffer = Exynos_OutputBufferGetQueue_Direct(pExynosComponent);
                if (pOMXBuffer == NULL) {
                    ret = OMX_ErrorUndefined;
                    goto EXIT;
                }
            } else {
                pOMXBuffer = dstOutputUseBuffer->bufferHeader;
            }

            pBufferInfo = Exynos_OSAL_Dequeue(&pHevcDec->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            pOMXBuffer->nFlags      = pBufferInfo->nFlags;
            pOMXBuffer->nTimeStamp  = pBufferInfo->timeStamp;
            Exynos_OMX_OutputBufferReturn(pOMXComponent, pOMXBuffer);
            Exynos_OSAL_Free(pBufferInfo);

            dstOutputUseBuffer->dataValid = OMX_FALSE;

            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    ret = Exynos_HevcDec_DstOut(pOMXComponent, pDstOutputData);
    if (((ret != OMX_ErrorNone) &&
         (ret != OMX_ErrorNoneReuseBuffer)) &&
         (pExynosComponent->currentState == OMX_StateExecuting)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                pExynosComponent, __FUNCTION__);
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(
    OMX_HANDLETYPE  hComponent,
    OMX_STRING      componentName)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;
    OMX_BOOL                         bSecureMode        = OMX_FALSE;
    int i = 0;

    Exynos_OSAL_Get_Log_Property(); // For debuging
    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_HEVC_DEC, componentName) == 0) {
        bSecureMode = OMX_FALSE;
    } else if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_HEVC_DRM_DEC, componentName) == 0) {
        bSecureMode = OMX_TRUE;
    } else {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unsupported component name(%s)", __FUNCTION__, componentName);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_VideoDecodeComponentInit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to VideoDecodeComponentInit (0x%x)", componentName, __FUNCTION__, ret);
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosComponent->codecType = (bSecureMode == OMX_TRUE)? HW_VIDEO_DEC_SECURE_CODEC:HW_VIDEO_DEC_CODEC;

    pExynosComponent->componentName = (OMX_STRING)Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pExynosComponent->componentName == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x)", pExynosComponent, __FUNCTION__, ret);
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);

    pHevcDec = Exynos_OSAL_Malloc(sizeof(EXYNOS_HEVCDEC_HANDLE));
    if (pHevcDec == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x)", pExynosComponent, __FUNCTION__, ret);
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pHevcDec, 0, sizeof(EXYNOS_HEVCDEC_HANDLE));
    pVideoDec               = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoDec->hCodecHandle = (OMX_HANDLETYPE)pHevcDec;
    pHevcDec->hMFCHevcHandle.nDisplayDelay = MAX_HEVC_DISPLAYDELAY_VALIDNUM + 1;

    Exynos_OSAL_Strcpy(pExynosComponent->componentName, componentName);

#ifdef USE_S3D_SUPPORT
    pHevcDec->hMFCHevcHandle.S3DFPArgmtType = OMX_SEC_FPARGMT_INVALID;
#endif

    /* Set componentVersion */
    pExynosComponent->componentVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->componentVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->componentVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->componentVersion.s.nStep         = STEP_NUMBER;
    /* Set specVersion */
    pExynosComponent->specVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->specVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->specVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->specVersion.s.nStep         = STEP_NUMBER;

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth  = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride      = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    if (IS_CUSTOM_COMPONENT(pExynosComponent->componentName) == OMX_TRUE) {
#ifdef USE_SMALL_SECURE_MEMORY
        if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)
            pExynosPort->portDefinition.nBufferSize = CUSTOM_LIMITED_DRM_INPUT_BUFFER_SIZE;
        else
#endif
            pExynosPort->portDefinition.nBufferSize = CUSTOM_DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    }

    pVideoDec->nMinInBufSize = DEFAULT_VIDEO_MIN_INPUT_BUFFER_SIZE;  /* for DRC */

    pExynosPort->portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/hevc");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_SINGLE;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth  = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride      = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_COPY;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_MULTIPLE;

#ifdef USE_SINGLE_PLANE_IN_DRM
    if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)
        pExynosPort->ePlaneType = PLANE_SINGLE;
#endif

    for(i = 0; i < ALL_PORT_NUM; i++) {
        INIT_SET_SIZE_VERSION(&pHevcDec->HevcComponent[i], OMX_VIDEO_PARAM_HEVCTYPE);
        pHevcDec->HevcComponent[i].nPortIndex = i;
        pHevcDec->HevcComponent[i].eProfile   = OMX_VIDEO_HEVCProfileMain;
        pHevcDec->HevcComponent[i].eLevel     = OMX_VIDEO_HEVCMainTierLevel5;
    }

    pOMXComponent->GetParameter      = &Exynos_HevcDec_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_HevcDec_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_HevcDec_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_HevcDec_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_HevcDec_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_HevcDec_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_HevcDec_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_HevcDec_Terminate;

    pVideoDec->exynos_codec_srcInputProcess  = &Exynos_HevcDec_srcInputBufferProcess;
    pVideoDec->exynos_codec_srcOutputProcess = &Exynos_HevcDec_srcOutputBufferProcess;
    pVideoDec->exynos_codec_dstInputProcess  = &Exynos_HevcDec_dstInputBufferProcess;
    pVideoDec->exynos_codec_dstOutputProcess = &Exynos_HevcDec_dstOutputBufferProcess;

    pVideoDec->exynos_codec_start            = &HevcCodecStart;
    pVideoDec->exynos_codec_stop             = &HevcCodecStop;
    pVideoDec->exynos_codec_bufferProcessRun = &HevcCodecOutputBufferProcessRun;
    pVideoDec->exynos_codec_enqueueAllBuffer = &HevcCodecEnQueueAllBuffer;

    pVideoDec->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;
    pVideoDec->exynos_codec_reconfigAllBuffers        = &HevcCodecReconfigAllBuffers;

    pVideoDec->exynos_codec_checkFormatSupport      = &CheckFormatHWSupport;
    pVideoDec->exynos_codec_checkResolutionChange   = &HevcCodecCheckResolution;

    pVideoDec->exynos_codec_updateExtraInfo = &HevcCodecUpdateExtraInfo;

    pVideoDec->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoDec->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Open", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pHevcDec);
        pHevcDec = pVideoDec->hCodecHandle = NULL;
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.videoInstInfo.eCodecType = VIDEO_CODING_HEVC;
    if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)
        pHevcDec->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_SECURE;
    else
        pHevcDec->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_NORMAL;

    if (Exynos_Video_GetInstInfo(&(pHevcDec->hMFCHevcHandle.videoInstInfo), VIDEO_TRUE /* dec */) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to GetInstInfo", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pHevcDec);
        pHevcDec = pVideoDec->hCodecHandle = NULL;
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportInfo.dec.bDrvDPBManageSupport == VIDEO_TRUE)
        pVideoDec->bDrvDPBManaging = OMX_TRUE;
    else
        pVideoDec->hRefHandle = Exynos_OSAL_RefCount_Create();


    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] GetInstInfo for dec : skype(%d)/HDR-DY(%d)/DrvDPBManaging(%d)", pExynosComponent, __FUNCTION__,
            (pHevcDec->hMFCHevcHandle.videoInstInfo.supportInfo.dec.bSkypeSupport),
            (pHevcDec->hMFCHevcHandle.videoInstInfo.supportInfo.dec.bHDRDynamicInfoSupport),
            (pHevcDec->hMFCHevcHandle.videoInstInfo.supportInfo.dec.bDrvDPBManageSupport));

    Exynos_Output_SetSupportFormat(pExynosComponent);
    SetProfileLevel(pExynosComponent);

    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-dec-use-buffer-copy", (OMX_INDEXTYPE)OMX_IndexExynosParamBufferCopy);

#ifdef USE_ANDROID
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-dec-compressed-color-format", (OMX_INDEXTYPE)OMX_IndexParamVideoCompressedColorFormat);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-imageformat-filter-enableInplace", (OMX_INDEXTYPE)OMX_IndexExynosParamImageConvertMode);
#endif

    pExynosComponent->currentState = OMX_StateLoaded;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(
    OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent    = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoDec        = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (((pExynosComponent->currentState != OMX_StateInvalid) &&
         (pExynosComponent->currentState != OMX_StateLoaded)) ||
        ((pExynosComponent->currentState == OMX_StateLoaded) &&
         (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle))) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] in curState(0x%x), OMX_FreeHandle() is called. change to OMX_StateInvalid",
                                            pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        Exynos_OMX_Component_AbnormalTermination(hComponent);
    }

    Exynos_OSAL_SharedMemory_Close(pVideoDec->hSharedMemory);

    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec != NULL) {
        Exynos_OSAL_Free(pHevcDec);
        pHevcDec = pVideoDec->hCodecHandle = NULL;
    }

    ret = Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to VideoDecodeComponentDeinit", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}
