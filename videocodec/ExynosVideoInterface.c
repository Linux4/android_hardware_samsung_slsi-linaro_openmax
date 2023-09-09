/*
 * Copyright 2013 Samsung Electronics S.LSI Co. LTD
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
 * @file       ExynosVideoInterface.c
 * @brief
 * @author     Satish Kumar Reddy (palli.satish@samsung.com)
 * @version    1.0
 * @history
 *    2013.08.14 : Initial versaion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "ExynosVideoApi.h"
#include "ExynosVideoDec.h"
#include "ExynosVideoEnc.h"

/* #define LOG_NDEBUG 0 */
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ExynosVideoInterface"

ExynosVideoErrorType Exynos_Video_GetInstInfo(
    ExynosVideoInstInfo *pVideoInstInfo,
    ExynosVideoBoolType  bIsDec)
{
    if (bIsDec == VIDEO_TRUE)
        return MFC_Exynos_Video_GetInstInfo_Decoder(pVideoInstInfo);
    else
        return MFC_Exynos_Video_GetInstInfo_Encoder(pVideoInstInfo);
}

int Exynos_Video_Register_Decoder(
    ExynosVideoDecOps       *pDecOps,
    ExynosVideoDecBufferOps *pInbufOps,
    ExynosVideoDecBufferOps *pOutbufOps)
{
    ExynosVideoErrorType ret = VIDEO_ERROR_NONE;

    if ((pDecOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    ret = MFC_Exynos_Video_Register_Decoder(pDecOps, pInbufOps, pOutbufOps);

EXIT:
    return ret;
}

int Exynos_Video_Register_Encoder(
    ExynosVideoEncOps       *pEncOps,
    ExynosVideoEncBufferOps *pInbufOps,
    ExynosVideoEncBufferOps *pOutbufOps)
{
    ExynosVideoErrorType ret = VIDEO_ERROR_NONE;

    if ((pEncOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    ret = MFC_Exynos_Video_Register_Encoder(pEncOps, pInbufOps, pOutbufOps);

EXIT:
    return ret;
}

void Exynos_Video_Unregister_Decoder(
    ExynosVideoDecOps       *pDecOps,
    ExynosVideoDecBufferOps *pInbufOps,
    ExynosVideoDecBufferOps *pOutbufOps)
{
    if ((pDecOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        goto EXIT;
    }

    memset(pDecOps, 0, sizeof(ExynosVideoDecOps));
    memset(pInbufOps, 0, sizeof(ExynosVideoDecBufferOps));
    memset(pOutbufOps, 0, sizeof(ExynosVideoDecBufferOps));

EXIT:
    return;
}

void Exynos_Video_Unregister_Encoder(
    ExynosVideoEncOps       *pEncOps,
    ExynosVideoEncBufferOps *pInbufOps,
    ExynosVideoEncBufferOps *pOutbufOps)
{
    if ((pEncOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        goto EXIT;
    }

    memset(pEncOps, 0, sizeof(ExynosVideoEncOps));
    memset(pInbufOps, 0, sizeof(ExynosVideoEncBufferOps));
    memset(pOutbufOps, 0, sizeof(ExynosVideoEncBufferOps));

EXIT:
    return;
}
