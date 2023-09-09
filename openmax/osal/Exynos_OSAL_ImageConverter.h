/*
 * Copyright 2019 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OSAL_ImageConverter.h
 * @brief
 * @author      Taehwan Kim (t_h.kim@samsung.com)
 * @author      Byunggwan Kang (bk0917.kang@samsung.com)
 * @author      Seungbeom Kim (sbcrux.kim@samsung.com)
 * @version     1.0.0
 * @history
 *   2019.02.12 : Create
 */

#ifndef Exynos_OSAL_IMAGE_CONVERTER
#define Exynos_OSAL_IMAGE_CONVERTER

#include "OMX_Types.h"
#include "OMX_Component.h"

#ifdef __cplusplus
extern "C" {
#endif
OMX_HANDLETYPE Exynos_OSAL_ImgConv_Create(OMX_U32 nWidth, OMX_U32 nHeight, OMX_U32 nMode);
void Exynos_OSAL_ImgConv_Terminate(OMX_HANDLETYPE hImgConv);
OMX_ERRORTYPE Exynos_OSAL_ImgConv_Run(OMX_HANDLETYPE hImgConv, OMX_COMPONENTTYPE *pOMXComponent, OMX_PTR pBuffer, OMX_PTR pHDRDynamic);
#ifdef __cplusplus
}
#endif

#endif
