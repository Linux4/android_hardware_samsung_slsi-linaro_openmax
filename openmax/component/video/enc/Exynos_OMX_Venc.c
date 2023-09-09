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
 * @file        Exynos_OMX_Venc.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Yunji Kim (yunji.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Exynos_OMX_Macros.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_VencControl.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_ETC.h"
#include "ExynosVideoApi.h"
#include "csc.h"

#include "Exynos_OSAL_Platform.h"

#ifdef USE_ANDROID
#include <system/graphics.h>
#include "exynos_format.h"
#endif

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VIDEO_ENC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


void Exynos_UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *pInputPort        = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *pOutputPort       = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    OMX_U32 width  = pInputPort->portDefinition.format.video.nFrameWidth;
    OMX_U32 height = pInputPort->portDefinition.format.video.nFrameHeight;

    FunctionIn();

    if (width && height) {
        switch((int)pInputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
        case OMX_SEC_COLOR_FormatNV12Tiled:
        case OMX_SEC_COLOR_FormatYVU420Planar:
        case OMX_SEC_COLOR_FormatNV21Linear:
            pInputPort->portDefinition.nBufferSize = (ALIGN(width, 16) * ALIGN(height, 16) * 3) / 2;
            break;
        case OMX_COLOR_FormatYUV420Planar16:  /* 10bit format */
            pInputPort->portDefinition.nBufferSize = (ALIGN(width * 2, 16) * ALIGN(height, 16) * 3) / 2;
            break;
        case OMX_COLOR_Format32bitARGB8888:
        case OMX_COLOR_Format32BitRGBA8888:
        case OMX_COLOR_Format32bitBGRA8888:
            pInputPort->portDefinition.nBufferSize = ALIGN(width, 16) * ALIGN(height, 16) * 4;

            /* the video stream is in YUV domain */
            pOutputPort->portDefinition.format.video.nFrameWidth  = width;
            pOutputPort->portDefinition.format.video.nFrameHeight = height;
            pOutputPort->portDefinition.nBufferSize = (ALIGN(width, 16) * ALIGN(height, 16) * 3) / 2;
            break;
#ifdef USE_ANDROID
        case OMX_COLOR_FormatAndroidOpaque:
            pInputPort->portDefinition.nBufferSize = (ALIGN(width, 16) * ALIGN(height, 16) * 3) / 2;
            break;
#endif
        default:
            pInputPort->portDefinition.nBufferSize = ALIGN(width, 16) * ALIGN(height, 16) * 2;
            break;
        }
    }

    if ((pOutputPort->portDefinition.format.video.nFrameWidth !=
            pInputPort->portDefinition.format.video.nFrameWidth) ||
        (pOutputPort->portDefinition.format.video.nFrameHeight !=
            pInputPort->portDefinition.format.video.nFrameHeight)) {

        pOutputPort->portDefinition.format.video.nFrameWidth =
            pInputPort->portDefinition.format.video.nFrameWidth;
        pOutputPort->portDefinition.format.video.nFrameHeight =
            pInputPort->portDefinition.format.video.nFrameHeight;
        pOutputPort->portDefinition.format.video.nStride =
            pInputPort->portDefinition.format.video.nStride;
        pOutputPort->portDefinition.format.video.nSliceHeight =
            pInputPort->portDefinition.format.video.nSliceHeight;

        pOutputPort->portDefinition.nBufferSize = ALIGN(pInputPort->portDefinition.nBufferSize, 512);
    }

#ifdef USE_CUSTOM_COMPONENT_SUPPORT
    pOutputPort->portDefinition.nBufferSize = Exynos_OSAL_GetOutBufferSize(width, height, pInputPort->portDefinition.nBufferSize);
#endif

#ifdef USE_SMALL_SECURE_MEMORY
    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) {
        if (pOutputPort->portDefinition.nBufferSize > CUSTOM_LIMITED_DRM_OUTPUT_BUFFER_SIZE) {
            pOutputPort->portDefinition.nBufferSize = CUSTOM_LIMITED_DRM_OUTPUT_BUFFER_SIZE;
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] use limited buffer size(%d) for output",
                                                pExynosComponent, __FUNCTION__, pOutputPort->portDefinition.nBufferSize);
        }
    }
#endif

    if (pOutputPort->portDefinition.nBufferSize < MIN_CPB_SIZE) {
        pOutputPort->portDefinition.nBufferSize = MIN_CPB_SIZE;
    }

    FunctionOut();

    return;
}

void Exynos_Input_SetSupportFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_COLOR_FORMATTYPE             ret        = OMX_COLOR_FormatUnused;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc  = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    if ((pVideoEnc == NULL) || (pInputPort == NULL))
        return ;

    if (pInputPort->supportFormat != NULL) {
        OMX_BOOL ret = OMX_FALSE;
        int nLastIndex = INPUT_PORT_SUPPORTFORMAT_DEFAULT_NUM;
        int i;

        /* default supported formats */
        pInputPort->supportFormat[0] = OMX_COLOR_FormatYUV420Planar;
        pInputPort->supportFormat[1] = OMX_COLOR_FormatYUV420SemiPlanar;
        pInputPort->supportFormat[2] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear;
        pInputPort->supportFormat[3] = OMX_COLOR_Format32bitARGB8888;
        pInputPort->supportFormat[4] = (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888;
#ifdef USE_ANDROID
        pInputPort->supportFormat[nLastIndex++] = OMX_COLOR_FormatAndroidOpaque;
#endif

        /* add extra formats, if It is supported by H/W. (CSC doesn't exist) */
        /* OMX_SEC_COLOR_FormatNV12Tiled */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled;

        /* OMX_SEC_COLOR_FormatYVU420Planar */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar;

        /* OMX_COLOR_Format32bitBGRA8888 */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent, OMX_COLOR_Format32bitBGRA8888);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = OMX_COLOR_Format32bitBGRA8888;

        /* OMX_COLOR_FormatYUV420Planar16 */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent, (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16;

        for (i = 0; i < nLastIndex; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] Supported Format[%d] : 0x%x",
                                                pExynosComponent, __FUNCTION__, i, pInputPort->supportFormat[i]);
        }

        pInputPort->supportFormat[nLastIndex] = OMX_COLOR_FormatUnused;
    }

    return ;
}

OMX_COLOR_FORMATTYPE Exynos_Input_GetActualColorFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_COLOR_FORMATTYPE             ret                = OMX_COLOR_FormatUnused;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE             eColorFormat       = pInputPort->portDefinition.format.video.eColorFormat;

#ifdef USE_ANDROID
    if (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque)
        eColorFormat = pVideoEnc->surfaceFormat;
#endif

    if (pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent, eColorFormat) == OMX_TRUE) {
        ret = eColorFormat;
        goto EXIT;
    }

    switch ((int)eColorFormat) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_COLOR_FormatYUV420Planar:       /* converted to NV12 using CSC */
    case OMX_COLOR_Format32bitARGB8888:      /* converted to NV12 using CSC */
    case OMX_COLOR_Format32BitRGBA8888:      /* converted to NV12 using CSC */
        ret = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
    case OMX_SEC_COLOR_FormatNV21Linear:
    case OMX_SEC_COLOR_FormatNV12Tiled:
        ret = eColorFormat;
        break;
    case OMX_COLOR_FormatYUV420Planar16:     /* TBD: convert 10bit to 8bit */
    default:
        ret = OMX_COLOR_FormatUnused;
        break;
    }

EXIT:
    return ret;
}

void Exynos_Free_CodecBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    CODEC_ENC_BUFFER               **ppCodecBuffer      = NULL;

    int nBufferCnt = 0, nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer = &(pVideoEnc->pMFCEncInputBuffer[0]);
        nBufferCnt = MFC_INPUT_BUFFER_NUM_MAX;
    } else {
        ppCodecBuffer = &(pVideoEnc->pMFCEncOutputBuffer[0]);
        nBufferCnt = MFC_OUTPUT_BUFFER_NUM_MAX;
    }

    nPlaneCnt = Exynos_GetPlaneFromPort(&pExynosComponent->pExynosPort[nPortIndex]);
    for (i = 0; i < nBufferCnt; i++) {
        if (ppCodecBuffer[i] != NULL) {
            for (j = 0; j < nPlaneCnt; j++) {
                if (ppCodecBuffer[i]->pVirAddr[j] != NULL) {
                    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] %s codec buffer[%d][%d] : %d",
                                                pExynosComponent, __FUNCTION__,
                                                (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                i, j, ppCodecBuffer[i]->fd[j]);
                    Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, ppCodecBuffer[i]->pVirAddr[j]);
                }
            }

            Exynos_OSAL_Free(ppCodecBuffer[i]);
            ppCodecBuffer[i] = NULL;
        }
    }

    FunctionOut();
}

OMX_ERRORTYPE Exynos_Allocate_CodecBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex,
    int                  nBufferCnt,
    unsigned int         nAllocLen[MAX_BUFFER_PLANE])
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    MEMORY_TYPE                      eMemoryType        = CACHED_MEMORY;
    CODEC_ENC_BUFFER               **ppCodecBuffer      = NULL;

    int nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer = &(pVideoEnc->pMFCEncInputBuffer[0]);
    } else {
        ppCodecBuffer = &(pVideoEnc->pMFCEncOutputBuffer[0]);
#ifdef USE_CSC_HW
        eMemoryType = NORMAL_MEMORY;
#endif
    }

    nPlaneCnt = Exynos_GetPlaneFromPort(&pExynosComponent->pExynosPort[nPortIndex]);

    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        eMemoryType = SECURE_MEMORY;

    for (i = 0; i < nBufferCnt; i++) {
        ppCodecBuffer[i] = (CODEC_ENC_BUFFER *)Exynos_OSAL_Malloc(sizeof(CODEC_ENC_BUFFER));
        if (ppCodecBuffer[i] == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(ppCodecBuffer[i], 0, sizeof(CODEC_ENC_BUFFER));

        for (j = 0; j < nPlaneCnt; j++) {
            ppCodecBuffer[i]->pVirAddr[j] =
                (void *)Exynos_OSAL_SharedMemory_Alloc(pVideoEnc->hSharedMemory, nAllocLen[j], eMemoryType);
            if (ppCodecBuffer[i]->pVirAddr[j] == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Alloc for %s codec buffer",
                                            pExynosComponent, __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            ppCodecBuffer[i]->fd[j] =
                Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, ppCodecBuffer[i]->pVirAddr[j]);
            ppCodecBuffer[i]->bufferSize[j] = nAllocLen[j];
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s codec buffer[%d][%d] : %d / size(%d)",
                                        pExynosComponent, __FUNCTION__,
                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                        i, j, ppCodecBuffer[i]->fd[j], ppCodecBuffer[i]->bufferSize[j]);
        }

        ppCodecBuffer[i]->dataSize = 0;
    }

    return OMX_ErrorNone;

EXIT:
    Exynos_Free_CodecBuffers(pOMXComponent, nPortIndex);

    FunctionOut();

    return ret;
}

OMX_BOOL Exynos_Check_BufferProcess_State(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nPortIndex)
{
    OMX_BOOL ret = OMX_FALSE;

    if ((pExynosComponent == NULL) ||
        (pExynosComponent->pExynosPort == NULL))
        return OMX_FALSE;

    if ((pExynosComponent->currentState == OMX_StateExecuting) &&
        ((pExynosComponent->pExynosPort[nPortIndex].portState == EXYNOS_OMX_PortStateIdle) ||
         (pExynosComponent->pExynosPort[nPortIndex].portState == EXYNOS_OMX_PortStateDisabling)) &&
        (pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
        (pExynosComponent->transientState != EXYNOS_OMX_TransStateIdleToExecuting)) {
        ret = OMX_TRUE;
    } else {
        ret = OMX_FALSE;
    }

    return ret;
}

OMX_ERRORTYPE Exynos_ResetAllPortConfig(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT           *pInputPort        = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort       = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    /* Input port */
    pInputPort->portDefinition.format.video.nFrameWidth             = DEFAULT_FRAME_WIDTH;
    pInputPort->portDefinition.format.video.nFrameHeight            = DEFAULT_FRAME_HEIGHT;
    pInputPort->portDefinition.format.video.nStride                 = 0; /*DEFAULT_FRAME_WIDTH;*/
    pInputPort->portDefinition.format.video.nSliceHeight            = 0;
    pInputPort->portDefinition.format.video.pNativeRender           = 0;
    pInputPort->portDefinition.format.video.bFlagErrorConcealment   = OMX_FALSE;
    pInputPort->portDefinition.format.video.eColorFormat            = OMX_COLOR_FormatYUV420SemiPlanar;

    pInputPort->portDefinition.nBufferSize  = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pInputPort->portDefinition.bEnabled     = OMX_TRUE;

    pInputPort->bufferProcessType   = BUFFER_COPY;
    pInputPort->portWayType         = WAY2_PORT;
    Exynos_SetPlaneToPort(pInputPort, MFC_DEFAULT_INPUT_BUFFER_PLANE);
    Exynos_ResetImgCropInfo(pOMXComponent, INPUT_PORT_INDEX);

    /* Output port */
    pOutputPort->portDefinition.format.video.nFrameWidth            = DEFAULT_FRAME_WIDTH;
    pOutputPort->portDefinition.format.video.nFrameHeight           = DEFAULT_FRAME_HEIGHT;
    pOutputPort->portDefinition.format.video.nStride                = 0; /*DEFAULT_FRAME_WIDTH;*/
    pOutputPort->portDefinition.format.video.nSliceHeight           = 0;
    pOutputPort->portDefinition.format.video.pNativeRender          = 0;
    pOutputPort->portDefinition.format.video.bFlagErrorConcealment  = OMX_FALSE;
    pOutputPort->portDefinition.format.video.eColorFormat           = OMX_COLOR_FormatUnused;

    pOutputPort->portDefinition.nBufferCountActual  = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pOutputPort->portDefinition.nBufferCountMin     = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pOutputPort->portDefinition.nBufferSize  = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pOutputPort->portDefinition.bEnabled     = OMX_TRUE;

    pOutputPort->bufferProcessType  = BUFFER_SHARE;
    pOutputPort->portWayType        = WAY2_PORT;
    pOutputPort->latestTimeStamp    = DEFAULT_TIMESTAMP_VAL;
    Exynos_SetPlaneToPort(pOutputPort, Exynos_OSAL_GetPlaneCount(OMX_COLOR_FormatYUV420Planar, pOutputPort->ePlaneType));
    Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);

    /* remove a configuration command that is in piled up */
    while (Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ) > 0) {
        OMX_PTR pDynamicConfigCMD = NULL;
        pDynamicConfigCMD = (OMX_PTR)Exynos_OSAL_Dequeue(&pExynosComponent->dynamicConfigQ);
        Exynos_OSAL_Free(pDynamicConfigCMD);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferToData(
    CODEC_ENC_BUFFER    *pCodecBuffer,
    EXYNOS_OMX_DATA     *pData,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int i;

    if (nPortIndex > OUTPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    pData->allocSize     = 0;
    pData->usedDataLen   = 0;
    pData->nFlags        = 0;
    pData->timeStamp     = 0;
    pData->pPrivate      = pCodecBuffer;
    pData->bufferHeader  = NULL;

    for (i = 0; i < MAX_BUFFER_PLANE; i++) {
        pData->buffer.addr[i]   = pCodecBuffer->pVirAddr[i];
        pData->buffer.fd[i]     = pCodecBuffer->fd[i];

        pData->allocSize += pCodecBuffer->bufferSize[i];
    }

    pData->dataLen       = 0;
    pData->remainDataLen = 0;

EXIT:
    return ret;
}

void Exynos_Wait_ProcessPause(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nPortIndex)
{
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosOMXPort  = NULL;

    FunctionIn();

    exynosOMXPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (((pExynosComponent->currentState == OMX_StatePause) ||
        (pExynosComponent->currentState == OMX_StateIdle) ||
        (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle) ||
        (pExynosComponent->transientState == EXYNOS_OMX_TransStateExecutingToIdle)) &&
        (pExynosComponent->transientState != EXYNOS_OMX_TransStateIdleToLoaded) &&
        (!CHECK_PORT_BEING_FLUSHED(exynosOMXPort))) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port -> pause : state(0x%x), transient state(0x%x)",
                                        pExynosComponent, __FUNCTION__,
                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                        pExynosComponent->currentState, pExynosComponent->transientState);
        Exynos_OSAL_SignalWait(pExynosComponent->pExynosPort[nPortIndex].pauseEvent, DEF_MAX_WAIT_TIME);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port -> resume : state(0x%x), transient state(0x%x)",
                                        pExynosComponent, __FUNCTION__,
                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                        pExynosComponent->currentState, pExynosComponent->transientState);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_SignalReset(pExynosComponent->pExynosPort[nPortIndex].pauseEvent);
    }

EXIT:
    FunctionOut();

    return;
}

OMX_BOOL Exynos_CSC_InputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_BOOL                       ret                = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER         *pInputUseBuffer    = &pInputPort->way.port2WayDataBuffer.inputDataBuffer;
    CODEC_ENC_BUFFER              *pCodecInputBuffer  = (CODEC_ENC_BUFFER *)pSrcInputData->pPrivate;
    OMX_COLOR_FORMATTYPE           eColorFormat       = pInputPort->portDefinition.format.video.eColorFormat;
    OMX_COLOR_FORMATTYPE           eSrcColorFormat    = OMX_COLOR_FormatUnused;

    void *pInputBuf                 = (void *)pInputUseBuffer->bufferHeader->pBuffer;
    void *pSrcBuf[MAX_BUFFER_PLANE] = { NULL, };
    void *pDstBuf[MAX_BUFFER_PLANE] = { NULL, };

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {0, 0, 0};

    CSC_MEMTYPE     csc_memType       = CSC_MEMORY_USERPTR;
    CSC_METHOD      csc_method        = CSC_METHOD_SW;
    CSC_ERRORCODE   csc_ret           = CSC_ErrorNone;
    unsigned int    csc_src_cacheable = 1;
    unsigned int    csc_dst_cacheable = 1;

    EXYNOS_OMX_IMG_INFO srcImgInfo, dstImgInfo;
    OMX_S32 nTargetWidth, nTargetHeight;
    int nDstAlignment = 0;
    int i, nPlaneCnt;
    OMX_BOOL bVerticalFlip   = OMX_FALSE;
    OMX_BOOL bHorizontalFlip = OMX_FALSE;

    FunctionIn();


    eSrcColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);

    /* choose csc method */
    csc_get_method(pVideoEnc->csc_handle, &csc_method);

    if (csc_method == CSC_METHOD_SW) {
        /* 1. color format is supported by MFC */
        if (eColorFormat == eSrcColorFormat) {
            csc_memType = CSC_MEMORY_MFC;  /* add padding data, if width is not satisfied with h/w alignment */
            csc_method  = CSC_METHOD_SW;
        }

        /* 2. blur filtering/rotation/cropping/positioning/mirror are supported by only H/W */
        if ((pExynosComponent->bUseImgCrop == OMX_TRUE) ||
            (pVideoEnc->bUseBlurFilter == OMX_TRUE) ||
            (pVideoEnc->eRotationType != ROTATE_0) ||
            (pVideoEnc->eMirrorType != OMX_MirrorNone)) {
            csc_ret = csc_set_method(pVideoEnc->csc_handle, CSC_METHOD_HW);
            if (csc_ret != CSC_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] csc_set_method(CSC_METHOD_HW) for blur/rotation is failed", pExynosComponent, __FUNCTION__);
                ret = OMX_FALSE;
                goto EXIT;
            }

            csc_memType = CSC_MEMORY_DMABUF;
            csc_method  = CSC_METHOD_HW;
            csc_src_cacheable = 0;
            csc_dst_cacheable = 0;
        }

        /* 3. forcefully want to use H/W */
        if (pInputPort->eMetaDataType != METADATA_TYPE_DISABLED) {
#ifdef USE_HW_CSC_GRALLOC_SOURCE
            if (pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) {
#ifdef USE_FIMC_CSC
                if (pVideoEnc->surfaceFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888)  /* FIMC doesn't support RGBA format */
#endif  // USE_FIMC_CSC
                {
                    csc_ret = csc_set_method(pVideoEnc->csc_handle, CSC_METHOD_HW);
                    if (csc_ret != CSC_ErrorNone) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] csc_set_method(CSC_METHOD_HW) for gralloc source is failed", pExynosComponent, __FUNCTION__);
                        ret = OMX_FALSE;
                        goto EXIT;
                    }

                    csc_memType = CSC_MEMORY_DMABUF;
                    csc_method  = CSC_METHOD_HW;
                    csc_src_cacheable = 0;
                    csc_dst_cacheable = 0;
                }
            }
#endif  // USE_HW_CSC_GRALLOC_SOURCE
        }
    } else {
        csc_memType = CSC_MEMORY_DMABUF;
        csc_src_cacheable = 0;
        csc_dst_cacheable = 0;
    }

    /******************/
    /* get image info */
    /******************/
    /* 1. SRC : OMX INPUT */
    srcImgInfo.nStride      = pInputPort->portDefinition.format.video.nFrameWidth;
    srcImgInfo.nSliceHeight = pInputPort->portDefinition.format.video.nFrameHeight;
    srcImgInfo.nImageWidth  = pInputPort->portDefinition.format.video.nFrameWidth;
    srcImgInfo.nImageHeight = pInputPort->portDefinition.format.video.nFrameHeight;
    if (pInputPort->bUseImgCrop[IMG_CROP_INPUT_PORT] == OMX_FALSE) {
        srcImgInfo.nLeft   = 0;
        srcImgInfo.nTop    = 0;
        srcImgInfo.nWidth  = pInputPort->portDefinition.format.video.nFrameWidth;
        srcImgInfo.nHeight = pInputPort->portDefinition.format.video.nFrameHeight;
    } else {
        srcImgInfo.nLeft   = pInputPort->cropRectangle[IMG_CROP_INPUT_PORT].nLeft;
        srcImgInfo.nTop    = pInputPort->cropRectangle[IMG_CROP_INPUT_PORT].nTop;
        srcImgInfo.nWidth  = pInputPort->cropRectangle[IMG_CROP_INPUT_PORT].nWidth;
        srcImgInfo.nHeight = pInputPort->cropRectangle[IMG_CROP_INPUT_PORT].nHeight;
    }

    /* 2. DST : MFC INPUT */
    if (pOutputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_TRUE) {
        /* use changing output resolution */
        nTargetWidth    = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
        nTargetHeight   = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
    } else {
        nTargetWidth    = pOutputPort->portDefinition.format.video.nFrameWidth;
        nTargetHeight   = pOutputPort->portDefinition.format.video.nFrameHeight;
    }

    switch (pVideoEnc->eRotationType) {
    case ROTATE_90:
        dstImgInfo.nStride      = ALIGN(nTargetHeight, 16);
        dstImgInfo.nSliceHeight = nTargetWidth;
        dstImgInfo.nImageWidth  = nTargetHeight;
        dstImgInfo.nImageHeight = nTargetWidth;

        if (pInputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_FALSE) {
            dstImgInfo.nLeft    = 0;
            dstImgInfo.nTop     = 0;
            dstImgInfo.nWidth   = nTargetHeight;
            dstImgInfo.nHeight  = nTargetWidth;
        } else {
            dstImgInfo.nLeft    = nTargetHeight
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nTop
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
            dstImgInfo.nTop     = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nLeft;
            dstImgInfo.nWidth   = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
            dstImgInfo.nHeight  = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
        }
        break;
    case ROTATE_180:
        dstImgInfo.nStride      = ALIGN(nTargetWidth, 16);
        dstImgInfo.nSliceHeight = nTargetHeight;
        dstImgInfo.nImageWidth  = nTargetWidth;
        dstImgInfo.nImageHeight = nTargetHeight;

        if (pInputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_FALSE) {
            dstImgInfo.nLeft    = 0;
            dstImgInfo.nTop     = 0;
            dstImgInfo.nWidth   = nTargetWidth;
            dstImgInfo.nHeight  = nTargetHeight;
        } else {
            dstImgInfo.nLeft    = pInputPort->portDefinition.format.video.nFrameWidth
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nLeft
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
            dstImgInfo.nTop     = nTargetHeight
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nTop
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
            dstImgInfo.nWidth   = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
            dstImgInfo.nHeight  = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
        }
        break;
    case ROTATE_270:
        dstImgInfo.nStride      = ALIGN(nTargetHeight, 16);
        dstImgInfo.nSliceHeight = nTargetWidth;
        dstImgInfo.nImageWidth  = nTargetHeight;
        dstImgInfo.nImageHeight = nTargetWidth;

        if (pInputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_FALSE) {
            dstImgInfo.nLeft    = 0;
            dstImgInfo.nTop     = 0;
            dstImgInfo.nWidth   = nTargetHeight;
            dstImgInfo.nHeight  = nTargetWidth;
        } else {
            dstImgInfo.nLeft    = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nTop;
            dstImgInfo.nTop     = pInputPort->portDefinition.format.video.nFrameWidth
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nLeft
                                    - pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
            dstImgInfo.nWidth   = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
            dstImgInfo.nHeight  = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
        }
        break;
    case ROTATE_0:
    default:
        dstImgInfo.nStride      = ALIGN(nTargetWidth, 16);
        dstImgInfo.nSliceHeight = nTargetHeight;
        dstImgInfo.nImageWidth  = nTargetWidth;
        dstImgInfo.nImageHeight = nTargetHeight;

        if (pInputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_FALSE) {
            dstImgInfo.nLeft    = 0;
            dstImgInfo.nTop     = 0;
            dstImgInfo.nWidth   = nTargetWidth;
            dstImgInfo.nHeight  = nTargetHeight;
        } else {
            dstImgInfo.nLeft    = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nLeft;
            dstImgInfo.nTop     = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nTop;
            dstImgInfo.nWidth   = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
            dstImgInfo.nHeight  = pInputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
        }
        break;
    }

    if ((pVideoEnc->eMirrorType == OMX_MirrorVertical) ||
        (pVideoEnc->eMirrorType == OMX_MirrorBoth))
            bVerticalFlip = OMX_TRUE;
    if ((pVideoEnc->eMirrorType == OMX_MirrorHorizontal) ||
        (pVideoEnc->eMirrorType == OMX_MirrorBoth))
            bHorizontalFlip = OMX_TRUE;

    if ((eSrcColorFormat == OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar) ||
        (eSrcColorFormat == OMX_SEC_COLOR_FormatS10bitYVU420SemiPlanar)) {
        dstImgInfo.nStride = ALIGN(dstImgInfo.nStride, S10B_FORMAT_8B_ALIGNMENT);
    }

    /*******************/
    /* get buffer info */
    /*******************/
    /* 1. SRC : OMX INPUT */
    if (pInputPort->eMetaDataType != METADATA_TYPE_DISABLED) {
        /* 1. meta data is enabled
         *    1) blur or rotation is used
         *       * blur, rotation is supported by HW.
         *    2) format is not supported at HW codec
         *       needs CSC(Color-Space-Conversion).
         */

        EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;
        OMX_ERRORTYPE                err = OMX_ErrorNone;

        if (pInputPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK) {
            /* METADATA_TYPE_GRAPHIC */
            EXYNOS_OMX_LOCK_RANGE range;
            OMX_U32               stride = 0;

            range.nWidth        = srcImgInfo.nStride;
            range.nHeight       = srcImgInfo.nSliceHeight;
            range.eColorFormat  = eColorFormat;  /* OMX_COLOR_FormatAndroidOpaque */
            stride              = range.nWidth;

            err = Exynos_OSAL_LockMetaData(pInputBuf,
                                           range,
                                           &stride, &bufferInfo,
                                           pInputPort->eMetaDataType);
            if (err != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to Exynos_OSAL_LockMetaData (err:0x%x)",
                                                    pExynosComponent, __FUNCTION__, err);
                goto EXIT;
            }

            srcImgInfo.nStride = stride;

            if (csc_method == CSC_METHOD_HW) {
                pSrcBuf[0]   = (void *)bufferInfo.fd[0];
                pSrcBuf[1]   = (void *)bufferInfo.fd[1];
                pSrcBuf[2]   = (void *)bufferInfo.fd[2];
            } else {
                pSrcBuf[0] = (void *)bufferInfo.addr[0];
                pSrcBuf[1] = (void *)bufferInfo.addr[1];
                pSrcBuf[2] = (void *)bufferInfo.addr[2];
            }
        } else {
            /* METADATA_TYPE_DATA or METADATA_TYPE_HANDLE */
            err = Exynos_OSAL_GetInfoFromMetaData(pInputBuf, &bufferInfo, pInputPort->eMetaDataType);
            if (err != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to Exynos_OSAL_GetInfoFromMetaData (err:0x%x)",
                                    pExynosComponent, __FUNCTION__, err);
                ret = OMX_FALSE;
                goto EXIT;
            }

            if (csc_method == CSC_METHOD_HW) {
                pSrcBuf[0]   = (void *)bufferInfo.fd[0];
                pSrcBuf[1]   = (void *)bufferInfo.fd[1];
                pSrcBuf[2]   = (void *)bufferInfo.fd[2];
            } else {
                Exynos_OSAL_GetPlaneSize(eSrcColorFormat, pInputPort->ePlaneType, srcImgInfo.nStride, srcImgInfo.nSliceHeight, nDataLen, nAllocLen);
                nPlaneCnt = Exynos_GetPlaneFromPort(pInputPort);

                /* get virtual address */
                for (i = 0; i < nPlaneCnt; i++) {
                    if (bufferInfo.fd[i] != 0) {
                        pSrcBuf[i] = Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, bufferInfo.fd[i]);

                        if(pSrcBuf[i] == NULL)  /* if there isn't addr, try to map using fd (in case of external allocation) */
                            pSrcBuf[i] = Exynos_OSAL_SharedMemory_Map(pVideoEnc->hSharedMemory, nAllocLen[i], bufferInfo.fd[i]);

                        if (pSrcBuf[i] == NULL) {
                            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to haredMemory_Map(%d) is failed",
                                                        pExynosComponent, __FUNCTION__, bufferInfo.fd[i]);
                            ret = OMX_FALSE;
                            goto EXIT;
                        }
                    }
                }
            }
        }
    } else {
        /* 2. meta data is not enabled (user application format)
         *
         *    1) HW codec cound't handle user application format
         *       separates to each color plan and adds the padding data if it is needed.
         *    2) format is not supported at HW codec
         *       needs CSC(Color-Space-Conversion).
         */
        if (csc_method == CSC_METHOD_HW) {
            pSrcBuf[0]   = (void *)Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, (char *)pInputBuf);
            pSrcBuf[1]   = NULL;
            pSrcBuf[2]   = NULL;
        } else {
            Exynos_OSAL_GetPlaneSize(eColorFormat, PLANE_SINGLE_USER, srcImgInfo.nStride, srcImgInfo.nSliceHeight, nDataLen, nAllocLen);
            pSrcBuf[0]  = (void *)((char *)pInputBuf);
            pSrcBuf[1]  = (void *)((char *)pInputBuf + nDataLen[0]);
            pSrcBuf[2]  = (void *)((char *)pInputBuf + nDataLen[0] + nDataLen[1]);
        }
    }

    /* 2. DST : MFC INPUT */
    if (csc_method == CSC_METHOD_HW) {
        pDstBuf[0] = (void *)pSrcInputData->buffer.fd[0];
        pDstBuf[1] = (void *)pSrcInputData->buffer.fd[1];
        pDstBuf[2] = (void *)pSrcInputData->buffer.fd[2];
    } else {
        if (pInputPort->ePlaneType == PLANE_SINGLE) {
            /* single-FD. only Y addr is valid */
            int nPlaneCnt = Exynos_OSAL_GetPlaneCount(eSrcColorFormat, PLANE_MULTIPLE);

            if (nPlaneCnt == 2) {
                /* Semi-Planar : interleaved */
                pDstBuf[0] = pSrcInputData->buffer.addr[0];

                pDstBuf[1] = (void *)(((char *)pDstBuf[0]) + GET_UV_OFFSET(dstImgInfo.nImageWidth, dstImgInfo.nImageHeight));
            } else if (nPlaneCnt == 3) {
                /* Planar */
                pDstBuf[0] = pSrcInputData->buffer.addr[0];

                pDstBuf[1] = (void *)(((char *)pDstBuf[0]) + GET_CB_OFFSET(dstImgInfo.nImageWidth, dstImgInfo.nImageHeight));
                pDstBuf[2] = (void *)(((char *)pDstBuf[0]) + GET_CR_OFFSET(dstImgInfo.nImageWidth, dstImgInfo.nImageHeight));
            }
        } else {
            /* multi-FD */
            pDstBuf[0] = pSrcInputData->buffer.addr[0];
            pDstBuf[1] = pSrcInputData->buffer.addr[1];
            pDstBuf[2] = pSrcInputData->buffer.addr[2];
        }
    }
    Exynos_OSAL_GetPlaneSize(eSrcColorFormat, pInputPort->ePlaneType, dstImgInfo.nStride, dstImgInfo.nSliceHeight, nDataLen, nAllocLen);
    pCodecInputBuffer->dataSize = 0;
    nPlaneCnt = Exynos_GetPlaneFromPort(pInputPort);
    for (i = 0; i < nPlaneCnt; i++)
        pCodecInputBuffer->dataSize += nDataLen[i];


    /**************************/
    /* [CSC] setup image info */
    /**************************/
    if (pVideoEnc->csc_set_format == OMX_FALSE) {
        /********************/
        /* get color format */
        /********************/
        unsigned int csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)eColorFormat, PLANE_SINGLE_USER);
        unsigned int csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)eSrcColorFormat, pInputPort->ePlaneType);

        int colorspace = CSC_EQ_COLORSPACE_SMPTE170M;
        int range      = (pInputPort->ColorAspects.nRangeType == RANGE_FULL)? CSC_EQ_RANGE_FULL:CSC_EQ_RANGE_NARROW;

        EXYNOS_OMX_VIDEO_COLORASPECTS colorAspects;
        colorAspects.nRangeType = (range == CSC_EQ_RANGE_FULL)? RANGE_FULL:RANGE_LIMITED;

        if (pInputPort->eMetaDataType != METADATA_TYPE_DISABLED) {
            if (pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC)
                csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)pVideoEnc->surfaceFormat, pInputPort->ePlaneType);
            else
                csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)eColorFormat, pInputPort->ePlaneType);
        }

        colorspace = Exynos_OSAL_DataSpaceToColorSpace(pInputPort->ColorAspects.nDataSpace, csc_src_color_format);
        Exynos_OSAL_ColorSpaceToColorAspects(colorspace, &colorAspects);
        Exynos_OSAL_GetColorAspectsForBitstream(&colorAspects, &(pOutputPort->ColorAspects));

        csc_set_src_format(
            pVideoEnc->csc_handle,   /* handle */
            srcImgInfo.nStride,      /* width */
            srcImgInfo.nSliceHeight, /* height */
            srcImgInfo.nLeft,        /* crop_left */
            srcImgInfo.nTop,         /* crop_top */
            srcImgInfo.nWidth,       /* crop_width */
            srcImgInfo.nHeight,      /* crop_height */
            csc_src_color_format,    /* color_format */
            csc_src_cacheable);      /* cacheable */

        csc_set_dst_format(
            pVideoEnc->csc_handle,   /* handle */
            dstImgInfo.nStride,      /* width */
            dstImgInfo.nSliceHeight, /* height */
            dstImgInfo.nLeft,        /* crop_left */
            dstImgInfo.nTop,         /* crop_top */
            dstImgInfo.nWidth,       /* crop_width */
            dstImgInfo.nHeight,      /* crop_height */
            csc_dst_color_format,    /* color_format */
            csc_dst_cacheable);      /* cacheable */

        csc_set_eq_property(
            pVideoEnc->csc_handle,   /* handle */
            CSC_EQ_MODE_USER,        /* user select */
            range,                   /* Limited, Full */
            colorspace);             /* BT.601, BT.709, BT.2020 */

        pVideoEnc->csc_set_format = OMX_TRUE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s CSC(%x) %s %s/ [SRC] resol(%d/%d), rect(%d/%d/%d/%d), format(0x%x) -> [DST] resol(%d/%d), rect(%d/%d/%d/%d), format(0x%x), range(0x%x), colorspace(0x%x)",
                                               pExynosComponent, __FUNCTION__,
                                               (csc_method == CSC_METHOD_SW)? "SW":"HW", csc_memType,
                                               (pVideoEnc->eRotationType != ROTATE_0)? "with rotation":"",
                                               (pVideoEnc->eMirrorType != OMX_MirrorNone) ? "with Flip":"",
                                               srcImgInfo.nStride, srcImgInfo.nSliceHeight,
                                               srcImgInfo.nLeft, srcImgInfo.nTop, srcImgInfo.nWidth, srcImgInfo.nHeight, csc_src_color_format,
                                               dstImgInfo.nStride, dstImgInfo.nSliceHeight,
                                               dstImgInfo.nLeft, dstImgInfo.nTop, dstImgInfo.nWidth, dstImgInfo.nHeight, csc_dst_color_format,
                                               range, colorspace);
    }

    /********************************/
    /* [CSC] setup blur filter info */
    /********************************/
    if (pVideoEnc->bUseBlurFilter == OMX_TRUE) {
        CSC_HW_FILTER filterType = CSC_FT_NONE;

        if (pVideoEnc->eBlurMode & BLUR_MODE_DOWNUP) {
            switch (pVideoEnc->eBlurResol) {
            case BLUR_RESOL_240:
                filterType = CSC_FT_240;
                break;
            case BLUR_RESOL_480:
                filterType = CSC_FT_480;
                break;
            case BLUR_RESOL_720:
                filterType = CSC_FT_720;
                break;
            case BLUR_RESOL_960:
                filterType = CSC_FT_960;
                break;
            case BLUR_RESOL_1080:
                filterType = CSC_FT_1080;
                break;
            default:
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invliad eBlurResol(%d)", pExynosComponent, __FUNCTION__, pVideoEnc->eBlurResol);
                ret = OMX_FALSE;
                goto EXIT;
            }
        }

        if (pVideoEnc->eBlurMode & BLUR_MODE_COEFFICIENT)
            filterType = CSC_FT_BLUR;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] blur filter is enabled : type(%x)", pExynosComponent, __FUNCTION__, filterType);

        csc_set_filter_property(
            pVideoEnc->csc_handle,
            filterType);
    }

    /***************************/
    /* [CSC] setup buffer info */
    /***************************/
    csc_set_src_buffer(
        pVideoEnc->csc_handle,  /* handle */
        pSrcBuf,
        csc_memType);
    csc_set_dst_buffer(
        pVideoEnc->csc_handle,  /* handle */
        pDstBuf,
        csc_memType);

    /*****************/
    /* [CSC] convert */
    /*****************/
    if ((pVideoEnc->eRotationType != ROTATE_0) ||
        (pVideoEnc->eMirrorType != OMX_MirrorNone))
        csc_ret = csc_convert_with_rotation(pVideoEnc->csc_handle, (int)pVideoEnc->eRotationType, bHorizontalFlip, bVerticalFlip);
    else
        csc_ret = csc_convert(pVideoEnc->csc_handle);
    ret = (csc_ret != CSC_ErrorNone)? OMX_FALSE:OMX_TRUE;

    if (pInputPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK)
        Exynos_OSAL_UnlockMetaData(pInputBuf, pInputPort->eMetaDataType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_BOOL Exynos_Preprocessor_InputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *srcInputData)
{
    OMX_BOOL                         ret                = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosInputPort    = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *inputUseBuffer     = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;
    OMX_U32                          nFrameWidth        = exynosInputPort->portDefinition.format.video.nFrameWidth;
    OMX_U32                          nFrameHeight       = exynosInputPort->portDefinition.format.video.nFrameHeight;
    OMX_COLOR_FORMATTYPE             eColorFormat       = exynosInputPort->portDefinition.format.video.eColorFormat;

    OMX_U32 copySize = 0;

    FunctionIn();

    if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
        if ((srcInputData->buffer.addr[0] == NULL) ||
            (srcInputData->pPrivate == NULL)) {
            ret = OMX_FALSE;
            goto EXIT;
        }
    }

    if (inputUseBuffer->dataValid == OMX_TRUE) {
        if (exynosInputPort->bufferProcessType & BUFFER_SHARE) {
            Exynos_Shared_BufferToData(exynosInputPort, inputUseBuffer, srcInputData);

            if (exynosInputPort->eMetaDataType != METADATA_TYPE_DISABLED) {
                OMX_ERRORTYPE                err = OMX_ErrorNone;
                EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;
                unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
                unsigned int nDataLen[MAX_BUFFER_PLANE]  = {0, 0, 0};

                OMX_COLOR_FORMATTYPE inputColorFormat = OMX_COLOR_FormatUnused;

                int i = 0, nPlaneCnt = 0;

                inputColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
                Exynos_OSAL_GetPlaneSize(inputColorFormat, exynosInputPort->ePlaneType, nFrameWidth, nFrameHeight, nDataLen, nAllocLen);
                nPlaneCnt = Exynos_GetPlaneFromPort(exynosInputPort);

                if (inputUseBuffer->dataLen <= 0) {
                    /* input data is not valid */
                    if (!(inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS)) {
                        /* w/o EOS flag */
                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] dataLen is zero w/o EOS flag(0x%x). this buffer(%p) will be discarded",
                                                                pExynosComponent, __FUNCTION__,
                                                                inputUseBuffer->nFlags, inputUseBuffer->bufferHeader);
                        ret = OMX_FALSE;
                        goto EXIT;
                    } else {
                        /* with EOS flag
                         * makes a buffer for EOS handling needed at MFC Processing scheme.
                         */
                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] dataLen is zero with EOS flag(0x%x)",
                                                                pExynosComponent, __FUNCTION__, inputUseBuffer->nFlags);
                        for (i = 0; i < nPlaneCnt; i++) {
                            srcInputData->buffer.addr[i] =
                                (void *)Exynos_OSAL_SharedMemory_Alloc(pVideoEnc->hSharedMemory, nAllocLen[i], NORMAL_MEMORY);
                            srcInputData->buffer.fd[i] =
                                Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, srcInputData->buffer.addr[i]);
                        }
                    }
                } else {
                    /* input data is valid */
#ifdef USE_ANDROID
                    for (i = 0; i < nPlaneCnt; i++) {
                        if ((srcInputData->buffer.fd[i] != 0) &&
                            (srcInputData->buffer.addr[i] == NULL)) {
                            srcInputData->buffer.addr[i] =
                                Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, srcInputData->buffer.fd[i]);
                            if(srcInputData->buffer.addr[i] == NULL) {
                                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] dynamically regist a buffer(fd[%u])",
                                                                pExynosComponent, __FUNCTION__, srcInputData->buffer.fd[i]);
                                srcInputData->buffer.addr[i] =
                                    Exynos_OSAL_SharedMemory_Map(pVideoEnc->hSharedMemory, nAllocLen[i], srcInputData->buffer.fd[i]);
                            }
                        }
                    }
#endif
                }
            }

            /* reset dataBuffer */
            Exynos_ResetDataBuffer(inputUseBuffer);

            ret = OMX_TRUE;
        } else if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
            if (inputUseBuffer->remainDataLen == 0) {
                inputUseBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                copySize = 0;
            } else {
                copySize = inputUseBuffer->remainDataLen;

                if ((eColorFormat == OMX_COLOR_Format32bitARGB8888) ||
                    (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888)) {
                    copySize = srcInputData->allocSize;
                }

                if (exynosInputPort->eMetaDataType & METADATA_TYPE_DATA)
                    copySize = inputUseBuffer->allocSize;
            }

            if (((srcInputData->allocSize) - (srcInputData->dataLen)) < copySize) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] codec buffer's remaining space(%d) is smaller than input data(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    ((srcInputData->allocSize) - (srcInputData->dataLen)), copySize);
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);

                pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                        pExynosComponent->callbackData,
                                                        OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                ret = OMX_FALSE;
                goto EXIT;
            }

            if (copySize > 0) {
                ret = Exynos_CSC_InputData(pOMXComponent, srcInputData);
                if (ret != OMX_TRUE) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to CSC_InputData", pExynosComponent, __FUNCTION__);
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);

                    pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                            pExynosComponent->callbackData,
                                                            OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                    ret = OMX_FALSE;
                    goto EXIT;
                }
            }

            inputUseBuffer->dataLen         -= copySize;
            inputUseBuffer->remainDataLen   -= copySize;
            inputUseBuffer->usedDataLen     += copySize;

            srcInputData->dataLen           += copySize;
            srcInputData->remainDataLen     += copySize;

            srcInputData->timeStamp     = inputUseBuffer->timeStamp;
            srcInputData->nFlags        = inputUseBuffer->nFlags;
            srcInputData->bufferHeader  = inputUseBuffer->bufferHeader;

            /* return OMX buffer and reset dataBuffer */
            Exynos_InputBufferReturn(pOMXComponent, inputUseBuffer);

            ret = OMX_TRUE;
        }

        if ((srcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            if (srcInputData->dataLen != 0) {
                pExynosComponent->bBehaviorEOS = OMX_TRUE;
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] behavior EOS frame", pExynosComponent, __FUNCTION__);
            }
        }

        if ((pExynosComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) &&
            ((srcInputData->nFlags & OMX_BUFFERFLAG_CODECCONFIG) != OMX_BUFFERFLAG_CODECCONFIG)) {
            pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_TRUE;
            pExynosComponent->checkTimeStamp.startTimeStamp = srcInputData->timeStamp;
            pExynosComponent->checkTimeStamp.nStartFlags = srcInputData->nFlags;
            pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] first frame timestamp after seeking %lld us (%.2f secs)",
                            pExynosComponent, __FUNCTION__, srcInputData->timeStamp, (double)(srcInputData->timeStamp / 1E6));
        }
    }

EXIT:

    if (ret != OMX_TRUE) {
        /* return OMX buffer and reset dataBuffer */
        Exynos_InputBufferReturn(pOMXComponent, inputUseBuffer);
    }

    FunctionOut();

    return ret;
}

OMX_BOOL Exynos_Postprocess_OutputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *dstOutputData)
{
    OMX_BOOL                  ret               = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *outputUseBuffer   = &exynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;

    OMX_U32 copySize = 0;

    FunctionIn();

    if (exynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        if (Exynos_Shared_DataToBuffer(exynosOutputPort, outputUseBuffer, dstOutputData) == OMX_ErrorNone)
            outputUseBuffer->dataValid = OMX_TRUE;
    }

    if (outputUseBuffer->dataValid == OMX_TRUE) {
        if (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp == OMX_TRUE) {
            if (pExynosComponent->checkTimeStamp.startTimeStamp == dstOutputData->timeStamp) {
                pExynosComponent->checkTimeStamp.startTimeStamp = RESET_TIMESTAMP_VAL;
                pExynosComponent->checkTimeStamp.nStartFlags = 0x0;
                pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
                pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            } else {
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] drop frame after seeking(in: %lld vs out: %lld)",
                                                        pExynosComponent, __FUNCTION__,
                                                        pExynosComponent->checkTimeStamp.startTimeStamp, dstOutputData->timeStamp);
                if (exynosOutputPort->bufferProcessType & BUFFER_SHARE)
                    Exynos_OMX_FillThisBufferAgain(pOMXComponent, outputUseBuffer->bufferHeader);

                ret = OMX_TRUE;
                goto EXIT;
            }
        } else if (pExynosComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) {
            ret = OMX_TRUE;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] not set check timestamp after seeking", pExynosComponent, __FUNCTION__);

            if (exynosOutputPort->bufferProcessType & BUFFER_SHARE)
                Exynos_OMX_FillThisBufferAgain(pOMXComponent, outputUseBuffer->bufferHeader);

            goto EXIT;
        }

        if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
            if ((dstOutputData->remainDataLen <= (outputUseBuffer->allocSize - outputUseBuffer->dataLen)) &&
                (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                copySize = dstOutputData->remainDataLen;
                if (copySize > 0) {
                    Exynos_OSAL_Memcpy((outputUseBuffer->bufferHeader->pBuffer + outputUseBuffer->dataLen),
                                       ((char *)dstOutputData->buffer.addr[0] + dstOutputData->usedDataLen),
                                       copySize);
                }

                outputUseBuffer->dataLen += copySize;
                outputUseBuffer->remainDataLen += copySize;
                outputUseBuffer->nFlags = dstOutputData->nFlags;
                outputUseBuffer->timeStamp = dstOutputData->timeStamp;

                ret = OMX_TRUE;

                if ((outputUseBuffer->remainDataLen > 0) ||
                    (outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) ||
                    (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                    Exynos_OutputBufferReturn(pOMXComponent, outputUseBuffer);
                }
            } else if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort)) {
                outputUseBuffer->dataLen = 0;
                outputUseBuffer->remainDataLen = 0;
                outputUseBuffer->nFlags = dstOutputData->nFlags;
                outputUseBuffer->timeStamp = dstOutputData->timeStamp;
                Exynos_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            } else {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] codec buffer's remaining space(%d) is smaller than output data(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    (outputUseBuffer->allocSize - outputUseBuffer->dataLen), dstOutputData->remainDataLen);
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);

                pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                        pExynosComponent->callbackData,
                                                        OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                ret = OMX_FALSE;
            }
        } else if (exynosOutputPort->bufferProcessType & BUFFER_SHARE) {
            if ((outputUseBuffer->remainDataLen > 0) ||
                (outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) ||
                (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {

                Exynos_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            } else {
                Exynos_OMX_FillThisBufferAgain(pOMXComponent, outputUseBuffer->bufferHeader);
                Exynos_ResetDataBuffer(outputUseBuffer);
            }
        }
    } else {
        ret = OMX_FALSE;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ExtensionSetup(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *srcInputUseBuffer  = &pInputPort->way.port2WayDataBuffer.inputDataBuffer;
    OMX_COLOR_FORMATTYPE             eColorFormat       = pInputPort->portDefinition.format.video.eColorFormat;
    OMX_COLOR_FORMATTYPE             eActualFormat      = OMX_COLOR_FormatUnused;

    unsigned int nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = {0, 0, 0};

    int i = 0;

    if (pInputPort->eMetaDataType != METADATA_TYPE_DISABLED) {
        if ((srcInputUseBuffer->dataLen == 0) &&
            (srcInputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS)) {
            /* In this case, the metadata is not valid.
             * sets dummy info in order to return EOS flag at output port through FBD.
             * IL client should do stop sequence.
             */
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] dataLen is zero with EOS flag(0x%x) at first input",
                                                pExynosComponent, __FUNCTION__, srcInputUseBuffer->nFlags);

            if (pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) {
                pVideoEnc->surfaceFormat = OMX_COLOR_FormatYUV420SemiPlanar;
                pInputPort->bufferProcessType = BUFFER_SHARE;
                Exynos_SetPlaneToPort(pInputPort, Exynos_OSAL_GetPlaneCount(pVideoEnc->surfaceFormat, pInputPort->ePlaneType));
            } else {
                Exynos_SetPlaneToPort(pInputPort, Exynos_OSAL_GetPlaneCount(eColorFormat, pInputPort->ePlaneType));
            }

            goto EXIT;
        } else {
            EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;

            ret = Exynos_OSAL_GetInfoFromMetaData(srcInputUseBuffer->bufferHeader->pBuffer,
                                                    &bufferInfo, pInputPort->eMetaDataType);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to Exynos_OSAL_GetInfoFromMetaData (err:0x%x)",
                                    pExynosComponent, __FUNCTION__, ret);
                goto EXIT;
            }

            if (pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) {
                pVideoEnc->surfaceFormat = bufferInfo.eColorFormat;
                eActualFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
                if (eActualFormat == OMX_COLOR_FormatUnused) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] unsupported color format : ANB color is 0x%x",
                                                    pExynosComponent, __FUNCTION__, pVideoEnc->surfaceFormat);
                    ret = OMX_ErrorNotImplemented;
                    goto EXIT;
                }

                if ((pVideoEnc->surfaceFormat == eActualFormat) &&
                    !(pInputPort->bufferProcessType & BUFFER_COPY_FORCE)) {
                    pInputPort->bufferProcessType = BUFFER_SHARE;
                    Exynos_SetPlaneToPort(pInputPort, Exynos_OSAL_GetPlaneCount(pVideoEnc->surfaceFormat, pInputPort->ePlaneType));
                } else {
                    pInputPort->bufferProcessType = BUFFER_COPY;
                }
            } else {
                Exynos_SetPlaneToPort(pInputPort, Exynos_OSAL_GetPlaneCount(eColorFormat, pInputPort->ePlaneType));
            }
        }
    }

    /* forcefully have to use BUFFER_COPY mode, if blur filter is used or rotation is needed or image flip is needed*/
    if ((pVideoEnc->bUseBlurFilter == OMX_TRUE) ||
        (pVideoEnc->eRotationType != ROTATE_0) ||
        (pVideoEnc->eMirrorType != OMX_MirrorNone)) {
        pInputPort->bufferProcessType = BUFFER_COPY;
         Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] %s %s %s: enabled", pExynosComponent, __FUNCTION__,
                                                    (pVideoEnc->bUseBlurFilter == OMX_TRUE)? "blur filter":"",
                                                    (pVideoEnc->eRotationType != ROTATE_0)? "rotation":"",
                                                    (pVideoEnc->eMirrorType != OMX_MirrorNone)? "image flip":"");
    }

    if ((pInputPort->bufferProcessType & BUFFER_COPY) == BUFFER_COPY) {
        OMX_U32 nFrameWidth  = pOutputPort->portDefinition.format.video.nFrameWidth;
        OMX_U32 nFrameHeight = pOutputPort->portDefinition.format.video.nFrameHeight;

        if ((pVideoEnc->eRotationType == ROTATE_90) ||
            (pVideoEnc->eRotationType == ROTATE_270)) {
            nFrameWidth  = pOutputPort->portDefinition.format.video.nFrameHeight;
            nFrameHeight = pOutputPort->portDefinition.format.video.nFrameWidth;
        }

        if (pVideoEnc->bEncDRC == OMX_TRUE) {
            Exynos_Free_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX);
            Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);
            pVideoEnc->bEncDRC = OMX_FALSE;
        }

        eActualFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
        if (eActualFormat == OMX_COLOR_FormatUnused) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] unsupported color format : 0x%x", pExynosComponent, __FUNCTION__, eActualFormat);
            ret = OMX_ErrorNotImplemented;
            goto EXIT;
        }

        Exynos_SetPlaneToPort(pInputPort, Exynos_OSAL_GetPlaneCount(eActualFormat, pInputPort->ePlaneType));
        Exynos_OSAL_GetPlaneSize(eActualFormat, pInputPort->ePlaneType, nFrameWidth, nFrameHeight, nDataLen, nAllocLen);

        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX, MFC_INPUT_BUFFER_NUM_MAX, nAllocLen);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Allocate_CodecBuffers", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)
            Exynos_CodecBufferEnqueue(pExynosComponent, INPUT_PORT_INDEX, pVideoEnc->pMFCEncInputBuffer[i]);
    } else if (pInputPort->bufferProcessType == BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

EXIT:
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] eActualFormat: 0x%x, eColorFormat: 0x%x, eBufferColorFormat: 0x%x, bufferProcessType: 0x%x",
                                            pExynosComponent, __FUNCTION__,
                                            eActualFormat, eColorFormat, pVideoEnc->surfaceFormat, pInputPort->bufferProcessType);

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SrcInputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosInputPort    = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *srcInputUseBuffer  = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;
    EXYNOS_OMX_DATA                 *pSrcInputData      = &exynosInputPort->processData;

    OMX_BOOL bCheckInputData = OMX_FALSE;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);
        Exynos_Wait_ProcessPause(pExynosComponent, INPUT_PORT_INDEX);
        if ((exynosInputPort->semWaitPortEnable[INPUT_WAY_INDEX] != NULL) &&
            ((CHECK_PORT_BEING_DISABLED(exynosInputPort)) ||
             (!CHECK_PORT_ENABLED(exynosInputPort)))) {
            /* sema will be posted at PortEnable */
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> wait(port enable)", pExynosComponent, __FUNCTION__);
            Exynos_OSAL_SemaphoreWait(exynosInputPort->semWaitPortEnable[INPUT_WAY_INDEX]);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> post(port enable)", pExynosComponent, __FUNCTION__);
            continue;
        }

        while ((Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosInputPort))
                break;

            Exynos_OSAL_MutexLock(srcInputUseBuffer->bufferMutex);
            if (pVideoEnc->bFirstInput == OMX_FALSE) {
                if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                    OMX_PTR codecBuffer;
                    if ((pSrcInputData->buffer.addr[0] == NULL) ||
                        (pSrcInputData->pPrivate == NULL)) {
                        Exynos_CodecBufferDequeue(pExynosComponent, INPUT_PORT_INDEX, &codecBuffer);
                        if (pVideoEnc->bExitBufferProcessThread) {
                            Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                            goto EXIT;
                        }

                        if (codecBuffer != NULL) {
                            Exynos_CodecBufferToData(codecBuffer, pSrcInputData, INPUT_PORT_INDEX);
                        }
                        Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                        break;
                    }
                }

                if (srcInputUseBuffer->dataValid == OMX_TRUE) {
                    bCheckInputData = Exynos_Preprocessor_InputData(pOMXComponent, pSrcInputData);
                } else {
                    bCheckInputData = OMX_FALSE;
                }
            }

            if ((bCheckInputData == OMX_FALSE) &&
                (!CHECK_PORT_BEING_FLUSHED(exynosInputPort))) {
                ret = Exynos_InputBufferGetQueue(pExynosComponent);
                if (pVideoEnc->bExitBufferProcessThread) {
                    Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                    goto EXIT;
                }

                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                    break;
                }

                if ((pVideoEnc->bFirstInput == OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosInputPort))) {
                    ret = Exynos_OMX_ExtensionSetup(hComponent);
                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);
                        (*(pExynosComponent->pCallbacks->EventHandler)) (pOMXComponent,
                                    pExynosComponent->callbackData,
                                    (OMX_U32)OMX_EventError,
                                    (OMX_U32)OMX_ErrorNotImplemented,
                                    INPUT_PORT_INDEX, NULL);
                        Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                        break;
                    }

                    pVideoEnc->bFirstInput = OMX_FALSE;
                }

                Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                break;
            }

            if (CHECK_PORT_BEING_FLUSHED(exynosInputPort)) {
                Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                break;
            }

            ret = pVideoEnc->exynos_codec_srcInputProcess(pOMXComponent, pSrcInputData);

            Exynos_ResetCodecData(pSrcInputData);
            Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);

            if ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorCodecInit)
                pVideoEnc->bExitBufferProcessThread = OMX_TRUE;
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SrcOutputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *srcOutputUseBuffer = &exynosInputPort->way.port2WayDataBuffer.outputDataBuffer;
    EXYNOS_OMX_DATA           srcOutputData;

    FunctionIn();

    Exynos_ResetCodecData(&srcOutputData);

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);
        if ((exynosInputPort->semWaitPortEnable[OUTPUT_WAY_INDEX] != NULL) &&
            ((CHECK_PORT_BEING_DISABLED(exynosInputPort)) ||
             (!CHECK_PORT_ENABLED(exynosInputPort)))) {
            /* sema will be posted at PortEnable */
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> wait(port enable)", pExynosComponent, __FUNCTION__);
            Exynos_OSAL_SemaphoreWait(exynosInputPort->semWaitPortEnable[OUTPUT_WAY_INDEX]);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> post(port enable)", pExynosComponent, __FUNCTION__);
            continue;
        }

        while (!pVideoEnc->bExitBufferProcessThread) {
            if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                if (Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX) == OMX_FALSE)
                    break;
            }
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosInputPort))
                break;

            Exynos_OSAL_MutexLock(srcOutputUseBuffer->bufferMutex);
            Exynos_OSAL_Memset(&srcOutputData, 0, sizeof(EXYNOS_OMX_DATA));

            ret = pVideoEnc->exynos_codec_srcOutputProcess(pOMXComponent, &srcOutputData);

            if (ret == OMX_ErrorNone) {
                if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                    OMX_PTR codecBuffer;
                    codecBuffer = srcOutputData.pPrivate;
                    if (codecBuffer != NULL)
                        Exynos_CodecBufferEnqueue(pExynosComponent, INPUT_PORT_INDEX, codecBuffer);
                }
                if (exynosInputPort->bufferProcessType & BUFFER_SHARE) {
                    Exynos_Shared_DataToBuffer(exynosInputPort, srcOutputUseBuffer, &srcOutputData);
                    Exynos_InputBufferReturn(pOMXComponent, srcOutputUseBuffer);
                }
                Exynos_ResetCodecData(&srcOutputData);
            }
            Exynos_OSAL_MutexUnlock(srcOutputUseBuffer->bufferMutex);
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_DstInputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *dstInputUseBuffer = &exynosOutputPort->way.port2WayDataBuffer.inputDataBuffer;
    EXYNOS_OMX_DATA           dstInputData;

    FunctionIn();

    Exynos_ResetCodecData(&dstInputData);

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);
        if ((exynosOutputPort->semWaitPortEnable[INPUT_WAY_INDEX] != NULL) &&
            ((CHECK_PORT_BEING_DISABLED(exynosOutputPort)) ||
             (!CHECK_PORT_ENABLED(exynosOutputPort)))) {
            /* sema will be posted at PortEnable */
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> wait(port enable)", pExynosComponent, __FUNCTION__);
            Exynos_OSAL_SemaphoreWait(exynosOutputPort->semWaitPortEnable[INPUT_WAY_INDEX]);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> post(port enable)", pExynosComponent, __FUNCTION__);
            continue;
        }

        while ((Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))
                break;

            Exynos_OSAL_MutexLock(dstInputUseBuffer->bufferMutex);

            if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
                CODEC_ENC_BUFFER *pCodecBuffer = NULL;
                ret = Exynos_CodecBufferDequeue(pExynosComponent, OUTPUT_PORT_INDEX, (OMX_PTR *)&pCodecBuffer);
                if (pVideoEnc->bExitBufferProcessThread) {
                    Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                    goto EXIT;
                }

                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                    break;
                }
                Exynos_CodecBufferToData(pCodecBuffer, &dstInputData, OUTPUT_PORT_INDEX);
            }

            if (exynosOutputPort->bufferProcessType & BUFFER_SHARE) {
                if ((dstInputUseBuffer->dataValid != OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                    ret = Exynos_OutputBufferGetQueue(pExynosComponent);
                    if (pVideoEnc->bExitBufferProcessThread) {
                        Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                        goto EXIT;
                    }

                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                        break;
                    }

                    ret = Exynos_Shared_BufferToData(exynosOutputPort, dstInputUseBuffer, &dstInputData);
                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to GetBufferFdFromMetaData from %p",
                                                        pExynosComponent, __FUNCTION__, dstInputUseBuffer->bufferHeader);
                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);
                        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                   pExynosComponent->callbackData,
                                                                   OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                        Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                        break;
                    }

                    if (exynosOutputPort->eMetaDataType == METADATA_TYPE_HANDLE) {
                        OMX_PTR dataBuffer = NULL;

                        dataBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, dstInputData.buffer.fd[0]);
                        if (dataBuffer == NULL) {
                            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid data buffer(%u)",
                                                                pExynosComponent, __FUNCTION__, dstInputData.buffer.fd[0]);
                            ret = OMX_ErrorUndefined;
                            Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                            break;
                        }

                        dstInputData.buffer.addr[0] = dataBuffer;
                    } else {
                        if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) {
                            unsigned long ionFD = (unsigned long)dstInputData.buffer.addr[0];

                            OMX_PTR dataBuffer = NULL;
                            dataBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, ionFD);
                            if (dataBuffer == NULL) {
                                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid data buffer(%u)",
                                                                pExynosComponent, __FUNCTION__, ionFD);
                                ret = OMX_ErrorUndefined;
                                Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                                break;
                            }

                            dstInputData.buffer.fd[0]   = ionFD;
                            dstInputData.buffer.addr[0] = dataBuffer;
                        } else {
                            dstInputData.buffer.fd[0] =
                                Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory,
                                            dstInputData.buffer.addr[0]);
                        }
                    }

                    Exynos_ResetDataBuffer(dstInputUseBuffer);
                }
            }

            if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort)) {
                Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                break;
            }

            ret = pVideoEnc->exynos_codec_dstInputProcess(pOMXComponent, &dstInputData);

            Exynos_ResetCodecData(&dstInputData);
            Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_DstOutputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *dstOutputUseBuffer = &exynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    EXYNOS_OMX_DATA          *pDstOutputData = &exynosOutputPort->processData;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);
        Exynos_Wait_ProcessPause(pExynosComponent, OUTPUT_PORT_INDEX);
        if ((exynosOutputPort->semWaitPortEnable[OUTPUT_WAY_INDEX] != NULL) &&
            ((CHECK_PORT_BEING_DISABLED(exynosOutputPort)) ||
             (!CHECK_PORT_ENABLED(exynosOutputPort)))) {
            /* sema will be posted at PortEnable */
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> wait(port enable)", pExynosComponent, __FUNCTION__);
            Exynos_OSAL_SemaphoreWait(exynosOutputPort->semWaitPortEnable[OUTPUT_WAY_INDEX]);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> post(port enable)", pExynosComponent, __FUNCTION__);
            continue;
        }

        while ((Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))
                break;

            Exynos_OSAL_MutexLock(dstOutputUseBuffer->bufferMutex);

            if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
                if ((dstOutputUseBuffer->dataValid != OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                    ret = Exynos_OutputBufferGetQueue(pExynosComponent);
                    if (pVideoEnc->bExitBufferProcessThread) {
                        Exynos_OSAL_MutexUnlock(dstOutputUseBuffer->bufferMutex);
                        goto EXIT;
                    }

                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_MutexUnlock(dstOutputUseBuffer->bufferMutex);
                        break;
                    }
                }
            }

            if ((dstOutputUseBuffer->dataValid == OMX_TRUE) ||
                (exynosOutputPort->bufferProcessType & BUFFER_SHARE))
                ret = pVideoEnc->exynos_codec_dstOutputProcess(pOMXComponent, pDstOutputData);

            if (exynosOutputPort->bufferProcessType & BUFFER_SHARE) {
                if (ret == OMX_ErrorNoneReuseBuffer)
                    Exynos_OMX_FillThisBufferAgain(hComponent, pDstOutputData->bufferHeader);
                else
                    Exynos_Postprocess_OutputData(pOMXComponent, pDstOutputData);
            }

            if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
                if ((ret == OMX_ErrorNone) &&
                    (dstOutputUseBuffer->dataValid == OMX_TRUE)) {
                    Exynos_Postprocess_OutputData(pOMXComponent, pDstOutputData);
                }

                if (pDstOutputData->pPrivate != NULL) {
                    Exynos_CodecBufferEnqueue(pExynosComponent, OUTPUT_PORT_INDEX, pDstOutputData->pPrivate);
                    pDstOutputData->pPrivate = NULL;
                }
            }

            /* reset outputData */
            Exynos_ResetCodecData(pDstOutputData);
            Exynos_OSAL_MutexUnlock(dstOutputUseBuffer->bufferMutex);
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_SrcInputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    Exynos_OMX_SrcInputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_SrcOutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    Exynos_OMX_SrcOutputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_DstInputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    Exynos_OMX_DstInputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_DstOutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    Exynos_OMX_DstOutputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferProcess_Create(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_FALSE;

    ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hDstOutputThread,
                 Exynos_OMX_DstOutputProcessThread,
                 pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hSrcOutputThread,
                     Exynos_OMX_SrcOutputProcessThread,
                     pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hDstInputThread,
                     Exynos_OMX_DstInputProcessThread,
                     pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hSrcInputThread,
                     Exynos_OMX_SrcInputProcessThread,
                     pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferProcess_Terminate(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    OMX_S32 countValue = 0;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_TRUE;

    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bufferSemID);
    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].codecSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].codecSemID);
    pVideoEnc->exynos_codec_bufferProcessRun(pOMXComponent, OUTPUT_PORT_INDEX);

    /* srcInput */
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hSrcInputThread);
    pVideoEnc->hSrcInputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] src input thread is terminated", pExynosComponent, __FUNCTION__);

    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bufferSemID);
    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].codecSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].codecSemID);

    /* dstInput */
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hDstInputThread);
    pVideoEnc->hDstInputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] dst input thread is terminated", pExynosComponent, __FUNCTION__);

    pVideoEnc->exynos_codec_stop(pOMXComponent, INPUT_PORT_INDEX);
    pVideoEnc->exynos_codec_bufferProcessRun(pOMXComponent, INPUT_PORT_INDEX);

    /* srcOutput */
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hSrcOutputThread);
    pVideoEnc->hSrcOutputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] src output thread is terminated", pExynosComponent, __FUNCTION__);

    pVideoEnc->exynos_codec_stop(pOMXComponent, OUTPUT_PORT_INDEX);
    pVideoEnc->exynos_codec_bufferProcessRun(pOMXComponent, OUTPUT_PORT_INDEX);

    /* dstOutput */
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hDstOutputThread);
    pVideoEnc->hDstOutputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] dst output thread is terminated", pExynosComponent, __FUNCTION__);

    pExynosComponent->checkTimeStamp.needSetStartTimeStamp      = OMX_FALSE;
    pExynosComponent->checkTimeStamp.needCheckStartTimeStamp    = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    ret = Exynos_OMX_BaseComponent_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to BaseComponent_Constructor", __FUNCTION__);
        goto EXIT;
    }

    ret = Exynos_OMX_Port_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Port_Constructor", __FUNCTION__);
        Exynos_OMX_BaseComponent_Destructor(pOMXComponent);
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pVideoEnc = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_VIDEOENC_COMPONENT));
    if (pVideoEnc == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_Port_Destructor(pOMXComponent);
        Exynos_OMX_BaseComponent_Destructor(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Memset(pVideoEnc, 0, sizeof(EXYNOS_OMX_VIDEOENC_COMPONENT));
    pExynosComponent->hComponentHandle = (OMX_HANDLETYPE)pVideoEnc;

    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

    pVideoEnc->bFirstInput  = OMX_TRUE;
    pVideoEnc->bFirstOutput = OMX_FALSE;
    pVideoEnc->bQosChanged  = OMX_FALSE;
    pVideoEnc->nQosRatio       = 0;
    pVideoEnc->quantization.nQpI = 4; // I frame quantization parameter
    pVideoEnc->quantization.nQpP = 5; // P frame quantization parameter
    pVideoEnc->quantization.nQpB = 5; // B frame quantization parameter

    pVideoEnc->bDropControl     = OMX_TRUE;
    pVideoEnc->bUseBlurFilter   = OMX_FALSE;
    pVideoEnc->eBlurMode        = BLUR_MODE_NONE;
    pVideoEnc->eBlurResol       = BLUR_RESOL_240;
    pVideoEnc->eMirrorType      = OMX_MirrorNone;

    pVideoEnc->eRotationType    = ROTATE_0;

#ifdef USE_ANDROID
    pVideoEnc->pPerfHandle = Exynos_OSAL_CreatePerformanceHandle(OMX_TRUE /* bIsEncoder = true */);
#endif

    Exynos_OSAL_SignalCreate(&(pVideoEnc->hEncDRCSyncEvent));

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort->supportFormat, 0, (sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX));

    pExynosPort->portDefinition.nBufferCountActual = MAX_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferCountMin = MAX_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferSize = 0;
    pExynosPort->portDefinition.eDomain = OMX_PortDomainVideo;

    pExynosPort->portDefinition.format.video.cMIMEType = Exynos_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    if (pExynosPort->portDefinition.format.video.cMIMEType == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pExynosPort->portDefinition.format.video.nFrameWidth = 0;
    pExynosPort->portDefinition.format.video.nFrameHeight= 0;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.format.video.nBitrate = 1000000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.format.video.pNativeWindow = NULL;
    pVideoEnc->eControlRate[INPUT_PORT_INDEX] = OMX_Video_ControlRateVariable;

    pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort->supportFormat, 0, (sizeof(OMX_COLOR_FORMATTYPE) * OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX));

    pExynosPort->portDefinition.nBufferCountActual = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferCountMin = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.eDomain = OMX_PortDomainVideo;

    pExynosPort->portDefinition.format.video.cMIMEType = Exynos_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    if (pExynosPort->portDefinition.format.video.cMIMEType == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pExynosPort->portDefinition.format.video.nFrameWidth = 0;
    pExynosPort->portDefinition.format.video.nFrameHeight= 0;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.format.video.nBitrate = 1000000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.format.video.pNativeWindow = NULL;
    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] = OMX_Video_ControlRateVariable;

    pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;

    pOMXComponent->UseBuffer              = &Exynos_OMX_UseBuffer;
    pOMXComponent->AllocateBuffer         = &Exynos_OMX_AllocateBuffer;
    pOMXComponent->FreeBuffer             = &Exynos_OMX_FreeBuffer;

#ifdef TUNNELING_SUPPORT
    pOMXComponent->ComponentTunnelRequest           = &Exynos_OMX_ComponentTunnelRequest;
    pExynosComponent->exynos_AllocateTunnelBuffer   = &Exynos_OMX_AllocateTunnelBuffer;
    pExynosComponent->exynos_FreeTunnelBuffer       = &Exynos_OMX_FreeTunnelBuffer;
#endif

    pExynosComponent->exynos_BufferProcessCreate    = &Exynos_OMX_BufferProcess_Create;
    pExynosComponent->exynos_BufferProcessTerminate = &Exynos_OMX_BufferProcess_Terminate;
    pExynosComponent->exynos_BufferFlush          = &Exynos_OMX_BufferFlush;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentDeinit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;

    int i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

#ifdef USE_ANDROID
    Exynos_OSAL_ReleasePerformanceHandle(pVideoEnc->pPerfHandle);
#endif

    if (pVideoEnc->bEncDRCSync == OMX_TRUE) {
        Exynos_OSAL_SignalSet(pVideoEnc->hEncDRCSyncEvent);
    }
    Exynos_OSAL_SignalTerminate(pVideoEnc->hEncDRCSyncEvent);

    Exynos_OSAL_Free(pVideoEnc);
    pExynosComponent->hComponentHandle = pVideoEnc = NULL;

    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    if (pExynosPort->processData.extInfo != NULL) {
        Exynos_OSAL_Free(pExynosPort->processData.extInfo);
        pExynosPort->processData.extInfo = NULL;
    }

    for(i = 0; i < ALL_PORT_NUM; i++) {
        pExynosPort = &pExynosComponent->pExynosPort[i];
        Exynos_OSAL_Free(pExynosPort->portDefinition.format.video.cMIMEType);
        pExynosPort->portDefinition.format.video.cMIMEType = NULL;

        Exynos_OSAL_Free(pExynosPort->supportFormat);
        pExynosPort->supportFormat = NULL;
    }

    ret = Exynos_OMX_Port_Destructor(pOMXComponent);

    ret = Exynos_OMX_BaseComponent_Destructor(hComponent);

EXIT:
    FunctionOut();

    return ret;
}
