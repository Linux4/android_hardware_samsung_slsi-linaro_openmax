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
 * @file        Exynos_OMX_VencControl.c
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
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_VencControl.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_SharedMemory.h"

#include "Exynos_OSAL_Platform.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VIDEO_ENCCONTROL"
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
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE     *pTempBufferHdr    = NULL;
    OMX_U32                   i                 = 0;

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

    pTempBufferHdr = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (pTempBufferHdr == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = pTempBufferHdr;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(pTempBufferHdr, OMX_BUFFERHEADERTYPE);

            pTempBufferHdr->pBuffer        = pBuffer;
            pTempBufferHdr->nAllocLen      = nSizeBytes;
            pTempBufferHdr->pAppPrivate    = pAppPrivate;

            if (nPortIndex == INPUT_PORT_INDEX)
                pTempBufferHdr->nInputPortIndex = INPUT_PORT_INDEX;
            else
                pTempBufferHdr->nOutputPortIndex = OUTPUT_PORT_INDEX;

            pExynosPort->assignedBufferNum++;

            *ppBufferHdr = pTempBufferHdr;

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
                                        (*ppBufferHdr), nSizeBytes);
            goto EXIT;
        }
    }

    Exynos_OSAL_Free(pTempBufferHdr);
    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *pTempBufferHdr     = NULL;
    OMX_U8                          *pTempBuffer        = NULL;
    unsigned long                    fdTempBuffer       = 0;
    MEMORY_TYPE                      eMemType           = NORMAL_MEMORY;
    OMX_U32                          i                  = 0;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

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

    if ((pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) &&
        (nPortIndex == OUTPUT_PORT_INDEX))
        eMemType |= SECURE_MEMORY;

    if (pExynosPort->bNeedContigMem == OMX_TRUE)
        eMemType |= CONTIG_MEMORY;

    if (!((nPortIndex == INPUT_PORT_INDEX) &&
          (pExynosPort->bufferProcessType & BUFFER_SHARE)))
        eMemType |= CACHED_MEMORY;

    if ((nPortIndex == OUTPUT_PORT_INDEX) &&
        (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
        eMemType |= CONTIG_MEMORY;  /* always set for HDCP */

#ifdef USE_VIDEO_EXT_FOR_WFD_HDCP
        if ((eMemType & SECURE_MEMORY) ||  // secure component
            (eMemType & CONTIG_MEMORY)) {  // Normal DRM
            eMemType |= EXT_MEMORY;
        }
#endif
        pTempBuffer = Exynos_OSAL_AllocMetaDataBuffer(pVideoEnc->hSharedMemory,
                                                      pExynosPort->eMetaDataType,
                                                      nSizeBytes,
                                                      eMemType);
        if (pTempBuffer == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Alloc", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    } else {
        pTempBuffer = Exynos_OSAL_SharedMemory_Alloc(pVideoEnc->hSharedMemory, nSizeBytes, eMemType);
        if (pTempBuffer == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Alloc", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        fdTempBuffer = Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, pTempBuffer);
    }

    pTempBufferHdr = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (pTempBufferHdr == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = pTempBufferHdr;
            pExynosPort->extendBufferHeader[i].buf_fd[0] = fdTempBuffer;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ALLOCATED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(pTempBufferHdr, OMX_BUFFERHEADERTYPE);

            if ((pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) &&
                (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
                pTempBufferHdr->pBuffer = (void *)fdTempBuffer;
            } else {
                pTempBufferHdr->pBuffer = pTempBuffer;
            }

            pTempBufferHdr->nAllocLen      = nSizeBytes;
            pTempBufferHdr->pAppPrivate    = pAppPrivate;

            if (nPortIndex == INPUT_PORT_INDEX)
                pTempBufferHdr->nInputPortIndex = INPUT_PORT_INDEX;
            else
                pTempBufferHdr->nOutputPortIndex = OUTPUT_PORT_INDEX;

            pExynosPort->assignedBufferNum++;

            *ppBufferHdr = pTempBufferHdr;

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
                                                    (*ppBufferHdr), nSizeBytes);
            goto EXIT;
        }
    }

    ret = OMX_ErrorInsufficientResources;

EXIT:
    if (ret == OMX_ErrorInsufficientResources) {
        if (pTempBufferHdr != NULL)
            Exynos_OSAL_Free(pTempBufferHdr);

        if (pTempBuffer != NULL) {
            if ((nPortIndex == OUTPUT_PORT_INDEX) &&
                (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
                Exynos_OSAL_FreeMetaDataBuffer(pVideoEnc->hSharedMemory, pExynosPort->eMetaDataType, pTempBuffer);
            } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) &&
                       (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
                OMX_PTR mapBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, fdTempBuffer);

                Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, pTempBuffer);
            } else {
                Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, pTempBuffer);
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
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *pOMXBufferHdr      = NULL;
    OMX_U32                          i                  = 0;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, nPortIndex);
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (CHECK_PORT_TUNNELED(pExynosPort) &&
        CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

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

    for (i = 0; i < /*pExynosPort->portDefinition.nBufferCountActual*/MAX_BUFFER_NUM; i++) {
        if ((pExynosPort->bufferStateAllocate[i] != BUFFER_STATE_FREE) &&
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
                    if ((nPortIndex == OUTPUT_PORT_INDEX) &&
                        (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
                        Exynos_OSAL_FreeMetaDataBuffer(pVideoEnc->hSharedMemory,
                                                       pExynosPort->eMetaDataType,
                                                       pOMXBufferHdr->pBuffer);
                    } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) &&
                               (pExynosPort->eMetaDataType == OUTPUT_PORT_INDEX)) {
                        unsigned long ionFD = (unsigned long)pOMXBufferHdr->pBuffer;

                        OMX_PTR mapBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, ionFD);
                        Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, mapBuffer);
                    } else {
                        Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, pOMXBufferHdr->pBuffer);
                    }

                    pOMXBufferHdr->pBuffer  = NULL;
                    pBufferHdr->pBuffer     = NULL;
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
OMX_ERRORTYPE Exynos_OMX_AllocateTunnelBuffer(
    EXYNOS_OMX_BASEPORT     *pOMXBasePort,
    OMX_U32                  nPortIndex)
{
    OMX_ERRORTYPE                 ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT          *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE         *pTempBufferHdr    = NULL;
    OMX_U8                       *pTempBuffer       = NULL;
    OMX_U32                       nBufferSize       = 0;
    OMX_PARAM_PORTDEFINITIONTYPE  portDefinition;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FreeTunnelBuffer(
    EXYNOS_OMX_BASEPORT     *pOMXBasePort,
    OMX_U32                  nPortIndex)
{
    OMX_ERRORTYPE            ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT     *pExynosPort    = NULL;
    OMX_BUFFERHEADERTYPE    *pTempBufferHdr = NULL;
    OMX_U8                  *pTempBuffer    = NULL;
    OMX_U32                  nBufferSize    = 0;

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

OMX_ERRORTYPE Exynos_OMX_GetFlushBuffer(
    EXYNOS_OMX_BASEPORT     *pExynosPort,
    EXYNOS_OMX_DATABUFFER   *pDataBuffer[])
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

OMX_ERRORTYPE Exynos_OMX_FlushPort(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_S32              nPortIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE     *pBufferHdr        = NULL;
    EXYNOS_OMX_DATABUFFER    *pDataBuffer[2]    = {NULL, NULL};
    EXYNOS_OMX_MESSAGE       *pMessage          = NULL;
    OMX_S32                   nSemaCnt          = 0;
    int                       i                 = 0;

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

    if ((nPortIndex < 0) ||
        (nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    while (Exynos_OSAL_GetElemNum(&pExynosPort->bufferQ) > 0) {
        Exynos_OSAL_Get_SemaphoreCount(pExynosPort->bufferSemID, &nSemaCnt);
        if (nSemaCnt == 0)
            Exynos_OSAL_SemaphorePost(pExynosPort->bufferSemID);

        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
        if ((pMessage != NULL) &&
            (pMessage->type != EXYNOS_OMX_CommandFakeBuffer)) {
            pBufferHdr = (OMX_BUFFERHEADERTYPE *)pMessage->pCmdData;
            pBufferHdr->nFilledLen = 0;

            if (nPortIndex == OUTPUT_PORT_INDEX) {
                Exynos_OMX_OutputBufferReturn(pOMXComponent, pBufferHdr);
            } else if (nPortIndex == INPUT_PORT_INDEX) {
                Exynos_OMX_InputBufferReturn(pOMXComponent, pBufferHdr);
            }
        }
        Exynos_OSAL_Free(pMessage);
        pMessage = NULL;
    }

    Exynos_OMX_GetFlushBuffer(pExynosPort, pDataBuffer);
    if ((pDataBuffer[0] != NULL) &&
        (pDataBuffer[0]->dataValid == OMX_TRUE)) {
        if (nPortIndex == INPUT_PORT_INDEX)
            Exynos_InputBufferReturn(pOMXComponent, pDataBuffer[0]);
        else if (nPortIndex == OUTPUT_PORT_INDEX)
            Exynos_OutputBufferReturn(pOMXComponent, pDataBuffer[0]);
    }
    if ((pDataBuffer[1] != NULL) &&
        (pDataBuffer[1]->dataValid == OMX_TRUE)) {
        if (nPortIndex == INPUT_PORT_INDEX)
            Exynos_InputBufferReturn(pOMXComponent, pDataBuffer[1]);
        else if (nPortIndex == OUTPUT_PORT_INDEX)
            Exynos_OutputBufferReturn(pOMXComponent, pDataBuffer[1]);
    }

    if (pExynosPort->bufferProcessType & BUFFER_SHARE) {
        if (pExynosPort->processData.bufferHeader != NULL) {
            if (nPortIndex == INPUT_PORT_INDEX) {
                if (pExynosPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK)
                    Exynos_OSAL_UnlockMetaData(pExynosPort->processData.bufferHeader->pBuffer, pExynosPort->eMetaDataType);

                Exynos_OMX_InputBufferReturn(pOMXComponent, pExynosPort->processData.bufferHeader);
            } else if (nPortIndex == OUTPUT_PORT_INDEX) {
                Exynos_OMX_OutputBufferReturn(pOMXComponent, pExynosPort->processData.bufferHeader);
            }
        }
        Exynos_ResetCodecData(&pExynosPort->processData);

        for (i = 0; i < (OMX_S32)pExynosPort->portDefinition.nBufferCountActual; i++) {
            if (pExynosPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                if (nPortIndex == OUTPUT_PORT_INDEX) {
                    Exynos_OMX_OutputBufferReturn(pOMXComponent,
                                                  pExynosPort->extendBufferHeader[i].OMXBufferHeader);
                } else if (nPortIndex == INPUT_PORT_INDEX) {
                    if (pExynosPort->eMetaDataType & METADATA_TYPE_BUFFER_LOCK)
                        Exynos_OSAL_UnlockMetaData(pExynosPort->extendBufferHeader[i].OMXBufferHeader->pBuffer, pExynosPort->eMetaDataType);

                    Exynos_OMX_InputBufferReturn(pOMXComponent,
                                                 pExynosPort->extendBufferHeader[i].OMXBufferHeader);
                }
            }
        }
    }

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

OMX_ERRORTYPE Exynos_OMX_BufferFlush(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_S32              nPortIndex,
    OMX_BOOL             bEvent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_DATABUFFER           *pDataBuffer[2]     = {NULL, NULL};

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if ((nPortIndex < 0) ||
        (nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] OMX_CommandFlush start, port:%d, event:%d",
                                    pExynosComponent, __FUNCTION__, nPortIndex, bEvent);

    Exynos_OSAL_SignalSet(pExynosPort->pauseEvent);

    Exynos_OMX_GetFlushBuffer(pExynosPort, pDataBuffer);
    if (pDataBuffer[0] == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosPort->bufferProcessType & BUFFER_COPY)
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

    pVideoEnc->exynos_codec_bufferProcessRun(pOMXComponent, nPortIndex);

    Exynos_OSAL_MutexLock(pDataBuffer[0]->bufferMutex);
    pVideoEnc->exynos_codec_stop(pOMXComponent, nPortIndex);

    if (pDataBuffer[1] != NULL)
        Exynos_OSAL_MutexLock(pDataBuffer[1]->bufferMutex);

    ret = Exynos_OMX_FlushPort(pOMXComponent, nPortIndex);
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pExynosPort->bufferProcessType & BUFFER_COPY)
        pVideoEnc->exynos_codec_enqueueAllBuffer(pOMXComponent, nPortIndex);

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
        }

#ifdef PERFORMANCE_DEBUG
        Exynos_OSAL_CountReset(pExynosPort->hBufferCount);
#endif
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] OMX_CommandFlush end, port:%d, event:%d",
                                        pExynosComponent, __FUNCTION__, nPortIndex, bEvent);

EXIT:
    if (pDataBuffer[1] != NULL)
        Exynos_OSAL_MutexUnlock(pDataBuffer[1]->bufferMutex);

    if (pDataBuffer[0] != NULL)
        Exynos_OSAL_MutexUnlock(pDataBuffer[0]->bufferMutex);

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_InputBufferReturn(
    OMX_COMPONENTTYPE       *pOMXComponent,
    EXYNOS_OMX_DATABUFFER   *pDataBuffer)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pExynosPort        = NULL;
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
    pExynosPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);

    pBufferHdr = pDataBuffer->bufferHeader;

    if (pBufferHdr != NULL) {
        if (pExynosPort->markType.hMarkTargetComponent != NULL) {
            pBufferHdr->hMarkTargetComponent            = pExynosPort->markType.hMarkTargetComponent;
            pBufferHdr->pMarkData                       = pExynosPort->markType.pMarkData;
            pExynosPort->markType.hMarkTargetComponent  = NULL;
            pExynosPort->markType.pMarkData             = NULL;
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

OMX_ERRORTYPE Exynos_InputBufferGetQueue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent)
{
    OMX_ERRORTYPE          ret          = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;
    EXYNOS_OMX_MESSAGE    *pMessage     = NULL;
    EXYNOS_OMX_DATABUFFER *pDataBuffer  = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);
    pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid state(0x%x)",
                                                    pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> wait(bufferSemID)",
                                                    pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input port -> post(bufferSemID)",
                                                    pExynosComponent, __FUNCTION__);

        if (pDataBuffer->dataValid != OMX_TRUE) {
            pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
            if (pMessage == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (pMessage->type == EXYNOS_OMX_CommandFakeBuffer) {
                Exynos_OSAL_Free(pMessage);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecFlush;
                goto EXIT;
            }

            pDataBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(pMessage->pCmdData);
            pDataBuffer->allocSize     = pDataBuffer->bufferHeader->nAllocLen;
            pDataBuffer->dataLen       = pDataBuffer->bufferHeader->nFilledLen;
            pDataBuffer->remainDataLen = pDataBuffer->dataLen;
            pDataBuffer->usedDataLen   = 0;
            pDataBuffer->dataValid     = OMX_TRUE;
            pDataBuffer->nFlags        = pDataBuffer->bufferHeader->nFlags;
            pDataBuffer->timeStamp     = pDataBuffer->bufferHeader->nTimeStamp;

            Exynos_OSAL_Free(pMessage);

            if (pDataBuffer->allocSize < pDataBuffer->dataLen) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] buffer size(%d) is smaller than dataLen(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    pDataBuffer->allocSize, pDataBuffer->dataLen);
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
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE     *pBufferHdr        = NULL;

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
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pDataBuffer != NULL)
        pBufferHdr = pDataBuffer->bufferHeader;

    if (pBufferHdr != NULL) {
        pBufferHdr->nFilledLen = pDataBuffer->remainDataLen;
        pBufferHdr->nOffset    = 0;
        pBufferHdr->nFlags     = pDataBuffer->nFlags;
        pBufferHdr->nTimeStamp = pDataBuffer->timeStamp;

        if ((pExynosPort->eMetaDataType & METADATA_TYPE_DATA) &&
            (pBufferHdr->nFilledLen > 0)) {
            pBufferHdr->nFilledLen = pBufferHdr->nAllocLen;
        }

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

OMX_ERRORTYPE Exynos_OutputBufferGetQueue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent)
{
    OMX_ERRORTYPE          ret          = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;
    EXYNOS_OMX_MESSAGE    *pMessage     = NULL;
    EXYNOS_OMX_DATABUFFER *pDataBuffer  = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pExynosPort->bufferProcessType & BUFFER_COPY) {
        pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.outputDataBuffer);
    } else if (pExynosPort->bufferProcessType & BUFFER_SHARE) {
        pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);
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
        if (pDataBuffer->dataValid != OMX_TRUE) {
            pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
            if (pMessage == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (pMessage->type == EXYNOS_OMX_CommandFakeBuffer) {
                Exynos_OSAL_Free(pMessage);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecFlush;
                goto EXIT;
            }

            pDataBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(pMessage->pCmdData);
            pDataBuffer->allocSize     = pDataBuffer->bufferHeader->nAllocLen;
            pDataBuffer->dataLen       = 0; //dataBuffer->bufferHeader->nFilledLen;
            pDataBuffer->remainDataLen = pDataBuffer->dataLen;
            pDataBuffer->usedDataLen   = 0; //dataBuffer->bufferHeader->nOffset;
            pDataBuffer->dataValid     = OMX_TRUE;
            /* pDataBuffer->nFlags             = pDataBuffer->bufferHeader->nFlags; */
            /* pDtaBuffer->nTimeStamp         = pDataBuffer->bufferHeader->nTimeStamp; */
/*
            if (pExynosPort->bufferProcessType & BUFFER_SHARE)
                pDataBuffer->pPrivate      = pDataBuffer->bufferHeader->pOutputPortPrivate;
            else if (pExynosPort->bufferProcessType & BUFFER_COPY) {
                pExynosPort->processData.dataBuffer = pDataBuffer->bufferHeader->pBuffer;
                pExynosPort->processData.allocSize  = pDataBuffer->bufferHeader->nAllocLen;
            }
*/
            Exynos_OSAL_Free(pMessage);
        }

        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_BUFFERHEADERTYPE *Exynos_OutputBufferGetQueue_Direct(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent)
{
    OMX_BUFFERHEADERTYPE  *pBufferHdr   = NULL;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;
    EXYNOS_OMX_MESSAGE    *pMessage     = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        pBufferHdr = NULL;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        pBufferHdr = NULL;
        goto EXIT;
    } else if ((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> wait(bufferSemID)",
                                                pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output port -> post(bufferSemID)",
                                                pExynosComponent, __FUNCTION__);

        pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
        if (pMessage == NULL) {
            pBufferHdr = NULL;
            goto EXIT;
        }
        if (pMessage->type == EXYNOS_OMX_CommandFakeBuffer) {
            Exynos_OSAL_Free(pMessage);
            pBufferHdr = NULL;
            goto EXIT;
        }

        pBufferHdr  = (OMX_BUFFERHEADERTYPE *)(pMessage->pCmdData);
        Exynos_OSAL_Free(pMessage);
    }

EXIT:
    FunctionOut();

    return pBufferHdr;
}

OMX_ERRORTYPE Exynos_CodecBufferEnqueue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_U32                      nPortIndex,
    OMX_PTR                      pData)
{
    OMX_ERRORTYPE          ret         = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT   *pExynosPort = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[nPortIndex]);

    if (pData == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = Exynos_OSAL_Queue(&pExynosPort->codecBufferQ, (void *)pData);
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

OMX_ERRORTYPE Exynos_CodecBufferDequeue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_U32                      nPortIndex,
    OMX_PTR                     *pData)
{
    OMX_ERRORTYPE          ret         = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT   *pExynosPort = NULL;
    OMX_PTR                pTempData   = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[nPortIndex]);

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port -> wait(codecSemID)",
                                            pExynosComponent, __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output");
    Exynos_OSAL_SemaphoreWait(pExynosPort->codecSemID);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port -> post(codecSemID)",
                                            pExynosComponent, __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output");

    pTempData = (OMX_PTR)Exynos_OSAL_Dequeue(&pExynosPort->codecBufferQ);
    if (pTempData != NULL) {
        *pData = (OMX_PTR)pTempData;
        ret = OMX_ErrorNone;
    } else {
        *pData = NULL;
        ret = OMX_ErrorUndefined;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferReset(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_U32                      nPortIndex)
{
    OMX_ERRORTYPE          ret          = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[nPortIndex]);

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

OMX_ERRORTYPE Exynos_OMX_VideoEncodeGetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoInit:
    {
        OMX_PORT_PARAM_TYPE *pPortParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pPortParam->nPorts           = pExynosComponent->portParam.nPorts;
        pPortParam->nStartPortNumber = pExynosComponent->portParam.nStartPortNumber;
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex  = pPortFormat->nPortIndex;
        OMX_U32                         nIndex      = pPortFormat->nIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef    = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (nPortIndex == INPUT_PORT_INDEX) {
            if (nIndex > (INPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1)) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
            pPortDef = &pExynosPort->portDefinition;

            pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;

            if (pExynosPort->supportFormat[nIndex] == OMX_COLOR_FormatUnused) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }
            pPortFormat->eColorFormat = pExynosPort->supportFormat[nIndex];
        } else if (nPortIndex == OUTPUT_PORT_INDEX) {
            if (nIndex > (OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1)) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            pPortDef = &pExynosPort->portDefinition;

            pPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
            pPortFormat->eColorFormat       = pPortDef->format.video.eColorFormat;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE     *pVideoBitrate  = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32                          nPortIndex     = pVideoBitrate->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE    *pPortDef       = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
            if (pVideoEnc == NULL) {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoBitrate->eControlRate = pVideoEnc->eControlRate[nPortIndex];
            pVideoBitrate->nTargetBitrate = pPortDef->format.video.nBitrate;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoQuantization:
    {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE  *pVideoQuantization = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;
        OMX_U32                            nPortIndex         = pVideoQuantization->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE      *pPortDef           = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoQuantization->nQpI = pVideoEnc->quantization.nQpI;
            pVideoQuantization->nQpP = pVideoEnc->quantization.nQpP;
            pVideoQuantization->nQpB = pVideoEnc->quantization.nQpB;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef      = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                       nPortIndex    = pPortDef->nPortIndex;
        ret = Exynos_OMX_GetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        ret = Exynos_OSAL_GetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        if (ret != OMX_ErrorNone)
            goto EXIT;
    }
        break;
    case OMX_IndexVendorNeedContigMemory:
    {
        EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *pPortMemType = (EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *)pComponentParameterStructure;
        OMX_U32                             nPortIndex   = pPortMemType->nPortIndex;

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
    case OMX_IndexParamVideoIntraRefresh:
    {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
        OMX_U32                           nPortIndex    = pIntraRefresh->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pIntraRefresh, sizeof(OMX_VIDEO_PARAM_INTRAREFRESHTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pIntraRefresh->eRefreshMode = pVideoEnc->intraRefresh.eRefreshMode;
        pIntraRefresh->nAirMBs      = pVideoEnc->intraRefresh.nAirMBs;
        pIntraRefresh->nAirRef      = pVideoEnc->intraRefresh.nAirRef;
        pIntraRefresh->nCirMBs      = pVideoEnc->intraRefresh.nCirMBs;

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamRotationInfo:
    {
        EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *pRotationInfo = (EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *)pComponentParameterStructure;
        OMX_U32                               nPortIndex    = pRotationInfo->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pRotationInfo, sizeof(EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pRotationInfo->eRotationType = pVideoEnc->eRotationType;
    }
        break;
    case OMX_IndexParamVideoDropControl:
    {
        OMX_CONFIG_BOOLEANTYPE *pDropControl = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDropControl, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDropControl->bEnabled = pVideoEnc->bDropControl;
    }
        break;
    case OMX_IndexParamVideoDisableDFR:
    {
        OMX_CONFIG_BOOLEANTYPE *pDisableDFR = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDisableDFR, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDisableDFR->bEnabled = pVideoEnc->bDisableDFR;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamConsumerUsageBits:
    {
        ret = Exynos_OSAL_GetParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
#endif
    default:
    {
        ret = Exynos_OMX_GetParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeSetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex  = pPortFormat->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef    = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        pPortDef = &(pExynosComponent->pExynosPort[nPortIndex].portDefinition);

        if ((nPortIndex == INPUT_PORT_INDEX) &&
            ((pPortFormat->xFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] xFramerate is invalid(%d)",
                                              pExynosComponent, __FUNCTION__, pPortFormat->xFramerate >> 16);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pPortDef->format.video.eColorFormat       = pPortFormat->eColorFormat;
        pPortDef->format.video.eCompressionFormat = pPortFormat->eCompressionFormat;
        pPortDef->format.video.xFramerate         = pPortFormat->xFramerate;
    }
        break;
    case OMX_IndexParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE  *pVideoBitrate = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32                       nPortIndex    = pVideoBitrate->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef      = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pPortDef = &(pExynosComponent->pExynosPort[nPortIndex].portDefinition);
            pVideoEnc->eControlRate[nPortIndex] = pVideoBitrate->eControlRate;
            pPortDef->format.video.nBitrate = pVideoBitrate->nTargetBitrate;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoQuantization:
    {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE *pVideoQuantization = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;
        OMX_U32                           nPortIndex         = pVideoQuantization->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE     *pPortDef           = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoEnc->quantization.nQpI = pVideoQuantization->nQpI;
            pVideoEnc->quantization.nQpP = pVideoQuantization->nQpP;
            pVideoEnc->quantization.nQpB = pVideoQuantization->nQpB;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef   = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                       nPortIndex = pPortDef->nPortIndex;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            if (pExynosPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        if (pPortDef->nBufferCountActual < pExynosPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if ((nPortIndex == INPUT_PORT_INDEX) &&
            ((pPortDef->format.video.xFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s[%x] xFramerate is invalid(%d)",
                                              __FUNCTION__, OMX_IndexParamPortDefinition, pPortDef->format.video.xFramerate >> 16);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        Exynos_OSAL_Memcpy(((char *)&pExynosPort->portDefinition) + nOffset,
                           ((char *)pPortDef) + nOffset,
                           pPortDef->nSize - nOffset);
        if (nPortIndex == INPUT_PORT_INDEX) {
            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            Exynos_UpdateFrameSize(pOMXComponent);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] the size of output buffer is %d",
                                                pExynosComponent, __FUNCTION__, pExynosPort->portDefinition.nBufferSize);
        }
        ret = OMX_ErrorNone;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamStoreMetaDataBuffer:
    case OMX_IndexParamAllocateNativeHandle:
    {
        ret = Exynos_OSAL_SetParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
#endif
    case OMX_IndexVendorNeedContigMemory:
    {
        EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *pPortMemType = (EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *)pComponentParameterStructure;
        OMX_U32                             nPortIndex   = pPortMemType->nPortIndex;

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

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            if (pExynosPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        pExynosPort->bNeedContigMem = pPortMemType->bNeedContigMem;
    }
        break;
    case OMX_IndexParamVideoIntraRefresh:
    {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
        OMX_U32                           nPortIndex    = pIntraRefresh->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pIntraRefresh, sizeof(OMX_VIDEO_PARAM_INTRAREFRESHTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pIntraRefresh->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic) {
            pVideoEnc->intraRefresh.eRefreshMode    = pIntraRefresh->eRefreshMode;
            pVideoEnc->intraRefresh.nCirMBs         = pIntraRefresh->nCirMBs;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "OMX_VIDEO_IntraRefreshCyclic Enable, nCirMBs: %d",
                            pVideoEnc->intraRefresh.nCirMBs);
        } else {
            ret = OMX_ErrorUnsupportedSetting;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamEnableBlurFilter:
    {
        EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER *pBlurMode  = (EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER *)pComponentParameterStructure;
        OMX_U32                                   nPortIndex = pBlurMode->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pBlurMode, sizeof(EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pVideoEnc->bUseBlurFilter = pBlurMode->bUseBlurFilter;
    }
        break;
    case OMX_IndexParamRotationInfo:
    {
        EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *pRotationInfo = (EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *)pComponentParameterStructure;
        OMX_U32                               nPortIndex    = pRotationInfo->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pRotationInfo, sizeof(EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pVideoEnc->bFirstInput != OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] can't change a rotation info", pExynosComponent, __FUNCTION__);
        } else {
            if ((pRotationInfo->eRotationType != ROTATE_0) &&
                (pRotationInfo->eRotationType != ROTATE_90) &&
                (pRotationInfo->eRotationType != ROTATE_180) &&
                (pRotationInfo->eRotationType != ROTATE_270)) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can't accecpt a rotation value(%d)", pExynosComponent, __FUNCTION__,
                                                    pRotationInfo->eRotationType);
                ret = OMX_ErrorUnsupportedSetting;
                goto EXIT;
            }

            pVideoEnc->eRotationType = pRotationInfo->eRotationType;
        }
    }
        break;
    case OMX_IndexExynosParamImageCrop:
    {
        EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP *pImageCrop = (EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pImageCrop, sizeof(EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

        pExynosComponent->bUseImgCrop = pImageCrop->bEnabled;

        if (pExynosComponent->bUseImgCrop == OMX_TRUE) {
            pExynosPort->bufferProcessType |= BUFFER_COPY_FORCE;
        } else {
            pExynosPort->bufferProcessType &= (~BUFFER_COPY_FORCE);
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoDropControl:
    {
        OMX_CONFIG_BOOLEANTYPE *pDropControl = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDropControl, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoEnc->bDropControl = pDropControl->bEnabled;
    }
        break;
    case OMX_IndexParamVideoDisableDFR:
    {
        OMX_CONFIG_BOOLEANTYPE *pDisableDFR = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDisableDFR, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoEnc->bDisableDFR = pDisableDFR->bEnabled;
    }
        break;
    default:
    {
        ret = Exynos_OMX_SetParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeGetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nParamIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       nPortIndex     = pConfigBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort    = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pConfigBitrate->nEncodeBitrate = pExynosPort->portDefinition.format.video.nBitrate;
        }
    }
        break;
    case OMX_IndexConfigVideoFramerate:
    {
        OMX_CONFIG_FRAMERATETYPE *pConfigFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   nPortIndex       = pConfigFramerate->nPortIndex;
        EXYNOS_OMX_BASEPORT      *pExynosPort      = NULL;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        pConfigFramerate->xEncodeFramerate = pExynosPort->portDefinition.format.video.xFramerate;
    }
        break;
    case OMX_IndexVendorGetBufferFD:
    {
        EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO *pBufferInfo = (EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pBufferInfo, sizeof(EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pBufferInfo->fd = Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, pBufferInfo->pVirAddr);
    }
        break;
    case OMX_IndexConfigCommonInputCrop:
    {
        OMX_CONFIG_RECTTYPE *pDstRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pSrcRectType = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].cropRectangle[IMG_CROP_INPUT_PORT]);

        if (pDstRectType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
            ret = OMX_ErrorIncorrectStateOperation;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Need OMX_IndexExynosParamEnableCrop set", pExynosComponent, __FUNCTION__);
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
        OMX_CONFIG_RECTTYPE *pSrcRectType = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].cropRectangle[IMG_CROP_OUTPUT_PORT]);

        if (pDstRectType->nPortIndex != INPUT_PORT_INDEX) {
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
    case OMX_IndexConfigCommonMirror:
    {
        OMX_CONFIG_MIRRORTYPE *pMirror = (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pMirror->eMirror = pVideoEnc->eMirrorType;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoColorAspects:
    {
        ret = Exynos_OSAL_GetConfig(hComponent, nParamIndex, pComponentConfigStructure);
    }
        break;
    case OMX_IndexConfigAndroidVendorExtension:
    {
        ret = Exynos_OSAL_GetVendorExt(hComponent, pComponentConfigStructure);
    }
        break;
#endif
    default:
    {
        ret = Exynos_OMX_GetConfig(hComponent, nParamIndex, pComponentConfigStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeSetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nParamIndex,
    OMX_PTR         pComponentConfigStructure)
    {
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       nPortIndex     = pConfigBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort    = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            if (pVideoEnc->eControlRate[nPortIndex] == OMX_Video_ControlRateDisable) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Rate control(eControlRate) is disable. can not change a bitrate",
                                                    pExynosComponent, __FUNCTION__);
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pExynosPort->portDefinition.format.video.nBitrate = pConfigBitrate->nEncodeBitrate;
        }
    }
        break;
    case OMX_IndexConfigVideoFramerate:
    {
        OMX_CONFIG_FRAMERATETYPE *pConfigFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   nPortIndex       = pConfigFramerate->nPortIndex;
        EXYNOS_OMX_BASEPORT      *pExynosPort      = NULL;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((nPortIndex == INPUT_PORT_INDEX) &&
            ((pConfigFramerate->xEncodeFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] xFramerate is invalid(%d)",
                                              pExynosComponent, __FUNCTION__, pConfigFramerate->xEncodeFramerate >> 16);
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        pExynosPort->portDefinition.format.video.xFramerate = pConfigFramerate->xEncodeFramerate;
    }
        break;
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        OMX_CONFIG_INTRAREFRESHVOPTYPE *pIntraRefreshVOP = (OMX_CONFIG_INTRAREFRESHVOPTYPE *)pComponentConfigStructure;
        OMX_U32                         nPortIndex       = pIntraRefreshVOP->nPortIndex;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc->IntraRefreshVOP = pIntraRefreshVOP->IntraRefreshVOP;
        }
    }
        break;
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
        pVideoEnc->nQosRatio = pConfigRate->nU32 >> 16;

        if (pVideoEnc->nQosRatio == (((OMX_U32)INT_MAX) >> 16)) {
            pVideoEnc->nQosRatio = 1000;
        } else {
            pVideoEnc->nQosRatio = ((xFramerate >> 16) == 0)? 100:(OMX_U32)((pConfigRate->nU32 / (double)xFramerate) * 100);
        }

        pVideoEnc->bQosChanged = OMX_TRUE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] qos ratio: 0x%x", pExynosComponent, __FUNCTION__, pVideoEnc->nQosRatio);

        ret = OMX_ErrorNone;
    }
    case OMX_IndexConfigPriority:
    {
        OMX_PARAM_U32TYPE *pPriority = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
        OMX_U32            priority  = 0;

        ret = Exynos_OMX_Check_SizeVersion(pPriority, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoEnc->nPriority = pPriority->nU32;;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] priority : 0x%x", pExynosComponent, __FUNCTION__, pVideoEnc->nPriority);

        ret = OMX_ErrorNone;
    }
        break;
        break;
    case OMX_IndexConfigBlurInfo:
    {
        EXYNOS_OMX_VIDEO_CONFIG_BLURINFO *pBlurMode   = (EXYNOS_OMX_VIDEO_CONFIG_BLURINFO *)pComponentConfigStructure;
        OMX_U32                           nPortIndex  = pBlurMode->nPortIndex;
        EXYNOS_OMX_BASEPORT              *pExynosPort = NULL;

        int nEncResol;

        ret = Exynos_OMX_Check_SizeVersion(pBlurMode, sizeof(EXYNOS_OMX_VIDEO_CONFIG_BLURINFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        nEncResol   = pExynosPort->portDefinition.format.video.nFrameWidth * pExynosPort->portDefinition.format.video.nFrameHeight;

        if (pVideoEnc->bUseBlurFilter == OMX_TRUE) {
            if ((pBlurMode->eBlurMode & BLUR_MODE_DOWNUP) &&
                (nEncResol < (int)pBlurMode->eTargetResol)) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Resolution(%d x %d) is smaller than target resolution",
                                                    pExynosComponent, __FUNCTION__,
                                                    pExynosPort->portDefinition.format.video.nFrameWidth,
                                                    pExynosPort->portDefinition.format.video.nFrameHeight);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            pVideoEnc->eBlurMode = pBlurMode->eBlurMode;
            pVideoEnc->eBlurResol = pBlurMode->eTargetResol;
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Blur Filter is not enabled, it will be discard",
                                                    pExynosComponent, __FUNCTION__);
        }

        ret = (OMX_ERRORTYPE)OMX_ErrorNoneExpiration;
    }
        break;
    case OMX_IndexConfigCommonInputCrop:
    {
        OMX_CONFIG_RECTTYPE *pSrcRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pDstRectType = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].cropRectangle[IMG_CROP_INPUT_PORT]);

        if (pSrcRectType->nPortIndex != INPUT_PORT_INDEX) {
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

        pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bUseImgCrop[IMG_CROP_INPUT_PORT] = OMX_TRUE;
    }
        break;
    case OMX_IndexConfigCommonOutputCrop:
    {
        OMX_CONFIG_RECTTYPE *pSrcRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pDstRectType = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].cropRectangle[IMG_CROP_OUTPUT_PORT]);

        if (pSrcRectType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
            ret = OMX_ErrorBadParameter;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] image cropping is not enabled.", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_TRUE) {
            /* positioning is not supported yet when DRC is required */
            ret = OMX_ErrorBadParameter;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] positioning is not supported yet when DRC is required", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDstRectType->nTop      = pSrcRectType->nTop;
        pDstRectType->nLeft     = pSrcRectType->nLeft;
        pDstRectType->nHeight   = pSrcRectType->nHeight;
        pDstRectType->nWidth    = pSrcRectType->nWidth;

        pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bUseImgCrop[IMG_CROP_OUTPUT_PORT] = OMX_TRUE;
    }
        break;
    case OMX_IndexConfigCommonOutputSize:
    {
        OMX_FRAMESIZETYPE   *pFrameSize     = (OMX_FRAMESIZETYPE *)pComponentConfigStructure;
        OMX_CONFIG_RECTTYPE *pTargetFrame   = NULL;
        EXYNOS_OMX_BASEPORT *pOutputPort    = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pFrameSize, sizeof(OMX_FRAMESIZETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pFrameSize->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pOutputPort     = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);
        pTargetFrame    = &(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT]);

        /* the size of target resolution can not be bigger than the size set on output port */
        if ((pFrameSize->nWidth > pOutputPort->portDefinition.format.video.nFrameWidth) ||
            (pFrameSize->nHeight > pOutputPort->portDefinition.format.video.nFrameHeight)) {
            ret = OMX_ErrorBadParameter;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] target size(%d x %d) is bigger than output(%d x %d)",
                                                pExynosComponent, __FUNCTION__,
                                                pFrameSize->nWidth,
                                                pFrameSize->nHeight,
                                                pOutputPort->portDefinition.format.video.nFrameWidth,
                                                pOutputPort->portDefinition.format.video.nFrameHeight);
            goto EXIT;
        }

        if (pVideoEnc->bFirstInput == OMX_TRUE) {
            pExynosComponent->bUseImgCrop = OMX_TRUE;
            pOutputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] = OMX_TRUE;

            /* must use BUFFER_COPY */
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bufferProcessType |= BUFFER_COPY_FORCE;
            /* positioning is not supported yet when DRC is required */
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bUseImgCrop[IMG_CROP_OUTPUT_PORT] = OMX_FALSE;

            /* update target size */
            pTargetFrame->nTop      = 0;
            pTargetFrame->nLeft     = 0;
            pTargetFrame->nWidth    = pFrameSize->nWidth;
            pTargetFrame->nHeight   = pFrameSize->nHeight;

            ret = (OMX_ERRORTYPE)OMX_ErrorNoneExpiration;
        } else {
            if (pExynosComponent->bUseImgCrop == OMX_FALSE) {
                ret = OMX_ErrorBadParameter;
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] CommonOutputSize must be enabled before sending first input", pExynosComponent, __FUNCTION__);
                goto EXIT;
            }

            /*
             * postpone to update target size.
             * it will be updated at dynamic config Q with DRC handling
             */
             ret = OMX_ErrorNone;
        }
    }
        break;
    case OMX_IndexConfigCommonRotate:
    {
        OMX_CONFIG_ROTATIONTYPE *pRotationInfo = (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;
        OMX_U32                  nPortIndex    = pRotationInfo->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pRotationInfo, sizeof(OMX_CONFIG_ROTATIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pVideoEnc->bFirstInput != OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] can't change a rotation info", pExynosComponent, __FUNCTION__);
        } else {
            if ((pRotationInfo->nRotation != ROTATE_0) &&
                    (pRotationInfo->nRotation != ROTATE_90) &&
                    (pRotationInfo->nRotation != ROTATE_180) &&
                    (pRotationInfo->nRotation != ROTATE_270)) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can't accecpt a rotation value(%d)", pExynosComponent, __FUNCTION__,
                        pRotationInfo->nRotation);
                ret = OMX_ErrorUnsupportedSetting;
                goto EXIT;
            }

            pVideoEnc->eRotationType = (EXYNOS_OMX_ROTATION_TYPE)pRotationInfo->nRotation;
        }
    }
        break;
    case OMX_IndexConfigCommonMirror:
    {
        OMX_CONFIG_MIRRORTYPE *pMirror = (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;
        OMX_BOOL bVerticalFlip;

        ret = Exynos_OMX_Check_SizeVersion(pMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
        if (pMirror->nPortIndex != INPUT_PORT_INDEX) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid port index", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pExynosComponent->currentState == OMX_StateExecuting) ||
            (pExynosComponent->currentState == OMX_StateInvalid) ||
            (pExynosComponent->transientState == EXYNOS_OMX_TransStateIdleToLoaded)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid state(0x%x)", pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        if ((pMirror->eMirror != OMX_MirrorHorizontal) &&
            (pMirror->eMirror != OMX_MirrorVertical) &&
            (pMirror->eMirror != OMX_MirrorBoth)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUnsupportedSetting;
            goto EXIT;
        }

        pVideoEnc->eMirrorType = pMirror->eMirror;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoColorAspects:
    {
        ret = Exynos_OSAL_SetConfig(hComponent, nParamIndex, pComponentConfigStructure);
    }
        break;
    case OMX_IndexConfigAndroidVendorExtension:
    {
        ret = Exynos_OSAL_SetVendorExt(hComponent, pComponentConfigStructure);
        if (ret == OMX_ErrorNone)
            ret = (OMX_ERRORTYPE)OMX_ErrorNoneExpiration;
    }
        break;
#endif
    default:
    {
        ret = Exynos_OMX_SetConfig(hComponent, nParamIndex, pComponentConfigStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeGetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      szParamName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (szParamName == NULL) ||
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

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_CONFIG_VIDEO_INTRAPERIOD) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigVideoIntraPeriod;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_PARAM_NEED_CONTIG_MEMORY) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorNeedContigMemory;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_CONFIG_GET_BUFFER_FD) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorGetBufferFD;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_PARAM_VIDEO_QPRANGE_TYPE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamVideoQPRange;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_CONFIG_VIDEO_QPRANGE_TYPE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoQPRange;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_PARAM_ENABLE_BLUR_FILTER) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamEnableBlurFilter;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_CONFIG_BLUR_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigBlurInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_PARAM_ROATION_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamRotationInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamStoreMetaDataBuffer;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_CONFIG_VIDEO_COLOR_ASPECTS_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoColorAspects;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_PARAM_ALLOCATE_NATIVE_HANDLE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamAllocateNativeHandle;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    ret = Exynos_OMX_GetExtensionIndex(hComponent, szParamName, pIndexType);

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
                /* if dataLen is zero with EOS flag, it is not an error.
                 * in this case, buffer handle can be null by framework.
                 */
                if ((pUseBuffer->dataLen <= 0) &&
                    (pUseBuffer->nFlags & OMX_BUFFERFLAG_EOS)) {
                    pData->allocSize     = pUseBuffer->allocSize;
                    pData->dataLen       = pUseBuffer->dataLen;
                    pData->usedDataLen   = pUseBuffer->usedDataLen;
                    pData->remainDataLen = pUseBuffer->remainDataLen;
                    pData->timeStamp     = pUseBuffer->timeStamp;
                    pData->nFlags        = pUseBuffer->nFlags;
                    pData->pPrivate      = pUseBuffer->pPrivate;
                    pData->bufferHeader  = pUseBuffer->bufferHeader;
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s]: Failed to Exynos_OSAL_LockMetaData() but, this buffer is for EOS handling.", __FUNCTION__);
                    goto EXIT;
                }

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
