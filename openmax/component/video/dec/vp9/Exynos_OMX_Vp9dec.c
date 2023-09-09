/*
 *
 * Copyright 2014 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OMX_Vp9dec.c
 * @brief
 * @author     Taehwan Kim (t_h.kim@samsung.com)
 *             SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version    2.0.0
 * @history
 *   2014.07.24 : Create
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
#include "Exynos_OMX_Vp9dec.h"
#include "ExynosVideoApi.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"

#include "Exynos_OSAL_Platform.h"

#include "VendorVideoAPI.h"

/* To use CSC_METHOD_HW in EXYNOS OMX, gralloc should allocate physical memory using FIMC */
/* It means GRALLOC_USAGE_HW_FIMC1 should be set on Native Window usage */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VP9_DEC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


static OMX_ERRORTYPE SetProfileLevel(
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec        = NULL;

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

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVp9Dec->hMFCVp9Handle.profiles[nProfileCnt++] = OMX_VIDEO_VP9Profile0;
    switch (pVp9Dec->hMFCVp9Handle.videoInstInfo.HwVersion) {
    case MFC_1501:
    case MFC_150:
    case MFC_1410:
    case MFC_1400:
    case MFC_140:
    case MFC_130:
        pVp9Dec->hMFCVp9Handle.profiles[nProfileCnt++] = OMX_VIDEO_VP9Profile2;
        pVp9Dec->hMFCVp9Handle.profiles[nProfileCnt++] = OMX_VIDEO_VP9Profile2HDR;
        pVp9Dec->hMFCVp9Handle.profiles[nProfileCnt++] = OMX_VIDEO_VP9Profile2HDR10Plus;
        break;
    case MFC_120:
    case MFC_1220:
        pVp9Dec->hMFCVp9Handle.profiles[nProfileCnt++] = OMX_VIDEO_VP9Profile2;
        pVp9Dec->hMFCVp9Handle.profiles[nProfileCnt++] = OMX_VIDEO_VP9Profile2HDR;
        break;
    default:
        break;
    }
    pVp9Dec->hMFCVp9Handle.nProfileCnt = nProfileCnt;

    switch (pVp9Dec->hMFCVp9Handle.videoInstInfo.HwVersion) {
    case MFC_1501:
    case MFC_150:
        pVp9Dec->hMFCVp9Handle.maxLevel = OMX_VIDEO_VP9Level6;
        break;
    case MFC_1410:
    case MFC_1400:
    case MFC_140:
    case MFC_130:
    case MFC_120:
    case MFC_1220:
        pVp9Dec->hMFCVp9Handle.maxLevel = OMX_VIDEO_VP9Level51;
        break;
    default:
        pVp9Dec->hMFCVp9Handle.maxLevel = OMX_VIDEO_VP9Level41;
        break;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE GetIndexToProfileLevel(
    EXYNOS_OMX_BASECOMPONENT         *pExynosComponent,
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevelType)
{
    OMX_ERRORTYPE                    ret           = OMX_ErrorNone;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec     = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec       = NULL;

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

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (pVp9Dec->hMFCVp9Handle.nProfileCnt <= (int)pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pVp9Dec->hMFCVp9Handle.profiles[pProfileLevelType->nProfileIndex];
    pProfileLevelType->eLevel   = pVp9Dec->hMFCVp9Handle.maxLevel;
#else
    while ((pVp9Dec->hMFCVp9Handle.maxLevel >> nLevelCnt) > 0) {
        nLevelCnt++;
    }

    if ((pVp9Dec->hMFCVp9Handle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    nMaxIndex = pVp9Dec->hMFCVp9Handle.nProfileCnt * nLevelCnt;
    if (nMaxIndex <= pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pVp9Dec->hMFCVp9Handle.profiles[pProfileLevelType->nProfileIndex / nLevelCnt];
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
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec    = NULL;

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

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec == NULL)
        goto EXIT;

    while ((pVp9Dec->hMFCVp9Handle.maxLevel >> nLevelCnt++) > 0);

    if ((pVp9Dec->hMFCVp9Handle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pVp9Dec->hMFCVp9Handle.nProfileCnt; i++) {
        if (pVp9Dec->hMFCVp9Handle.profiles[i] == pProfileLevelType->eProfile) {
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

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] : profile(%x)/level(%x) is %ssupported", pExynosComponent, __FUNCTION__,
                                            pProfileLevelType->eProfile, pProfileLevelType->eLevel,
                                            (bProfileSupport && bLevelSupport)? "":"not ");

EXIT:
    FunctionOut();

    return (bProfileSupport && bLevelSupport);
}

static OMX_ERRORTYPE GetCodecOutputPrivateData(OMX_PTR codecBuffer, OMX_PTR addr[], OMX_U32 size[])
{
    OMX_ERRORTYPE       ret          = OMX_ErrorNone;
    ExynosVideoBuffer  *pCodecBuffer = NULL;

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

OMX_BOOL Check_VP9_StartCode(
    OMX_U8     *pInputStream,
    OMX_U32     streamSize)
{
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] buffer: %p, streamSize: %d", __FUNCTION__, pInputStream, streamSize);
    if (streamSize < 1) {
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

OMX_BOOL CheckFormatHWSupport(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_COLOR_FORMATTYPE         eColorFormat)
{
    OMX_BOOL                         ret            = OMX_FALSE;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec        = NULL;
    EXYNOS_OMX_BASEPORT             *pOutputPort    = NULL;
    ExynosVideoColorFormatType       eVideoFormat   = VIDEO_COLORFORMAT_UNKNOWN;
    int i;

    if (pExynosComponent == NULL)
        goto EXIT;

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL)
        goto EXIT;

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec == NULL)
        goto EXIT;
    pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    eVideoFormat = (ExynosVideoColorFormatType)Exynos_OSAL_OMX2VideoFormat(eColorFormat, pOutputPort->ePlaneType);

    for (i = 0; i < VIDEO_COLORFORMAT_MAX; i++) {
        if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportFormat[i] == VIDEO_COLORFORMAT_UNKNOWN)
            break;

        if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportFormat[i] == eVideoFormat) {
            ret = OMX_TRUE;
            break;
        }
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE VP9CodecOpen(EXYNOS_VP9DEC_HANDLE *pVp9Dec, ExynosVideoInstInfo *pVideoInstInfo)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if ((pVp9Dec == NULL) ||
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

    pVp9Dec->hMFCVp9Handle.pDecOps    = pDecOps;
    pVp9Dec->hMFCVp9Handle.pInbufOps  = pInbufOps;
    pVp9Dec->hMFCVp9Handle.pOutbufOps = pOutbufOps;

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
    pVp9Dec->hMFCVp9Handle.hMFCHandle = pVp9Dec->hMFCVp9Handle.pDecOps->Init(pVideoInstInfo);
    if (pVp9Dec->hMFCVp9Handle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to init", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pDecOps != NULL) {
            Exynos_OSAL_Free(pDecOps);
            pVp9Dec->hMFCVp9Handle.pDecOps = NULL;
        }
        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pVp9Dec->hMFCVp9Handle.pInbufOps = NULL;
        }
        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pVp9Dec->hMFCVp9Handle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP9CodecClose(EXYNOS_VP9DEC_HANDLE *pVp9Dec)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pVp9Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    pDecOps    = pVp9Dec->hMFCVp9Handle.pDecOps;
    pInbufOps  = pVp9Dec->hMFCVp9Handle.pInbufOps;
    pOutbufOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pDecOps->Finalize(hMFCHandle);
        pVp9Dec->hMFCVp9Handle.hMFCHandle = NULL;
        pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc = OMX_FALSE;
        pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst = OMX_FALSE;
    }

    /* Unregister function pointers */
    Exynos_Video_Unregister_Decoder(pDecOps, pInbufOps, pOutbufOps);

    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pVp9Dec->hMFCVp9Handle.pOutbufOps = NULL;
    }
    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pVp9Dec->hMFCVp9Handle.pInbufOps = NULL;
    }
    if (pDecOps != NULL) {
        Exynos_OSAL_Free(pDecOps);
        pVp9Dec->hMFCVp9Handle.pDecOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP9CodecStart(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = NULL;
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

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    pInbufOps  = pVp9Dec->hMFCVp9Handle.pInbufOps;
    pOutbufOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc == OMX_TRUE)) {
        pInbufOps->Run(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst == OMX_TRUE)) {
        pOutbufOps->Run(hMFCHandle);
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP9CodecStop(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = NULL;
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

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    pInbufOps  = pVp9Dec->hMFCVp9Handle.pInbufOps;
    pOutbufOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;

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

OMX_ERRORTYPE VP9CodecOutputBufferProcessRun(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = NULL;

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

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pVp9Dec->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pVp9Dec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pVp9Dec->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        } else if (pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Vp9CodecReconfigAllBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = &pExynosComponent->pExynosPort[nPortIndex];
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    ExynosVideoDecBufferOps         *pBufferOps         = NULL;

    FunctionIn();

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pVp9Dec->bSourceStart == OMX_TRUE)) {
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pVp9Dec->bDestinationStart == OMX_TRUE)) {
        pBufferOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;

        if (pExynosPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
            /**********************************/
            /* Codec Buffer Free & Unregister */
            /**********************************/
            Exynos_Free_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX);
            Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);
            pBufferOps->Clear_RegisteredBuffer(hMFCHandle);
            pBufferOps->Cleanup_Buffer(hMFCHandle);

            pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst = OMX_FALSE;

            /******************************************************/
            /* V4L2 Destnation Setup for DPB Buffer Number Change */
            /******************************************************/
            ret = VP9CodecDstSetup(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to Vp9CodecDstSetup(0x%x)",
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

            pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst = OMX_FALSE;
        }
    } else {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP9CodecEnQueueAllBuffer(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;

    ExynosVideoDecBufferOps *pInbufOps  = pVp9Dec->hMFCVp9Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;

    int i;

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
               (pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);

        for (i = 0; i < pVp9Dec->hMFCVp9Handle.maxDPBNum; i++) {
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

void Vp9CodecUpdateHdrInfo(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec           = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort        = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    void                          *hMFCHandle        = pVp9Dec->hMFCVp9Handle.hMFCHandle;

    ExynosVideoDecOps  *pDecOps     = pVp9Dec->hMFCVp9Handle.pDecOps;
    ExynosVideoHdrInfo *pHdrInfo    = &(pVp9Dec->hMFCVp9Handle.sHdrInfo);

    if (pDecOps->Get_HDRInfo(hMFCHandle, pHdrInfo) == VIDEO_ERROR_NONE) {
        /* update bitstream's info to input port */
        EXYNOS_OMX_VIDEO_COLORASPECTS *pColorAspects = &(pInputPort->ColorAspects);

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] eType(0x%x) is changed", pExynosComponent, __FUNCTION__, pHdrInfo->eChangedType);

        /* color aspects */
        if (pHdrInfo->eValidType & (HDR_INFO_COLOR_ASPECTS | HDR_INFO_RANGE)) {
            /* HDR_INFO_COLOR_ASPECTS (8) */
            pColorAspects->nCoeffType      = pHdrInfo->sColorAspects.eCoeffType;
            pColorAspects->nPrimaryType    = pHdrInfo->sColorAspects.ePrimariesType;
            pColorAspects->nTransferType   = pHdrInfo->sColorAspects.eTransferType;

            /* HDR_INFO_RANGE (16) */
            pColorAspects->nRangeType      = pHdrInfo->sColorAspects.eRangeType;
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
        }
#endif
    }

EXIT:

    return;
}

OMX_ERRORTYPE VP9CodecUpdateExtraInfo(
    OMX_COMPONENTTYPE   *pOMXComponent,
    ExynosVideoMeta     *pMeta)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT           *pOutputPort       = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec           = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;

    if (pMeta == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pMeta->eType = VIDEO_INFO_TYPE_INVALID;

    /* hdr static info : the bitstream doesn't have this. always sets value set by framework */
    {
        ExynosType1                     *pMetaHDR       = &(pMeta->sHdrStaticInfo.sType1);
        EXYNOS_OMX_VIDEO_HDRSTATICINFO  *pHDRStaticInfo = &(pOutputPort->HDRStaticInfo);

        EXYNOS_OMX_VIDEO_HDRSTATICINFO invalid;
        Exynos_OSAL_Memset(&invalid, 0, sizeof(invalid));

        if (Exynos_OSAL_Memcmp(pHDRStaticInfo, &invalid, sizeof(EXYNOS_OMX_VIDEO_HDRSTATICINFO))) {
            pMeta->eType = VIDEO_INFO_TYPE_HDR_STATIC;

            pMetaHDR->mMaxFrameAverageLightLevel = pHDRStaticInfo->nMaxPicAverageLight;
            pMetaHDR->mMaxContentLightLevel      = pHDRStaticInfo->nMaxContentLight;
            pMetaHDR->mMaxDisplayLuminance       = pHDRStaticInfo->nMaxDisplayLuminance;
            pMetaHDR->mMinDisplayLuminance       = pHDRStaticInfo->nMinDisplayLuminance;

            pMetaHDR->mR.x = pHDRStaticInfo->red.x;
            pMetaHDR->mR.y = pHDRStaticInfo->red.y;

            pMetaHDR->mG.x = pHDRStaticInfo->green.x;
            pMetaHDR->mG.y = pHDRStaticInfo->green.y;

            pMetaHDR->mB.x = pHDRStaticInfo->blue.x;
            pMetaHDR->mB.y = pHDRStaticInfo->blue.y;

            pMetaHDR->mW.x = pHDRStaticInfo->white.x;
            pMetaHDR->mW.y = pHDRStaticInfo->white.y;
        }
    }

    /* Normal format for SBWC black bar */
    {
        if (pVp9Dec->hMFCVp9Handle.nActualFormat != 0) {
            pMeta->nPixelFormat  = pVp9Dec->hMFCVp9Handle.nActualFormat;
            pMeta->eType        |= VIDEO_INFO_TYPE_CHECK_PIXEL_FORMAT;

            pVp9Dec->hMFCVp9Handle.nActualFormat = 0;
        } else {
            pMeta->nPixelFormat = 0;
        }
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE VP9CodecUpdateHDR10PlusInfo(
    OMX_COMPONENTTYPE   *pOMXComponent,
    ExynosVideoMeta     *pMeta,
    void                *pHDR10PlusInfo)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ExynosHdrDynamicInfo          *pExynosHDR10PlusInfo = NULL;

    if ((pMeta == NULL) || (pHDR10PlusInfo == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosHDR10PlusInfo = &(pMeta->sHdrDynamicInfo);

    ret = setHDR10PlusInfoForVendorPath(pOMXComponent, pExynosHDR10PlusInfo, pHDR10PlusInfo);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set HDR10Plus Info for Vendor path", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    pExynosHDR10PlusInfo->valid = 1;

    pMeta->eType |= VIDEO_INFO_TYPE_HDR_DYNAMIC;

EXIT:
    return ret;
}

OMX_ERRORTYPE Vp9CodecUpdateBlackBarCrop(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec            = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec              = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle           = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    OMX_CONFIG_RECTTYPE           *pBlackBarCropRect    = &pVideoDec->blackBarCropRect;

    ExynosVideoDecBufferOps  *pOutbufOps  = pVp9Dec->hMFCVp9Handle.pOutbufOps;
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

OMX_ERRORTYPE Vp9CodecCheckResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_EXCEPTION_STATE     eOutputExcepState  = pOutputPort->exceptionFlag;

    ExynosVideoDecOps             *pDecOps            = pVp9Dec->hMFCVp9Handle.pDecOps;
    ExynosVideoDecBufferOps       *pOutbufOps         = pVp9Dec->hMFCVp9Handle.pOutbufOps;
    ExynosVideoGeometry            codecOutbufConf;

    OMX_CONFIG_RECTTYPE          *pCropRectangle        = &(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT]);
    OMX_PARAM_PORTDEFINITIONTYPE *pInputPortDefinition  = &(pInputPort->portDefinition);
    OMX_PARAM_PORTDEFINITIONTYPE *pOutputPortDefinition = &(pOutputPort->portDefinition);

    int maxDPBNum = 0;

    FunctionIn();

    /* get geometry */
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
    if ((codecOutbufConf.nFrameWidth != pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth) ||
        (codecOutbufConf.nFrameHeight != pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight) ||
        (codecOutbufConf.nStride != pVp9Dec->hMFCVp9Handle.codecOutbufConf.nStride) ||
#if 0  // TODO: check posibility
        (codecOutbufConf.eColorFormat != pVp9Dec->hMFCVp9Handle.codecOutbufConf.eColorFormat) ||
        (codecOutbufConf.eFilledDataType != pVp9Dec->hMFCVp9Handle.codecOutbufConf.eFilledDataType) ||
#endif
        (maxDPBNum != pVp9Dec->hMFCVp9Handle.maxDPBNum)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][DRC] W(%d), H(%d) -> W(%d), H(%d)",
                            pExynosComponent, __FUNCTION__,
                            pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth,
                            pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight,
                            codecOutbufConf.nFrameWidth,
                            codecOutbufConf.nFrameHeight);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][DRC] DPB(%d), FORMAT(0x%x), TYPE(0x%x) -> DPB(%d), FORMAT(0x%x), TYPE(0x%x)",
                            pExynosComponent, __FUNCTION__,
                            pVp9Dec->hMFCVp9Handle.maxDPBNum,
                            pVp9Dec->hMFCVp9Handle.codecOutbufConf.eColorFormat,
                            pVp9Dec->hMFCVp9Handle.codecOutbufConf.eFilledDataType,
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
    if ((codecOutbufConf.cropRect.nTop != pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nTop) ||
        (codecOutbufConf.cropRect.nLeft != pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nLeft) ||
        (codecOutbufConf.cropRect.nWidth != pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth) ||
        (codecOutbufConf.cropRect.nHeight != pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][DRC] CROP: W(%d), H(%d) -> W(%d), H(%d)",
                            pExynosComponent, __FUNCTION__,
                            pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth,
                            pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight,
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

    Exynos_OSAL_Memcpy(&pVp9Dec->hMFCVp9Handle.codecOutbufConf, &codecOutbufConf, sizeof(codecOutbufConf));
    pVp9Dec->hMFCVp9Handle.maxDPBNum = maxDPBNum;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Vp9CodecUpdateResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    ExynosVideoDecOps             *pDecOps            = pVp9Dec->hMFCVp9Handle.pDecOps;
    ExynosVideoDecBufferOps       *pOutbufOps         = pVp9Dec->hMFCVp9Handle.pOutbufOps;

    OMX_CONFIG_RECTTYPE             *pCropRectangle         = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE    *pInputPortDefinition   = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE    *pOutputPortDefinition  = NULL;

    OMX_BOOL bFormatChanged = OMX_FALSE;

    FunctionIn();

    /* get geometry for output */
    Exynos_OSAL_Memset(&pVp9Dec->hMFCVp9Handle.codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    if (pOutbufOps->Get_Geometry(hMFCHandle, &pVp9Dec->hMFCVp9Handle.codecOutbufConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to get geometry about output", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorHardware;
        goto EXIT;
    }

    /* get dpb count */
    pVp9Dec->hMFCVp9Handle.maxDPBNum = pDecOps->Get_ActualBufferCount(hMFCHandle);
#if 0  /* no needs EXTRA_DPB, it was confirmed codec team */
    if (pVideoDec->bThumbnailMode == OMX_FALSE)
        pVp9Dec->hMFCVp9Handle.maxDPBNum += EXTRA_DPB_NUM;
#endif
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] maxDPBNum: %d", pExynosComponent, __FUNCTION__, pVp9Dec->hMFCVp9Handle.maxDPBNum);

    pCropRectangle          = &(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT]);
    pInputPortDefinition    = &(pInputPort->portDefinition);
    pOutputPortDefinition   = &(pOutputPort->portDefinition);

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] past info: width(%d) height(%d)",
                                            pExynosComponent, __FUNCTION__,
                                            pInputPortDefinition->format.video.nFrameWidth,
                                            pInputPortDefinition->format.video.nFrameHeight);

    /* output format is changed internally (8bit <> 10bit) */
    if (pVp9Dec->hMFCVp9Handle.MFCOutputColorType != pVp9Dec->hMFCVp9Handle.codecOutbufConf.eColorFormat) {
        OMX_COLOR_FORMATTYPE eOutputFormat = Exynos_OSAL_Video2OMXFormat(pVp9Dec->hMFCVp9Handle.codecOutbufConf.eColorFormat);

        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] The format(%x) is changed to %x by H/W Codec",
                                    pExynosComponent, __FUNCTION__,
                                    pVp9Dec->hMFCVp9Handle.MFCOutputColorType,
                                    pVp9Dec->hMFCVp9Handle.codecOutbufConf.eColorFormat);

        pVp9Dec->hMFCVp9Handle.MFCOutputColorType = pVp9Dec->hMFCVp9Handle.codecOutbufConf.eColorFormat;
        Exynos_SetPlaneToPort(pOutputPort, Exynos_OSAL_GetPlaneCount(eOutputFormat, pOutputPort->ePlaneType));

        bFormatChanged = OMX_TRUE;

        if (!(pVp9Dec->hMFCVp9Handle.codecOutbufConf.eFilledDataType & DATA_TYPE_8BIT_SBWC)) {
            if (pVideoDec->nCompColorFormat != OMX_COLOR_FormatUnused) {
                /* SBWC to non SBWC : must update eColorFormat */
                pOutputPort->portDefinition.format.video.eColorFormat = eOutputFormat;
            }

            pVideoDec->nCompColorFormat = OMX_COLOR_FormatUnused;
        } else {
            pVideoDec->nCompColorFormat = eOutputFormat;
        }
    }

    switch (pVp9Dec->hMFCVp9Handle.codecOutbufConf.eFilledDataType) {
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

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] resolution info: width(%d / %d), height(%d / %d)",
                                        pExynosComponent, __FUNCTION__,
                                        pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth,
                                        pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth,
                                        pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight,
                                        pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight);

    if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
        pCropRectangle->nTop     = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nTop;
        pCropRectangle->nLeft    = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nLeft;
        pCropRectangle->nWidth   = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth;
        pCropRectangle->nHeight  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight;
    }

    if (pOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        if ((pVideoDec->bReconfigDPB) ||
            (bFormatChanged) ||
            (pInputPortDefinition->format.video.nFrameWidth != pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth) ||
            (pInputPortDefinition->format.video.nFrameHeight != pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pInputPortDefinition->format.video.nFrameWidth  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nFrameHeight = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight;
            pInputPortDefinition->format.video.nStride      = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nSliceHeight = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight;
#if 0
            /* don't need to change */
            pOutputPortDefinition->nBufferCountActual       = pOutputPort->portDefinition.nBufferCountActual;
            pOutputPortDefinition->nBufferCountMin          = pOutputPort->portDefinition.nBufferCountMin;
#endif
            Exynos_UpdateFrameSize(pOMXComponent);

            Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
            pCropRectangle->nTop     = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nTop;
            pCropRectangle->nLeft    = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nLeft;
            pCropRectangle->nWidth   = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth;
            pCropRectangle->nHeight  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight;

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
            (pInputPortDefinition->format.video.nFrameWidth != pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth) ||
            (pInputPortDefinition->format.video.nFrameHeight != pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight) ||
            ((OMX_S32)pOutputPortDefinition->nBufferCountActual != pVp9Dec->hMFCVp9Handle.maxDPBNum) ||
            ((OMX_S32)pOutputPortDefinition->nBufferCountMin < pVp9Dec->hMFCVp9Handle.maxDPBNum)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pInputPortDefinition->format.video.nFrameWidth  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nFrameHeight = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight;
            pInputPortDefinition->format.video.nStride      = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nSliceHeight = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight;

            pOutputPortDefinition->nBufferCountActual       = pVp9Dec->hMFCVp9Handle.maxDPBNum;
            pOutputPortDefinition->nBufferCountMin          = pVp9Dec->hMFCVp9Handle.maxDPBNum;

            Exynos_UpdateFrameSize(pOMXComponent);

            Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
            pCropRectangle->nTop     = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nTop;
            pCropRectangle->nLeft    = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nLeft;
            pCropRectangle->nWidth   = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth;
            pCropRectangle->nHeight  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight;

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
    if ((pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth != pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth) ||
        (pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight != pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight)) {

        if ((pOutputPort->bufferProcessType & BUFFER_COPY) &&
            (pExynosComponent->bUseImgCrop == OMX_TRUE)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;
        }

        pInputPortDefinition->format.video.nFrameWidth  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth;
        pInputPortDefinition->format.video.nFrameHeight = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight;
        pInputPortDefinition->format.video.nStride      = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameWidth;
        pInputPortDefinition->format.video.nSliceHeight = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nFrameHeight;

        Exynos_UpdateFrameSize(pOMXComponent);

        Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);
        pCropRectangle->nTop     = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nTop;
        pCropRectangle->nLeft    = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nLeft;
        pCropRectangle->nWidth   = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth;
        pCropRectangle->nHeight  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight;

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
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Vp9CodecUpdateDisplayResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    ExynosVideoDecBufferOps       *pOutbufOps         = pVp9Dec->hMFCVp9Handle.pOutbufOps;

    OMX_CONFIG_RECTTYPE           *pCropRectangle     = &(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT]);
    ExynosVideoGeometry           *pCodecOutbufConf   = &pVp9Dec->hMFCVp9Handle.codecOutbufConf;
    ExynosVideoGeometry            newCodecOutbufConf;

    FunctionIn();

    Exynos_OSAL_Memset(&newCodecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    if (pOutbufOps->Get_Geometry(hMFCHandle, &newCodecOutbufConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to get geometry", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorHardware;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Inter Resolution Chnage: resol(%d x %d), stride(%d), crop(l:%d, t:%d, w:%d, h:%d) -> resol(%d x %d), stride(%d), crop(l:%d, t:%d, w:%d, h:%d)",
                                        pExynosComponent, __FUNCTION__,
                                        pCodecOutbufConf->nFrameWidth, pCodecOutbufConf->nFrameHeight, pCodecOutbufConf->nStride,
                                        pCodecOutbufConf->cropRect.nLeft, pCodecOutbufConf->cropRect.nTop,
                                        pCodecOutbufConf->cropRect.nWidth, pCodecOutbufConf->cropRect.nHeight,
                                        newCodecOutbufConf.nFrameWidth, newCodecOutbufConf.nFrameHeight, newCodecOutbufConf.nStride,
                                        newCodecOutbufConf.cropRect.nLeft, newCodecOutbufConf.cropRect.nTop,
                                        newCodecOutbufConf.cropRect.nWidth, newCodecOutbufConf.cropRect.nHeight);

    Exynos_OSAL_Memcpy(pCodecOutbufConf, &newCodecOutbufConf, sizeof(ExynosVideoGeometry));

    pCropRectangle->nLeft    = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nLeft;
    pCropRectangle->nTop     = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nTop;
    pCropRectangle->nWidth   = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nWidth;
    pCropRectangle->nHeight  = pVp9Dec->hMFCVp9Handle.codecOutbufConf.cropRect.nHeight;

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

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP9CodecHeaderDecoding(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec           = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize      = pSrcInputData->dataLen;

    ExynosVideoDecBufferOps *pInbufOps  = pVp9Dec->hMFCVp9Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;

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
    Exynos_OSAL_Memset(&pVp9Dec->hMFCVp9Handle.codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    if (pOutbufOps->Get_Geometry(hMFCHandle, &pVp9Dec->hMFCVp9Handle.codecOutbufConf) != VIDEO_ERROR_NONE) {
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

        pInbufOps->Stop(hMFCHandle);
        goto EXIT;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP9CodecSrcSetup(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec           = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE           eOutputFormat     = pExynosOutputPort->portDefinition.format.video.eColorFormat;
    OMX_U32                        allocFrameSize      = 0;

    EXYNOS_OMX_VIDEO_COLORASPECTS   *pFWCA           = &(pExynosOutputPort->ColorAspects);
    EXYNOS_OMX_VIDEO_COLORASPECTS   *pBSCA           = &(pExynosInputPort->ColorAspects);

    ExynosVideoDecOps       *pDecOps    = pVp9Dec->hMFCVp9Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pVp9Dec->hMFCVp9Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;
    ExynosVideoGeometry      bufferConf;

    OMX_U32  nInBufferCnt   = 0;
    OMX_BOOL bSupportFormat = OMX_FALSE;

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
        ret = Exynos_OSAL_Queue(&pVp9Dec->bypassBufferInfoQ, (void *)pBufferInfo);

        if (pExynosOutputPort->bufferProcessType == BUFFER_SHARE) {
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationInStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        } else if (pExynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    /* MFC VP9 Decoder doesn't need CodecPrivate(as CSD) */
    if (pSrcInputData->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] VP9 decoder ignores CodecPrivate data(CSD)", pExynosComponent, __FUNCTION__);
        ret = (OMX_ERRORTYPE)OMX_ErrorCorruptedFrame;
        goto EXIT;
    }

    if (pVideoDec->bThumbnailMode == OMX_TRUE)
        pDecOps->Set_IFrameDecoding(hMFCHandle);

    if ((pDecOps->Enable_DTSMode != NULL) &&
        (pVideoDec->bDTSMode == OMX_TRUE))
        pDecOps->Enable_DTSMode(hMFCHandle);

    if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportInfo.dec.bPrioritySupport == VIDEO_TRUE)
        pDecOps->Set_Priority(hMFCHandle, pVideoDec->nPriority);

    /* input buffer info */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));
    bufferConf.eCompressionFormat = VIDEO_CODING_VP9;
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
        OMX_COLOR_FORMATTYPE eCheckFormat = OMX_SEC_COLOR_FormatNV12Tiled;
        bSupportFormat = CheckFormatHWSupport(pExynosComponent, eCheckFormat);
        if (bSupportFormat != OMX_TRUE) {
            eCheckFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            bSupportFormat = CheckFormatHWSupport(pExynosComponent, eCheckFormat);
        }
        if (bSupportFormat == OMX_TRUE) {  /* supported by CSC(NV12T/NV12 -> format) */
            bufferConf.eColorFormat = Exynos_OSAL_OMX2VideoFormat(eCheckFormat, pExynosOutputPort->ePlaneType);
            Exynos_SetPlaneToPort(pExynosOutputPort, Exynos_OSAL_GetPlaneCount(eCheckFormat, pExynosOutputPort->ePlaneType));
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Can not support this format (0x%x)", pExynosComponent, __FUNCTION__, eOutputFormat);
            ret = OMX_ErrorNotImplemented;
            pInbufOps->Cleanup_Buffer(hMFCHandle);
            goto EXIT;
        }
    }

    pVp9Dec->hMFCVp9Handle.MFCOutputColorType = bufferConf.eColorFormat;
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output video format is 0x%x",
                                            pExynosComponent, __FUNCTION__, bufferConf.eColorFormat);

    bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    if (pOutbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set geometry about output", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        pInbufOps->Cleanup_Buffer(hMFCHandle);
        goto EXIT;
    }

    ret = VP9CodecHeaderDecoding(pOMXComponent, pSrcInputData);
    if (ret != OMX_ErrorNone) {
        pInbufOps->Cleanup_Buffer(hMFCHandle);
        goto EXIT;
    }

    pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc = OMX_TRUE;

    VP9CodecStart(pOMXComponent, INPUT_PORT_INDEX);

    ret = Vp9CodecUpdateResolution(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc = OMX_FALSE;
        VP9CodecStop(pOMXComponent, INPUT_PORT_INDEX);
        pInbufOps->Cleanup_Buffer(hMFCHandle);
        goto EXIT;
    }

    Exynos_OSAL_SleepMillisec(0);
    ret = (OMX_ERRORTYPE)OMX_ErrorInputDataDecodeYet;
    VP9CodecStop(pOMXComponent, INPUT_PORT_INDEX);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP9CodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec            = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec              = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle           = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort    = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecOps       *pDecOps    = pVp9Dec->hMFCVp9Handle.pDecOps;
    ExynosVideoDecBufferOps *pOutbufOps = pVp9Dec->hMFCVp9Handle.pOutbufOps;

    int i, nOutbufs, nPlaneCnt;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {0, 0, 0};

    FunctionIn();

    nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    for (i = 0; i < nPlaneCnt; i++)
        nAllocLen[i] = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nAlignPlaneSize[i];

    VP9CodecStop(pOMXComponent, OUTPUT_PORT_INDEX);

    /* for adaptive playback */
    if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportInfo.dec.bDrvDPBManageSupport != VIDEO_TRUE) {
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
        nOutbufs = pVp9Dec->hMFCVp9Handle.maxDPBNum;
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

        pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst = OMX_TRUE;
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

        pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst = OMX_TRUE;
    }

    if (VP9CodecStart(pOMXComponent, OUTPUT_PORT_INDEX) != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to run output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP9Dec_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec           = NULL;

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
    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoVp9:
    {
        OMX_VIDEO_PARAM_VP9TYPE *pDstVP9Component = (OMX_VIDEO_PARAM_VP9TYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_VP9TYPE *pSrcVP9Component = NULL;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstVP9Component, sizeof(OMX_VIDEO_PARAM_VP9TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstVP9Component->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcVP9Component = &pVp9Dec->VP9Component[pDstVP9Component->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstVP9Component) + nOffset,
                           ((char *)pSrcVP9Component) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_VP9TYPE) - nOffset);
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

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_VP9_DEC_ROLE);
    }
        break;
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pComponentParameterStructure;

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
        OMX_VIDEO_PARAM_PROFILELEVELTYPE    *pDstProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pComponentParameterStructure;
        OMX_VIDEO_PARAM_VP9TYPE             *pSrcVP9Component = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcVP9Component = &pVp9Dec->VP9Component[pDstProfileLevel->nPortIndex];

        pDstProfileLevel->eProfile = pSrcVP9Component->eProfile;
        pDstProfileLevel->eLevel = pSrcVP9Component->eLevel;
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

        pSrcErrorCorrectionType = &pVp9Dec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP9Dec_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec           = NULL;

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
    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamVideoVp9:
    {
        OMX_VIDEO_PARAM_VP9TYPE *pDstVP9Component = NULL;
        OMX_VIDEO_PARAM_VP9TYPE *pSrcVP9Component = (OMX_VIDEO_PARAM_VP9TYPE *)pComponentParameterStructure;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcVP9Component, sizeof(OMX_VIDEO_PARAM_VP9TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcVP9Component->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstVP9Component = &pVp9Dec->VP9Component[pSrcVP9Component->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstVP9Component) + nOffset,
                           ((char *)pSrcVP9Component) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_VP9TYPE) - nOffset);
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

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_VP9_DEC_ROLE)) {
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP9;
        } else {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel  = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_VP9TYPE          *pDstVP9Component  = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstVP9Component = &pVp9Dec->VP9Component[pSrcProfileLevel->nPortIndex];

        if (OMX_FALSE == CheckProfileLevelSupport(pExynosComponent, pSrcProfileLevel)) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pDstVP9Component->eProfile  = pSrcProfileLevel->eProfile;
        pDstVP9Component->eLevel    = pSrcProfileLevel->eLevel;
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

        pDstErrorCorrectionType = &pVp9Dec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP9Dec_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec           = NULL;

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
    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;

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

            if (pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc == OMX_FALSE) {
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
#ifdef USE_ANDROID
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

OMX_ERRORTYPE Exynos_VP9Dec_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec           = NULL;

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
    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
#ifdef USE_ANDROID
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

OMX_ERRORTYPE Exynos_VP9Dec_GetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE  hComponent,
    OMX_IN  OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE  *pIndexType)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

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

#ifdef USE_ANDROID
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_HDR10_PLUS_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoHdr10PlusInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    ret = Exynos_OMX_VideoDecodeGetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP9Dec_ComponentRoleEnum(
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
        Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_VP9_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_VP9Dec_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec           = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;

    ExynosVideoInstInfo *pVideoInstInfo = &(pVp9Dec->hMFCVp9Handle.videoInstInfo);

    CSC_METHOD csc_method = CSC_METHOD_SW;
    int i;

    FunctionIn();

    pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc = OMX_FALSE;
    pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst = OMX_FALSE;
    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] CodecOpen W:%d H:%d Bitrate:%d FPS:%d", pExynosComponent, __FUNCTION__,
                                                                                             pExynosInputPort->portDefinition.format.video.nFrameWidth,
                                                                                             pExynosInputPort->portDefinition.format.video.nFrameHeight,
                                                                                             pExynosInputPort->portDefinition.format.video.nBitrate,
                                                                                             pExynosInputPort->portDefinition.format.video.xFramerate);

    pVideoInstInfo->nSize        = sizeof(ExynosVideoInstInfo);
    pVideoInstInfo->nWidth       = pExynosInputPort->portDefinition.format.video.nFrameWidth;
    pVideoInstInfo->nHeight      = pExynosInputPort->portDefinition.format.video.nFrameHeight;
    pVideoInstInfo->nBitrate     = pExynosInputPort->portDefinition.format.video.nBitrate;
    pVideoInstInfo->xFramerate   = pExynosInputPort->portDefinition.format.video.xFramerate;

    /* VP9 Codec Open */
    ret = VP9CodecOpen(pVp9Dec, pVideoInstInfo);
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

    pVp9Dec->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pVp9Dec->hSourceStartEvent);
    pVp9Dec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pVp9Dec->hDestinationInStartEvent);
    Exynos_OSAL_SignalCreate(&pVp9Dec->hDestinationOutStartEvent);

    INIT_ARRAY_TO_VAL(pExynosComponent->timeStamp, DEFAULT_TIMESTAMP_VAL, MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pVp9Dec->hMFCVp9Handle.indexTimestamp = 0;
    pVp9Dec->hMFCVp9Handle.outputIndexTimestamp = 0;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    Exynos_OSAL_QueueCreate(&pVp9Dec->bypassBufferInfoQ, QUEUE_ELEMENTS);

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
OMX_ERRORTYPE Exynos_VP9Dec_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
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
            EXYNOS_VP9DEC_HANDLE *pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;

            if (pVideoDec->csc_handle != NULL) {
                csc_deinit(pVideoDec->csc_handle);
                pVideoDec->csc_handle = NULL;
            }

            if (pVp9Dec != NULL) {
                Exynos_OSAL_QueueTerminate(&pVp9Dec->bypassBufferInfoQ);

                Exynos_OSAL_SignalTerminate(pVp9Dec->hDestinationInStartEvent);
                pVp9Dec->hDestinationInStartEvent = NULL;
                Exynos_OSAL_SignalTerminate(pVp9Dec->hDestinationOutStartEvent);
                pVp9Dec->hDestinationOutStartEvent = NULL;
                pVp9Dec->bDestinationStart = OMX_FALSE;

                Exynos_OSAL_SignalTerminate(pVp9Dec->hSourceStartEvent);
                pVp9Dec->hSourceStartEvent = NULL;
                pVp9Dec->bSourceStart = OMX_FALSE;
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

            if (pVp9Dec != NULL) {
                VP9CodecClose(pVp9Dec);
            }
        }
    }

    Exynos_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP9Dec_SrcIn(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE          *pVp9Dec           = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize      = pSrcInputData->dataLen;

    ExynosVideoDecOps       *pDecOps     = pVp9Dec->hMFCVp9Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps   = pVp9Dec->hMFCVp9Handle.pInbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    OMX_BUFFERHEADERTYPE tempBufferHeader;
    void *pPrivate = NULL;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {oneFrameSize, 0, 0};
    OMX_BOOL bInStartCode = OMX_FALSE;

    OMX_U32 nConfigCnt       = 0;
    OMX_PTR pHDR10plusConfig = NULL;

    int i;

    FunctionIn();

    if (pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = VP9CodecSrcSetup(pOMXComponent, pSrcInputData);
        goto EXIT;
    }

    if ((pVideoDec->bForceHeaderParsing == OMX_FALSE) &&
        (pVp9Dec->bDestinationStart == OMX_FALSE) &&
        (pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst == OMX_FALSE)) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] do DstSetup", pExynosComponent, __FUNCTION__);
        ret = VP9CodecDstSetup(pOMXComponent);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Vp9CodecDstSetup(0x%x)",
                                            pExynosComponent, __FUNCTION__, ret);
            goto EXIT;
        }
    }

    if (((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) ||
            ((bInStartCode = Check_VP9_StartCode(pSrcInputData->buffer.addr[0], oneFrameSize)) == OMX_TRUE)) ||
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        pExynosComponent->timeStamp[pVp9Dec->hMFCVp9Handle.indexTimestamp] = pSrcInputData->timeStamp;
        pExynosComponent->nFlags[pVp9Dec->hMFCVp9Handle.indexTimestamp] = pSrcInputData->nFlags;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input / buffer header(%p), dataLen(%d), nFlags: 0x%x, timestamp %lld us (%.2f secs), tag: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        pSrcInputData->bufferHeader, oneFrameSize, pSrcInputData->nFlags,
                                                        pSrcInputData->timeStamp, (double)(pSrcInputData->timeStamp / 1E6),
                                                        pVp9Dec->hMFCVp9Handle.indexTimestamp);

        pDecOps->Set_FrameTag(hMFCHandle, pVp9Dec->hMFCVp9Handle.indexTimestamp);

        if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportInfo.dec.bHDRDynamicInfoSupport == VIDEO_TRUE) {
            nConfigCnt = Exynos_OSAL_GetElemNum(&pExynosComponent->HDR10plusConfigQ);
            if (nConfigCnt > 0) {
                pHDR10plusConfig = Exynos_OSAL_Dequeue(&pExynosComponent->HDR10plusConfigQ);

                if (pHDR10plusConfig == NULL) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to dequeue HDR10+ config", pExynosComponent, __FUNCTION__);
                    ret = OMX_ErrorUndefined;
                    goto EXIT;
                }

                for (i = 0; i < MAX_BUFFER_REF; i++) {
                    if (pExynosComponent->HDR10plusList[i].bOccupied == OMX_FALSE) {
                        int nParamSize = *(OMX_U32 *)pHDR10plusConfig;

                        pExynosComponent->HDR10plusList[i].pHDR10PlusInfo = Exynos_OSAL_Malloc(nParamSize);
                        if (pExynosComponent->HDR10plusList[i].pHDR10PlusInfo == NULL) {
                            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
                            ret = OMX_ErrorInsufficientResources;
                            goto EXIT;
                        }

                        pExynosComponent->HDR10plusList[i].bOccupied = OMX_TRUE;
                        pExynosComponent->HDR10plusList[i].nTag      = pVp9Dec->hMFCVp9Handle.indexTimestamp;
                        Exynos_OSAL_Memcpy(pExynosComponent->HDR10plusList[i].pHDR10PlusInfo, pHDR10plusConfig, nParamSize);
                        Exynos_OSAL_Free(pHDR10plusConfig);

                        break;
                    }
                }
            }
        }

        pVp9Dec->hMFCVp9Handle.indexTimestamp++;
        pVp9Dec->hMFCVp9Handle.indexTimestamp %= MAX_TIMESTAMP;

        if (pVideoDec->bQosChanged == OMX_TRUE) {
            if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportInfo.dec.bOperatingRateSupport == VIDEO_TRUE) {
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
        VP9CodecStart(pOMXComponent, INPUT_PORT_INDEX);
        if (pVp9Dec->bSourceStart == OMX_FALSE) {
            pVp9Dec->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pVp9Dec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        if ((pVp9Dec->bDestinationStart == OMX_FALSE) &&
            (pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst == OMX_TRUE)) {
            pVp9Dec->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationOutStartEvent);
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

OMX_ERRORTYPE Exynos_VP9Dec_SrcOut(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pExynosInputPort   = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoDecBufferOps *pInbufOps      = pVp9Dec->hMFCVp9Handle.pInbufOps;
    ExynosVideoBuffer       *pVideoBuffer   = NULL;
    ExynosVideoBuffer        videoBuffer;

    FunctionIn();

    if (pVp9Dec->hMFCVp9Handle.bConfiguredMFCSrc == OMX_TRUE) {
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

OMX_ERRORTYPE Exynos_VP9Dec_DstIn(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecBufferOps *pOutbufOps  = pVp9Dec->hMFCVp9Handle.pOutbufOps;
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
        nAllocLen[i] = pVp9Dec->hMFCVp9Handle.codecOutbufConf.nAlignPlaneSize[i];

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

    VP9CodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP9Dec_DstOut(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pVp9Dec->hMFCVp9Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    DECODE_CODEC_EXTRA_BUFFERINFO   *pBufferInfo        = NULL;

    ExynosVideoDecOps           *pDecOps        = pVp9Dec->hMFCVp9Handle.pDecOps;
    ExynosVideoDecBufferOps     *pOutbufOps     = pVp9Dec->hMFCVp9Handle.pOutbufOps;
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

    int i;

    FunctionIn();

    if (pVp9Dec->bDestinationStart == OMX_FALSE) {
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
            (displayStatus == VIDEO_FRAME_STATUS_LAST_FRAME) ||
            (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_INTER_RESOL_CHANGE) ||
            (CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
            ret = OMX_ErrorNone;
            break;
        }
    }

   if ((pVideoDec->bThumbnailMode == OMX_FALSE) &&
       (displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL)) {
        if (pVideoDec->bReconfigDPB != OMX_TRUE) {
            pOutputPort->exceptionFlag = NEED_PORT_FLUSH;
            pVideoDec->bReconfigDPB = OMX_TRUE;
            Vp9CodecUpdateResolution(pOMXComponent);
            pVideoDec->csc_set_format = OMX_FALSE;
        }
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    /* inter-frame display resolution change */
    if (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_INTER_RESOL_CHANGE) {
        if (pExynosComponent->bUseImgCrop == OMX_TRUE) /* reset ImgCropInfo because resolution change makes ImgCropInfo invalid. */
            Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);

        Vp9CodecUpdateDisplayResolution(pOMXComponent);
    }

    pVp9Dec->hMFCVp9Handle.outputIndexTimestamp++;
    pVp9Dec->hMFCVp9Handle.outputIndexTimestamp %= MAX_TIMESTAMP;

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
    bufferGeometry  = &pVp9Dec->hMFCVp9Handle.codecOutbufConf;
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
        /* color aspects : the bitstream has only this */
        if (pVideoBuffer->frameType & VIDEO_FRAME_WITH_HDR_INFO)
            Vp9CodecUpdateHdrInfo(pOMXComponent);

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
                        pVp9Dec->hMFCVp9Handle.nActualFormat = nPixelFormat;
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
        VP9CodecUpdateExtraInfo(pOMXComponent, pVideoBuffer->planes[2].addr);
    }

    indexTimestamp = pDecOps->Get_FrameTag(hMFCHandle);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] out indexTimestamp: %d", pExynosComponent, __FUNCTION__, indexTimestamp);
    if ((indexTimestamp < 0) || (indexTimestamp >= MAX_TIMESTAMP)) {
        if ((pExynosComponent->checkTimeStamp.needSetStartTimeStamp != OMX_TRUE) &&
            (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp != OMX_TRUE)) {
            if (indexTimestamp == INDEX_AFTER_EOS) {
                pDstOutputData->timeStamp = 0x00;
                pDstOutputData->nFlags = 0x00;
            } else {
                pDstOutputData->timeStamp = pExynosComponent->timeStamp[pVp9Dec->hMFCVp9Handle.outputIndexTimestamp];
                pDstOutputData->nFlags = pExynosComponent->nFlags[pVp9Dec->hMFCVp9Handle.outputIndexTimestamp];
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] missing out indexTimestamp: %d", pExynosComponent, __FUNCTION__, indexTimestamp);
            }
        } else {
            pDstOutputData->timeStamp = 0x00;
            pDstOutputData->nFlags = 0x00;
        }
    } else {
        /* Find HDR10+ info for framework */
        if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportInfo.dec.bHDRDynamicInfoSupport == VIDEO_TRUE) {
            for (i = 0; i < MAX_BUFFER_REF; i++) {
                if ((pExynosComponent->HDR10plusList[i].bOccupied == OMX_TRUE) &&
                    (pExynosComponent->HDR10plusList[i].nTag == indexTimestamp)) {
                    if (Exynos_OSAL_Queue(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].HdrDynamicInfoQ, (void *)pExynosComponent->HDR10plusList[i].pHDR10PlusInfo) != 0) {
                        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Failed to Queue HDR10+ info", pExynosComponent, __FUNCTION__);
                        Exynos_OSAL_Free(pExynosComponent->HDR10plusList[i].pHDR10PlusInfo);
                        pExynosComponent->HDR10plusList[i].pHDR10PlusInfo = NULL;
                        pExynosComponent->HDR10plusList[i].bOccupied      = OMX_FALSE;

                        break;
                    }

                    /* This code is for supporting HDR10Plus vendor path.
                     * This will be removed if HWC supports official interface for HDR10Plus.
                     */
                    if ((pOutputPort->bufferProcessType == BUFFER_SHARE) &&
                        (pVideoBuffer->planes[2].addr != NULL)) {
                        VP9CodecUpdateHDR10PlusInfo(pOMXComponent, pVideoBuffer->planes[2].addr, (void *)pExynosComponent->HDR10plusList[i].pHDR10PlusInfo);
                    }

                    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] send event(OMX_EventConfigUpdate)",
                                                    pExynosComponent, __FUNCTION__);
                    /** Send ConfigUpdate Event call back **/
                    (*(pExynosComponent->pCallbacks->EventHandler))
                        (pOMXComponent,
                         pExynosComponent->callbackData,
                         OMX_EventConfigUpdate, /* The command was completed */
                         OMX_DirOutput,         /* This is the port index */
                         OMX_IndexConfigVideoHdr10PlusInfo,
                         NULL);

                    break;
                }
            }
        }

        /* For timestamp correction. if mfc support frametype detect */
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] disp_pic_frame_type: %d", pExynosComponent, __FUNCTION__, pVideoBuffer->frameType);

        /* NEED TIMESTAMP REORDER */
        if (pVideoDec->bDTSMode == OMX_TRUE) {
            if ((pVideoBuffer->frameType & VIDEO_FRAME_I) ||
                ((pVideoBuffer->frameType & VIDEO_FRAME_OTHERS) &&
                    ((pExynosComponent->nFlags[indexTimestamp] & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) ||
                (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp == OMX_TRUE))
               pVp9Dec->hMFCVp9Handle.outputIndexTimestamp = indexTimestamp;
            else
               indexTimestamp = pVp9Dec->hMFCVp9Handle.outputIndexTimestamp;
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

    if (pVideoBuffer->frameType & VIDEO_FRAME_WITH_BLACK_BAR) {
        if (Vp9CodecUpdateBlackBarCrop(pOMXComponent) != OMX_ErrorNone)
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

OMX_ERRORTYPE Exynos_VP9Dec_srcInputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pExynosInputPort   = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

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

    ret = Exynos_VP9Dec_SrcIn(pOMXComponent, pSrcInputData);
    if ((ret != OMX_ErrorNone) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorInputDataDecodeYet) &&
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

OMX_ERRORTYPE Exynos_VP9Dec_srcOutputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
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
    if ((pVp9Dec->bSourceStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pExynosInputPort))) {
        Exynos_OSAL_SignalWait(pVp9Dec->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoDec->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get SourceStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pVp9Dec->hSourceStartEvent);
    }

    ret = Exynos_VP9Dec_SrcOut(pOMXComponent, pSrcOutputData);
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

OMX_ERRORTYPE Exynos_VP9Dec_dstInputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

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

    if ((pVp9Dec->bDestinationStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
        Exynos_OSAL_SignalWait(pVp9Dec->hDestinationInStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoDec->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationInStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pVp9Dec->hDestinationInStartEvent);
    }

    if (pExynosOutputPort->bufferProcessType == BUFFER_SHARE) {
        if (Exynos_OSAL_GetElemNum(&pVp9Dec->bypassBufferInfoQ) > 0) {
            BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Dequeue(&pVp9Dec->bypassBufferInfoQ);
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
            ret = VP9CodecDstSetup(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Vp9CodecDstSetup(0x%x)", pExynosComponent, __FUNCTION__, ret);
                goto EXIT;
            }

            pVideoDec->bReconfigDPB = OMX_FALSE;
            Exynos_OSAL_SignalSet(pVp9Dec->hDestinationOutStartEvent);
        }
    }

    if (pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_VP9Dec_DstIn(pOMXComponent, pDstInputData);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                    pExynosComponent, __FUNCTION__);
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
        }
    } else {
        if ((pVp9Dec->bDestinationStart == OMX_FALSE) &&
            (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            ret = (OMX_ERRORTYPE)OMX_ErrorNoneReuseBuffer;
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP9Dec_dstOutputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

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

    if (((pVp9Dec->bDestinationStart == OMX_FALSE) ||
         (pVp9Dec->hMFCVp9Handle.bConfiguredMFCDst == OMX_FALSE)) &&
        (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
        Exynos_OSAL_SignalWait(pVp9Dec->hDestinationOutStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoDec->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationOutStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pVp9Dec->hDestinationOutStartEvent);
    }

    if (pExynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        if (Exynos_OSAL_GetElemNum(&pVp9Dec->bypassBufferInfoQ) > 0) {
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

            pBufferInfo = Exynos_OSAL_Dequeue(&pVp9Dec->bypassBufferInfoQ);
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

    ret = Exynos_VP9Dec_DstOut(pOMXComponent, pDstOutputData);
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
    EXYNOS_VP9DEC_HANDLE            *pVp9Dec            = NULL;
    OMX_BOOL                         bSecureMode        = OMX_FALSE;
    int i = 0;

    Exynos_OSAL_Get_Log_Property(); // For debuging
    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_VP9_DEC, componentName) == 0) {
        bSecureMode = OMX_FALSE;
    } else if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_VP9_DRM_DEC, componentName) == 0) {
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

    pVp9Dec = Exynos_OSAL_Malloc(sizeof(EXYNOS_VP9DEC_HANDLE));
    if (pVp9Dec == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x)", pExynosComponent, __FUNCTION__, ret);
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pVp9Dec, 0, sizeof(EXYNOS_VP9DEC_HANDLE));
    pVideoDec               = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoDec->hCodecHandle = (OMX_HANDLETYPE)pVp9Dec;
    Exynos_OSAL_Strcpy(pExynosComponent->componentName, componentName);

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
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
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

    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP9;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/x-vnd.on2.vp9");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_SINGLE;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
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
        INIT_SET_SIZE_VERSION(&pVp9Dec->VP9Component[i], OMX_VIDEO_PARAM_VP9TYPE);
        pVp9Dec->VP9Component[i].nPortIndex = i;
        pVp9Dec->VP9Component[i].eProfile   = OMX_VIDEO_VP9Profile0;
        pVp9Dec->VP9Component[i].eLevel     = OMX_VIDEO_VP9Level41;
    }

    pOMXComponent->GetParameter      = &Exynos_VP9Dec_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_VP9Dec_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_VP9Dec_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_VP9Dec_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_VP9Dec_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_VP9Dec_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_VP9Dec_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_VP9Dec_Terminate;

    pVideoDec->exynos_codec_srcInputProcess  = &Exynos_VP9Dec_srcInputBufferProcess;
    pVideoDec->exynos_codec_srcOutputProcess = &Exynos_VP9Dec_srcOutputBufferProcess;
    pVideoDec->exynos_codec_dstInputProcess  = &Exynos_VP9Dec_dstInputBufferProcess;
    pVideoDec->exynos_codec_dstOutputProcess = &Exynos_VP9Dec_dstOutputBufferProcess;

    pVideoDec->exynos_codec_start            = &VP9CodecStart;
    pVideoDec->exynos_codec_stop             = &VP9CodecStop;
    pVideoDec->exynos_codec_bufferProcessRun = &VP9CodecOutputBufferProcessRun;
    pVideoDec->exynos_codec_enqueueAllBuffer = &VP9CodecEnQueueAllBuffer;

    pVideoDec->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;
    pVideoDec->exynos_codec_reconfigAllBuffers        = &Vp9CodecReconfigAllBuffers;

    pVideoDec->exynos_codec_checkFormatSupport      = &CheckFormatHWSupport;
    pVideoDec->exynos_codec_checkResolutionChange   = &Vp9CodecCheckResolution;

    pVideoDec->exynos_codec_updateExtraInfo = &VP9CodecUpdateExtraInfo;

    pVideoDec->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoDec->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Open", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pVp9Dec);
        pVp9Dec = pVideoDec->hCodecHandle = NULL;
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pVp9Dec->hMFCVp9Handle.videoInstInfo.eCodecType = VIDEO_CODING_VP9;
    if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)
        pVp9Dec->hMFCVp9Handle.videoInstInfo.eSecurityType = VIDEO_SECURE;
    else
        pVp9Dec->hMFCVp9Handle.videoInstInfo.eSecurityType = VIDEO_NORMAL;

    if (Exynos_Video_GetInstInfo(&(pVp9Dec->hMFCVp9Handle.videoInstInfo), VIDEO_TRUE /* dec */) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to GetInstInfo", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pVp9Dec);
        pVp9Dec = pVideoDec->hCodecHandle = NULL;
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportInfo.dec.bDrvDPBManageSupport == VIDEO_TRUE)
        pVideoDec->bDrvDPBManaging = OMX_TRUE;
    else
        pVideoDec->hRefHandle = Exynos_OSAL_RefCount_Create();

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] GetInstInfo for dec DrvDPBManaging(%d)", pExynosComponent, __FUNCTION__,
            (pVp9Dec->hMFCVp9Handle.videoInstInfo.supportInfo.dec.bDrvDPBManageSupport));

    Exynos_Output_SetSupportFormat(pExynosComponent);
    SetProfileLevel(pExynosComponent);

#ifdef USE_ANDROID
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-dec-compressed-color-format", (OMX_INDEXTYPE)OMX_IndexParamVideoCompressedColorFormat);
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
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_VP9DEC_HANDLE        *pVp9Dec            = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

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

    pVp9Dec = (EXYNOS_VP9DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pVp9Dec != NULL) {
        Exynos_OSAL_Free(pVp9Dec);
        pVp9Dec = pVideoDec->hCodecHandle = NULL;
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
