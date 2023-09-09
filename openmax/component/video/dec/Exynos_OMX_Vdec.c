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
 * @file        Exynos_OMX_Vdec.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              HyeYeon Chung (hyeon.chung@samsung.com)
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
#include "Exynos_OMX_Vdec.h"
#include "Exynos_OMX_VdecControl.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_ETC.h"

#include "Exynos_OSAL_Platform.h"

#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VIDEO_DEC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


void Exynos_UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *exynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    OMX_U32 width = 0, height = 0;

    FunctionIn();

    if ((exynosOutputPort->portDefinition.format.video.nFrameWidth !=
            exynosInputPort->portDefinition.format.video.nFrameWidth) ||
        (exynosOutputPort->portDefinition.format.video.nFrameHeight !=
            exynosInputPort->portDefinition.format.video.nFrameHeight) ||
        (exynosOutputPort->portDefinition.format.video.nStride !=
            exynosInputPort->portDefinition.format.video.nStride) ||
        (exynosOutputPort->portDefinition.format.video.nSliceHeight !=
            exynosInputPort->portDefinition.format.video.nSliceHeight)) {

        exynosOutputPort->portDefinition.format.video.nFrameWidth =
            exynosInputPort->portDefinition.format.video.nFrameWidth;
        exynosOutputPort->portDefinition.format.video.nFrameHeight =
            exynosInputPort->portDefinition.format.video.nFrameHeight;
        exynosOutputPort->portDefinition.format.video.nStride =
            exynosInputPort->portDefinition.format.video.nStride;
        exynosOutputPort->portDefinition.format.video.nSliceHeight =
            exynosInputPort->portDefinition.format.video.nSliceHeight;
    }

    width  = exynosOutputPort->portDefinition.format.video.nStride;
    height = exynosOutputPort->portDefinition.format.video.nSliceHeight;

    if (width && height) {
        switch((int)exynosOutputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
        case OMX_SEC_COLOR_FormatNV12Tiled:
        case OMX_SEC_COLOR_FormatYVU420Planar:
        case OMX_SEC_COLOR_FormatNV21Linear:
            exynosOutputPort->portDefinition.nBufferSize = (ALIGN(width, 16) * ALIGN(height, 16) * 3) / 2;
            break;
        case OMX_COLOR_FormatYUV420Planar16:  /* 10bit format */
            exynosOutputPort->portDefinition.nBufferSize = (ALIGN(width * 2, 16) * ALIGN(height, 16) * 3) / 2;
            break;
        case OMX_COLOR_Format32bitARGB8888:
            exynosOutputPort->portDefinition.format.video.nStride = exynosInputPort->portDefinition.format.video.nStride * 4;
            exynosOutputPort->portDefinition.nBufferSize = ALIGN(width, 16) * ALIGN(height, 16) * 4;
            break;
        case OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC:
        case OMX_SEC_COLOR_FormatYVU420SemiPlanarSBWC:
            exynosOutputPort->portDefinition.nBufferSize = SBWC_8B_Y_SIZE(width, height) + SBWC_8B_Y_HEADER_SIZE(width, height) +
                                                           SBWC_8B_CBCR_SIZE(width, height) + SBWC_8B_CBCR_HEADER_SIZE(width, height);
            break;
        case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC:
        case OMX_SEC_COLOR_Format10bitYVU420SemiPlanarSBWC:
            exynosOutputPort->portDefinition.nBufferSize = SBWC_10B_Y_SIZE(width, height) + SBWC_10B_Y_HEADER_SIZE(width, height) +
                                                           SBWC_10B_CBCR_SIZE(width, height) + SBWC_10B_CBCR_HEADER_SIZE(width, height);
            break;
        default:
            exynosOutputPort->portDefinition.nBufferSize = ALIGN(width, 16) * ALIGN(height, 16) * 2;
            break;
        }
    }

    FunctionOut();

    return;
}

void Exynos_Output_SetSupportFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec   = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    if ((pVideoDec == NULL) || (pOutputPort == NULL))
        return ;

    if (pOutputPort->supportFormat != NULL) {
        OMX_BOOL ret = OMX_FALSE;
        int nLastIndex = OUTPUT_PORT_SUPPORTFORMAT_DEFAULT_NUM;
        int i;

        /* Default supported formats                                                                  */
        /* Customer wants OMX_COLOR_FormatYUV420SemiPlanar in the default colors format.              */
        /* But, Google wants OMX_COLOR_FormatYUV420Planar in the default colors format.               */
        /* Google's Videoeditor uses OMX_COLOR_FormatYUV420Planar(YV12) in the default colors format. */
        /* Therefore, only when you load the OpenMAX IL component by the customer name,               */
        /* to change the default OMX_COLOR_FormatYUV420SemiPlanar color format.                       */
        /* It is determined by case-sensitive.                                                        */
        if (IS_CUSTOM_COMPONENT(pExynosComponent->componentName) != OMX_TRUE) {
            /* Google GED & S.LSI Component, for video editor*/
            pOutputPort->supportFormat[0] = OMX_COLOR_FormatYUV420Planar;
            pOutputPort->supportFormat[1] = OMX_COLOR_FormatYUV420SemiPlanar;
        } else {
            /* Customer Component*/
            pOutputPort->supportFormat[0] = OMX_COLOR_FormatYUV420SemiPlanar;
            pOutputPort->supportFormat[1] = OMX_COLOR_FormatYUV420Planar;
        }
        pOutputPort->supportFormat[2] = OMX_COLOR_Format32bitARGB8888;

        /* add extra formats, if It is supported by H/W. (CSC doesn't exist) */
        /* OMX_SEC_COLOR_FormatNV12Tiled */
        ret = pVideoDec->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled);
        if (ret == OMX_TRUE)
            pOutputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled;

        /* OMX_SEC_COLOR_FormatYVU420Planar */
        ret = pVideoDec->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar);
        if (ret == OMX_TRUE)
            pOutputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar;

        /* OMX_SEC_COLOR_FormatNV21Linear */
        ret = pVideoDec->exynos_codec_checkFormatSupport(pExynosComponent, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear);
        if (ret == OMX_TRUE)
            pOutputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear;

        /* OMX_COLOR_FormatYUV420Planar16 */
        ret = pVideoDec->exynos_codec_checkFormatSupport(pExynosComponent, (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16);
        if (ret == OMX_TRUE)
            pOutputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16;

        /* OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC */
        ret = pVideoDec->exynos_codec_checkFormatSupport(pExynosComponent, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC);
        if (ret == OMX_TRUE)
            pOutputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC;

        for (i = 0; i < nLastIndex; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] Supported Format[%d] : 0x%x",
                                                pExynosComponent, __FUNCTION__, i, pOutputPort->supportFormat[i]);
        }

        pOutputPort->supportFormat[nLastIndex] = OMX_COLOR_FormatUnused;
    }

    return ;
}

void Exynos_Free_CodecBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    CODEC_DEC_BUFFER               **ppCodecBuffer      = NULL;

    int nBufferCnt = 0, nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer = &(pVideoDec->pMFCDecInputBuffer[0]);
        nBufferCnt = MFC_INPUT_BUFFER_NUM_MAX;
    } else {
        ppCodecBuffer = &(pVideoDec->pMFCDecOutputBuffer[0]);
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
                    Exynos_OSAL_SharedMemory_Free(pVideoDec->hSharedMemory, ppCodecBuffer[i]->pVirAddr[j]);
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
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    MEMORY_TYPE                      eMemoryType        = CACHED_MEMORY;
    CODEC_DEC_BUFFER               **ppCodecBuffer      = NULL;

    int nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer = &(pVideoDec->pMFCDecInputBuffer[0]);
    } else {
        ppCodecBuffer = &(pVideoDec->pMFCDecOutputBuffer[0]);
#ifdef USE_CSC_HW
        eMemoryType = NORMAL_MEMORY;
#endif
    }
    nPlaneCnt = Exynos_GetPlaneFromPort(&pExynosComponent->pExynosPort[nPortIndex]);

    if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)
        eMemoryType = SECURE_MEMORY;

    for (i = 0; i < nBufferCnt; i++) {
        ppCodecBuffer[i] = (CODEC_DEC_BUFFER *)Exynos_OSAL_Malloc(sizeof(CODEC_DEC_BUFFER));
        if (ppCodecBuffer[i] == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(ppCodecBuffer[i], 0, sizeof(CODEC_DEC_BUFFER));

        for (j = 0; j < nPlaneCnt; j++) {
            ppCodecBuffer[i]->pVirAddr[j] =
                (void *)Exynos_OSAL_SharedMemory_Alloc(pVideoDec->hSharedMemory, nAllocLen[j], eMemoryType);
            if (ppCodecBuffer[i]->pVirAddr[j] == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Alloc for %s codec buffer",
                                            pExynosComponent, __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            ppCodecBuffer[i]->fd[j] =
                Exynos_OSAL_SharedMemory_VirtToION(pVideoDec->hSharedMemory, ppCodecBuffer[i]->pVirAddr[j]);
            ppCodecBuffer[i]->bufferSize[j] = nAllocLen[j];
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s codec buffer[%d][%d] : %d",
                                        pExynosComponent, __FUNCTION__,
                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                        i, j, ppCodecBuffer[i]->fd[j]);
        }

        ppCodecBuffer[i]->dataSize = 0;
    }

    FunctionOut();

    return OMX_ErrorNone;

EXIT:
    Exynos_Free_CodecBuffers(pOMXComponent, nPortIndex);

    FunctionOut();

    return ret;
}

void Exynos_SetReorderTimestamp(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_U32                     *nIndex,
    OMX_TICKS                    timeStamp,
    OMX_U32                      nFlags) {

    int i;

    FunctionIn();

    if ((pExynosComponent == NULL) || (nIndex == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    /* find a empty slot */
    for (i = 0; i < MAX_TIMESTAMP; i++) {
        if (pExynosComponent->bTimestampSlotUsed[*nIndex] == OMX_FALSE)
            break;

        (*nIndex)++;
        (*nIndex) %= MAX_TIMESTAMP;
    }

    if (i >= MAX_TIMESTAMP)
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Can not find empty slot of timestamp. Timestamp slot is full.",
                                            pExynosComponent, __FUNCTION__);

    pExynosComponent->timeStamp[*nIndex]          = timeStamp;
    pExynosComponent->nFlags[*nIndex]             = nFlags;
    pExynosComponent->bTimestampSlotUsed[*nIndex] = OMX_TRUE;

EXIT:
    FunctionOut();

    return;
}

void Exynos_GetReorderTimestamp(
    EXYNOS_OMX_BASECOMPONENT            *pExynosComponent,
    EXYNOS_OMX_CURRENT_FRAME_TIMESTAMP  *sCurrentTimestamp,
    OMX_S32                              nFrameIndex,
    OMX_S32                              eFrameType) {

    EXYNOS_OMX_BASEPORT         *pExynosOutputPort  = NULL;
    int i = 0;

    FunctionIn();

    if ((pExynosComponent == NULL) || (sCurrentTimestamp == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    Exynos_OSAL_Memset(sCurrentTimestamp, 0, sizeof(EXYNOS_OMX_CURRENT_FRAME_TIMESTAMP));
    sCurrentTimestamp->timeStamp = DEFAULT_TIMESTAMP_VAL;

    for (i = 0; i < MAX_TIMESTAMP; i++) {
        /* NOTE: In case of CODECCONFIG, no return any frame */
        if ((pExynosComponent->bTimestampSlotUsed[i] == OMX_TRUE) &&
            (pExynosComponent->nFlags[i] != (OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME))) {

            /* NOTE: In case of EOS, timestamp is not valid */
            if ((sCurrentTimestamp->timeStamp == DEFAULT_TIMESTAMP_VAL) ||
                ((sCurrentTimestamp->timeStamp > pExynosComponent->timeStamp[i]) &&
                    (((pExynosComponent->nFlags[i] & OMX_BUFFERFLAG_EOS) != OMX_BUFFERFLAG_EOS) ||
                     (pExynosComponent->bBehaviorEOS == OMX_TRUE))) ||
                ((sCurrentTimestamp->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
                sCurrentTimestamp->timeStamp = pExynosComponent->timeStamp[i];
                sCurrentTimestamp->nFlags    = pExynosComponent->nFlags[i];
                sCurrentTimestamp->nIndex    = i;
            }
        }
    }

    if (sCurrentTimestamp->timeStamp == DEFAULT_TIMESTAMP_VAL)
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] could not find a valid timestamp", pExynosComponent, __FUNCTION__);

    /* PTS : all index is same as tag */
    /* DTS : only in case of I-Frame, the index is same as tag */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] disp_pic_frame_type: %d", pExynosComponent, __FUNCTION__, eFrameType);
    if ((ExynosVideoFrameType)eFrameType & VIDEO_FRAME_I) {
        /* Timestamp is weird */
        if (sCurrentTimestamp->nIndex != nFrameIndex) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Timestamp is not same in spite of I-Frame", pExynosComponent, __FUNCTION__);

            /* trust a tag index returned from D/D */
            sCurrentTimestamp->timeStamp = pExynosComponent->timeStamp[nFrameIndex];
            sCurrentTimestamp->nFlags    = pExynosComponent->nFlags[nFrameIndex];
            sCurrentTimestamp->nIndex    = nFrameIndex;

            /* delete past timestamps */
            for(i = 0; i < MAX_TIMESTAMP; i++) {
                if ((pExynosComponent->bTimestampSlotUsed[i] == OMX_TRUE) &&
                    ((sCurrentTimestamp->timeStamp > pExynosComponent->timeStamp[i]) &&
                        ((pExynosComponent->nFlags[i] & OMX_BUFFERFLAG_EOS) != OMX_BUFFERFLAG_EOS))) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] clear an past timestamp %lld us (%.2f secs)",
                                                            pExynosComponent, __FUNCTION__,
                                                            pExynosComponent->timeStamp[i], (double)(pExynosComponent->timeStamp[i] / 1E6));
                    pExynosComponent->nFlags[i]             = 0x00;
                    pExynosComponent->bTimestampSlotUsed[i] = OMX_FALSE;
                }

                if ((pExynosComponent->bTimestampSlotUsed[i] == OMX_FALSE) &&
                    (sCurrentTimestamp->timeStamp < pExynosComponent->timeStamp[i])) {
                    pExynosComponent->bTimestampSlotUsed[i] = OMX_TRUE;
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] revive an past timestamp %lld us (%.2f secs) by I-frame sync",
                                                            pExynosComponent, __FUNCTION__,
                                                            pExynosComponent->timeStamp[i], (double)(pExynosComponent->timeStamp[i] / 1E6));
                }
            }
        }

        if (sCurrentTimestamp->timeStamp == DEFAULT_TIMESTAMP_VAL)
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] the index of frame(%d) about I-frame is wrong",
                                                pExynosComponent, __FUNCTION__, nFrameIndex);

        sCurrentTimestamp->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
    }

    if (sCurrentTimestamp->timeStamp != DEFAULT_TIMESTAMP_VAL) {
        if (pExynosOutputPort->latestTimeStamp <= sCurrentTimestamp->timeStamp) {
            pExynosOutputPort->latestTimeStamp = sCurrentTimestamp->timeStamp;
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] timestamp(%lld) is smaller than latest timeStamp(%lld), uses latestTimeStamp",
                                pExynosComponent, __FUNCTION__,
                                sCurrentTimestamp->timeStamp, pExynosOutputPort->latestTimeStamp);
            sCurrentTimestamp->timeStamp = pExynosOutputPort->latestTimeStamp;
        }
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] can't find a valid timestamp, uses latestTimeStamp(%lld)",
                                pExynosComponent, __FUNCTION__, pExynosOutputPort->latestTimeStamp);
        sCurrentTimestamp->timeStamp = pExynosOutputPort->latestTimeStamp;
    }

EXIT:
    FunctionOut();

    return;
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

    int i = 0;

    FunctionIn();

    /* Input port */
    pInputPort->portDefinition.format.video.nFrameWidth             = DEFAULT_FRAME_WIDTH;
    pInputPort->portDefinition.format.video.nFrameHeight            = DEFAULT_FRAME_HEIGHT;
    pInputPort->portDefinition.format.video.nStride                 = 0; /*DEFAULT_FRAME_WIDTH;*/
    pInputPort->portDefinition.format.video.nSliceHeight            = 0;
    pInputPort->portDefinition.format.video.pNativeRender           = 0;
    pInputPort->portDefinition.format.video.bFlagErrorConcealment   = OMX_FALSE;
    pInputPort->portDefinition.format.video.eColorFormat            = OMX_COLOR_FormatUnused;

    pInputPort->portDefinition.nBufferSize  = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pInputPort->portDefinition.bEnabled     = OMX_TRUE;

    pInputPort->bufferProcessType   = BUFFER_SHARE;
    pOutputPort->eMetaDataType      = METADATA_TYPE_DISABLED;

    pInputPort->portWayType         = WAY2_PORT;
    Exynos_SetPlaneToPort(pInputPort, MFC_DEFAULT_INPUT_BUFFER_PLANE);

    for (i = 0; i < IMG_CROP_PORT_MAX; i++) {
        pInputPort->cropRectangle[i].nTop     = 0;
        pInputPort->cropRectangle[i].nLeft    = 0;
        pInputPort->cropRectangle[i].nWidth   = DEFAULT_FRAME_WIDTH;
        pInputPort->cropRectangle[i].nHeight  = DEFAULT_FRAME_HEIGHT;
    }
    Exynos_ResetImgCropInfo(pOMXComponent, INPUT_PORT_INDEX);

    /* Output port */
    pOutputPort->portDefinition.format.video.nFrameWidth            = DEFAULT_FRAME_WIDTH;
    pOutputPort->portDefinition.format.video.nFrameHeight           = DEFAULT_FRAME_HEIGHT;
    pOutputPort->portDefinition.format.video.nStride                = 0; /*DEFAULT_FRAME_WIDTH;*/
    pOutputPort->portDefinition.format.video.nSliceHeight           = 0;
    pOutputPort->portDefinition.format.video.pNativeRender          = 0;
    pOutputPort->portDefinition.format.video.bFlagErrorConcealment  = OMX_FALSE;
    pOutputPort->portDefinition.format.video.eColorFormat           = OMX_COLOR_FormatYUV420Planar;

    pOutputPort->portDefinition.nBufferCountActual  = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pOutputPort->portDefinition.nBufferCountMin     = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pOutputPort->portDefinition.nBufferSize  = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pOutputPort->portDefinition.bEnabled     = OMX_TRUE;

    pOutputPort->bufferProcessType  = BUFFER_COPY;
    pOutputPort->eMetaDataType      = METADATA_TYPE_DISABLED;

    pOutputPort->portWayType        = WAY2_PORT;
    pOutputPort->latestTimeStamp    = DEFAULT_TIMESTAMP_VAL;
    Exynos_SetPlaneToPort(pOutputPort, Exynos_OSAL_GetPlaneCount(OMX_COLOR_FormatYUV420Planar, pOutputPort->ePlaneType));

    for (i = 0; i < IMG_CROP_PORT_MAX; i++) {
        pOutputPort->cropRectangle[i].nTop     = 0;
        pOutputPort->cropRectangle[i].nLeft    = 0;
        pOutputPort->cropRectangle[i].nWidth   = DEFAULT_FRAME_WIDTH;
        pOutputPort->cropRectangle[i].nHeight  = DEFAULT_FRAME_HEIGHT;
    }
    Exynos_ResetImgCropInfo(pOMXComponent, OUTPUT_PORT_INDEX);

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferToData(
    CODEC_DEC_BUFFER    *pCodecBuffer,
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

    if (nPortIndex == INPUT_PORT_INDEX) {
        pData->dataLen       = pCodecBuffer->dataSize;
        pData->remainDataLen = pCodecBuffer->dataSize;
    } else {
        pData->dataLen       = 0;
        pData->remainDataLen = 0;
    }

EXIT:
    return ret;
}

void Exynos_Wait_ProcessPause(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nPortIndex)
{
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosOMXPort  = &pExynosComponent->pExynosPort[nPortIndex];

    FunctionIn();

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
        if (pVideoDec->bExitBufferProcessThread)
            goto EXIT;
        Exynos_OSAL_SignalReset(pExynosComponent->pExynosPort[nPortIndex].pauseEvent);
    }

EXIT:
    FunctionOut();

    return;
}

OMX_BOOL Exynos_CSC_OutputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstOutputData)
{
    OMX_BOOL                       ret              = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec        = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);
    EXYNOS_OMX_DATABUFFER         *pOutputUseBuffer = &(pOutputPort->way.port2WayDataBuffer.outputDataBuffer);
    DECODE_CODEC_EXTRA_BUFFERINFO *pExtBufferInfo   = (DECODE_CODEC_EXTRA_BUFFERINFO *)pDstOutputData->extInfo;
    OMX_COLOR_FORMATTYPE           eColorFormat     = pOutputPort->portDefinition.format.video.eColorFormat;

    void *pOutputBuf                = (void *)pOutputUseBuffer->bufferHeader->pBuffer;
    void *pSrcBuf[MAX_BUFFER_PLANE] = { NULL, };
    void *pDstBuf[MAX_BUFFER_PLANE] = { NULL, };

    CSC_MEMTYPE     csc_memType       = CSC_MEMORY_USERPTR;
    CSC_METHOD      csc_method        = CSC_METHOD_SW;
    CSC_ERRORCODE   csc_ret           = CSC_ErrorNone;
    unsigned int    csc_src_cacheable = 1;
    unsigned int    csc_dst_cacheable = 1;

    EXYNOS_OMX_IMG_INFO srcImgInfo, dstImgInfo;

    FunctionIn();

    /* choose csc method */
    csc_get_method(pVideoDec->csc_handle, &csc_method);
    if (csc_method == CSC_METHOD_SW) {
        if (((pVideoDec->eDataType == DATA_TYPE_10BIT) &&
            (eColorFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16)) ||
            ((pVideoDec->eDataType == DATA_TYPE_8BIT_SBWC) ||
             (pVideoDec->eDataType == DATA_TYPE_10BIT_SBWC))) {
            csc_memType = CSC_MEMORY_DMABUF;
            csc_src_cacheable = 0;
            csc_dst_cacheable = 0;

            csc_method = CSC_METHOD_HW;
            csc_set_method(pVideoDec->csc_handle, csc_method);
        } else if (pVideoDec->exynos_codec_checkFormatSupport(pExynosComponent, eColorFormat) == OMX_TRUE) {
            /* only remove stride value */
            csc_memType = CSC_MEMORY_MFC;
        }
    } else {
        csc_memType = CSC_MEMORY_DMABUF;
        csc_src_cacheable = 0;
        csc_dst_cacheable = 0;
    }

    /******************/
    /* get image info */
    /******************/
    /* 1. SRC : MFC OUTPUT */
    srcImgInfo.nStride      = pExtBufferInfo->imageStride;
    srcImgInfo.nSliceHeight = pExtBufferInfo->imageHeight;
    srcImgInfo.nImageWidth  = pExtBufferInfo->imageWidth;
    srcImgInfo.nImageHeight = pExtBufferInfo->imageHeight;

    if (pOutputPort->bUseImgCrop[IMG_CROP_INPUT_PORT] == OMX_FALSE) {
        /* read original image fully except padding data */
        srcImgInfo.nLeft   = 0;
        srcImgInfo.nTop    = 0;
        srcImgInfo.nWidth  = pExtBufferInfo->imageWidth;
        srcImgInfo.nHeight = pExtBufferInfo->imageHeight;
    } else {
        /* crop an original image */
        srcImgInfo.nLeft   = pOutputPort->cropRectangle[IMG_CROP_INPUT_PORT].nLeft;
        srcImgInfo.nTop    = pOutputPort->cropRectangle[IMG_CROP_INPUT_PORT].nTop;
        srcImgInfo.nWidth  = pOutputPort->cropRectangle[IMG_CROP_INPUT_PORT].nWidth;
        srcImgInfo.nHeight = pOutputPort->cropRectangle[IMG_CROP_INPUT_PORT].nHeight;
    }

    /* 2. DST : OMX OUTPUT */
    dstImgInfo.nStride      = pOutputPort->portDefinition.format.video.nFrameWidth;  // pExtBufferInfo->imageWidth??
    dstImgInfo.nSliceHeight = pOutputPort->portDefinition.format.video.nFrameHeight;  // pExtBufferInfo->imageHeight??
    dstImgInfo.nImageWidth  = pExtBufferInfo->imageWidth;
    dstImgInfo.nImageHeight = pExtBufferInfo->imageHeight;

    if (pOutputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_FALSE) {
        /* write image fully */
        dstImgInfo.nLeft        = 0;
        dstImgInfo.nTop         = 0;
        dstImgInfo.nWidth       = pExtBufferInfo->imageWidth;
        dstImgInfo.nHeight      = pExtBufferInfo->imageHeight;
    } else {
        /* use positioning and scaling */
        dstImgInfo.nLeft        = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nLeft;
        dstImgInfo.nTop         = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nTop;
        dstImgInfo.nWidth       = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
        dstImgInfo.nHeight      = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
    }

    /*******************/
    /* get buffer info */
    /*******************/
    /* 1. SRC : MFC OUTPUT */
    if (csc_method == CSC_METHOD_HW) {
        pSrcBuf[0] = (void *)pDstOutputData->buffer.fd[0];
        pSrcBuf[1] = (void *)pDstOutputData->buffer.fd[1];
        pSrcBuf[2] = (void *)pDstOutputData->buffer.fd[2];
    } else {
        if (pOutputPort->ePlaneType == PLANE_SINGLE) {
            /* single-FD. only Y addr is valid */
            int nPlaneCnt = Exynos_OSAL_GetPlaneCount(pExtBufferInfo->colorFormat, PLANE_MULTIPLE);

            pSrcBuf[0] = pDstOutputData->buffer.addr[0];

            switch (pVideoDec->eDataType) {
            case DATA_TYPE_10BIT:
                if (nPlaneCnt == 2) {  /* Semi-Planar : interleaved */
                    pSrcBuf[1] = (void *)(((char *)pSrcBuf[0]) + GET_UV_OFFSET((srcImgInfo.nImageWidth * 2), srcImgInfo.nImageHeight));
                } else if (nPlaneCnt == 3) {  /* Planar */
                    pSrcBuf[1] = (void *)(((char *)pSrcBuf[0]) + GET_CB_OFFSET((srcImgInfo.nImageWidth * 2), srcImgInfo.nImageHeight));
                    pSrcBuf[2] = (void *)(((char *)pSrcBuf[0]) + GET_CR_OFFSET((srcImgInfo.nImageWidth * 2), srcImgInfo.nImageHeight));
                }
                break;
            case DATA_TYPE_8BIT_WITH_2BIT:
                if (nPlaneCnt == 2) {  /* Semi-Planar : interleaved */
                    pSrcBuf[1] = (void *)(((char *)pSrcBuf[0]) + GET_10B_UV_OFFSET(srcImgInfo.nImageWidth, srcImgInfo.nImageHeight));
                } else if (nPlaneCnt == 3) {  /* Planar */
                    pSrcBuf[1] = (void *)(((char *)pSrcBuf[0]) + GET_10B_CB_OFFSET(srcImgInfo.nImageWidth, srcImgInfo.nImageHeight));
                    pSrcBuf[2] = (void *)(((char *)pSrcBuf[0]) + GET_10B_CR_OFFSET(srcImgInfo.nImageWidth, srcImgInfo.nImageHeight));
                }
                break;
            default:
                if (nPlaneCnt == 2) {  /* Semi-Planar : interleaved */
                    pSrcBuf[1] = (void *)(((char *)pSrcBuf[0]) + GET_UV_OFFSET(srcImgInfo.nImageWidth, srcImgInfo.nImageHeight));
                } else if (nPlaneCnt == 3) {  /* Planar */
                    pSrcBuf[1] = (void *)(((char *)pSrcBuf[0]) + GET_CB_OFFSET(srcImgInfo.nImageWidth, srcImgInfo.nImageHeight));
                    pSrcBuf[2] = (void *)(((char *)pSrcBuf[0]) + GET_CR_OFFSET(srcImgInfo.nImageWidth, srcImgInfo.nImageHeight));
                }
                break;
            }
        } else {
            /* multi-FD */
            pSrcBuf[0] = pDstOutputData->buffer.addr[0];
            pSrcBuf[1] = pDstOutputData->buffer.addr[1];
            pSrcBuf[2] = pDstOutputData->buffer.addr[2];
        }
    }

    /* 2. DST : OMX OUTPUT */
    if (pOutputPort->eMetaDataType != METADATA_TYPE_DISABLED) {
        /* 1. meta data is enabled (surface buffer)
         *    1) format is not supported at HW codec
         *       needs CSC(Color-Space-Conversion).
         */
        if (pOutputPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK) {
            /* graphic buffer */
            EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;

            OMX_ERRORTYPE         err = OMX_ErrorNone;
            EXYNOS_OMX_LOCK_RANGE range;
            OMX_U32               stride = 0;

            range.nWidth        = dstImgInfo.nStride;
            range.nHeight       = dstImgInfo.nSliceHeight;
            range.eColorFormat  = eColorFormat;

            err = Exynos_OSAL_LockMetaData(pOutputBuf, range, &stride, &bufferInfo, pOutputPort->eMetaDataType);
            if (err != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to Exynos_OSAL_LockMetaData (err:0x%x)",
                                                    pExynosComponent, __FUNCTION__, err);
                ret = OMX_FALSE;
                goto EXIT;
            }

            dstImgInfo.nStride = stride;

            /* update extra information to vendor path for renderer */
            {
                ExynosVideoMeta *pMeta = (ExynosVideoMeta *)bufferInfo.addr[2];

                if (pVideoDec->exynos_codec_updateExtraInfo != NULL)
                    pVideoDec->exynos_codec_updateExtraInfo(pOMXComponent, pMeta);
            }

            if (csc_method == CSC_METHOD_HW) {
                pDstBuf[0]  = (void *)bufferInfo.fd[0];
                pDstBuf[1]  = (void *)bufferInfo.fd[1];
                pDstBuf[2]  = (void *)bufferInfo.fd[2];
            } else {
                pDstBuf[0]  = (void *)bufferInfo.addr[0];
                pDstBuf[1]  = (void *)bufferInfo.addr[1];
                pDstBuf[2]  = (void *)bufferInfo.addr[2];
            }
        } else {
            /*************/
            /*    TBD    */
            /*************/
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: OMX_ErrorNotImplemented(%d)", pExynosComponent, __FUNCTION__, __LINE__);
            /* OMX_ErrorNotImplemented */
            ret = OMX_FALSE;
            goto EXIT;
        }
    } else {
        /* 2. meta data is not enabled (user application format)
         *    1) user application couldn't handle non-continuous data
         *    2) format is not supported at HW codec
         *       needs CSC(Color-Space-Conversion).
         */

        if (csc_method == CSC_METHOD_HW) {
            pDstBuf[0] = (void *)Exynos_OSAL_SharedMemory_VirtToION(pVideoDec->hSharedMemory, pOutputBuf);
            pDstBuf[1] = NULL;
            pDstBuf[2] = NULL;
        } else {
            unsigned int nAllocLen[MAX_BUFFER_PLANE] = { 0, 0, 0 };
            unsigned int nDataLen[MAX_BUFFER_PLANE]  = { 0, 0, 0 };

            Exynos_OSAL_GetPlaneSize(eColorFormat, PLANE_SINGLE_USER, dstImgInfo.nStride, dstImgInfo.nSliceHeight, nDataLen, nAllocLen);

            pDstBuf[0]  = (void *)((char *)pOutputBuf);
            pDstBuf[1]  = (void *)((char *)pOutputBuf + nDataLen[0]);
            pDstBuf[2]  = (void *)((char *)pOutputBuf + nDataLen[0] + nDataLen[1]);
        }
    }

    /**************************/
    /* [CSC] setup image info */
    /**************************/
    if (pVideoDec->csc_set_format == OMX_FALSE) {
        /********************/
        /* get color format */
        /********************/
        unsigned int csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat(pExtBufferInfo->colorFormat, pOutputPort->ePlaneType);
        unsigned int csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat(eColorFormat, PLANE_SINGLE_USER);

        if (pOutputPort->eMetaDataType != METADATA_TYPE_DISABLED)
            csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat(eColorFormat, pOutputPort->ePlaneType);

        csc_set_src_format(
            pVideoDec->csc_handle,   /* handle */
            srcImgInfo.nStride,      /* width */
            srcImgInfo.nSliceHeight, /* height */
            srcImgInfo.nLeft,        /* crop_left */
            srcImgInfo.nTop,         /* crop_top */
            srcImgInfo.nWidth,       /* crop_width */
            srcImgInfo.nHeight,      /* crop_height */
            csc_src_color_format,    /* color_format */
            csc_src_cacheable);      /* cacheable */

        csc_set_dst_format(
            pVideoDec->csc_handle,   /* handle */
            dstImgInfo.nStride,      /* width */
            dstImgInfo.nSliceHeight, /* height */
            dstImgInfo.nLeft,        /* crop_left */
            dstImgInfo.nTop,         /* crop_top */
            dstImgInfo.nWidth,       /* crop_width */
            dstImgInfo.nHeight,      /* crop_height */
            csc_dst_color_format,    /* color_format */
            csc_dst_cacheable);      /* cacheable */

        csc_set_eq_property(
            pVideoDec->csc_handle,         /* handle */
            CSC_EQ_MODE_USER,              /* user select */
            CSC_EQ_RANGE_NARROW,           /* narrow */
            CSC_EQ_COLORSPACE_SMPTE170M);  /* bt.601 */

        pVideoDec->csc_set_format = OMX_TRUE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s CSC(%x) / [SRC] resol(%dx%d), rect(%d/%d/%d/%d), format(0x%x) -> [DST] resol(%dx%d), rect(%d/%d/%d/%d), format(0x%x)",
                                               pExynosComponent, __FUNCTION__,
                                               (csc_method == CSC_METHOD_SW)? "SW":"HW", csc_memType,
                                               srcImgInfo.nStride, srcImgInfo.nSliceHeight,
                                               srcImgInfo.nLeft, srcImgInfo.nTop, srcImgInfo.nWidth, srcImgInfo.nHeight, csc_src_color_format,
                                               dstImgInfo.nStride, dstImgInfo.nSliceHeight,
                                               dstImgInfo.nLeft, dstImgInfo.nTop, dstImgInfo.nWidth, dstImgInfo.nHeight, csc_dst_color_format);
    }

    /***************************/
    /* [CSC] setup buffer info */
    /***************************/
    csc_set_src_buffer(
        pVideoDec->csc_handle,  /* handle */
        pSrcBuf,
        csc_memType);
    csc_set_dst_buffer(
        pVideoDec->csc_handle,  /* handle */
        pDstBuf,
        csc_memType);

    /*****************/
    /* [CSC] convert */
    /*****************/
    csc_ret = csc_convert(pVideoDec->csc_handle);
    ret = (csc_ret != CSC_ErrorNone)? OMX_FALSE:OMX_TRUE;

    if (pOutputPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK)
        Exynos_OSAL_UnlockMetaData(pOutputBuf, pOutputPort->eMetaDataType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_BOOL Exynos_Preprocessor_InputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *srcInputData)
{
    OMX_BOOL                         ret                = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosInputPort    = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *inputUseBuffer     = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;

    OMX_BYTE pInputStream = NULL;
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

            if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) {
                unsigned long ionFD = 0;
                OMX_PTR dataBuffer = NULL;

                if (exynosInputPort->eMetaDataType == METADATA_TYPE_HANDLE) {
                    if (srcInputData->buffer.fd[0] == 0) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] FD is invalid at %p",
                                                         pExynosComponent, __FUNCTION__, inputUseBuffer->bufferHeader);
                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);
                        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                pExynosComponent->callbackData,
                                                                OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                        ret = OMX_FALSE;
                        goto EXIT;
                    }

                    ionFD = srcInputData->buffer.fd[0];
                } else {
                    /* backward compatibility */
                    ionFD = (unsigned long)srcInputData->buffer.addr[0];
                }

                dataBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoDec->hSharedMemory, ionFD);
                if (dataBuffer == NULL) {
                    ret = OMX_FALSE;
                    goto EXIT;
                }

                srcInputData->buffer.fd[0]   = ionFD;
                srcInputData->buffer.addr[0] = dataBuffer;
            }

            if (exynosInputPort->eMetaDataType == METADATA_TYPE_DISABLED) {
                srcInputData->buffer.fd[0] =
                    Exynos_OSAL_SharedMemory_VirtToION(pVideoDec->hSharedMemory,
                                srcInputData->buffer.addr[0]);
            }

            /* reset dataBuffer */
            Exynos_ResetDataBuffer(inputUseBuffer);

            ret = OMX_TRUE;
        } else if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
            pInputStream = inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
            copySize = inputUseBuffer->remainDataLen;

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
                Exynos_OSAL_Memcpy((OMX_PTR)((char *)srcInputData->buffer.addr[0] + srcInputData->dataLen),
                                   pInputStream, copySize);
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
            EXYNOS_OMX_BASEPORT *pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

            pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_TRUE;
            pExynosComponent->checkTimeStamp.startTimeStamp = srcInputData->timeStamp;
            pOutputPort->latestTimeStamp = srcInputData->timeStamp;  // for reordering timestamp mode
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
    OMX_BOOL                         ret              = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec        = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *outputUseBuffer  = &exynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    DECODE_CODEC_EXTRA_BUFFERINFO   *pBufferInfo      = (DECODE_CODEC_EXTRA_BUFFERINFO *)dstOutputData->extInfo;

    FunctionIn();

    if (exynosOutputPort->bufferProcessType == BUFFER_SHARE) {
        if (Exynos_Shared_DataToBuffer(exynosOutputPort, outputUseBuffer, dstOutputData) == OMX_ErrorNone) {
            outputUseBuffer->dataValid = OMX_TRUE;
        } else {
            ret = OMX_FALSE;
            goto EXIT;
        }
    }

    if (outputUseBuffer->dataValid == OMX_TRUE) {
        if ((pExynosComponent->checkTimeStamp.needCheckStartTimeStamp == OMX_TRUE) &&
            ((dstOutputData->nFlags & OMX_BUFFERFLAG_EOS) != OMX_BUFFERFLAG_EOS)) {
            unsigned int discardFlags = (OMX_BUFFERFLAG_SYNCFRAME | OMX_BUFFERFLAG_DATACORRUPT | OMX_BUFFERFLAG_ENDOFFRAME);

            if ((pExynosComponent->checkTimeStamp.startTimeStamp == dstOutputData->timeStamp) &&
                ((pExynosComponent->checkTimeStamp.nStartFlags & ~discardFlags) ==
                 (dstOutputData->nFlags & ~discardFlags))) {
                pExynosComponent->checkTimeStamp.startTimeStamp = RESET_TIMESTAMP_VAL;
                pExynosComponent->checkTimeStamp.nStartFlags = 0x0;
                pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
                pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            } else {
                if (pExynosComponent->checkTimeStamp.startTimeStamp < dstOutputData->timeStamp) {
                    pExynosComponent->checkTimeStamp.startTimeStamp = RESET_TIMESTAMP_VAL;
                    pExynosComponent->checkTimeStamp.nStartFlags = 0x0;
                    pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
                    pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
                } else {
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] drop frame after seeking(in: %lld vs out: %lld)",
                                                            pExynosComponent, __FUNCTION__,
                                                            pExynosComponent->checkTimeStamp.startTimeStamp, dstOutputData->timeStamp);
                    if (exynosOutputPort->bufferProcessType == BUFFER_SHARE)
                        Exynos_OMX_FillThisBufferAgain(pOMXComponent, outputUseBuffer->bufferHeader);

                    ret = OMX_TRUE;
                    goto EXIT;
                }
            }
        } else if (pExynosComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) {
            ret = OMX_TRUE;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] not set check timestamp after seeking", pExynosComponent, __FUNCTION__);

            if (exynosOutputPort->bufferProcessType == BUFFER_SHARE)
                Exynos_OMX_FillThisBufferAgain(pOMXComponent, outputUseBuffer->bufferHeader);

            goto EXIT;
        }

        if (exynosOutputPort->eMetaDataType == METADATA_TYPE_GRAPHIC)
            Exynos_OSAL_UpdateDataspaceToGraphicMeta(outputUseBuffer->bufferHeader->pBuffer, exynosOutputPort->ColorAspects.nDataSpace);

        if (exynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
            if (exynosOutputPort->eMetaDataType & METADATA_TYPE_DATA)
                dstOutputData->remainDataLen = outputUseBuffer->allocSize;

            if (((exynosOutputPort->eMetaDataType & METADATA_TYPE_DATA) ||
                 (dstOutputData->remainDataLen <= (outputUseBuffer->allocSize - outputUseBuffer->dataLen))) &&
                (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {

                if (dstOutputData->remainDataLen > 0) {
                    ret = Exynos_CSC_OutputData(pOMXComponent, dstOutputData);
                } else {
                    ret = OMX_TRUE;
                }

                if (ret == OMX_TRUE) {
                    outputUseBuffer->dataLen       += dstOutputData->remainDataLen;
                    outputUseBuffer->remainDataLen += dstOutputData->remainDataLen;
                    outputUseBuffer->nFlags         = dstOutputData->nFlags;
                    outputUseBuffer->timeStamp      = dstOutputData->timeStamp;

                    if ((outputUseBuffer->remainDataLen > 0) ||
                        (outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) ||
                        (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
#ifdef USE_ANDROID
                        if ((pVideoDec->hImgConv != NULL) &&
                            (outputUseBuffer->remainDataLen > 0)) {
                            if (Exynos_OSAL_ImgConv_Run(pVideoDec->hImgConv, pOMXComponent,
                                                        (OMX_PTR)outputUseBuffer->bufferHeader->pBuffer,
                                                        (OMX_PTR)&(pBufferInfo->HDRDynamic)) == OMX_ErrorNone) {
                                outputUseBuffer->nFlags |= OMX_BUFFERFLAG_CONVERTEDIMAGE;
                            }
                        }
#endif
                        Exynos_OutputBufferReturn(pOMXComponent, outputUseBuffer);
                    }
                } else {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to CSC", pExynosComponent, __FUNCTION__);
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);
                    pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                            pExynosComponent->callbackData,
                                                            OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                    ret = OMX_FALSE;
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
        } else if (exynosOutputPort->bufferProcessType == BUFFER_SHARE) {
            if ((outputUseBuffer->remainDataLen > 0) ||
                (outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) ||
                (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
#ifdef USE_ANDROID
                if ((pVideoDec->hImgConv != NULL) &&
                    (outputUseBuffer->remainDataLen > 0)) {
                    if (Exynos_OSAL_ImgConv_Run(pVideoDec->hImgConv, pOMXComponent,
                                                (OMX_PTR)outputUseBuffer->bufferHeader->pBuffer,
                                                (OMX_PTR)&(pBufferInfo->HDRDynamic)) == OMX_ErrorNone) {
                        outputUseBuffer->nFlags |= OMX_BUFFERFLAG_CONVERTEDIMAGE;
                    }
                }
#endif
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

OMX_ERRORTYPE Exynos_OMX_SrcInputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosInputPort    = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT             *exynosOutputPort   = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *srcInputUseBuffer  = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;
    EXYNOS_OMX_DATA                 *pSrcInputData      = &exynosInputPort->processData;

    OMX_BOOL bCheckInputData = OMX_FALSE;

    FunctionIn();

    while (!pVideoDec->bExitBufferProcessThread) {
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
               (!pVideoDec->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if ((CHECK_PORT_BEING_FLUSHED(exynosInputPort)) ||
                (exynosOutputPort->exceptionFlag == INVALID_STATE))
                break;

            if (((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorInputDataDecodeYet) ||
                ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorNoneSrcSetupFinish)) {
                if (exynosOutputPort->exceptionFlag != GENERAL_STATE)
                    break;
            }

            Exynos_OSAL_MutexLock(srcInputUseBuffer->bufferMutex);
            if ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorInputDataDecodeYet) {
                if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                    OMX_PTR codecBuffer;
                    if ((pSrcInputData->buffer.addr[0] == NULL) ||
                        (pSrcInputData->pPrivate == NULL)) {
                        Exynos_CodecBufferDeQueue(pExynosComponent, INPUT_PORT_INDEX, &codecBuffer);
                        if (pVideoDec->bExitBufferProcessThread) {
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

                if ((bCheckInputData == OMX_FALSE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosInputPort))) {
                    ret = Exynos_InputBufferGetQueue(pExynosComponent);
                    if (pVideoDec->bExitBufferProcessThread) {
                        Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                        goto EXIT;
                    }

                    Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                    break;
                }

                if (CHECK_PORT_BEING_FLUSHED(exynosInputPort)) {
                    Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                    break;
                }
            }

            /* if input flush is occured before obtaining bufferMutex,
             * bufferHeader can be NULL.
             */
            if (pSrcInputData->bufferHeader == NULL) {
                ret = OMX_ErrorNone;
                Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                break;
            }

            ret = pVideoDec->exynos_codec_srcInputProcess(pOMXComponent, pSrcInputData);
            if (((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorCorruptedFrame) ||
                ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorCorruptedHeader)) {
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input data is weird(0x%x)",
                                                        pExynosComponent, __FUNCTION__, ret);
                if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                    OMX_PTR codecBuffer;
                    codecBuffer = pSrcInputData->pPrivate;
                    if (codecBuffer != NULL)
                        Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, codecBuffer);
                }

                if (exynosInputPort->bufferProcessType & BUFFER_SHARE) {
                    Exynos_OMX_InputBufferReturn(pOMXComponent, pSrcInputData->bufferHeader);
                }
            }

            if ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorInputDataDecodeYet) {
                Exynos_ResetCodecData(pSrcInputData);
            }

            Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);

            if ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorCodecInit)
                pVideoDec->bExitBufferProcessThread = OMX_TRUE;
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
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *srcOutputUseBuffer = &exynosInputPort->way.port2WayDataBuffer.outputDataBuffer;
    EXYNOS_OMX_DATA           srcOutputData;

    FunctionIn();

    Exynos_ResetCodecData(&srcOutputData);

    while (!pVideoDec->bExitBufferProcessThread) {
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

        while (!pVideoDec->bExitBufferProcessThread) {
            if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                if (Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX) == OMX_FALSE)
                    break;
            }
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosInputPort))
                break;

            Exynos_OSAL_MutexLock(srcOutputUseBuffer->bufferMutex);
            ret = pVideoDec->exynos_codec_srcOutputProcess(pOMXComponent, &srcOutputData);

            if (ret == OMX_ErrorNone) {
                if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                    OMX_PTR codecBuffer;
                    codecBuffer = srcOutputData.pPrivate;
                    if (codecBuffer != NULL)
                        Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, codecBuffer);
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
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *dstInputUseBuffer = &exynosOutputPort->way.port2WayDataBuffer.inputDataBuffer;
    EXYNOS_OMX_DATA           dstInputData;

    FunctionIn();

    Exynos_ResetCodecData(&dstInputData);

    while (!pVideoDec->bExitBufferProcessThread) {
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
               (!pVideoDec->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if ((CHECK_PORT_BEING_FLUSHED(exynosOutputPort)) ||
                (exynosOutputPort->exceptionFlag != GENERAL_STATE))
                break;

            Exynos_OSAL_MutexLock(dstInputUseBuffer->bufferMutex);
            if ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorOutputBufferUseYet) {
                if (exynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
                    CODEC_DEC_BUFFER *pCodecBuffer = NULL;
                    ret = Exynos_CodecBufferDeQueue(pExynosComponent, OUTPUT_PORT_INDEX, (OMX_PTR *)&pCodecBuffer);
                    if (pVideoDec->bExitBufferProcessThread) {
                        Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                        goto EXIT;
                    }

                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                        break;
                    }
                    Exynos_CodecBufferToData(pCodecBuffer, &dstInputData, OUTPUT_PORT_INDEX);
                }

                if (exynosOutputPort->bufferProcessType == BUFFER_SHARE) {
                    if ((dstInputUseBuffer->dataValid != OMX_TRUE) &&
                        (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                        ret = Exynos_OutputBufferGetQueue(pExynosComponent);
                        if (pVideoDec->bExitBufferProcessThread) {
                            Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                            goto EXIT;
                        }

                        if (ret != OMX_ErrorNone) {
                            Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                            break;
                        }

                        ret = Exynos_Shared_BufferToData(exynosOutputPort, dstInputUseBuffer, &dstInputData);
                        if (ret != OMX_ErrorNone) {
                            dstInputUseBuffer->dataValid = OMX_FALSE;
                            Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                            break;
                        }
#ifdef USE_ANDROID
                        if (pVideoDec->bDrvDPBManaging != OMX_TRUE) {
                            if ((exynosOutputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) ||
                                (exynosOutputPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE)) {
                                Exynos_OSAL_RefCount_Increase(pVideoDec->hRefHandle,
                                                            dstInputData.bufferHeader->pBuffer,
                                                            exynosOutputPort);
                            }
                        }
#endif
                        Exynos_ResetDataBuffer(dstInputUseBuffer);
                    }
                }

                if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort)) {
                    Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                    break;
                }
            }

            ret = pVideoDec->exynos_codec_dstInputProcess(pOMXComponent, &dstInputData);
            if ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorOutputBufferUseYet) {
                /*
                 * process data could be invalid by flush operation at next dstInputPorcess().
                 * then, need to clean up buffer internally for reusing it.
                 */
                if ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorNoneReuseBuffer) {
                    if (exynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
                        Exynos_CodecBufferEnQueue(pExynosComponent, OUTPUT_PORT_INDEX, dstInputData.pPrivate);
                    }

                    if (exynosOutputPort->bufferProcessType == BUFFER_SHARE) {
#ifdef USE_ANDROID
                        if (pVideoDec->bDrvDPBManaging != OMX_TRUE) {
                            if ((exynosOutputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) ||
                                (exynosOutputPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE)) {
                                ReleaseDPB dpbFD[VIDEO_BUFFER_MAX_NUM];
                                Exynos_OSAL_Memset(dpbFD, 0, sizeof(dpbFD));

                                dpbFD[0].fd = dstInputData.buffer.fd[0];
                                dpbFD[1].fd = -1;

                                Exynos_OSAL_RefCount_Decrease(pVideoDec->hRefHandle,
                                            dstInputData.bufferHeader->pBuffer, dpbFD, exynosOutputPort);
                            }
                        }
#endif
                        Exynos_OMX_FillThisBufferAgain(hComponent, dstInputData.bufferHeader);
                    }
                }

                Exynos_ResetCodecData(&dstInputData);
            }

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
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *dstOutputUseBuffer = &exynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    EXYNOS_OMX_DATA          *pDstOutputData = &exynosOutputPort->processData;

    FunctionIn();

    while (!pVideoDec->bExitBufferProcessThread) {
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
               (!pVideoDec->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))
                break;

            Exynos_OSAL_MutexLock(dstOutputUseBuffer->bufferMutex);
            if (exynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
                if ((dstOutputUseBuffer->dataValid != OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                    ret = Exynos_OutputBufferGetQueue(pExynosComponent);
                    if (pVideoDec->bExitBufferProcessThread) {
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
                (exynosOutputPort->bufferProcessType == BUFFER_SHARE)) {
                ret = pVideoDec->exynos_codec_dstOutputProcess(pOMXComponent, pDstOutputData);
            }

            if (((ret == OMX_ErrorNone) && (dstOutputUseBuffer->dataValid == OMX_TRUE)) ||
                (exynosOutputPort->bufferProcessType == BUFFER_SHARE)) {
#ifdef USE_ANDROID
                if (pVideoDec->bDrvDPBManaging != OMX_TRUE) {
                    if ((exynosOutputPort->bufferProcessType == BUFFER_SHARE) &&
                        (pDstOutputData->bufferHeader != NULL)) {
                        DECODE_CODEC_EXTRA_BUFFERINFO *pBufferInfo = NULL;
                        int i;

                        pBufferInfo = (DECODE_CODEC_EXTRA_BUFFERINFO *)pDstOutputData->extInfo;
                        Exynos_OSAL_RefCount_Decrease(pVideoDec->hRefHandle,
                                                      pDstOutputData->bufferHeader->pBuffer,
                                                      pBufferInfo->PDSB.dpbFD,
                                                      exynosOutputPort);
                    }
                }
#endif
                if (ret == OMX_ErrorNoneReuseBuffer){
                    if (exynosOutputPort->bufferProcessType == BUFFER_SHARE) {
                        ret = Exynos_OMX_FillThisBufferAgain(hComponent, pDstOutputData->bufferHeader);
                        if (ret != OMX_ErrorNone) {
                            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] FillThisBufferAgain failed(0x%x)", __FUNCTION__, pOMXComponent, ret);
                        }
                    }
                } else {
                    Exynos_Postprocess_OutputData(pOMXComponent, pDstOutputData);
                }
            }

            if (exynosOutputPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
                if (pDstOutputData->pPrivate != NULL) {
                    Exynos_CodecBufferEnQueue(pExynosComponent, OUTPUT_PORT_INDEX, pDstOutputData->pPrivate);
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
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    OMX_COMPONENTTYPE   *pOMXComponent  = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    Exynos_OMX_SrcInputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_SrcOutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    OMX_COMPONENTTYPE   *pOMXComponent  = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    Exynos_OMX_SrcOutputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_DstInputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    OMX_COMPONENTTYPE   *pOMXComponent  = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    Exynos_OMX_DstInputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_DstOutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    OMX_COMPONENTTYPE   *pOMXComponent  = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

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
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    FunctionIn();

    pVideoDec->bExitBufferProcessThread = OMX_FALSE;

    ret = Exynos_OSAL_ThreadCreate(&pVideoDec->hDstOutputThread,
                 Exynos_OMX_DstOutputProcessThread,
                 pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoDec->hSrcOutputThread,
                     Exynos_OMX_SrcOutputProcessThread,
                     pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoDec->hDstInputThread,
                     Exynos_OMX_DstInputProcessThread,
                     pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoDec->hSrcInputThread,
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
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    OMX_S32 countValue = 0;

    FunctionIn();

    pVideoDec->bExitBufferProcessThread = OMX_TRUE;

    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bufferSemID);
    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].codecSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].codecSemID);

    /* srcInput */
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoDec->hSrcInputThread);
    pVideoDec->hSrcInputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] src input thread is terminated", pExynosComponent, __FUNCTION__);

    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bufferSemID);
    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].codecSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].codecSemID);
    pVideoDec->exynos_codec_bufferProcessRun(pOMXComponent, OUTPUT_PORT_INDEX);

    /* dstInput */
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoDec->hDstInputThread);
    pVideoDec->hDstInputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[INPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] dst input thread is terminated", pExynosComponent, __FUNCTION__);

    /* srcOutput */
    pVideoDec->exynos_codec_stop(pOMXComponent, INPUT_PORT_INDEX);
    pVideoDec->exynos_codec_bufferProcessRun(pOMXComponent, INPUT_PORT_INDEX);

    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoDec->hSrcOutputThread);
    pVideoDec->hSrcOutputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] src output thread is terminated", pExynosComponent, __FUNCTION__);

    /* dstOutput */
    pVideoDec->exynos_codec_stop(pOMXComponent, OUTPUT_PORT_INDEX);
    pVideoDec->exynos_codec_bufferProcessRun(pOMXComponent, OUTPUT_PORT_INDEX);

    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX]);
    Exynos_OSAL_ThreadTerminate(pVideoDec->hDstOutputThread);
    pVideoDec->hDstOutputThread = NULL;
    Exynos_OSAL_Set_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].semWaitPortEnable[OUTPUT_WAY_INDEX], 0);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] dst output thread is terminated", pExynosComponent, __FUNCTION__);

    pExynosComponent->checkTimeStamp.needSetStartTimeStamp      = OMX_FALSE;
    pExynosComponent->checkTimeStamp.needCheckStartTimeStamp    = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoDecodeComponentInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    DECODE_CODEC_EXTRA_BUFFERINFO   *pBufferInfo        = NULL;

    int i = 0;

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

    pVideoDec = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_VIDEODEC_COMPONENT));
    if (pVideoDec == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_Port_Destructor(pOMXComponent);
        Exynos_OMX_BaseComponent_Destructor(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Memset(pVideoDec, 0, sizeof(EXYNOS_OMX_VIDEODEC_COMPONENT));
    pVideoDec->bForceHeaderParsing      = OMX_FALSE;
    pVideoDec->bReconfigDPB             = OMX_FALSE;
    pVideoDec->bDTSMode                 = OMX_FALSE;
    pVideoDec->bReorderMode             = OMX_FALSE;
    pVideoDec->eDataType                = DATA_TYPE_8BIT;
    pVideoDec->bQosChanged              = OMX_FALSE;
    pVideoDec->nQosRatio                = 0;
    pVideoDec->bThumbnailMode           = OMX_FALSE;
    pVideoDec->bSearchBlackBarChanged   = OMX_FALSE;
    pVideoDec->bSearchBlackBar          = OMX_FALSE;
    pVideoDec->nImageConvMode           = 1;
    pVideoDec->bForceUseNonCompFormat   = OMX_FALSE;
    pVideoDec->bDiscardCSDError         = OMX_FALSE;
#ifdef USE_COMPRESSED_COLOR
    pVideoDec->nCompColorFormat         = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC;
#endif
    pExynosComponent->hComponentHandle = (OMX_HANDLETYPE)pVideoDec;

    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

#ifdef USE_ANDROID
    pVideoDec->pPerfHandle = Exynos_OSAL_CreatePerformanceHandle(OMX_FALSE /* bIsEncoder = false */);
#endif

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort->supportFormat, 0, (sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX));

    pExynosPort->portDefinition.nBufferCountActual = MAX_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferCountMin = MIN_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferSize = 0;
    pExynosPort->portDefinition.eDomain = OMX_PortDomainVideo;

    pExynosPort->portDefinition.format.video.cMIMEType = Exynos_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    if (pExynosPort->portDefinition.format.video.cMIMEType == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
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
    pExynosPort->portDefinition.format.video.nBitrate = 64000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.format.video.pNativeWindow = NULL;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
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
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
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
    pExynosPort->portDefinition.format.video.nBitrate = 64000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.format.video.pNativeWindow = NULL;

    pExynosPort->processData.extInfo = (OMX_PTR)Exynos_OSAL_Malloc(sizeof(DECODE_CODEC_EXTRA_BUFFERINFO));
    if (pExynosPort->processData.extInfo == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(((char *)pExynosPort->processData.extInfo), 0, sizeof(DECODE_CODEC_EXTRA_BUFFERINFO));
    pBufferInfo = (DECODE_CODEC_EXTRA_BUFFERINFO *)(pExynosPort->processData.extInfo);
    for (i = 0; i < VIDEO_BUFFER_MAX_NUM; i++) {
        pBufferInfo->PDSB.dpbFD[i].fd  = 0;
        pBufferInfo->PDSB.dpbFD[i].fd1 = 0;
        pBufferInfo->PDSB.dpbFD[i].fd2 = 0;
    }

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
    pExynosComponent->exynos_BufferFlush            = &Exynos_OMX_BufferFlush;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoDecodeComponentDeinit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    int                    i = 0;

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

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

#ifdef USE_ANDROID
    if (pVideoDec->bDrvDPBManaging != OMX_TRUE)
        Exynos_OSAL_RefCount_Terminate(pVideoDec->hRefHandle);

    Exynos_OSAL_ImgConv_Terminate(pVideoDec->hImgConv);

    Exynos_OSAL_ReleasePerformanceHandle(pVideoDec->pPerfHandle);
#endif

    Exynos_OSAL_Free(pVideoDec);
    pExynosComponent->hComponentHandle = pVideoDec = NULL;

    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
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
