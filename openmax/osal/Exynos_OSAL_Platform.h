/*
 * Copyright 2016 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OSAL_Platform.h
 * @brief
 * @author      Taehwan Kim (t_h.kim@samsung.com)
 * @version     1.0.0
 * @history
 *   2016.07.05 : Create
 */

#ifndef Exynos_OSAL_PLATFORM
#define Exynos_OSAL_PLATFORM

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Index.h"
#include "Exynos_OSAL_SharedMemory.h"

#include "Exynos_OSAL_Android.h"
#include "Exynos_OSAL_ImageConverter.h"

#ifdef __cplusplus
extern "C" {
#endif
OMX_ERRORTYPE Exynos_OSAL_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_INDEXTYPE nIndex,
                                       OMX_INOUT OMX_PTR pComponentParameterStructure);
OMX_ERRORTYPE Exynos_OSAL_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_INDEXTYPE nIndex,
                                       OMX_IN OMX_PTR pComponentParameterStructure);

OMX_ERRORTYPE Exynos_OSAL_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE nIndex,
                                    OMX_INOUT OMX_PTR pComponentConfigStructure);
OMX_ERRORTYPE Exynos_OSAL_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE nIndex,
                                    OMX_IN OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE Exynos_OSAL_LockMetaData(OMX_IN OMX_PTR pBuffer,
                                       OMX_IN EXYNOS_OMX_LOCK_RANGE range,
                                       OMX_OUT OMX_U32 *pStride,
                                       OMX_OUT EXYNOS_OMX_MULTIPLANE_BUFFER *pBufferInfo,
                                       OMX_IN EXYNOS_METADATA_TYPE eMetaType);
OMX_ERRORTYPE Exynos_OSAL_UnlockMetaData(OMX_IN OMX_PTR pBuffer,
                                         OMX_IN EXYNOS_METADATA_TYPE eMetaType);

OMX_ERRORTYPE Exynos_OSAL_GetInfoFromMetaData(OMX_IN OMX_PTR pBuffer, OMX_OUT EXYNOS_OMX_MULTIPLANE_BUFFER *pBufferInfo, OMX_IN EXYNOS_METADATA_TYPE eMetaDataType);

OMX_PTR Exynos_OSAL_AllocMetaDataBuffer(OMX_HANDLETYPE hSharedMemory, EXYNOS_METADATA_TYPE eMetaDataType, OMX_U32 nSizeBytes, MEMORY_TYPE eMemoryType);
OMX_ERRORTYPE Exynos_OSAL_FreeMetaDataBuffer(OMX_HANDLETYPE hSharedMemory, EXYNOS_METADATA_TYPE eMetaDataType, OMX_PTR pTempBuffer);
#ifdef __cplusplus
}
#endif
#endif
