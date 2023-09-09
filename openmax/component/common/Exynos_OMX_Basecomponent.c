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
 * @file       Exynos_OMX_Basecomponent.c
 * @brief
 * @author     SeungBeom Kim (sbcrux.kim@samsung.com)
 *             Taehwan Kim (t_h.kim@samsung.com)
 * @version    2.5.0
 * @history
 *    2012.02.20 : Create
 *    2017.08.03 : Change event handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Exynos_OSAL_Event.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Resourcemanager.h"
#include "Exynos_OMX_Macros.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_BASE_COMP"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

static OMX_ERRORTYPE Exynos_CheckStateSet(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam);
static OMX_ERRORTYPE Exynos_CheckPortFlush(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam);
static OMX_ERRORTYPE Exynos_CheckPortDisable(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam);
static OMX_ERRORTYPE Exynos_CheckPortEnable(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam);
static OMX_ERRORTYPE Exynos_CheckMarkBuffer(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam);
static OMX_ERRORTYPE Exynos_OMX_CommandQueue(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData);

/* Change CHECK_SIZE_VERSION Macro */
OMX_ERRORTYPE Exynos_OMX_Check_SizeVersion(OMX_PTR header, OMX_U32 size)
{
    OMX_ERRORTYPE        ret        = OMX_ErrorNone;
    OMX_VERSIONTYPE     *version    = NULL;

    if (header == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    version = (OMX_VERSIONTYPE*)((char*)header + sizeof(OMX_U32));
    if (*((OMX_U32*)header) != size) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_FUNC_TRACE, "[%s] nVersionMajor:%d, nVersionMinor:%d", __FUNCTION__, version->s.nVersionMajor, version->s.nVersionMinor);

    if ((version->s.nVersionMajor != VERSIONMAJOR_NUMBER) ||
        (version->s.nVersionMinor > VERSIONMINOR_NUMBER)) {
        ret = OMX_ErrorVersionMismatch;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    return ret;
}


/* OMX Interface */
OMX_ERRORTYPE Exynos_OMX_GetComponentVersion(
    OMX_IN  OMX_HANDLETYPE   hComponent,
    OMX_OUT OMX_STRING       pComponentName,
    OMX_OUT OMX_VERSIONTYPE *pComponentVersion,
    OMX_OUT OMX_VERSIONTYPE *pSpecVersion,
    OMX_OUT OMX_UUIDTYPE    *pComponentUUID)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    unsigned long             compUUID[3];

    FunctionIn();

    /* check parameters */
    if (hComponent     == NULL ||
        pComponentName == NULL || pComponentVersion == NULL ||
        pSpecVersion   == NULL || pComponentUUID    == NULL) {
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

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    Exynos_OSAL_Strcpy(pComponentName, pExynosComponent->componentName);
    Exynos_OSAL_Memcpy(pComponentVersion, &(pExynosComponent->componentVersion), sizeof(OMX_VERSIONTYPE));
    Exynos_OSAL_Memcpy(pSpecVersion, &(pExynosComponent->specVersion), sizeof(OMX_VERSIONTYPE));

    /* Fill UUID with handle address, PID and UID.
     * This should guarantee uiniqness */
    compUUID[0] = (unsigned long)pOMXComponent;
    compUUID[1] = (unsigned long)getpid();
    compUUID[2] = (unsigned long)getuid();
    Exynos_OSAL_Memcpy(*pComponentUUID, compUUID, 3 * sizeof(*compUUID));

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SendCommand(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_COMMANDTYPE Cmd,
    OMX_IN OMX_U32         nParam,
    OMX_IN OMX_PTR         pCmdData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (Cmd) {
    case OMX_CommandStateSet :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_CommandStateSet(%s)",
                                                pExynosComponent, __FUNCTION__, stateString(nParam));
        ret = Exynos_CheckStateSet(pExynosComponent, nParam);
        break;
    case OMX_CommandFlush :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_CommandFlush(port:0x%x)",
                                                pExynosComponent, __FUNCTION__, nParam);
        ret = Exynos_CheckPortFlush(pExynosComponent, nParam);
        break;
    case OMX_CommandPortDisable :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_CommandPortDisable(port:0x%x)",
                                                pExynosComponent, __FUNCTION__, nParam);
        ret = Exynos_CheckPortDisable(pExynosComponent, nParam);
        break;
    case OMX_CommandPortEnable :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_CommandPortEnable(port:0x%x)",
                                                pExynosComponent, __FUNCTION__, nParam);
        ret = Exynos_CheckPortEnable(pExynosComponent, nParam);
        break;
    case OMX_CommandMarkBuffer :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_CommandMarkBuffer(0x%x)",
                                                pExynosComponent, __FUNCTION__, nParam);

        ret = Exynos_CheckMarkBuffer(pExynosComponent, nParam);
        break;
    default:
        ret = OMX_ErrorBadParameter;
        break;
    }

    if (ret == OMX_ErrorNone) {
        ret = Exynos_OMX_CommandQueue(pExynosComponent, Cmd, nParam, pCmdData);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OMX_CommandQueue()", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        /* wait for msg handler's ack except for MarkBuffer */
        if (Cmd != OMX_CommandMarkBuffer)
            Exynos_OSAL_SemaphoreWait(pExynosComponent->hSemaMsgProgress);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_GetState(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_OUT OMX_STATETYPE *pState)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pState == NULL) {
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

    *pState = pExynosComponent->currentState;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SetCallbacks(
    OMX_IN OMX_HANDLETYPE    hComponent,
    OMX_IN OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN OMX_PTR           pAppData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pCallbacks == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;


    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pExynosComponent->currentState != OMX_StateLoaded) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pExynosComponent->pCallbacks   = pCallbacks;
    pExynosComponent->callbackData = pAppData;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

#ifdef EGL_IMAGE_SUPPORT
OMX_ERRORTYPE Exynos_OMX_UseEGLImage(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN void                     *eglImage)
{
    return OMX_ErrorNotImplemented;
}
#endif

static OMX_ERRORTYPE Exynos_CheckStateSet(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if (pExynosComponent->currentState == (OMX_STATETYPE)nParam) {
         ret = OMX_ErrorSameState;
         goto EXIT;
    }

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch ((OMX_STATETYPE)nParam) {
    case OMX_StateInvalid:
    {
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_StateLoaded:
    {
        if ((pExynosComponent->currentState != OMX_StateIdle) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateTransition;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_StateWaitForResources:
    {
        if (pExynosComponent->currentState != OMX_StateLoaded) {
            ret = OMX_ErrorIncorrectStateTransition;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_StateIdle:
    {
        if (pExynosComponent->currentState == OMX_StateInvalid) {
            ret = OMX_ErrorIncorrectStateTransition;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_StateExecuting:
    {
        if ((pExynosComponent->currentState != OMX_StateIdle) &&
            (pExynosComponent->currentState != OMX_StatePause)) {
            ret = OMX_ErrorIncorrectStateTransition;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_StatePause:
    {
        if ((pExynosComponent->currentState != OMX_StateIdle) &&
            (pExynosComponent->currentState != OMX_StateExecuting)) {
            ret = OMX_ErrorIncorrectStateTransition;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    default:
        ret = OMX_ErrorBadParameter;
        break;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE Exynos_CheckPortFlush(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE        ret         = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT *pExynosPort = NULL;
    OMX_U32              nPortIndex  = nParam;

    OMX_U32 i = 0;

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ((nPortIndex != (OMX_U32)ALL_PORT_INDEX) &&
        (nPortIndex >= pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pExynosComponent->currentState == OMX_StateExecuting) ||
        (pExynosComponent->currentState == OMX_StatePause)) {
        if ((nPortIndex != (OMX_U32)ALL_PORT_INDEX) &&
           (nPortIndex >= pExynosComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (nPortIndex == (OMX_U32)ALL_PORT_INDEX) {
            for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
                pExynosPort = &pExynosComponent->pExynosPort[i];

                if (!CHECK_PORT_ENABLED(pExynosPort)) {
                    ret = OMX_ErrorIncorrectStateOperation;
                    goto EXIT;
                }
            }
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            if (!CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE Exynos_CheckPortDisable(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;
    OMX_U32              nPortIndex     = nParam;

    OMX_U32 i = 0;

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ((nPortIndex != (OMX_U32)ALL_PORT_INDEX) &&
        (nPortIndex >= pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (nPortIndex == (OMX_U32)ALL_PORT_INDEX) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosPort = &pExynosComponent->pExynosPort[i];

            if (!CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        ret = OMX_ErrorNone;
    } else {
        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if (!CHECK_PORT_ENABLED(pExynosPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE Exynos_CheckPortEnable(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;
    OMX_U32              nPortIndex     = nParam;

    OMX_U32 i = 0;

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ((nPortIndex != (OMX_U32)ALL_PORT_INDEX) &&
        (nPortIndex >= pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (nPortIndex == (OMX_U32)ALL_PORT_INDEX) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosPort = &pExynosComponent->pExynosPort[i];

            if (CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        ret = OMX_ErrorNone;
    } else {
        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if (CHECK_PORT_ENABLED(pExynosPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE Exynos_CheckMarkBuffer(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if (nParam >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pExynosComponent->currentState == OMX_StateExecuting) ||
        (pExynosComponent->currentState == OMX_StatePause)) {
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE Exynos_SetStateSet(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    OMX_U32 i = 0;

    FunctionIn();

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    switch ((OMX_STATETYPE)nParam) {
    case OMX_StateIdle:
    {
        /* Loaded to Idle */
        if (pExynosComponent->currentState == OMX_StateLoaded) {
            pExynosComponent->transientState = EXYNOS_OMX_TransStateLoadedToIdle;

            for(i = 0; i < pExynosComponent->portParam.nPorts; i++)
                pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateEnabling;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateLoaded to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        /* Executing to Idle */
        if (pExynosComponent->currentState == OMX_StateExecuting) {
            EXYNOS_OMX_BASEPORT *pExynosPort = NULL;

            pExynosComponent->transientState = EXYNOS_OMX_TransStateExecutingToIdle;

            pExynosPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);
            if ((pExynosPort->portDefinition.bEnabled == OMX_FALSE) &&
                (pExynosPort->portState == EXYNOS_OMX_PortStateEnabling)) {
                pExynosPort->exceptionFlag = INVALID_STATE;
            }

            pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);
            if ((pExynosPort->portDefinition.bEnabled == OMX_FALSE) &&
                (pExynosPort->portState == EXYNOS_OMX_PortStateEnabling)) {
                pExynosPort->exceptionFlag = INVALID_STATE;
            }

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateExecuting to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        /* Pause to Idle */
        if (pExynosComponent->currentState == OMX_StatePause) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StatePause to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        if (pExynosComponent->currentState == OMX_StateInvalid) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_StateExecuting:
    {
        /* Idle to Executing */
        if (pExynosComponent->currentState == OMX_StateIdle) {
            pExynosComponent->transientState = EXYNOS_OMX_TransStateIdleToExecuting;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateIdle to OMX_StateExecuting", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        /* Pause to Executing */
        if (pExynosComponent->currentState == OMX_StatePause) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StatePause to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        ret = OMX_ErrorBadParameter;
    }
        break;
    case OMX_StateLoaded:
    {
        /* Idle to Loaded */
        if (pExynosComponent->currentState == OMX_StateIdle) {
            pExynosComponent->transientState = EXYNOS_OMX_TransStateIdleToLoaded;

            for (i = 0; i < pExynosComponent->portParam.nPorts; i++)
                pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateDisabling;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateIdle to OMX_StateLoaded", pExynosComponent, __FUNCTION__);
        }
    }
        break;
    case OMX_StateInvalid:
    {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++)
            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateInvalid;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s to OMX_StateInvalid",
                                            pExynosComponent, __FUNCTION__, stateString(pExynosComponent->currentState));
    }
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s to %s", pExynosComponent, __FUNCTION__,
                                            stateString(pExynosComponent->currentState), stateString(nParam));
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_SetPortFlush(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE        ret         = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort = NULL;
    OMX_U32              nPortIndex  = nParam;

    OMX_U32 i;

    FunctionIn();

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ((pExynosComponent->currentState == OMX_StateExecuting) ||
        (pExynosComponent->currentState == OMX_StatePause)) {
        if ((nPortIndex != (OMX_U32)ALL_PORT_INDEX) &&
            (nPortIndex >= pExynosComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (nPortIndex == (OMX_U32)ALL_PORT_INDEX) {
            for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
                pExynosPort = &pExynosComponent->pExynosPort[i];

                if (!CHECK_PORT_ENABLED(pExynosPort)) {
                    ret = OMX_ErrorIncorrectStateOperation;
                    goto EXIT;
                }

                pExynosPort->portState = EXYNOS_OMX_PortStateFlushing;
            }
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

            if (!CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }

            pExynosPort->portState = EXYNOS_OMX_PortStateFlushing;
        }
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_SetPortDisable(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;
    OMX_U32              nPortIndex     = nParam;

    OMX_U32 i = 0;

    FunctionIn();

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ((nPortIndex != (OMX_U32)ALL_PORT_INDEX) &&
        (nPortIndex >= pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (nPortIndex == (OMX_U32)ALL_PORT_INDEX) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosPort = &pExynosComponent->pExynosPort[i];

            if (!CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }

            pExynosPort->portState = EXYNOS_OMX_PortStateDisabling;
        }
    } else {
        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if (!CHECK_PORT_ENABLED(pExynosPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        pExynosPort->portState = EXYNOS_OMX_PortStateDisabling;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_SetPortEnable(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;
    OMX_U32              nPortIndex     = nParam;

    OMX_U32 i;

    FunctionIn();

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ((nPortIndex != (OMX_U32)ALL_PORT_INDEX) &&
        (nPortIndex >= pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (nPortIndex == (OMX_U32)ALL_PORT_INDEX) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosPort = &pExynosComponent->pExynosPort[i];

            if (CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }

            pExynosPort->portState = EXYNOS_OMX_PortStateEnabling;
        }
    } else {
        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if (CHECK_PORT_ENABLED(pExynosPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        pExynosPort->portState = EXYNOS_OMX_PortStateEnabling;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;

}

static OMX_ERRORTYPE Exynos_SetMarkBuffer(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_U32                     nParam)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if (nParam >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pExynosComponent->currentState == OMX_StateExecuting) ||
        (pExynosComponent->currentState == OMX_StatePause)) {
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
    }

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_ComponentStateSet(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              destState)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    OMX_STATETYPE             currentState      = OMX_StateLoaded;

    int i;

    FunctionIn();

    if (pOMXComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    currentState     = pExynosComponent->currentState;

    if (pExynosComponent->currentState == (OMX_STATETYPE)destState) {
         ret = OMX_ErrorSameState;
         goto EXIT;
    }

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] current:(%s) dest:(%s)", pExynosComponent, __FUNCTION__,
                                        stateString(currentState), stateString(destState));
    switch ((OMX_STATETYPE)destState) {
    case OMX_StateInvalid:
    {
        switch (currentState) {
        case OMX_StateWaitForResources:
            Exynos_OMX_Out_WaitForResource(pOMXComponent);
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
        case OMX_StateLoaded:
            pExynosComponent->currentState = OMX_StateInvalid;

            ret = pExynosComponent->exynos_BufferProcessTerminate(pOMXComponent);

            for (i = 0; i < ALL_PORT_NUM; i++) {
                /* terminate mutex in way */
                if (pExynosComponent->pExynosPort[i].portWayType == WAY1_PORT) {
                    Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    pExynosComponent->pExynosPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                } else if (pExynosComponent->pExynosPort[i].portWayType == WAY2_PORT) {
                    Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;

                    Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                }

                /* terminate mutex for port */
                Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].hPortMutex);
                pExynosComponent->pExynosPort[i].hPortMutex = NULL;
            }

            /* terminate signal about pause */
            for (i = 0; i < ALL_PORT_NUM; i++) {
                Exynos_OSAL_SignalTerminate(pExynosComponent->pExynosPort[i].pauseEvent);
                pExynosComponent->pExynosPort[i].pauseEvent = NULL;
            }

            /* terminate sema for buffer handling on port */
            for (i = 0; i < ALL_PORT_NUM; i++) {
                Exynos_OSAL_SemaphoreTerminate(pExynosComponent->pExynosPort[i].bufferSemID);
                pExynosComponent->pExynosPort[i].bufferSemID = NULL;
            }

            //if (currentState != OMX_StateLoaded)
            pExynosComponent->exynos_codec_componentTerminate(pOMXComponent);

            ret = OMX_ErrorInvalidState;

            if (pExynosComponent->abendState == OMX_TRUE)
                goto NO_EVENT_EXIT;

            break;
        default:
            ret = OMX_ErrorInvalidState;
            break;
        }
    }
        break;
    case OMX_StateLoaded:
    {
        switch (currentState) {
        case OMX_StateIdle:
            for(i = 0; i < (int)pExynosComponent->portParam.nPorts; i++)
                pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateDisabling;

            ret = pExynosComponent->exynos_BufferProcessTerminate(pOMXComponent);

            for (i = 0; i < ALL_PORT_NUM; i++) {
                /* terminate mutex in way */
                if (pExynosComponent->pExynosPort[i].portWayType == WAY1_PORT) {
                    Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    pExynosComponent->pExynosPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                } else if (pExynosComponent->pExynosPort[i].portWayType == WAY2_PORT) {
                    Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;

                    Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                }

                /* terminate mutex for port */
                Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].hPortMutex);
                pExynosComponent->pExynosPort[i].hPortMutex = NULL;
            }

            /* terminate signal about pause */
            for (i = 0; i < ALL_PORT_NUM; i++) {
                Exynos_OSAL_SignalTerminate(pExynosComponent->pExynosPort[i].pauseEvent);
                pExynosComponent->pExynosPort[i].pauseEvent = NULL;
            }

            /* terminate sema for buffer handling on port */
            for (i = 0; i < ALL_PORT_NUM; i++) {
                Exynos_OSAL_SemaphoreTerminate(pExynosComponent->pExynosPort[i].bufferSemID);
                pExynosComponent->pExynosPort[i].bufferSemID = NULL;
            }

            pExynosComponent->exynos_codec_componentTerminate(pOMXComponent);

#ifdef TUNNELING_SUPPORT
            for (i = 0; i < (pExynosComponent->portParam.nPorts); i++) {
                pExynosPort = &(pExynosComponent->pExynosPort[i]);
                if (CHECK_PORT_TUNNELED(pExynosPort) &&
                    CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    while (Exynos_OSAL_GetElemNum(&(pExynosPort->bufferQ)) > 0) {
                        EXYNOS_OMX_MESSAGE *pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
                        if (pMessage != NULL)
                            Exynos_OSAL_Free(pMessage);
                    }

                    ret = pExynosComponent->exynos_FreeTunnelBuffer(pExynosPort, i);
                    if (OMX_ErrorNone != ret)
                        goto EXIT;
                }
            }
#endif

            /* if all buffers are freed, send an event */
            if ((pExynosComponent->pExynosPort[INPUT_PORT_INDEX].assignedBufferNum <= 0) &&
                (pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].assignedBufferNum <= 0)) {
                Exynos_OMX_SendEventCommand(pExynosComponent, EVENT_CMD_STATE_TO_LOAD_STATE, NULL);
            }

            goto NO_EVENT_EXIT;
        case OMX_StateWaitForResources:
            ret = Exynos_OMX_Out_WaitForResource(pOMXComponent);

            pExynosComponent->currentState = OMX_StateLoaded;
            break;
        case OMX_StateExecuting:
        case OMX_StatePause:
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
    }
        break;
    case OMX_StateIdle:
    {
        switch (currentState) {
        case OMX_StateLoaded:
            for (i = 0; i < (int)pExynosComponent->portParam.nPorts; i++) {
                pExynosPort = &(pExynosComponent->pExynosPort[i]);

                pExynosPort->portState = EXYNOS_OMX_PortStateEnabling;

#ifdef TUNNELING_SUPPORT
                if (CHECK_PORT_TUNNELED(pExynosPort) &&
                    CHECK_PORT_BUFFER_SUPPLIER(pExynosPort) &&
                    CHECK_PORT_ENABLED(pExynosPort)) {
                    ret = pExynosComponent->exynos_AllocateTunnelBuffer(pExynosPort, i);
                    if (ret!=OMX_ErrorNone)
                        goto EXIT;
                }
#endif
            }

            Exynos_OSAL_Get_Log_Property(); // For debuging, Function called when GetHandle function is success

            ret = pExynosComponent->exynos_codec_componentInit(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to exynos_codec_componentInit() (0x%x)", pExynosComponent, __FUNCTION__, ret);
#ifdef TUNNELING_SUPPORT
                /*
                 * if (CHECK_PORT_TUNNELED == OMX_TRUE) thenTunnel Buffer Free
                 */
#endif
                goto EXIT;
            }

            /* create sema for buffer handling on port */
            for (i = 0; i < ALL_PORT_NUM; i++) {
                ret = Exynos_OSAL_SemaphoreCreate(&pExynosComponent->pExynosPort[i].bufferSemID);
                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SemaphoreCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
                    ret = OMX_ErrorInsufficientResources;
                    goto FAIL_TO_IDLE;
                }
            }

            /* create signal about pause */
            for (i = 0; i < ALL_PORT_NUM; i++) {
                ret = Exynos_OSAL_SignalCreate(&pExynosComponent->pExynosPort[i].pauseEvent);
                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_SignalCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
                    ret = OMX_ErrorInsufficientResources;
                    goto FAIL_TO_IDLE;
                }
            }

            for (i = 0; i < ALL_PORT_NUM; i++) {
                /* create mutex in way */
                if (pExynosComponent->pExynosPort[i].portWayType == WAY1_PORT) {
                    ret = Exynos_OSAL_MutexCreate(&pExynosComponent->pExynosPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_MutexCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
                        ret = OMX_ErrorInsufficientResources;
                        goto FAIL_TO_IDLE;
                    }
                } else if (pExynosComponent->pExynosPort[i].portWayType == WAY2_PORT) {
                    ret = Exynos_OSAL_MutexCreate(&pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_MutexCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
                        ret = OMX_ErrorInsufficientResources;
                        goto FAIL_TO_IDLE;
                    }
                    ret = Exynos_OSAL_MutexCreate(&pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_MutexCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
                        ret = OMX_ErrorInsufficientResources;
                        goto FAIL_TO_IDLE;
                    }
                }

                /* create mutex for port */
                ret = Exynos_OSAL_MutexCreate(&pExynosComponent->pExynosPort[i].hPortMutex);
                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_MutexCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
                    ret = OMX_ErrorInsufficientResources;
                    goto FAIL_TO_IDLE;
                }
            }

            ret = pExynosComponent->exynos_BufferProcessCreate(pOMXComponent);
            if (ret != OMX_ErrorNone) {
FAIL_TO_IDLE:
#ifdef TUNNELING_SUPPORT
                /*
                 * if (CHECK_PORT_TUNNELED == OMX_TRUE) thenTunnel Buffer Free
                 */
#endif
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    /* terminate mutex in way */
                    if (pExynosComponent->pExynosPort[i].portWayType == WAY1_PORT) {
                        Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                        pExynosComponent->pExynosPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                    } else if (pExynosComponent->pExynosPort[i].portWayType == WAY2_PORT) {
                        Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                        pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;

                        Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                        pExynosComponent->pExynosPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                    }

                    /* terminate mutex for port */
                    Exynos_OSAL_MutexTerminate(pExynosComponent->pExynosPort[i].hPortMutex);
                    pExynosComponent->pExynosPort[i].hPortMutex = NULL;
                }

                /* terminate signal about pause */
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Exynos_OSAL_SignalTerminate(pExynosComponent->pExynosPort[i].pauseEvent);
                    pExynosComponent->pExynosPort[i].pauseEvent = NULL;
                }

                /* terminate sema for buffer handling on port */
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Exynos_OSAL_SemaphoreTerminate(pExynosComponent->pExynosPort[i].bufferSemID);
                    pExynosComponent->pExynosPort[i].bufferSemID = NULL;
                }

                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            /* if all buffers are allocated, send an event */
            if (CHECK_PORT_POPULATED(&(pExynosComponent->pExynosPort[INPUT_PORT_INDEX])) &&
                CHECK_PORT_POPULATED(&(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]))) {
                Exynos_OMX_SendEventCommand(pExynosComponent, EVENT_CMD_STATE_TO_IDLE_STATE, NULL);
            }

            goto NO_EVENT_EXIT;
            break;
        case OMX_StateExecuting:
            Exynos_SetPortFlush(pExynosComponent, ALL_PORT_INDEX);

            ret = Exynos_OMX_BufferFlushProcess(pOMXComponent, ALL_PORT_INDEX, OMX_FALSE);

            pExynosComponent->transientState    = EXYNOS_OMX_TransStateMax;
            pExynosComponent->currentState      = OMX_StateIdle;
            break;
        case OMX_StatePause:
            Exynos_SetPortFlush(pExynosComponent, ALL_PORT_INDEX);

            ret = Exynos_OMX_BufferFlushProcess(pOMXComponent, ALL_PORT_INDEX, OMX_FALSE);

            pExynosComponent->currentState = OMX_StateIdle;
            break;
        case OMX_StateWaitForResources:
            pExynosComponent->currentState = OMX_StateIdle;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
    }
        break;
    case OMX_StateExecuting:
    {
        int j;

        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
#ifdef TUNNELING_SUPPORT
            for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
                pExynosPort = &pExynosComponent->pExynosPort[i];

                if (CHECK_PORT_TUNNELED(pExynosPort) &&
                    CHECK_PORT_BUFFER_SUPPLIER(pExynosPort) &&
                    CHECK_PORT_ENABLED(pExynosPort)) {
                    for (j = 0; j < pExynosPort->tunnelBufferNum; j++)
                        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[i].bufferSemID);
                }
            }
#endif
            pExynosComponent->transientState    = EXYNOS_OMX_TransStateMax;
            pExynosComponent->currentState      = OMX_StateExecuting;

            /* set signal about pause */
            for (i = 0; i < ALL_PORT_NUM; i++)
                Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[i].pauseEvent);
            break;
        case OMX_StatePause:
#ifdef TUNNELING_SUPPORT
            for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
                pExynosPort = &pExynosComponent->pExynosPort[i];

                if (CHECK_PORT_TUNNELED(pExynosPort) &&
                    CHECK_PORT_BUFFER_SUPPLIER(pExynosPort) &&
                    CHECK_PORT_ENABLED(pExynosPort)) {
                    OMX_S32 semaValue = 0, cnt = 0;

                    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[i].bufferSemID, &semaValue);
                    if (Exynos_OSAL_GetElemNum(&pExynosPort->bufferQ) > semaValue) {
                        cnt = Exynos_OSAL_GetElemNum(&pExynosPort->bufferQ) - semaValue;
                        for (j = 0; j < cnt; j++)
                            Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[i].bufferSemID);
                    }
                }
            }
#endif
            pExynosComponent->currentState = OMX_StateExecuting;

            /* set signal about pause */
            for (i = 0; i < ALL_PORT_NUM; i++)
                Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[i].pauseEvent);
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
    }
        break;
    case OMX_StatePause:
    {
        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
            pExynosComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateExecuting:
            pExynosComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
    }
        break;
    case OMX_StateWaitForResources:
    {
        switch (currentState) {
        case OMX_StateLoaded:
            ret = Exynos_OMX_In_WaitForResource(pOMXComponent);
            pExynosComponent->currentState = OMX_StateWaitForResources;
            break;
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
    }
        break;
    default:
        ret = OMX_ErrorIncorrectStateTransition;
        break;
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(%s)",
                                        pExynosComponent, __FUNCTION__, stateString(destState));

            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventCmdComplete, OMX_CommandStateSet,
                                                       destState, NULL);
        }
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OMX_ComponentStateSet(%s) (0x%x)",
                                            pExynosComponent, __FUNCTION__, stateString(destState), ret);
        if ((pExynosComponent != NULL) &&
            (pExynosComponent->pCallbacks != NULL)) {
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventError, ret, 0, NULL);
        }
    }

NO_EVENT_EXIT:  /* postpone to send event */
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_EventHandler(
    OMX_COMPONENTTYPE *pOMXComponent,
    EVENT_COMMAD_TYPE  cmd)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    int i = 0;

    if ((pOMXComponent == NULL) ||
        (pOMXComponent->pComponentPrivate == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    switch (cmd) {
    case EVENT_CMD_STATE_TO_LOAD_STATE:  /* Idle to Loaded */
    {
        EXYNOS_OMX_BASEPORT *pExynosPort = NULL;

        if (pExynosComponent->currentState == OMX_StateLoaded) {
            /* event was already handled */
            break;
        }

        if ((pExynosComponent->currentState != OMX_StateIdle) ||
            (pExynosComponent->transientState != EXYNOS_OMX_TransStateIdleToLoaded)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid state(state:0x%x, transient:0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                pExynosComponent->currentState, pExynosComponent->transientState);
            break;
        }

        for (i = 0; i < (int)pExynosComponent->portParam.nPorts; i++) {
            pExynosPort = &(pExynosComponent->pExynosPort[i]);

            if (CHECK_PORT_ENABLED(pExynosPort)) {
                while (Exynos_OSAL_GetElemNum(&pExynosPort->bufferQ) > 0) {
                    EXYNOS_OMX_MESSAGE *pMsg = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
                    if (pMsg != NULL)
                        Exynos_OSAL_Free(pMsg);
                }
            }

            pExynosPort->portState = EXYNOS_OMX_PortStateLoaded;
        }

        pExynosComponent->transientState = EXYNOS_OMX_TransStateMax;
        pExynosComponent->currentState   = OMX_StateLoaded;

        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(OMX_StateLoaded)", pExynosComponent, __FUNCTION__);

            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandStateSet,
                                                       OMX_StateLoaded,
                                                       NULL);
        }
    }
        break;
    case EVENT_CMD_STATE_TO_IDLE_STATE:  /* Loaded to Idle */
    {
        if (pExynosComponent->currentState == OMX_StateIdle) {
            /* event was already handled */
            break;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) ||
            (pExynosComponent->transientState != EXYNOS_OMX_TransStateLoadedToIdle)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid state(state:0x%x, transient:0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                pExynosComponent->currentState, pExynosComponent->transientState);
            break;
        }

        for (i = 0; i < (int)pExynosComponent->portParam.nPorts; i++)
            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateIdle;

        pExynosComponent->transientState = EXYNOS_OMX_TransStateMax;
        pExynosComponent->currentState   = OMX_StateIdle;

        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(OMX_StateIdle)", pExynosComponent, __FUNCTION__);

            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandStateSet,
                                                       OMX_StateIdle,
                                                       NULL);
        }
    }
        break;
    case EVENT_CMD_DISABLE_INPUT_PORT:  /* PortDisable(INPUT_PORT) */
    {
        if (!CHECK_PORT_ENABLED((&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]))) {
            /* event was already handled */
            break;
        }

        Exynos_OMX_DisablePort(pOMXComponent, INPUT_PORT_INDEX);

        pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portState = EXYNOS_OMX_PortStateLoaded;

        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(Disable/input port)", pExynosComponent, __FUNCTION__);

            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortDisable, INPUT_PORT_INDEX, NULL);
        }
    }
        break;
    case EVENT_CMD_DISABLE_OUTPUT_PORT:
    {
        if (!CHECK_PORT_ENABLED((&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]))) {
            /* event was already handled */
            break;
        }

        Exynos_OMX_DisablePort(pOMXComponent, OUTPUT_PORT_INDEX);

        pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].portState = EXYNOS_OMX_PortStateLoaded;

        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(Disable/output port)", pExynosComponent, __FUNCTION__);

            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortDisable, OUTPUT_PORT_INDEX, NULL);
        }
    }
        break;
    case EVENT_CMD_ENABLE_INPUT_PORT:  /* PortEnable(INPUT_PORT) */
    {
        if (CHECK_PORT_ENABLED((&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]))) {
            /* event was already handled */
            break;
        }

        Exynos_OMX_EnablePort(pOMXComponent, INPUT_PORT_INDEX);

        pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portState = EXYNOS_OMX_PortStateIdle;

        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(Enable/input port)", pExynosComponent, __FUNCTION__);

            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable, INPUT_PORT_INDEX, NULL);
        }
    }
        break;
    case EVENT_CMD_ENABLE_OUTPUT_PORT:  /* PortEnable(OUTPUT_PORT) */
    {
        if (CHECK_PORT_ENABLED((&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]))) {
            /* event was already handled */
            break;
        }

        Exynos_OMX_EnablePort(pOMXComponent, OUTPUT_PORT_INDEX);

        pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].portState = EXYNOS_OMX_PortStateIdle;

        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(Enable/output port)", pExynosComponent, __FUNCTION__);

            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                       pExynosComponent->callbackData,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable, OUTPUT_PORT_INDEX, NULL);
        }
    }
        break;
    default:
        ret = OMX_ErrorBadParameter;
        break;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_MessageHandlerThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if (threadData == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    while (pExynosComponent->bExitMessageHandlerThread == OMX_FALSE) {
        EXYNOS_OMX_MESSAGE *pMessage = NULL;

        Exynos_OSAL_SemaphoreWait(pExynosComponent->hSemaMsgWait);

        pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosComponent->messageQ);
        if (pMessage != NULL) {
            switch ((int)pMessage->type) {
            case OMX_CommandStateSet:
            {
                Exynos_SetStateSet(pExynosComponent, pMessage->param);

                if (pMessage->param != OMX_StateInvalid)
                    Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgProgress);

                ret = Exynos_OMX_ComponentStateSet(pOMXComponent, pMessage->param);

                if (pMessage->param == OMX_StateInvalid)
                    Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgProgress);
            }
                break;
            case OMX_CommandFlush:
            {
                Exynos_SetPortFlush(pExynosComponent, pMessage->param);

                Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgProgress);

                ret = Exynos_OMX_BufferFlushProcess(pOMXComponent, pMessage->param, OMX_TRUE);
            }
                break;
            case OMX_CommandPortDisable:
            {
                Exynos_SetPortDisable(pExynosComponent, pMessage->param);

                Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgProgress);

                ret = Exynos_OMX_PortDisableProcess(pOMXComponent, pMessage->param);
            }
                break;
            case OMX_CommandPortEnable:
            {
                Exynos_SetPortEnable(pExynosComponent, pMessage->param);

                Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgProgress);

                ret = Exynos_OMX_PortEnableProcess(pOMXComponent, pMessage->param);
            }
                break;
            case OMX_CommandMarkBuffer:
            {
                OMX_U32 nPortIndex = pMessage->param;

                if ((pMessage->pCmdData != NULL) &&
                    ((nPortIndex == INPUT_PORT_INDEX) ||
                     (nPortIndex == OUTPUT_PORT_INDEX))) {
                    OMX_MARKTYPE *pMarkType = (OMX_MARKTYPE *)(pMessage->pCmdData);

                    pExynosComponent->pExynosPort[nPortIndex].markType.hMarkTargetComponent = pMarkType->hMarkTargetComponent;
                    pExynosComponent->pExynosPort[nPortIndex].markType.pMarkData            = pMarkType->pMarkData;
                }
            }
                break;
            case Exynos_OMX_CommandSendEvent:
            {
                Exynos_OMX_EventHandler(pOMXComponent, (EVENT_COMMAD_TYPE)pMessage->param);
            }
                break;
            case EXYNOS_OMX_CommandComponentDeInit:
            {
                pExynosComponent->bExitMessageHandlerThread = OMX_TRUE;
            }
                break;
            default:
                break;
            }

            Exynos_OSAL_Free(pMessage);
            pMessage = NULL;
        }
    }

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_CommandQueue(
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent,
    OMX_COMMANDTYPE           Cmd,
    OMX_U32                   nParam,
    OMX_PTR                   pCmdData)
{
    OMX_ERRORTYPE       ret     = OMX_ErrorNone;
    EXYNOS_OMX_MESSAGE *command = NULL;

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    command = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_MESSAGE));
    if (command == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Exynos_OSAL_Malloc()", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    command->type       = (OMX_U32)Cmd;
    command->param      = nParam;
    command->pCmdData   = pCmdData;

    ret = Exynos_OSAL_Queue(&pExynosComponent->messageQ, (void *)command);
    if (ret != 0) {
        Exynos_OSAL_Free(command);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    ret = Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgWait);

EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pParams)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pParams == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch ((int)nParamIndex) {
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamVideoInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit:
    {
        OMX_PORT_PARAM_TYPE *pPortParam = (OMX_PORT_PARAM_TYPE *)pParams;

        ret = Exynos_OMX_Check_SizeVersion(pPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pPortParam->nPorts               = 0;
        pPortParam->nStartPortNumber     = 0;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pDstPortDef    = (OMX_PARAM_PORTDEFINITIONTYPE *)pParams;
        OMX_U32                       nPortIndex     = pDstPortDef->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort    = NULL;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pDstPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        Exynos_OSAL_Memcpy(((char *)pDstPortDef) + nOffset,
                           ((char *)&pExynosPort->portDefinition) + nOffset,
                           pDstPortDef->nSize - nOffset);
    }
        break;
    case OMX_IndexParamPriorityMgmt:
    {
        OMX_PRIORITYMGMTTYPE *pPriority = (OMX_PRIORITYMGMTTYPE *)pParams;

        ret = Exynos_OMX_Check_SizeVersion(pPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pPriority->nGroupID       = pExynosComponent->compPriority.nGroupID;
        pPriority->nGroupPriority = pExynosComponent->compPriority.nGroupPriority;
    }
        break;

    case OMX_IndexParamCompBufferSupplier:
    {
        OMX_PARAM_BUFFERSUPPLIERTYPE *pSupplierType = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pParams;
        OMX_U32                       nPortIndex    = pSupplierType->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort   = NULL;

        if ((pExynosComponent->currentState == OMX_StateLoaded) ||
            (pExynosComponent->currentState == OMX_StateWaitForResources)) {
            if (nPortIndex >= pExynosComponent->portParam.nPorts) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }

            ret = Exynos_OMX_Check_SizeVersion(pSupplierType, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
            if (ret != OMX_ErrorNone) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

            if (pExynosPort->portDefinition.eDir == OMX_DirInput) {
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    pSupplierType->eBufferSupplier = OMX_BufferSupplyInput;
                } else if (CHECK_PORT_TUNNELED(pExynosPort)) {
                    pSupplierType->eBufferSupplier = OMX_BufferSupplyOutput;
                } else {
                    pSupplierType->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            } else {
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    pSupplierType->eBufferSupplier = OMX_BufferSupplyOutput;
                } else if (CHECK_PORT_TUNNELED(pExynosPort)) {
                    pSupplierType->eBufferSupplier = OMX_BufferSupplyInput;
                } else {
                    pSupplierType->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            }
        }
        else
        {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexExynosParamImageCrop:
    {
        EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP *pImageCrop = (EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP *)pParams;

        ret = Exynos_OMX_Check_SizeVersion(pImageCrop, sizeof(EXYNOS_OMX_VIDEO_PARAM_IMAGE_CROP));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pImageCrop->bEnabled = pExynosComponent->bUseImgCrop;

        ret = OMX_ErrorNone;
    }
        break;

    default:
    {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
        break;
    }

    ret = OMX_ErrorNone;

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_IN OMX_PTR        pParams)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pParams == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch ((int)nParamIndex) {
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamVideoInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit:
    {
        OMX_PORT_PARAM_TYPE *pPortParam = (OMX_PORT_PARAM_TYPE *)pParams;

        ret = Exynos_OMX_Check_SizeVersion(pPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        /* ret = OMX_ErrorUndefined; */
        /* Exynos_OSAL_Memcpy(&pExynosComponent->portParam, pPortParam, sizeof(OMX_PORT_PARAM_TYPE)); */
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pSrcPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pParams;
        OMX_U32                       nPortIndex  = pSrcPortDef->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort = NULL;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pSrcPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
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
        if (pSrcPortDef->nBufferCountActual < pExynosPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        Exynos_OSAL_Memcpy(((char *)&pExynosPort->portDefinition) + nOffset,
                           ((char *)pSrcPortDef) + nOffset,
                           pSrcPortDef->nSize - nOffset);
    }
        break;
    case OMX_IndexParamPriorityMgmt:
    {
        OMX_PRIORITYMGMTTYPE *pPriority = (OMX_PRIORITYMGMTTYPE *)pParams;

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosComponent->compPriority.nGroupID         = pPriority->nGroupID;
        pExynosComponent->compPriority.nGroupPriority   = pPriority->nGroupPriority;
    }
        break;
    case OMX_IndexParamCompBufferSupplier:
    {
        OMX_PARAM_BUFFERSUPPLIERTYPE *pSupplierType = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pParams;
        OMX_U32                       nPortIndex    = pSupplierType->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort   = NULL;


        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pSupplierType, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
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

        if (pSupplierType->eBufferSupplier == OMX_BufferSupplyUnspecified) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        if (CHECK_PORT_TUNNELED(pExynosPort) == 0) {
            ret = OMX_ErrorNone; /*OMX_ErrorNone ?????*/
            goto EXIT;
        }

        if (pExynosPort->portDefinition.eDir == OMX_DirInput) {
            if (pSupplierType->eBufferSupplier == OMX_BufferSupplyInput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    ret = OMX_ErrorNone;
                }
                */
                pExynosPort->tunnelFlags |= EXYNOS_TUNNEL_IS_SUPPLIER;
                pSupplierType->nPortIndex = pExynosPort->tunneledPort;

                ret = OMX_SetParameter(pExynosPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, pSupplierType);

                goto EXIT;
            } else if (pSupplierType->eBufferSupplier == OMX_BufferSupplyOutput) {
                ret = OMX_ErrorNone;

                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    pExynosPort->tunnelFlags &= ~EXYNOS_TUNNEL_IS_SUPPLIER;
                    pSupplierType->nPortIndex = pExynosPort->tunneledPort;

                    ret = OMX_SetParameter(pExynosPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, pSupplierType);
                }

                goto EXIT;
            }
        } else if (pExynosPort->portDefinition.eDir == OMX_DirOutput) {
            if (pSupplierType->eBufferSupplier == OMX_BufferSupplyInput) {
                ret = OMX_ErrorNone;

                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    pExynosPort->tunnelFlags &= ~EXYNOS_TUNNEL_IS_SUPPLIER;
                    ret = OMX_ErrorNone;
                }

                goto EXIT;
            } else if (pSupplierType->eBufferSupplier == OMX_BufferSupplyOutput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    ret = OMX_ErrorNone;
                }
                */

                pExynosPort->tunnelFlags |= EXYNOS_TUNNEL_IS_SUPPLIER;
                ret = OMX_ErrorNone;

                goto EXIT;
            }
        }
    }
        break;
    default:
    {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
        break;
    }

    ret = OMX_ErrorNone;

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_GetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nConfigIndex,
    OMX_INOUT OMX_PTR     pConfigs)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pConfigs == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nConfigIndex) {
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nConfigIndex,
    OMX_IN OMX_PTR        pConfigs)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pConfigs == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nConfigIndex) {
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (cParameterName == NULL) ||
        (pIndexType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;


    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_CUSTOM_INDEX_PARAM_IMAGE_CROP) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexExynosParamImageCrop;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    ret = OMX_ErrorBadParameter;

EXIT:
    FunctionOut();

    return ret;
}

OMX_PTR Exynos_OMX_MakeDynamicConfig(
    OMX_INDEXTYPE   nConfigIndex,
    OMX_PTR         pConfigs)
{
    OMX_PTR ret                  = NULL;
    OMX_S32 nConfigSize = 0;

    switch ((int)nConfigIndex) {
    case OMX_IndexConfigVideoIntraPeriod:
    {
        nConfigSize = sizeof(OMX_U32);

        ret = Exynos_OSAL_Malloc(sizeof(OMX_U32) + nConfigSize);
    }
        break;
    case OMX_IndexConfigVideoRoiInfo:
    {
        EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *pRoiInfo       = (EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *)pConfigs;
        OMX_S32                          nRoiMBInfoSize = 0;

        nConfigSize = *(OMX_U32 *)pConfigs;

        if (pRoiInfo->bUseRoiInfo == OMX_TRUE)
            nRoiMBInfoSize = pRoiInfo->nRoiMBInfoSize;

        ret = Exynos_OSAL_Malloc(sizeof(OMX_U32) + nConfigSize + nRoiMBInfoSize);

        if (ret != NULL)
            Exynos_OSAL_Memcpy((OMX_PTR)((OMX_U8 *)ret + sizeof(OMX_U32) + nConfigSize), pRoiInfo->pRoiMBInfo, nRoiMBInfoSize);
    }
        break;
    default:
        nConfigSize = *(OMX_U32 *)pConfigs;

        ret = Exynos_OSAL_Malloc(sizeof(OMX_U32) + nConfigSize);
        break;
    }

    if (ret != NULL) {
        *((OMX_S32 *)ret) = (OMX_S32)nConfigIndex;
        Exynos_OSAL_Memcpy((OMX_PTR)((OMX_U8 *)ret + sizeof(OMX_U32)), pConfigs, nConfigSize);
    }

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SendEventCommand(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    EVENT_COMMAD_TYPE            Cmd,
    OMX_PTR                      pCmdData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] current state is OMX_StateInvalid", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    ret = Exynos_OMX_CommandQueue(pExynosComponent, (OMX_COMMANDTYPE)Exynos_OMX_CommandSendEvent, Cmd, pCmdData);

EXIT:
    FunctionOut();

    return ret;
}

void Exynos_OMX_Component_AbnormalTermination(OMX_HANDLETYPE hComponent)
{
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort      = NULL;

    int i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    if (pOMXComponent->pComponentPrivate == NULL)
        goto EXIT;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    /* clear a msg command piled on queue */
    while(Exynos_OSAL_GetElemNum(&pExynosComponent->messageQ) > 0)
        Exynos_OSAL_Free(Exynos_OSAL_Dequeue(&pExynosComponent->messageQ));

    pExynosComponent->abendState = OMX_TRUE;

    /* change to invalid state */
    Exynos_OMX_SendCommand(hComponent, OMX_CommandStateSet, OMX_StateInvalid, NULL);

EXIT:
    FunctionOut();

    return;
}

OMX_ERRORTYPE Exynos_OMX_BaseComponent_Constructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] lib version is %s", __FUNCTION__, IS_64BIT_OS? "64bit":"32bit");

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] OMX_ErrorBadParameter (0x%x)", __FUNCTION__, ret);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_BASECOMPONENT));
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Malloc (0x%x)", __FUNCTION__, ret);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent, 0, sizeof(EXYNOS_OMX_BASECOMPONENT));
    pOMXComponent->pComponentPrivate = (OMX_PTR)pExynosComponent;

    ret = Exynos_OSAL_SemaphoreCreate(&pExynosComponent->hSemaMsgWait);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SemaphoreCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
        goto EXIT;
    }

    ret = Exynos_OSAL_SemaphoreCreate(&pExynosComponent->hSemaMsgProgress);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SemaphoreCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
        goto EXIT;
    }

    ret = Exynos_OSAL_MutexCreate(&pExynosComponent->compMutex);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to MutexCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
        goto EXIT;
    }

    ret = Exynos_OSAL_MutexCreate(&pExynosComponent->compEventMutex);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to MutexCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
        goto EXIT;
    }

    pExynosComponent->bExitMessageHandlerThread = OMX_FALSE;
    Exynos_OSAL_QueueCreate(&pExynosComponent->messageQ, MAX_QUEUE_ELEMENTS);
    ret = Exynos_OSAL_ThreadCreate(&pExynosComponent->hMessageHandler, Exynos_OMX_MessageHandlerThread, pOMXComponent);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to ThreadCreate (0x%x)", pExynosComponent, __FUNCTION__, ret);
        goto EXIT;
    }

    Exynos_OSAL_QueueCreate(&pExynosComponent->dynamicConfigQ, MAX_QUEUE_ELEMENTS);

    Exynos_OSAL_QueueCreate(&pExynosComponent->HDR10plusConfigQ, MAX_QUEUE_ELEMENTS);

    pOMXComponent->GetComponentVersion = &Exynos_OMX_GetComponentVersion;
    pOMXComponent->SendCommand         = &Exynos_OMX_SendCommand;
    pOMXComponent->GetState            = &Exynos_OMX_GetState;
    pOMXComponent->SetCallbacks        = &Exynos_OMX_SetCallbacks;

#ifdef EGL_IMAGE_SUPPORT
    pOMXComponent->UseEGLImage         = &Exynos_OMX_UseEGLImage;
#endif

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BaseComponent_Destructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    OMX_S32                   semaValue = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    while(Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ) > 0) {
        Exynos_OSAL_Free(Exynos_OSAL_Dequeue(&pExynosComponent->dynamicConfigQ));
    }
    Exynos_OSAL_QueueTerminate(&pExynosComponent->dynamicConfigQ);

    while(Exynos_OSAL_GetElemNum(&pExynosComponent->HDR10plusConfigQ) > 0) {
        Exynos_OSAL_Free(Exynos_OSAL_Dequeue(&pExynosComponent->HDR10plusConfigQ));
    }
    Exynos_OSAL_QueueTerminate(&pExynosComponent->HDR10plusConfigQ);

    Exynos_OMX_CommandQueue(pExynosComponent, (OMX_COMMANDTYPE)EXYNOS_OMX_CommandComponentDeInit, 0, NULL);
    Exynos_OSAL_SleepMillisec(0);
    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->hSemaMsgWait, &semaValue);
    if (semaValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgWait);
    Exynos_OSAL_SemaphorePost(pExynosComponent->hSemaMsgWait);

    Exynos_OSAL_ThreadTerminate(pExynosComponent->hMessageHandler);
    pExynosComponent->hMessageHandler = NULL;

    Exynos_OSAL_MutexTerminate(pExynosComponent->compEventMutex);
    pExynosComponent->compMutex = NULL;

    Exynos_OSAL_MutexTerminate(pExynosComponent->compMutex);
    pExynosComponent->compMutex = NULL;

    Exynos_OSAL_SemaphoreTerminate(pExynosComponent->hSemaMsgProgress);
    pExynosComponent->hSemaMsgProgress = NULL;

    Exynos_OSAL_SemaphoreTerminate(pExynosComponent->hSemaMsgWait);
    pExynosComponent->hSemaMsgWait = NULL;
    Exynos_OSAL_QueueTerminate(&pExynosComponent->messageQ);

    Exynos_OSAL_Free(pExynosComponent);
    pExynosComponent = NULL;

    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}
