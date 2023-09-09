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
 * @file       Exynos_OMX_ABaseport.h
 * @brief
 * @author     SeungBeom Kim (sbcrux.kim@samsung.com)
 *             HyeYeon Chung (hyeon.chung@samsung.com)
 * @version    2.0.0
 * @history
 *    2012.02.20 : Create
 */

#ifndef EXYNOS_OMX_ABASE_PORT
#define EXYNOS_OMX_ABASE_PORT

#include "OMX_Component.h"
#include "Exynos_OMX_Def.h"
#include "Exynos_OSAL_Queue.h"
#include "Exynos_OMX_Def.h"

#ifdef PERFORMANCE_DEBUG
#include <sys/time.h>
#endif

#define BUFFER_STATE_ALLOCATED  (1 << 0)
#define BUFFER_STATE_ASSIGNED   (1 << 1)
#define HEADER_STATE_ALLOCATED  (1 << 2)
#define BUFFER_STATE_FREE        0

#define MAX_BUFFER_NUM          40

#define INPUT_PORT_INDEX    0
#define OUTPUT_PORT_INDEX   1
#define ALL_PORT_INDEX     -1
#define ALL_PORT_NUM        2


typedef struct _EXYNOS_OMX_BUFFERHEADERTYPE
{
    OMX_BUFFERHEADERTYPE *OMXBufferHeader;
    OMX_BOOL              bBufferInOMX;
} EXYNOS_OMX_BUFFERHEADERTYPE;

typedef struct _EXYNOS_OMX_DATABUFFER
{
    OMX_HANDLETYPE        bufferMutex;
    OMX_BUFFERHEADERTYPE* bufferHeader;
    OMX_BOOL              dataValid;
    OMX_U32               allocSize;
    OMX_U32               dataLen;
    OMX_U32               usedDataLen;
    OMX_U32               remainDataLen;
    OMX_U32               nFlags;
    OMX_TICKS             timeStamp;
    OMX_PTR               pPrivate;
} EXYNOS_OMX_DATABUFFER;

typedef struct _EXYNOS_OMX_MULTIPLANE_BUFFER
{
    OMX_U32                 nPlanes;
    unsigned long           fd[MAX_BUFFER_PLANE];
    OMX_PTR                 addr[MAX_BUFFER_PLANE];
    OMX_COLOR_FORMATTYPE    eColorFormat;
} EXYNOS_OMX_MULTIPLANE_BUFFER;

typedef struct _EXYNOS_OMX_DATA
{
    EXYNOS_OMX_MULTIPLANE_BUFFER buffer;
    OMX_U32   allocSize;
    OMX_U32   dataLen;
    OMX_U32   usedDataLen;
    OMX_U32   remainDataLen;
    OMX_U32   nFlags;
    OMX_TICKS timeStamp;
    OMX_PTR   pPrivate;

    /* For Share Buffer */
    OMX_BUFFERHEADERTYPE *bufferHeader;
} EXYNOS_OMX_DATA;

typedef enum _EXYNOS_OMX_EXCEPTION_STATE
{
    GENERAL_STATE = 0x00,
    NEED_PORT_FLUSH,
    NEED_PORT_DISABLE,
    INVALID_STATE,
} EXYNOS_OMX_EXCEPTION_STATE;

typedef struct _EXYNOS_OMX_BASEPORT
{
    EXYNOS_OMX_BUFFERHEADERTYPE   *extendBufferHeader;
    OMX_U32                       *bufferStateAllocate;
    OMX_PARAM_PORTDEFINITIONTYPE   portDefinition;
    OMX_HANDLETYPE                 bufferSemID;
    EXYNOS_QUEUE                   bufferQ;
    OMX_S32                        assignedBufferNum;
    EXYNOS_OMX_PORT_STATETYPE      portState;
    OMX_HANDLETYPE                 loadedResource;
    OMX_HANDLETYPE                 unloadedResource;

    OMX_MARKTYPE                   markType;

    /* Tunnel Info */
    OMX_HANDLETYPE                 tunneledComponent;
    OMX_U32                        tunneledPort;
    OMX_U32                        tunnelBufferNum;
    OMX_BUFFERSUPPLIERTYPE         bufferSupplier;
    OMX_U32                        tunnelFlags;

    OMX_COLOR_FORMATTYPE          *supportFormat;

    OMX_HANDLETYPE                 pauseEvent;

    EXYNOS_OMX_DATABUFFER          dataBuffer;

    /* Data */
    EXYNOS_OMX_DATA                processData;

    OMX_HANDLETYPE                 hPortMutex;
    EXYNOS_OMX_EXCEPTION_STATE     exceptionFlag;
} EXYNOS_OMX_BASEPORT;


#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Exynos_OMX_PortEnableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex);
OMX_ERRORTYPE Exynos_OMX_PortDisableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex);
OMX_ERRORTYPE Exynos_OMX_BufferFlushProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent);
OMX_ERRORTYPE Exynos_OMX_Port_Constructor(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OMX_Port_Destructor(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_ResetCodecData(EXYNOS_OMX_DATA *pData);
OMX_ERRORTYPE Exynos_OMX_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE *bufferHeader);
OMX_ERRORTYPE Exynos_OMX_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE *bufferHeader);

#ifdef __cplusplus
};
#endif


#endif
