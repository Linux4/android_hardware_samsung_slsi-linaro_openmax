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
 * @file       Exynos_OMX_Basecomponent.h
 * @brief
 * @author     SeungBeom Kim (sbcrux.kim@samsung.com)
 *             Taehwan Kim (t_h.kim@samsung.com)
 * @version    2.5.0
 * @history
 *    2012.02.20 : Create
*    2017.08.03 : Change event handling
 */

#ifndef EXYNOS_OMX_BASECOMP
#define EXYNOS_OMX_BASECOMP

#include "Exynos_OMX_Def.h"
#include "OMX_Component.h"
#include "Exynos_OSAL_Queue.h"
#include "Exynos_OMX_Baseport.h"


typedef struct _EXYNOS_OMX_MESSAGE
{
    OMX_U32 type;
    OMX_U32 param;
    OMX_PTR pCmdData;
} EXYNOS_OMX_MESSAGE;

/* for Check TimeStamp after Seek */
typedef struct _EXYNOS_OMX_TIMESTAMP
{
    OMX_BOOL  needSetStartTimeStamp;
    OMX_BOOL  needCheckStartTimeStamp;
    OMX_TICKS startTimeStamp;
    OMX_U32   nStartFlags;
} EXYNOS_OMX_TIMESTAMP;

typedef struct _EXYNOS_OMX_BASECOMPONENT
{
    OMX_STRING                  componentName;
    OMX_VERSIONTYPE             componentVersion;
    OMX_VERSIONTYPE             specVersion;

    OMX_STATETYPE               currentState;
    EXYNOS_OMX_TRANS_STATETYPE  transientState;
    OMX_BOOL                    abendState;

    EXYNOS_CODEC_TYPE           codecType;
    EXYNOS_OMX_PRIORITYMGMTTYPE compPriority;
    OMX_MARKTYPE                propagateMarkType;
    OMX_HANDLETYPE              compMutex;
    OMX_HANDLETYPE              compEventMutex;

    OMX_HANDLETYPE              hComponentHandle;

    /* Message Handler */
    OMX_BOOL                    bExitMessageHandlerThread;
    OMX_HANDLETYPE              hMessageHandler;
    OMX_HANDLETYPE              hSemaMsgWait;
    OMX_HANDLETYPE              hSemaMsgProgress;
    EXYNOS_QUEUE                messageQ;
    EXYNOS_QUEUE                dynamicConfigQ;

    /* Port */
    OMX_PORT_PARAM_TYPE         portParam;
    EXYNOS_OMX_BASEPORT        *pExynosPort;

    /* Callback function */
    OMX_CALLBACKTYPE           *pCallbacks;
    OMX_PTR                     callbackData;

    /* Save Timestamp */
    OMX_BOOL                    bTimestampSlotUsed[MAX_TIMESTAMP];
    OMX_TICKS                   timeStamp[MAX_TIMESTAMP];
    EXYNOS_OMX_TIMESTAMP        checkTimeStamp;

    /* Save Flags */
    OMX_U32                     nFlags[MAX_FLAGS];

    OMX_BOOL                    getAllDelayBuffer;
    OMX_BOOL                    reInputData;
    OMX_BOOL                    bUseImgCrop;

    /* HDR10+ */
    EXYNOS_QUEUE                     HDR10plusConfigQ;
    EXYNOS_OMX_VIDEO_HDR10PLUS_INFO  HDR10plusList[MAX_BUFFER_REF];

    OMX_BOOL bUseFlagEOF;
    OMX_BOOL bSaveFlagEOS;    /* bSaveFlagEOS is OMX_TRUE, if EOS flag is incoming. */
    OMX_BOOL bBehaviorEOS;    /* bBehaviorEOS is OMX_TRUE, if EOS flag with Data are incoming. */

    OMX_PTR                     vendorExts[MAX_VENDOR_EXT_NUM];

    OMX_ERRORTYPE (*exynos_codec_componentInit)(OMX_COMPONENTTYPE *pOMXComponent);
    OMX_ERRORTYPE (*exynos_codec_componentTerminate)(OMX_COMPONENTTYPE *pOMXComponent);

#ifdef TUNNELING_SUPPORT
    OMX_ERRORTYPE (*exynos_AllocateTunnelBuffer)(EXYNOS_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex);
    OMX_ERRORTYPE (*exynos_FreeTunnelBuffer)(EXYNOS_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex);
#endif

    OMX_ERRORTYPE (*exynos_BufferProcessCreate)(OMX_HANDLETYPE pOMXComponent);
    OMX_ERRORTYPE (*exynos_BufferProcessTerminate)(OMX_HANDLETYPE pOMXComponent);
    OMX_ERRORTYPE (*exynos_BufferFlush)(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent);
} EXYNOS_OMX_BASECOMPONENT;

OMX_ERRORTYPE Exynos_OMX_GetParameter(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nParamIndex,
    OMX_PTR        pParams);

OMX_ERRORTYPE Exynos_OMX_SetParameter(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nParamIndex,
    OMX_PTR        pParams);

OMX_ERRORTYPE Exynos_OMX_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nConfigIndex,
    OMX_PTR        pConfigs);

OMX_ERRORTYPE Exynos_OMX_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nConfigIndex,
    OMX_PTR        pConfigs);

OMX_ERRORTYPE Exynos_OMX_GetExtensionIndex(
    OMX_HANDLETYPE  hComponent,
    OMX_STRING      cParameterName,
    OMX_INDEXTYPE  *pIndexType);

OMX_PTR Exynos_OMX_MakeDynamicConfig(OMX_INDEXTYPE nConfigIndex, OMX_PTR pConfigs);
OMX_ERRORTYPE Exynos_OMX_SendEventCommand(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, EVENT_COMMAD_TYPE Cmd, OMX_PTR pCmdData);
void Exynos_OMX_Component_AbnormalTermination(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OMX_BaseComponent_Constructor(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OMX_BaseComponent_Destructor(OMX_HANDLETYPE hComponent);


#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Exynos_OMX_Check_SizeVersion(OMX_PTR header, OMX_U32 size);


#ifdef __cplusplus
};
#endif

#endif
