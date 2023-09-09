/*
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
 * @file        Exynos_OSAL_Android.h
 * @brief
 * @author      Seungbeom Kim (sbcrux.kim@samsung.com)
 * @author      Hyeyeon Chung (hyeon.chung@samsung.com)
 * @author      Yunji Kim (yunji.kim@samsung.com)
 * @author      Jinsung Yang (jsgood.yang@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#ifndef Exynos_OSAL_ANDROID
#define Exynos_OSAL_ANDROID

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Index.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OSAL_SharedMemory.h"

#include "ExynosVideoApi.h"

#ifdef __cplusplus
extern "C" {
#endif
OMX_HANDLETYPE Exynos_OSAL_RefCount_Create();
OMX_ERRORTYPE Exynos_OSAL_RefCount_Reset(OMX_HANDLETYPE hREF);
OMX_ERRORTYPE Exynos_OSAL_RefCount_Terminate(OMX_HANDLETYPE hREF);
OMX_ERRORTYPE Exynos_OSAL_RefCount_Increase(OMX_HANDLETYPE hREF, OMX_PTR pBuffer, EXYNOS_OMX_BASEPORT *pExynosPort);
OMX_ERRORTYPE Exynos_OSAL_RefCount_Decrease(OMX_HANDLETYPE hREF, OMX_PTR pBuffer, ReleaseDPB dpbFD[VIDEO_BUFFER_MAX_NUM], EXYNOS_OMX_BASEPORT *pExynosPort);

OMX_ERRORTYPE Exynos_OSAL_SetPrependSPSPPSToIDR(OMX_PTR pComponentParameterStructure,
                                                OMX_PTR pbPrependSpsPpsToIdr);

OMX_U32 Exynos_OSAL_GetDisplayExtraBufferCount(void);

void getColorAspectsPreferBitstream(EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA, EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA, void *pParam);
void getColorAspectsPreferFramework(EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA, EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA, void *pParam);

void Exynos_OSAL_GetColorAspectsForBitstream(EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA, EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA);
void Exynos_OSAL_ColorSpaceToColorAspects(int colorSpace, EXYNOS_OMX_VIDEO_COLORASPECTS *colorAspects);

OMX_ERRORTYPE setHDR10PlusInfoForFramework(OMX_COMPONENTTYPE *pOMXComponent, void *pHDRDynamicInfo);
OMX_ERRORTYPE setHDR10PlusInfoForVendorPath(OMX_COMPONENTTYPE *pOMXComponent, void *pExynosHDR10PlusInfo, void *pHDRDynamicInfo);

OMX_ERRORTYPE Exynos_OSAL_AddVendorExt(OMX_HANDLETYPE hComponent, OMX_STRING cExtName, OMX_INDEXTYPE nIndex);
void          Exynos_OSAL_DelVendorExts(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Exynos_OSAL_GetVendorExt(OMX_HANDLETYPE hComponent, OMX_PTR pConfig);
OMX_ERRORTYPE Exynos_OSAL_SetVendorExt(OMX_HANDLETYPE hComponent, OMX_PTR pConfig);

OMX_HANDLETYPE Exynos_OSAL_CreatePerformanceHandle(OMX_BOOL bIsEncoder);
void Exynos_OSAL_Performance(OMX_HANDLETYPE handle, int value, int fps);
void Exynos_OSAL_ReleasePerformanceHandle(OMX_HANDLETYPE handle);
OMX_ERRORTYPE Exynos_OSAL_UpdateDataSpaceFromAspects(EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA);
void Exynos_OSAL_UpdateDataSpaceFromBitstream(EXYNOS_OMX_BASECOMPONENT *pExynosComponent);
void Exynos_OSAL_UpdateDataspaceToGraphicMeta(OMX_PTR pBuf, int nDataSpace);

#ifdef __cplusplus
}
#endif

#endif
