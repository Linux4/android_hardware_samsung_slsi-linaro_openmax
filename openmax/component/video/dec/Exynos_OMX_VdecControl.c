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
 * @file        Exynos_OMX_VdecControl.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Taehwan Kim (t_h.kim@samsung.com)
 * @version     2.5.0
 * @history
 *   2012.02.20 : Create
 *   2017.08.03 : Change event handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Exynos_OMX_Macros.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OMX_Vdec.h"
#include "Exynos_OMX_VdecControl.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_SharedMemory.h"

#include "Exynos_OSAL_Platform.h"

#include "ExynosVideoApi.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VIDEO_DECCONTROL"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


OMX_ERRORTYPE Exynos_OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes,
    OMX_IN OMX_U8                   *pBuffer)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE        *temp_bufferHeader  = NULL;

    OMX_U32 i = 0;

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

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, nPortIndex);
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (pExynosPort->portState != EXYNOS_OMX_PortStateEnabling) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] %s port : invalid state : comp state(0x%x), port state(0x%x), enabled(0x%x)",
                        pExynosComponent, __FUNCTION__,
                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                        pExynosComponent->currentState, pExynosPort->portState, pExynosPort->portDefinition.bEnabled);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pExynosPort) &&
        CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    temp_bufferHeader = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (temp_bufferHeader == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(temp_bufferHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = temp_bufferHeader;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);

            temp_bufferHeader->pBuffer        = pBuffer;
            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;

            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;

            if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE) {
                EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;
                EXYNOS_OMX_LOCK_RANGE range;
                OMX_U32 stride;

                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] use grahpic buffer", pExynosComponent, __FUNCTION__);

                range.nWidth        = pExynosPort->portDefinition.format.video.nFrameWidth;
                range.nHeight       = pExynosPort->portDefinition.format.video.nFrameHeight;
                range.eColorFormat  = pExynosPort->portDefinition.format.video.eColorFormat;
                stride              = range.nWidth;

                ret = Exynos_OSAL_LockMetaData(temp_bufferHeader->pBuffer,
                                               range,
                                               &stride, &bufferInfo,
                                               pExynosPort->eMetaDataType);
                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_LockMetaData (0x%x)",
                                                        pExynosComponent, __FUNCTION__, ret);
                    goto EXIT;
                }

                pExynosPort->extendBufferHeader[i].buf_fd[0] = bufferInfo.fd[0];
                pExynosPort->extendBufferHeader[i].buf_fd[1] = bufferInfo.fd[1];
                pExynosPort->extendBufferHeader[i].buf_fd[2] = bufferInfo.fd[2];

                pExynosPort->extendBufferHeader[i].pYUVBuf[0] = bufferInfo.addr[0];
                pExynosPort->extendBufferHeader[i].pYUVBuf[1] = bufferInfo.addr[1];
                pExynosPort->extendBufferHeader[i].pYUVBuf[2] = bufferInfo.addr[2];

                Exynos_OSAL_UnlockMetaData(temp_bufferHeader->pBuffer, pExynosPort->eMetaDataType);

                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] buf %d pYUVBuf[0]:0x%x , pYUVBuf[1]:0x%x ", __FUNCTION__,
                                i, pExynosPort->extendBufferHeader[i].pYUVBuf[0],
                                pExynosPort->extendBufferHeader[i].pYUVBuf[1]);
            }

            pExynosPort->assignedBufferNum++;

            *ppBufferHdr = temp_bufferHeader;

            if (pExynosPort->assignedBufferNum == (OMX_S32)pExynosPort->portDefinition.nBufferCountActual) {
                pExynosPort->portDefinition.bPopulated = OMX_TRUE;

                if ((pExynosComponent->currentState == OMX_StateLoaded) &&
                    (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle)) {
                    if (CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]) &&
                        CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX])) {
                        Exynos_OMX_SendEventCommand(pExynosComponent, EVENT_CMD_STATE_TO_IDLE_STATE, NULL);
                    }
                } else if ((!CHECK_PORT_ENABLED(pExynosPort)) &&
                           (pExynosPort->portState == EXYNOS_OMX_PortStateEnabling)) {
                    Exynos_OMX_SendEventCommand(pExynosComponent,
                                                ((nPortIndex == INPUT_PORT_INDEX)? EVENT_CMD_ENABLE_INPUT_PORT:EVENT_CMD_ENABLE_OUTPUT_PORT),
                                                NULL);
                }
            }

            ret = OMX_ErrorNone;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port: buffer header(%p), size(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                    (*ppBufferHdr), nSizeBytes);
            goto EXIT;
        }
    }

    Exynos_OSAL_Free(temp_bufferHeader);
    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBuffer,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *temp_bufferHeader  = NULL;
    OMX_U8                          *temp_buffer        = NULL;
    unsigned long                    temp_buffer_fd     = -1;
    MEMORY_TYPE                      mem_type           = NORMAL_MEMORY;

    OMX_U32 i = 0;

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

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, nPortIndex);
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (pExynosPort->portState != EXYNOS_OMX_PortStateEnabling) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] %s port : invalid state : comp state(0x%x), port state(0x%x), enabled(0x%x)",
                        pExynosComponent, __FUNCTION__,
                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                        pExynosComponent->currentState, pExynosPort->portState, pExynosPort->portDefinition.bEnabled);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pExynosPort) &&
        CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
        (nPortIndex == INPUT_PORT_INDEX))
        mem_type |= SECURE_MEMORY;

    if (!((nPortIndex == OUTPUT_PORT_INDEX) &&
          (pExynosPort->bufferProcessType == BUFFER_SHARE)))
        mem_type |= CACHED_MEMORY;

    if (pExynosPort->bNeedContigMem == OMX_TRUE)
        mem_type |= CONTIG_MEMORY;

    if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
        (nPortIndex == INPUT_PORT_INDEX) &&
        (pExynosPort->eMetaDataType == METADATA_TYPE_HANDLE)) {  /* for DRM input : metadata */
        temp_buffer = Exynos_OSAL_AllocMetaDataBuffer(pVideoDec->hSharedMemory,
                                                      pExynosPort->eMetaDataType,
                                                      nSizeBytes,
                                                      mem_type);
        if (temp_buffer == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Alloc", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    } else {
        temp_buffer = Exynos_OSAL_SharedMemory_Alloc(pVideoDec->hSharedMemory, nSizeBytes, mem_type);
        if (temp_buffer == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Alloc", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        temp_buffer_fd = Exynos_OSAL_SharedMemory_VirtToION(pVideoDec->hSharedMemory, temp_buffer);
    }

    temp_bufferHeader = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (temp_bufferHeader == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(temp_bufferHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = temp_bufferHeader;
            pExynosPort->extendBufferHeader[i].buf_fd[0] = temp_buffer_fd;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ALLOCATED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);

            if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
                (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
                /* backward compatibility */
                temp_bufferHeader->pBuffer = (void *)temp_buffer_fd;
            } else {
                temp_bufferHeader->pBuffer = temp_buffer;
            }

            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;

            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;

            pExynosPort->assignedBufferNum++;

            *ppBuffer = temp_bufferHeader;

            if (pExynosPort->assignedBufferNum == (OMX_S32)pExynosPort->portDefinition.nBufferCountActual) {
                pExynosPort->portDefinition.bPopulated = OMX_TRUE;

                if ((pExynosComponent->currentState == OMX_StateLoaded) &&
                    (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle)) {
                    if (CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]) &&
                        CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX])) {
                        Exynos_OMX_SendEventCommand(pExynosComponent, EVENT_CMD_STATE_TO_IDLE_STATE, NULL);
                    }
                } else if((!CHECK_PORT_ENABLED(pExynosPort)) &&
                          (pExynosPort->portState == EXYNOS_OMX_PortStateEnabling)) {
                    Exynos_OMX_SendEventCommand(pExynosComponent,
                                                ((nPortIndex == INPUT_PORT_INDEX)? EVENT_CMD_ENABLE_INPUT_PORT:EVENT_CMD_ENABLE_OUTPUT_PORT),
                                                NULL);
                }
            }

            ret = OMX_ErrorNone;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port: buffer header(%p), size(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                    (*ppBuffer), nSizeBytes);
            goto EXIT;
        }
    }

    ret = OMX_ErrorInsufficientResources;

EXIT:
    if (ret == OMX_ErrorInsufficientResources) {
        if (temp_bufferHeader != NULL)
            Exynos_OSAL_Free(temp_bufferHeader);

        if (temp_buffer != NULL) {
            if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
                (nPortIndex == INPUT_PORT_INDEX) &&
                (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
                Exynos_OSAL_FreeMetaDataBuffer(pVideoDec->hSharedMemory, pExynosPort->eMetaDataType, temp_buffer);
            } else if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
                       (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
                OMX_PTR mapBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoDec->hSharedMemory, temp_buffer_fd);

                Exynos_OSAL_SharedMemory_Free(pVideoDec->hSharedMemory, temp_buffer);
            } else {
                Exynos_OSAL_SharedMemory_Free(pVideoDec->hSharedMemory, temp_buffer);
            }
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_U32               nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *pOMXBufferHdr      = NULL;

    OMX_U32 i = 0;

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

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, nPortIndex);
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if ((pExynosPort->portState != EXYNOS_OMX_PortStateDisabling) &&
        (pExynosPort->portState != EXYNOS_OMX_PortStateFlushingForDisable) &&
        (pExynosPort->portState != EXYNOS_OMX_PortStateInvalid)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] %s port : invalid state : comp state(0x%x), port state(0x%x), enabled(0x%x)",
                        pExynosComponent, __FUNCTION__,
                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                        pExynosComponent->currentState, pExynosPort->portState, pExynosPort->portDefinition.bEnabled);
        ret = OMX_ErrorIncorrectStateOperation;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);

        (*(pExynosComponent->pCallbacks->EventHandler)) (pOMXComponent,
                        pExynosComponent->callbackData,
                        (OMX_U32)OMX_EventError,
                        (OMX_U32)OMX_ErrorPortUnpopulated,
                        nPortIndex, NULL);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pExynosPort) &&
        CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (((pExynosPort->bufferStateAllocate[i] | BUFFER_STATE_FREE) != 0) &&
            (pExynosPort->extendBufferHeader[i].OMXBufferHeader != NULL)) {
            if (pExynosPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Buffer can't be freed because this(%p) is already in component", pExynosComponent, __FUNCTION__, pBufferHdr);
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] %s port : invalid state : comp state(0x%x), port state(0x%x), enabled(0x%x)",
                                                    pExynosComponent, __FUNCTION__,
                                                    (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                    pExynosComponent->currentState, pExynosPort->portState, pExynosPort->portDefinition.bEnabled);
            }

            pOMXBufferHdr = pExynosPort->extendBufferHeader[i].OMXBufferHeader;

            if (pOMXBufferHdr->pBuffer == pBufferHdr->pBuffer) {
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port: buffer header(%p)",
                                                        pExynosComponent, __FUNCTION__,
                                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                        pOMXBufferHdr);
                if (pExynosPort->bufferStateAllocate[i] & BUFFER_STATE_ALLOCATED) {
                    if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
                        (nPortIndex == INPUT_PORT_INDEX) &&
                        (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
                        Exynos_OSAL_FreeMetaDataBuffer(pVideoDec->hSharedMemory,
                                                       pExynosPort->eMetaDataType,
                                                       pOMXBufferHdr->pBuffer);
                    } else if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
                               (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
                        unsigned long ionFD = (unsigned long)pOMXBufferHdr->pBuffer;

                        OMX_PTR mapBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoDec->hSharedMemory, ionFD);
                        Exynos_OSAL_SharedMemory_Free(pVideoDec->hSharedMemory, mapBuffer);
                    } else {
                        Exynos_OSAL_SharedMemory_Free(pVideoDec->hSharedMemory, pOMXBufferHdr->pBuffer);
                    }

                    pOMXBufferHdr->pBuffer = NULL;
                    pBufferHdr->pBuffer = NULL;
                } else if (pExynosPort->bufferStateAllocate[i] & BUFFER_STATE_ASSIGNED) {
                    ; /* None*/
                }

                pExynosPort->assignedBufferNum--;

                if (pExynosPort->bufferStateAllocate[i] & HEADER_STATE_ALLOCATED) {
                    Exynos_OSAL_Free(pOMXBufferHdr);
                    pExynosPort->extendBufferHeader[i].OMXBufferHeader  = NULL;
                    pExynosPort->extendBufferHeader[i].bBufferInOMX     = OMX_FALSE;
                    pBufferHdr = NULL;
                }

                pExynosPort->bufferStateAllocate[i] = BUFFER_STATE_FREE;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pExynosPort->assignedBufferNum < (OMX_S32)pExynosPort->portDefinition.nBufferCountActual)
            pExynosPort->portDefinition.bPopulated = OMX_FALSE;

        if (pExynosPort->assignedBufferNum == 0) {
            if ((pExynosComponent->currentState == OMX_StateIdle) &&
                (pExynosComponent->transientState == EXYNOS_OMX_TransStateIdleToLoaded)) {
                if (!CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]) &&
                    !CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX])) {
                    Exynos_OMX_SendEventCommand(pExynosComponent, EVENT_CMD_STATE_TO_LOAD_STATE, NULL);
                }
            } else if((CHECK_PORT_ENABLED(pExynosPort)) &&
                      (pExynosPort->portState == EXYNOS_OMX_PortStateDisabling)) {
                Exynos_OMX_SendEventCommand(pExynosComponent,
                                            ((nPortIndex == INPUT_PORT_INDEX)? EVENT_CMD_DISABLE_INPUT_PORT:EVENT_CMD_DISABLE_OUTPUT_PORT),
                                            NULL);
            }
        }
    }

    FunctionOut();

    return ret;
}

#ifdef TUNNELING_SUPPORT
OMX_ERRORTYPE Exynos_OMX_AllocateTunnelBuffer(EXYNOS_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                 ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT          *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE         *temp_bufferHeader = NULL;
    OMX_U8                       *temp_buffer       = NULL;
    OMX_U32                       bufferSize        = 0;
    OMX_PARAM_PORTDEFINITIONTYPE  portDefinition;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FreeTunnelBuffer(EXYNOS_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE            ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT     *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE    *temp_bufferHeader  = NULL;
    OMX_U8                  *temp_buffer        = NULL;
    OMX_U32                  bufferSize         = 0;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentTunnelRequest(
    OMX_IN OMX_HANDLETYPE hComp,
    OMX_IN OMX_U32        nPort,
    OMX_IN OMX_HANDLETYPE hTunneledComp,
    OMX_IN OMX_U32        nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE *pTunnelSetup)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}
#endif

OMX_ERRORTYPE Exynos_OMX_GetFlushBuffer(EXYNOS_OMX_BASEPORT *pExynosPort, EXYNOS_OMX_DATABUFFER *pDataBuffer[])
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    *pDataBuffer = NULL;

    if (pExynosPort->portWayType == WAY1_PORT) {
        *pDataBuffer = &pExynosPort->way.port1WayDataBuffer.dataBuffer;
    } else if (pExynosPort->portWayType == WAY2_PORT) {
            pDataBuffer[0] = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);
            pDataBuffer[1] = &(pExynosPort->way.port2WayDataBuffer.outputDataBuffer);
    }

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FlushPort(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 portIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *bufferHeader       = NULL;
    EXYNOS_OMX_DATABUFFER           *pDataPortBuffer[2] = {NULL, NULL};
    EXYNOS_OMX_MESSAGE              *message            = NULL;

    OMX_S32 semValue = 0;
    int i = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
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
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    if ((portIndex < 0) ||
        (portIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[portIndex];

    while (Exynos_OSAL_GetElemNum(&pExynosPort->bufferQ) > 0) {
        Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[portIndex].bufferSemID, &semValue);
        if (semValue == 0)
            Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[portIndex].bufferSemID);

        Exynos_OSAL_SemaphoreWait(pExynosComponent->pExynosPort[portIndex].bufferSemID);
        message = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
        if ((message != NULL) &&
            (message->type != EXYNOS_OMX_CommandFakeBuffer)) {
            bufferHeader = (OMX_BUFFERHEADERTYPE *)message->pCmdData;
            bufferHeader->nFilledLen = 0;

            if (portIndex == OUTPUT_PORT_INDEX)
                Exynos_OMX_OutputBufferReturn(pOMXComponent, bufferHeader);
            else if (portIndex == INPUT_PORT_INDEX)
                Exynos_OMX_InputBufferReturn(pOMXComponent, bufferHeader);
        }

        Exynos_OSAL_Free(message);
        message = NULL;
    }

    Exynos_OMX_GetFlushBuffer(pExynosPort, pDataPortBuffer);
    if (portIndex == INPUT_PORT_INDEX) {
        if (pDataPortBuffer[0]->dataValid == OMX_TRUE)
            Exynos_InputBufferReturn(pOMXComponent, pDataPortBuffer[0]);
        if (pDataPortBuffer[1]->dataValid == OMX_TRUE)
            Exynos_InputBufferReturn(pOMXComponent, pDataPortBuffer[1]);
    } else if (portIndex == OUTPUT_PORT_INDEX) {
        if (pDataPortBuffer[0]->dataValid == OMX_TRUE)
            Exynos_OutputBufferReturn(pOMXComponent, pDataPortBuffer[0]);
        if (pDataPortBuffer[1]->dataValid == OMX_TRUE)
            Exynos_OutputBufferReturn(pOMXComponent, pDataPortBuffer[1]);
    }

    if (pExynosPort->bufferProcessType == BUFFER_SHARE) {
        if (pExynosPort->processData.bufferHeader != NULL) {
            if (portIndex == INPUT_PORT_INDEX) {
                Exynos_OMX_InputBufferReturn(pOMXComponent, pExynosPort->processData.bufferHeader);
            } else if (portIndex == OUTPUT_PORT_INDEX) {
                if (pExynosPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK)
                    Exynos_OSAL_UnlockMetaData(pExynosPort->processData.bufferHeader->pBuffer, pExynosPort->eMetaDataType);

                Exynos_OMX_OutputBufferReturn(pOMXComponent, pExynosPort->processData.bufferHeader);
            }
        }
        Exynos_ResetCodecData(&pExynosPort->processData);

        for (i = 0; i < MAX_BUFFER_NUM; i++) {
            if (pExynosPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                if (portIndex == OUTPUT_PORT_INDEX) {
                    if (pExynosPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK)
                        Exynos_OSAL_UnlockMetaData(pExynosPort->extendBufferHeader[i].OMXBufferHeader->pBuffer, pExynosPort->eMetaDataType);

                    Exynos_OMX_OutputBufferReturn(pOMXComponent, pExynosPort->extendBufferHeader[i].OMXBufferHeader);
                } else if (portIndex == INPUT_PORT_INDEX) {
                    Exynos_OMX_InputBufferReturn(pOMXComponent, pExynosPort->extendBufferHeader[i].OMXBufferHeader);
                }
            }
        }
    }

#ifdef USE_ANDROID
    if ((pExynosPort->bufferProcessType == BUFFER_SHARE) &&
        (portIndex == OUTPUT_PORT_INDEX) &&
        (pVideoDec->bDrvDPBManaging != OMX_TRUE))
        Exynos_OSAL_RefCount_Reset(pVideoDec->hRefHandle);
#endif

    if (pExynosPort->bufferSemID != NULL) {
        while (1) {
            OMX_S32 cnt = 0;
            Exynos_OSAL_Get_SemaphoreCount(pExynosPort->bufferSemID, &cnt);
            if (cnt == 0)
                break;
            else if (cnt > 0)
                Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
            else if (cnt < 0)
                Exynos_OSAL_SemaphorePost(pExynosPort->bufferSemID);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    Exynos_OSAL_ResetQueue(&pExynosPort->bufferQ);

EXIT:
    FunctionOut();

    return ret;
}


OMX_ERRORTYPE Exynos_OMX_ForceHeaderParsing(
    OMX_COMPONENTTYPE       *pOMXComponent,
    EXYNOS_OMX_DATABUFFER   *pSrcDataBuffer,
    EXYNOS_OMX_DATA         *pSrcInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    OMX_PTR  pCodecBuffer   = NULL;
    OMX_S32  nBufferCnt     = 0;
    OMX_BOOL bSubmitCSD     = OMX_FALSE;

    FunctionIn();

    pVideoDec->bForceHeaderParsing = OMX_TRUE;

    /* get a count of queued buffer */
    nBufferCnt = Exynos_OSAL_GetElemNum(&pInputPort->bufferQ);

    /* it is possible that input way has valid info,
     * it means that Exynos_Preprocessor_InputData is not handled yet.
     * so, do-while loop is used.
     */
    do {
        if (pSrcInputData->bufferHeader != NULL) {  /* processData */
            if (!(pSrcInputData->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) {
                /* if does not have CSD flag, think of that all CSDs were already parsed */
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] no more CSD buffer", pExynosComponent, __FUNCTION__);
                break;
            }
            ret = pVideoDec->exynos_codec_srcInputProcess(pOMXComponent, pSrcInputData);

            if (ret == OMX_ErrorNone)
                bSubmitCSD = OMX_TRUE;
        } else if (pSrcDataBuffer->dataValid == OMX_TRUE) {
            if (!(pSrcDataBuffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) {
                /* if does not have CSD flag, think of that all CSDs were already parsed */
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] no more CSD buffer", pExynosComponent, __FUNCTION__);
                break;
            }

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] has CSD buffer(%p), remains(%d)",
                                    pExynosComponent, __FUNCTION__, pSrcDataBuffer->bufferHeader, nBufferCnt);

            /* for BUFFER_COPY mode, get a codec buffer */
            if ((pInputPort->bufferProcessType & BUFFER_COPY) &&
                ((pSrcInputData->buffer.addr[0] == NULL) ||
                 (pSrcInputData->pPrivate == NULL))) {
                Exynos_CodecBufferDeQueue(pExynosComponent, INPUT_PORT_INDEX, &pCodecBuffer);
                if (pCodecBuffer == NULL) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can't find a valid codec buffer", pExynosComponent, __FUNCTION__);
                    ret = OMX_ErrorUndefined;
                    goto EXIT;
                }

                Exynos_CodecBufferToData(pCodecBuffer, pSrcInputData, INPUT_PORT_INDEX);
            }

            if (Exynos_Preprocessor_InputData(pOMXComponent, pSrcInputData) == OMX_TRUE) {
                ret = pVideoDec->exynos_codec_srcInputProcess(pOMXComponent, pSrcInputData);

                if (ret == OMX_ErrorNone)
                    bSubmitCSD = OMX_TRUE;
            } else {
                if (pInputPort->bufferProcessType & BUFFER_COPY)
                    Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pSrcInputData->pPrivate);
            }
        }

        switch ((int)ret) {
        case OMX_ErrorCorruptedFrame:
        case OMX_ErrorCorruptedHeader:
        case OMX_ErrorInputDataDecodeYet:  /* no need re-input scheme */
        {
            /* discard current buffer */
            if (pInputPort->bufferProcessType & BUFFER_COPY)
                Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pSrcInputData->pPrivate);

            if (pInputPort->bufferProcessType & BUFFER_SHARE)
                Exynos_OMX_InputBufferReturn(pOMXComponent, pSrcInputData->bufferHeader);
        }
            break;
        case OMX_ErrorNone:
        case OMX_ErrorNeedNextHeaderInfo:
        case OMX_ErrorNoneSrcSetupFinish:
        {
            /* no need to process anything */
        }
            break;
        default:
        {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] error is occurred at pre-processing(0x%x)",
                                                pExynosComponent, __FUNCTION__, ret);
            goto EXIT;
        }
            break;
        }

        /* info cleanup */
        Exynos_ResetDataBuffer(pSrcDataBuffer);
        Exynos_ResetCodecData(pSrcInputData);

        if (nBufferCnt > 0) {
            nBufferCnt--;

            /* get a buffer from port */
            ret = Exynos_InputBufferGetQueue(pExynosComponent);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Failed to InputBufferGetQueue", pExynosComponent, __FUNCTION__);
                break;
            }
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] no more buffer", pExynosComponent, __FUNCTION__);
            break;
        }
    } while (1);

EXIT:
    pVideoDec->exynos_codec_stop(pOMXComponent, INPUT_PORT_INDEX);

    if (bSubmitCSD == OMX_TRUE) {
        ret = pVideoDec->exynos_codec_checkResolutionChange(pOMXComponent);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] error is occurred at resolution check(0x%x)",
                                              pExynosComponent, __FUNCTION__, ret);
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventError, ret, 0, NULL);
        }
    }

    pVideoDec->bForceHeaderParsing = OMX_FALSE;

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferFlush(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_S32             nPortIndex,
    OMX_BOOL            bEvent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_DATABUFFER           *flushPortBuffer[2] = {NULL, NULL};
    EXYNOS_OMX_PORT_STATETYPE        ePortState         = EXYNOS_OMX_PortStateInvalid;

    FunctionIn();

    if (pOMXComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

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

    if ((nPortIndex < 0) ||
        (nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
    ePortState  = pExynosPort->portState;

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] OMX_CommandFlush start, port:%d, event:%d",
                                    pExynosComponent, __FUNCTION__, nPortIndex, bEvent);

    Exynos_OSAL_SignalSet(pExynosPort->pauseEvent);

    Exynos_OMX_GetFlushBuffer(pExynosPort, flushPortBuffer);
    if (flushPortBuffer[0] == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE))
        Exynos_OSAL_SemaphorePost(pExynosPort->codecSemID);

    if (pExynosPort->bufferSemID != NULL) {
        while (1) {
            OMX_S32 cnt = 0;
            Exynos_OSAL_Get_SemaphoreCount(pExynosPort->bufferSemID, &cnt);
            if (cnt > 0)
                break;
            else
                Exynos_OSAL_SemaphorePost(pExynosPort->bufferSemID);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    pVideoDec->exynos_codec_bufferProcessRun(pOMXComponent, nPortIndex);
    Exynos_OSAL_MutexLock(flushPortBuffer[0]->bufferMutex);

    pVideoDec->exynos_codec_stop(pOMXComponent, nPortIndex);

    if (flushPortBuffer[1] != NULL)
        Exynos_OSAL_MutexLock(flushPortBuffer[1]->bufferMutex);

    if (nPortIndex == INPUT_PORT_INDEX) {
        /* try to find a CSD buffer and parse it */
        Exynos_OMX_ForceHeaderParsing(pOMXComponent, flushPortBuffer[0], &(pExynosPort->processData));
    }

    ret = Exynos_OMX_FlushPort(pOMXComponent, nPortIndex);

    if ((ePortState == EXYNOS_OMX_PortStateFlushingForDisable) &&
        (pVideoDec->bReconfigDPB == OMX_TRUE)) {
        ret = pVideoDec->exynos_codec_reconfigAllBuffers(pOMXComponent, nPortIndex);
        if (ret != OMX_ErrorNone)
            goto EXIT;
    } else if (pExynosComponent->pExynosPort[nPortIndex].bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        pVideoDec->exynos_codec_enqueueAllBuffer(pOMXComponent, nPortIndex);
    }

    Exynos_ResetCodecData(&pExynosPort->processData);

    if (ret == OMX_ErrorNone) {
        if (nPortIndex == INPUT_PORT_INDEX) {
            pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_TRUE;
            pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, OMX_FALSE, sizeof(OMX_BOOL) * MAX_TIMESTAMP);
            INIT_ARRAY_TO_VAL(pExynosComponent->timeStamp, DEFAULT_TIMESTAMP_VAL, MAX_TIMESTAMP);
            Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
            pExynosComponent->getAllDelayBuffer = OMX_FALSE;
            pExynosComponent->bSaveFlagEOS = OMX_FALSE;
            pExynosComponent->bBehaviorEOS = OMX_FALSE;
            pExynosComponent->reInputData = OMX_FALSE;

            pVideoDec->exynos_codec_start(pOMXComponent, nPortIndex);
        } else {
            if (pVideoDec->bReconfigDPB != OMX_TRUE)
                pVideoDec->exynos_codec_start(pOMXComponent, nPortIndex);
        }

#ifdef PERFORMANCE_DEBUG
        Exynos_OSAL_CountReset(pExynosPort->hBufferCount);
#endif
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] OMX_CommandFlush end, port:%d, event:%d",
                                        pExynosComponent, __FUNCTION__, nPortIndex, bEvent);

EXIT:
    if (flushPortBuffer[1] != NULL)
        Exynos_OSAL_MutexUnlock(flushPortBuffer[1]->bufferMutex);

    if (flushPortBuffer[0] != NULL)
        Exynos_OSAL_MutexUnlock(flushPortBuffer[0]->bufferMutex);

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_InputBufferReturn(
    OMX_COMPONENTTYPE       *pOMXComponent,
    EXYNOS_OMX_DATABUFFER   *pDataBuffer)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pInputPort         = NULL;
    OMX_BUFFERHEADERTYPE        *pBufferHdr         = NULL;

    FunctionIn();

    if ((pOMXComponent == NULL) ||
        (pDataBuffer == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pInputPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);

    pBufferHdr = pDataBuffer->bufferHeader;

    if (pBufferHdr != NULL) {
        if (pInputPort->markType.hMarkTargetComponent != NULL) {
            pBufferHdr->hMarkTargetComponent            = pInputPort->markType.hMarkTargetComponent;
            pBufferHdr->pMarkData                       = pInputPort->markType.pMarkData;
            pInputPort->markType.hMarkTargetComponent   = NULL;
            pInputPort->markType.pMarkData              = NULL;
        }

        if (pBufferHdr->hMarkTargetComponent != NULL) {
            if (pBufferHdr->hMarkTargetComponent == pOMXComponent) {
                pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                pExynosComponent->callbackData,
                                OMX_EventMark,
                                0, 0, pBufferHdr->pMarkData);
            } else {
                pExynosComponent->propagateMarkType.hMarkTargetComponent    = pBufferHdr->hMarkTargetComponent;
                pExynosComponent->propagateMarkType.pMarkData               = pBufferHdr->pMarkData;
            }
        }

        pBufferHdr->nFilledLen  = 0;
        pBufferHdr->nOffset     = 0;

        Exynos_OMX_InputBufferReturn(pOMXComponent, pBufferHdr);
    }

    /* reset dataBuffer */
    Exynos_ResetDataBuffer(pDataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_InputBufferGetQueue(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorUndefined;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort    = NULL;
    EXYNOS_OMX_MESSAGE              *message        = NULL;
    EXYNOS_OMX_DATABUFFER           *inputUseBuffer = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec      = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    pExynosPort    = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    inputUseBuffer = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if (((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
                (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) ||
               (pVideoDec->bForceHeaderParsing == OMX_TRUE)) {
        if (pVideoDec->bForceHeaderParsing != OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> wait(bufferSemID)",
                                                        pExynosComponent, __FUNCTION__);
            Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> post(bufferSemID)",
                                                        pExynosComponent, __FUNCTION__);
        }

        if (inputUseBuffer->dataValid != OMX_TRUE) {
            message = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
            if (message == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (message->type == EXYNOS_OMX_CommandFakeBuffer) {
                Exynos_OSAL_Free(message);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecFlush;
                goto EXIT;
            }

            inputUseBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
            inputUseBuffer->allocSize     = inputUseBuffer->bufferHeader->nAllocLen;
            inputUseBuffer->dataLen       = inputUseBuffer->bufferHeader->nFilledLen;
            inputUseBuffer->remainDataLen = inputUseBuffer->dataLen;
            inputUseBuffer->usedDataLen   = 0;
            inputUseBuffer->dataValid     = OMX_TRUE;
            inputUseBuffer->nFlags        = inputUseBuffer->bufferHeader->nFlags;
            inputUseBuffer->timeStamp     = inputUseBuffer->bufferHeader->nTimeStamp;

            Exynos_OSAL_Free(message);

            if (inputUseBuffer->allocSize < inputUseBuffer->dataLen) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] buffer size(%d) is smaller than dataLen(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    inputUseBuffer->allocSize, inputUseBuffer->dataLen);
            }
        }

        ret = OMX_ErrorNone;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OutputBufferReturn(
    OMX_COMPONENTTYPE       *pOMXComponent,
    EXYNOS_OMX_DATABUFFER   *pDataBuffer)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pOutputPort        = NULL;
    OMX_BUFFERHEADERTYPE        *pBufferHdr         = pDataBuffer->bufferHeader;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pOutputPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pBufferHdr != NULL) {
        pBufferHdr->nFilledLen = pDataBuffer->remainDataLen;
        pBufferHdr->nOffset    = 0;
        pBufferHdr->nFlags     = pDataBuffer->nFlags;
        pBufferHdr->nTimeStamp = pDataBuffer->timeStamp;

        if ((pOutputPort->eMetaDataType & METADATA_TYPE_DATA) &&
            (pBufferHdr->nFilledLen > 0))
            pBufferHdr->nFilledLen = pBufferHdr->nAllocLen;

        if (pExynosComponent->propagateMarkType.hMarkTargetComponent != NULL) {
            pBufferHdr->hMarkTargetComponent = pExynosComponent->propagateMarkType.hMarkTargetComponent;
            pBufferHdr->pMarkData            = pExynosComponent->propagateMarkType.pMarkData;

            pExynosComponent->propagateMarkType.hMarkTargetComponent    = NULL;
            pExynosComponent->propagateMarkType.pMarkData               = NULL;
        }

        if ((pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] send event(OMX_EventBufferFlag)",
                                                pExynosComponent, __FUNCTION__);
            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                            pExynosComponent->callbackData,
                            OMX_EventBufferFlag,
                            OUTPUT_PORT_INDEX,
                            pBufferHdr->nFlags, NULL);
        }

        Exynos_OMX_OutputBufferReturn(pOMXComponent, pBufferHdr);
    }

    /* reset dataBuffer */
    Exynos_ResetDataBuffer(pDataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OutputBufferGetQueue(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE          ret              = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT   *pExynosPort      = NULL;
    EXYNOS_OMX_MESSAGE    *message          = NULL;
    EXYNOS_OMX_DATABUFFER *outputUseBuffer  = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pExynosPort->bufferProcessType & (BUFFER_COPY | BUFFER_COPY_FORCE)) {
        outputUseBuffer = &(pExynosPort->way.port2WayDataBuffer.outputDataBuffer);
    } else if (pExynosPort->bufferProcessType == BUFFER_SHARE) {
        outputUseBuffer = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);
    } else {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> wait(bufferSemID)",
                                                    pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> post(bufferSemID)",
                                                    pExynosComponent, __FUNCTION__);
        if (outputUseBuffer->dataValid != OMX_TRUE) {
            message = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
            if (message == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (message->type == EXYNOS_OMX_CommandFakeBuffer) {
                Exynos_OSAL_Free(message);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecFlush;
                goto EXIT;
            }

            outputUseBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
            outputUseBuffer->allocSize     = outputUseBuffer->bufferHeader->nAllocLen;
            outputUseBuffer->dataLen       = 0; //dataBuffer->bufferHeader->nFilledLen;
            outputUseBuffer->remainDataLen = outputUseBuffer->dataLen;
            outputUseBuffer->usedDataLen   = 0; //dataBuffer->bufferHeader->nOffset;
            outputUseBuffer->dataValid     = OMX_TRUE;
            /* dataBuffer->nFlags             = dataBuffer->bufferHeader->nFlags; */
            /* dataBuffer->nTimeStamp         = dataBuffer->bufferHeader->nTimeStamp; */
/*
            if (pExynosPort->bufferProcessType == BUFFER_SHARE)
                outputUseBuffer->pPrivate      = outputUseBuffer->bufferHeader->pOutputPortPrivate;
            else if (pExynosPort->bufferProcessType & BUFFER_COPY) {
                pExynosPort->processData.dataBuffer = outputUseBuffer->bufferHeader->pBuffer;
                pExynosPort->processData.allocSize  = outputUseBuffer->bufferHeader->nAllocLen;
            }
*/
            Exynos_OSAL_Free(message);
        }

        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;

}

OMX_BUFFERHEADERTYPE *Exynos_OutputBufferGetQueue_Direct(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_BUFFERHEADERTYPE  *retBuffer    = NULL;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;
    EXYNOS_OMX_MESSAGE    *message      = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        retBuffer = NULL;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        retBuffer = NULL;
        goto EXIT;
    } else if ((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> wait(bufferSemID)",
                                                    pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> post(bufferSemID)",
                                                    pExynosComponent, __FUNCTION__);

        message = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
        if (message == NULL) {
            retBuffer = NULL;
            goto EXIT;
        }

        if (message->type == EXYNOS_OMX_CommandFakeBuffer) {
            Exynos_OSAL_Free(message);
            retBuffer = NULL;
            goto EXIT;
        }

        retBuffer  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
        Exynos_OSAL_Free(message);
    }

EXIT:
    FunctionOut();

    return retBuffer;
}

OMX_ERRORTYPE Exynos_CodecBufferEnQueue(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 PortIndex, OMX_PTR data)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (PortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[PortIndex]);

    if (data == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    ret = Exynos_OSAL_Queue(&pExynosPort->codecBufferQ, (void *)data);
    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    Exynos_OSAL_SemaphorePost(pExynosPort->codecSemID);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferDeQueue(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 PortIndex, OMX_PTR *data)
{
    OMX_ERRORTYPE          ret         = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT   *pExynosPort = NULL;
    OMX_PTR                tempData    = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (PortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[PortIndex]);

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port -> wait(codecSemID)",
                                                pExynosComponent, __FUNCTION__,
                                                (PortIndex == INPUT_PORT_INDEX)? "input":"output");
    Exynos_OSAL_SemaphoreWait(pExynosPort->codecSemID);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port -> wait(codecSemID)",
                                                pExynosComponent, __FUNCTION__,
                                                (PortIndex == INPUT_PORT_INDEX)? "input":"output");

    tempData = (OMX_PTR)Exynos_OSAL_Dequeue(&pExynosPort->codecBufferQ);
    if (tempData == NULL) {
        *data = NULL;
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    *data = tempData;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferReset(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 PortIndex)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (PortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[PortIndex]);

    ret = Exynos_OSAL_ResetQueue(&pExynosPort->codecBufferQ);
    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    while (1) {
        OMX_S32 cnt = 0;
        Exynos_OSAL_Get_SemaphoreCount(pExynosPort->codecSemID, &cnt);
        if (cnt > 0)
            Exynos_OSAL_SemaphoreWait(pExynosPort->codecSemID);
        else
            break;
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoDecodeGetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (ComponentParameterStructure == NULL)) {
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

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoInit:
    {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)ComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        portParam->nPorts           = pExynosComponent->portParam.nPorts;
        portParam->nStartPortNumber = pExynosComponent->portParam.nStartPortNumber;
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
        OMX_U32                         nPortIndex  = pPortFormat->nPortIndex;
        OMX_U32                         nIndex      = pPortFormat->nIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef    = NULL;

        OMX_BOOL bFormatSupport = OMX_FALSE;

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((nPortIndex >= pExynosComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }


        if (nPortIndex == INPUT_PORT_INDEX) {
            if (nIndex > (INPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1)) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
            pPortDef    = &pExynosPort->portDefinition;

            pPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
            pPortFormat->eColorFormat       = pPortDef->format.video.eColorFormat;
        } else if (nPortIndex == OUTPUT_PORT_INDEX) {
            if (nIndex > (OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1)) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            pPortDef    = &pExynosPort->portDefinition;
            pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;

            if (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED) {
                if (pExynosPort->supportFormat[nIndex] == OMX_COLOR_FormatUnused) {
                    ret = OMX_ErrorNoMore;
                    goto EXIT;
                }
                pPortFormat->eColorFormat = pExynosPort->supportFormat[nIndex];
            } else {
                if (nIndex > 0) {
                    ret = OMX_ErrorNoMore;
                    goto EXIT;
                }

                bFormatSupport = pVideoDec->exynos_codec_checkFormatSupport(pExynosComponent, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled);
                if (bFormatSupport == OMX_TRUE)
                    pPortFormat->eColorFormat = Exynos_OSAL_OMX2HALPixelFormat((OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled, pExynosPort->ePlaneType);
                else
                    pPortFormat->eColorFormat = Exynos_OSAL_OMX2HALPixelFormat(OMX_COLOR_FormatYUV420SemiPlanar, pExynosPort->ePlaneType);
            }
        }

        ret = OMX_ErrorNone;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamDescribeColorFormat:
    case OMX_IndexParamGetAndroidNativeBuffer:
    {
        ret = Exynos_OSAL_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
        break;
#endif
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       nPortIndex     = portDefinition->nPortIndex;

        ret = Exynos_OMX_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
#ifdef USE_ANDROID
        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if ((pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC) ||
            (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE)) {
            switch (pVideoDec->eDataType) {
            case DATA_TYPE_8BIT_WITH_2BIT:
                portDefinition->format.video.eColorFormat =
                    (OMX_COLOR_FORMATTYPE)Exynos_OSAL_OMX2HALPixelFormat((OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar,
                                                    pExynosPort->ePlaneType);
                break;
            case DATA_TYPE_10BIT:
                portDefinition->format.video.eColorFormat =
                    (OMX_COLOR_FORMATTYPE)Exynos_OSAL_OMX2HALPixelFormat((OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16,
                                                    pExynosPort->ePlaneType);
                break;
            case DATA_TYPE_8BIT_SBWC:
                 portDefinition->format.video.eColorFormat =
                    (OMX_COLOR_FORMATTYPE)Exynos_OSAL_OMX2HALPixelFormat((OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC,
                                                    pExynosPort->ePlaneType);
                break;
            case DATA_TYPE_10BIT_SBWC:
                 portDefinition->format.video.eColorFormat =
                    (OMX_COLOR_FORMATTYPE)Exynos_OSAL_OMX2HALPixelFormat((OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC,
                                                    pExynosPort->ePlaneType);
                break;
            default:
                portDefinition->format.video.eColorFormat =
                    (OMX_COLOR_FORMATTYPE)Exynos_OSAL_OMX2HALPixelFormat(portDefinition->format.video.eColorFormat, pExynosPort->ePlaneType);
                break;
            }

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] portDefinition->format.video.eColorFormat: 0x%x",
                                                    pExynosComponent, __FUNCTION__, portDefinition->format.video.eColorFormat);
        }
#endif
        ret = Exynos_OSAL_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
        if (ret != OMX_ErrorNone)
            goto EXIT;
    }
        break;
    case OMX_IndexVendorNeedContigMemory:
    {
        EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE  *pPortMemType    = (EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *)ComponentParameterStructure;
        OMX_U32                              nPortIndex      = pPortMemType->nPortIndex;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPortMemType, sizeof(EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        pPortMemType->bNeedContigMem = pExynosPort->bNeedContigMem;
    }
        break;
    case OMX_IndexExynosParamCorruptedHeader:
    {
        EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER  *pCorruptedHeader = (EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER *)ComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pCorruptedHeader, sizeof(EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pCorruptedHeader->bDiscardEvent = pVideoDec->bDiscardCSDError;
    }
        break;
    case OMX_IndexParamVideoCompressedColorFormat:
    {
        OMX_PARAM_U32TYPE *pColorFormat = (OMX_PARAM_U32TYPE *)ComponentParameterStructure;

        if (pColorFormat->nPortIndex != OUTPUT_PORT_INDEX) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid port index", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pColorFormat, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pColorFormat->nU32 = pVideoDec->nCompColorFormat;

        ret = OMX_ErrorNone;
    }
        break;
    default:
    {
        ret = Exynos_OMX_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}
OMX_ERRORTYPE Exynos_OMX_VideoDecodeSetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (ComponentParameterStructure == NULL)) {
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

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
        OMX_U32                         nPortIndex  = pPortFormat->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef    = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((nPortIndex >= pExynosComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pPortDef->format.video.eColorFormat       = pPortFormat->eColorFormat;
            pPortDef->format.video.eCompressionFormat = pPortFormat->eCompressionFormat;
            pPortDef->format.video.xFramerate         = pPortFormat->xFramerate;

            if ((nPortIndex == OUTPUT_PORT_INDEX) &&
                (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
                pPortDef->format.video.eColorFormat = Exynos_OSAL_HAL2OMXColorFormat(pPortFormat->eColorFormat);
            }

            Exynos_UpdateFrameSize(pOMXComponent);
        }
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE  *pPortDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                        portIndex       = pPortDefinition->nPortIndex;
        OMX_U32 size;
        OMX_U32 realWidth, realHeight;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        if (portIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = Exynos_OMX_Check_SizeVersion(pPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[portIndex];

        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            if (pExynosPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }
        if (pPortDefinition->nBufferCountActual < pExynosPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

#ifdef USE_ANDROID
        /* to prevent a over-allocation about reserved memory */
        if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
            (portIndex == OUTPUT_PORT_INDEX)) {
            int nDisExtBufCnt = Exynos_OSAL_GetDisplayExtraBufferCount();

            if (pPortDefinition->nBufferCountActual > (pExynosPort->portDefinition.nBufferCountMin + nDisExtBufCnt)) {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
        }
#endif

        Exynos_OSAL_Memcpy(((char *)&pExynosPort->portDefinition) + nOffset,
                           ((char *)pPortDefinition) + nOffset,
                           pPortDefinition->nSize - nOffset);

#ifdef USE_ANDROID // Modified by Google engineer
        /* should not affect the format since in ANB case, the caller
                * is providing us a HAL format */
        if ((pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC) ||
            (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE)) {
            pExynosPort->portDefinition.format.video.eColorFormat =
                Exynos_OSAL_HAL2OMXColorFormat(pExynosPort->portDefinition.format.video.eColorFormat);
        }
#endif

        realWidth = pExynosPort->portDefinition.format.video.nFrameWidth;
        realHeight = pExynosPort->portDefinition.format.video.nFrameHeight;
        size = (ALIGN(realWidth, 16) * ALIGN(realHeight, 16) * 3) / 2;
        pExynosPort->portDefinition.format.video.nStride = realWidth;
        pExynosPort->portDefinition.format.video.nSliceHeight = realHeight;
        pExynosPort->portDefinition.nBufferSize = (size > pExynosPort->portDefinition.nBufferSize) ? size : pExynosPort->portDefinition.nBufferSize;

        if (portIndex == INPUT_PORT_INDEX) {
            if (pExynosPort->portDefinition.nBufferSize < pVideoDec->nMinInBufSize)
                pExynosPort->portDefinition.nBufferSize = pVideoDec->nMinInBufSize;

            if (IS_CUSTOM_COMPONENT(pExynosComponent->componentName) == OMX_TRUE) {
                if ((realWidth * realHeight) > (4096 * 2160)) {
                    pExynosPort->portDefinition.nBufferSize = CUSTOM_OVER_4K_VIDEO_INPUT_BUFFER_SIZE;
                } else {
                    pExynosPort->portDefinition.nBufferSize = CUSTOM_DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
                }
#ifdef USE_SMALL_SECURE_MEMORY
                if ((pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) &&
                    (pExynosPort->portDefinition.nBufferSize > CUSTOM_LIMITED_DRM_INPUT_BUFFER_SIZE)) {
                    pExynosPort->portDefinition.nBufferSize = CUSTOM_LIMITED_DRM_INPUT_BUFFER_SIZE;
                    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] use limited input size(%d) for DRM",
                                                        pExynosComponent, __FUNCTION__, pExynosPort->portDefinition.nBufferSize);
                }
#endif
            }

            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] the size of input buffer is %d",
                                                pExynosComponent, __FUNCTION__, pExynosPort->portDefinition.nBufferSize);

            Exynos_UpdateFrameSize(pOMXComponent);
        }
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamEnableAndroidBuffers:
    case OMX_IndexParamStoreMetaDataBuffer:
    case OMX_IndexParamAllocateNativeHandle:
    case OMX_IndexParamAndroidNatvieBufferConsumerUsage:
    {
        ret = Exynos_OSAL_SetParameter(hComponent, nIndex, ComponentParameterStructure);
    }
        break;
#endif
    case OMX_IndexVendorNeedContigMemory:
    {
        EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE  *pPortMemType    = (EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *)ComponentParameterStructure;
        OMX_U32                              nPortIndex      = pPortMemType->nPortIndex;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPortMemType, sizeof(EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            if (pExynosPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        pExynosPort->bNeedContigMem = pPortMemType->bNeedContigMem;
    }
        break;
    case OMX_IndexVendorSetDTSMode:
    {
        EXYNOS_OMX_VIDEO_PARAM_DTSMODE  *pDTSParam  = (EXYNOS_OMX_VIDEO_PARAM_DTSMODE *)ComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDTSParam, sizeof(EXYNOS_OMX_VIDEO_PARAM_DTSMODE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoDec->bDTSMode = pDTSParam->bDTSMode;
    }
        break;
    case OMX_IndexParamEnableThumbnailMode:
    {
        EXYNOS_OMX_VIDEO_THUMBNAILMODE  *pThumbnailMode = (EXYNOS_OMX_VIDEO_THUMBNAILMODE *)ComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pThumbnailMode, sizeof(EXYNOS_OMX_VIDEO_THUMBNAILMODE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoDec->bThumbnailMode = pThumbnailMode->bEnable;
        if (pVideoDec->bThumbnailMode == OMX_TRUE) {
            EXYNOS_OMX_BASEPORT *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            pExynosOutputPort->portDefinition.nBufferCountMin    = 1;
            pExynosOutputPort->portDefinition.nBufferCountActual = 1;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexExynosParamCorruptedHeader:
    {
        EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER  *pCorruptedHeader   = (EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER *)ComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pCorruptedHeader, sizeof(EXYNOS_OMX_VIDEO_PARAM_CORRUPTEDHEADER));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoDec->bDiscardCSDError = pCorruptedHeader->bDiscardEvent;
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexExynosParamImageCrop:
    {
        EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP *pImageCrop = (EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP *)ComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pImageCrop, sizeof(EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

        pExynosComponent->bUseImgCrop = pImageCrop->bEnabled;

        if (pExynosComponent->bUseImgCrop == OMX_TRUE) {
            pExynosPort->bufferProcessType |= BUFFER_COPY_FORCE;
        } else {
            pExynosPort->bufferProcessType &= (~BUFFER_COPY_FORCE);
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoCompressedColorFormat:
    {
        OMX_PARAM_U32TYPE *pColorFormat = (OMX_PARAM_U32TYPE *)ComponentParameterStructure;

        if (pColorFormat->nPortIndex != OUTPUT_PORT_INDEX) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid port index", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pColorFormat, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

#ifndef USE_COMPRESSED_COLOR
        if ((pColorFormat->nU32 < OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC) &&
            (pColorFormat->nU32 > OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L80)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, pColorFormat->nU32);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
#endif

        pVideoDec->nCompColorFormat = (OMX_COLOR_FORMATTYPE)pColorFormat->nU32;

        ret = OMX_ErrorNone;
    }
        break;
    default:
    {
        ret = Exynos_OMX_SetParameter(hComponent, nIndex, ComponentParameterStructure);
    }
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoDecodeGetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;

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

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexVendorGetBufferFD:
    {
        EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO *pBufferInfo = (EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pBufferInfo, sizeof(EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pBufferInfo->fd = Exynos_OSAL_SharedMemory_VirtToION(pVideoDec->hSharedMemory, pBufferInfo->pVirAddr);
    }
        break;
    case OMX_IndexExynosConfigPTSMode: /* MSRND */
    {
        *((OMX_BOOL *)pComponentConfigStructure) = (pVideoDec->bDTSMode == OMX_TRUE)? OMX_FALSE:OMX_TRUE;
    }
        break;
    case OMX_IndexConfigBlackBarCrop:
    {
        OMX_CONFIG_RECTTYPE *pBlackBarCropRect = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pBlackBarCropRect, sizeof(OMX_CONFIG_RECTTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pBlackBarCropRect->nPortIndex != OUTPUT_PORT_INDEX) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] This config is vaild at Output port only", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Memcpy(pBlackBarCropRect, &pVideoDec->blackBarCropRect, sizeof(OMX_CONFIG_RECTTYPE));
    }
        break;
    case OMX_IndexConfigCommonInputCrop:
    {
        OMX_CONFIG_RECTTYPE *pDstRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pSrcRectType = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].cropRectangle[IMG_CROP_INPUT_PORT]);

        if (pDstRectType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstRectType->nTop      = pSrcRectType->nTop;
        pDstRectType->nLeft     = pSrcRectType->nLeft;
        pDstRectType->nHeight   = pSrcRectType->nHeight;
        pDstRectType->nWidth    = pSrcRectType->nWidth;
    }
        break;
    case OMX_IndexConfigCommonOutputCrop:
    {
        OMX_CONFIG_RECTTYPE *pDstRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pSrcRectType = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].cropRectangle[IMG_CROP_OUTPUT_PORT]);

        if (pDstRectType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
            ret = OMX_ErrorBadParameter;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] image cropping is not enabled.", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDstRectType->nTop      = pSrcRectType->nTop;
        pDstRectType->nLeft     = pSrcRectType->nLeft;
        pDstRectType->nHeight   = pSrcRectType->nHeight;
        pDstRectType->nWidth    = pSrcRectType->nWidth;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoColorAspects:
    {
        ret = Exynos_OSAL_GetConfig(hComponent, nIndex, pComponentConfigStructure);
    }
        break;
    case OMX_IndexConfigAndroidVendorExtension:
    {
        ret = Exynos_OSAL_GetVendorExt(hComponent, pComponentConfigStructure);
    }
        break;
#endif
    default:
        ret = Exynos_OMX_GetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoDecodeSetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;

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

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigOperatingRate:  /* since M version */
    {
        OMX_PARAM_U32TYPE *pConfigRate = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
        OMX_U32            xFramerate  = 0;

        ret = Exynos_OMX_Check_SizeVersion(pConfigRate, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        xFramerate = pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.xFramerate;
        pVideoDec->nQosRatio = pConfigRate->nU32 >> 16;

        if (pVideoDec->nQosRatio == (((OMX_U32)INT_MAX) >> 16)) {
            pVideoDec->nOperatingRate = (OMX_U32)INT_MAX;
            pVideoDec->nQosRatio = 1000;
        } else {
            pVideoDec->nOperatingRate = (pConfigRate->nU32 >> 16) * 1000;
            pVideoDec->nQosRatio = ((xFramerate >> 16) == 0)? 100:(OMX_U32)((pConfigRate->nU32 / (double)xFramerate) * 100);
        }

        pVideoDec->bQosChanged = OMX_TRUE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] set operating rate(0x%x), qos ratio(0x%x)",
                                                        pExynosComponent, __FUNCTION__, pVideoDec->nOperatingRate, pVideoDec->nQosRatio);

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexConfigPriority:
    {
        OMX_PARAM_U32TYPE *pPriority = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
        OMX_U32            priority  = 0;

        ret = Exynos_OMX_Check_SizeVersion(pPriority, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoDec->nPriority = pPriority->nU32;;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] priority : 0x%x", pExynosComponent, __FUNCTION__, pVideoDec->nPriority);

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexExynosConfigPTSMode: /* MSRND */
    {
        pVideoDec->bDTSMode = ((*((OMX_BOOL *)pComponentConfigStructure)) == OMX_TRUE)? OMX_FALSE:OMX_TRUE;
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexConfigSearchBlackBar:
    {
        OMX_CONFIG_BOOLEANTYPE *pConfigSearchBlackBar = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pConfigSearchBlackBar, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoDec->bSearchBlackBar        = pConfigSearchBlackBar->bEnabled;
        pVideoDec->bSearchBlackBarChanged = OMX_TRUE;

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexConfigCommonInputCrop:
    {
        OMX_CONFIG_RECTTYPE *pSrcRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pDstRectType = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].cropRectangle[IMG_CROP_INPUT_PORT]);

        if (pSrcRectType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
            ret = OMX_ErrorBadParameter;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] image cropping is not enabled.", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDstRectType->nTop      = pSrcRectType->nTop;
        pDstRectType->nLeft     = pSrcRectType->nLeft;
        pDstRectType->nHeight   = pSrcRectType->nHeight;
        pDstRectType->nWidth    = pSrcRectType->nWidth;

        pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bUseImgCrop[IMG_CROP_INPUT_PORT] = OMX_TRUE;
    }
        break;
    case OMX_IndexConfigCommonOutputCrop:
    {
        OMX_CONFIG_RECTTYPE *pSrcRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pDstRectType = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].cropRectangle[IMG_CROP_OUTPUT_PORT]);

        if (pSrcRectType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
            ret = OMX_ErrorIncorrectStateOperation;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] image cropping is not enabled.", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDstRectType->nTop      = pSrcRectType->nTop;
        pDstRectType->nLeft     = pSrcRectType->nLeft;
        pDstRectType->nHeight   = pSrcRectType->nHeight;
        pDstRectType->nWidth    = pSrcRectType->nWidth;

        pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bUseImgCrop[IMG_CROP_OUTPUT_PORT] = OMX_TRUE;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoColorAspects:
    {
        ret = Exynos_OSAL_SetConfig(hComponent, nIndex, pComponentConfigStructure);
    }
        break;
    case OMX_IndexConfigAndroidVendorExtension:
    {
        ret = Exynos_OSAL_SetVendorExt(hComponent, pComponentConfigStructure);
    }
        break;
    case OMX_IndexConfigLowLatency:
    {
        OMX_CONFIG_BOOLEANTYPE *pConfigLowLatency = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pConfigLowLatency, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        /* nothing to do */

        ret = OMX_ErrorNone;
    }
        break;
#endif
    default:
        ret = Exynos_OMX_SetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoDecodeGetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;

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

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_NEED_CONTIG_MEMORY) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorNeedContigMemory;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_GET_BUFFER_FD) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorGetBufferFD;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_SET_DTS_MODE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorSetDTSMode;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ENABLE_THUMBNAIL) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamEnableThumbnailMode;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_BLACK_BAR_CROP_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigBlackBarCrop;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_SEARCH_BLACK_BAR) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigSearchBlackBar;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_USE_ANB2) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamUseAndroidNativeBuffer2;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_GET_ANB) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamGetAndroidNativeBuffer;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ENABLE_ANB) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamEnableAndroidBuffers;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamStoreMetaDataBuffer;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ALLOCATE_NATIVE_HANDLE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamAllocateNativeHandle;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_DESCRIBE_COLOR_FORMAT) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamDescribeColorFormat;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_COLOR_ASPECTS_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoColorAspects;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_HDR_STATIC_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoHdrStaticInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_SET_ANB_CONSUMER_USAGE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamAndroidNatvieBufferConsumerUsage;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    if (IS_CUSTOM_COMPONENT(pExynosComponent->componentName) == OMX_TRUE) {
        if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_CUSTOM_INDEX_CONFIG_PTS_MODE) == 0) {
            *pIndexType = (OMX_INDEXTYPE) OMX_IndexExynosConfigPTSMode;
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_CUSTOM_INDEX_PARAM_CORRUPTEDHEADER) == 0) {
            *pIndexType = (OMX_INDEXTYPE) OMX_IndexExynosParamCorruptedHeader;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    ret = Exynos_OMX_GetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Shared_BufferToData(
    EXYNOS_OMX_BASEPORT     *pExynosPort,
    EXYNOS_OMX_DATABUFFER   *pUseBuffer,
    EXYNOS_OMX_DATA         *pData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if ((pExynosPort == NULL) ||
        (pUseBuffer == NULL) ||
        (pData == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if ((pUseBuffer->bufferHeader == NULL) ||
        (pUseBuffer->bufferHeader->pBuffer == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosPort->exceptionFlag != GENERAL_STATE) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED) {
        pData->buffer.addr[0] = pUseBuffer->bufferHeader->pBuffer;
    } else {
        /* metadata type */
        EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;

        if (pExynosPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK) {
            EXYNOS_OMX_LOCK_RANGE range;
            OMX_U32 stride;

            range.nWidth        = pExynosPort->portDefinition.format.video.nFrameWidth;
            range.nHeight       = pExynosPort->portDefinition.format.video.nFrameHeight;
            range.eColorFormat  = pExynosPort->portDefinition.format.video.eColorFormat;
            stride              = range.nWidth;

            ret = Exynos_OSAL_LockMetaData(pUseBuffer->bufferHeader->pBuffer,
                                           range,
                                           &stride, &bufferInfo,
                                           pExynosPort->eMetaDataType);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]: Failed to Exynos_OSAL_LockMetaData (err:0x%x)",
                                    __FUNCTION__, ret);
                goto EXIT;
            }
        } else {
            ret = Exynos_OSAL_GetInfoFromMetaData(pUseBuffer->bufferHeader->pBuffer,
                                                    &bufferInfo, pExynosPort->eMetaDataType);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]: Failed to Exynos_OSAL_GetInfoFromMetaData (err:0x%x)",
                                    __FUNCTION__, ret);
                goto EXIT;
            }
        }

        pData->buffer.addr[0] = bufferInfo.addr[0];
        pData->buffer.addr[1] = bufferInfo.addr[1];
        pData->buffer.addr[2] = bufferInfo.addr[2];

        pData->buffer.fd[0] = bufferInfo.fd[0];
        pData->buffer.fd[1] = bufferInfo.fd[1];
        pData->buffer.fd[2] = bufferInfo.fd[2];
    }

    pData->allocSize     = pUseBuffer->allocSize;
    pData->dataLen       = pUseBuffer->dataLen;
    pData->usedDataLen   = pUseBuffer->usedDataLen;
    pData->remainDataLen = pUseBuffer->remainDataLen;
    pData->timeStamp     = pUseBuffer->timeStamp;
    pData->nFlags        = pUseBuffer->nFlags;
    pData->pPrivate      = pUseBuffer->pPrivate;
    pData->bufferHeader  = pUseBuffer->bufferHeader;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Shared_DataToBuffer(
    EXYNOS_OMX_BASEPORT     *pExynosPort,
    EXYNOS_OMX_DATABUFFER   *pUseBuffer,
    EXYNOS_OMX_DATA         *pData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if ((pExynosPort == NULL) ||
        (pUseBuffer == NULL) ||
        (pData == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if ((pData->bufferHeader == NULL) ||
        (pData->bufferHeader->pBuffer == NULL)) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    pUseBuffer->bufferHeader          = pData->bufferHeader;
    pUseBuffer->allocSize             = pData->allocSize;
    pUseBuffer->dataLen               = pData->dataLen;
    pUseBuffer->usedDataLen           = pData->usedDataLen;
    pUseBuffer->remainDataLen         = pData->remainDataLen;
    pUseBuffer->timeStamp             = pData->timeStamp;
    pUseBuffer->nFlags                = pData->nFlags;
    pUseBuffer->pPrivate              = pData->pPrivate;

    if (pExynosPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK)
        Exynos_OSAL_UnlockMetaData(pUseBuffer->bufferHeader->pBuffer, pExynosPort->eMetaDataType);

EXIT:
    FunctionOut();

    return ret;
}
