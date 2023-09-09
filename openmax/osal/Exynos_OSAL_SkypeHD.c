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
 * @file        Exynos_OSAL_SkypeHD.cpp
 * @brief
 * @author      Seungbeom Kim (sbcrux.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2015.04.27 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Macros.h"
#include "Exynos_OSAL_SkypeHD.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OMX_Def.h"
#include "exynos_format.h"

#include "ExynosVideoApi.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "Exynos_OSAL_SkypeHD"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

#include "Exynos_OSAL_SkypeHD.h"

/* ENCODE_ONLY */
#ifdef BUILD_ENC
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_H264enc.h"
#endif

/* DECODE_ONLY */
#ifdef BUILD_DEC
#include "Exynos_OMX_Vdec.h"
#include "Exynos_OMX_H264dec.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ENCODE_ONLY */
#ifdef BUILD_ENC
/* video enc */
OMX_ERRORTYPE Exynos_H264Enc_GetParameter_SkypeHD(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{

    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

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

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    switch ((int)nParamIndex) {
    case OMX_IndexSkypeParamDriverVersion:
    {
        OMX_VIDEO_PARAM_DRIVERVER *pDriverVer = (OMX_VIDEO_PARAM_DRIVERVER *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDriverVer, sizeof(OMX_VIDEO_PARAM_DRIVERVER));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDriverVer->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDriverVer->nDriverVersion = (OMX_U64)(pH264Enc->hMFCH264Handle.videoInstInfo.SwVersion);
    }
        break;
    case OMX_IndexSkypeParamLowLatency:
    {
        OMX_PARAM_U32TYPE *pLowLatency = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pLowLatency, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pLowLatency->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pLowLatency->nU32 = (pH264Enc->hMFCH264Handle.bLowLatency == OMX_TRUE)? 1:0;
    }
        break;
    case OMX_IndexSkypeParamEncoderMaxTemporalLayerCount:
    {
        OMX_PARAM_ENC_MAX_TEMPORALLAYER_COUNT *pTemporalLayer = (OMX_PARAM_ENC_MAX_TEMPORALLAYER_COUNT *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pTemporalLayer, sizeof(OMX_PARAM_ENC_MAX_TEMPORALLAYER_COUNT));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pTemporalLayer->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pTemporalLayer->nMaxCountP = OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS;
        pTemporalLayer->nMaxCountB = (OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS - 1);
    }
        break;
    case OMX_IndexSkypeParamEncoderMaxLTR:
    {
        OMX_PARAM_U32TYPE *pLTR = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pLTR, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pLTR->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pLTR->nU32 = OMX_VIDEO_MAX_LTR_FRAMES;
    }
        break;
    case OMX_IndexSkypeParamEncoderLTR:
    {
        OMX_PARAM_U32TYPE *pLTR = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pLTR, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pLTR->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pLTR->nU32 = pH264Enc->hMFCH264Handle.nLTRFrames;
    }
        break;
    case OMX_IndexSkypeParamEncoderPreprocess:
    {
        OMX_PARAM_ENC_PREPROCESS *pPreprocess = (OMX_PARAM_ENC_PREPROCESS *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pPreprocess, sizeof(OMX_PARAM_ENC_PREPROCESS));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pPreprocess->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pPreprocess->nResize   = 0;  /* 1:1 */
        pPreprocess->nRotation = 0;  /* not supported */
    }
        break;
    case OMX_IndexSkypeParamEncoderSar:
    {
        OMX_PARAM_ENC_SAR *pSar = (OMX_PARAM_ENC_SAR *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pSar, sizeof(OMX_PARAM_ENC_SAR));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSar->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSar->nWidth  = pH264Enc->hMFCH264Handle.stSarParam.SarWidth;
        pSar->nHeight = pH264Enc->hMFCH264Handle.stSarParam.SarHeight;
    }
        break;
    case OMX_IndexSkypeParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE     *pVideoBitrate  = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32                          nPortIndex     = pVideoBitrate->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE    *pPortDef       = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pPortDef = &(pExynosComponent->pExynosPort[nPortIndex].portDefinition);

            pVideoBitrate->eControlRate = pVideoEnc->eControlRate[nPortIndex];
            pVideoBitrate->nTargetBitrate = pPortDef->format.video.nBitrate;
        }
    }
        break;
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_SetParameter_SkypeHD(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

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

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    switch ((int)nIndex) {
    case OMX_IndexSkypeParamLowLatency:
    {
        OMX_PARAM_U32TYPE *pLowLatency = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pLowLatency, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pLowLatency->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pH264Enc->hMFCH264Handle.bLowLatency = (pLowLatency->nU32 == 0)? OMX_FALSE:OMX_TRUE;
    }
        break;
    case OMX_IndexSkypeParamEncoderLTR:
    {
        OMX_PARAM_U32TYPE *pLTR = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pLTR, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pLTR->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pLTR->nU32 > OMX_VIDEO_MAX_LTR_FRAMES) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pH264Enc->hMFCH264Handle.nLTRFrames = pLTR->nU32;
    }
        break;
    case OMX_IndexSkypeParamEncoderSar:
    {
        OMX_PARAM_ENC_SAR *pSar = (OMX_PARAM_ENC_SAR *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pSar, sizeof(OMX_PARAM_ENC_SAR));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSar->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pH264Enc->hMFCH264Handle.stSarParam.SarEnable = OMX_TRUE;
        pH264Enc->hMFCH264Handle.stSarParam.SarIndex  = 0xFF;  /* depends on width/height info */
        pH264Enc->hMFCH264Handle.stSarParam.SarWidth  = pSar->nWidth;
        pH264Enc->hMFCH264Handle.stSarParam.SarHeight = pSar->nHeight;
    }
        break;
    case OMX_IndexSkypeParamEncoderInputControl:
    {
        OMX_PARAM_U32TYPE *pInputControl = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pInputControl, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pInputControl->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pInputControl->nU32 > 0) {
            pH264Enc->hMFCH264Handle.bEnableSkypeHD = OMX_TRUE;
        } else {
            pH264Enc->hMFCH264Handle.bEnableSkypeHD = OMX_FALSE;
        }
    }
        break;
    case OMX_IndexSkypeParamVideoBitrate:
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

            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] Bitrate mode is set through vendor extension",
                                                       pExynosComponent, __FUNCTION__);
            pH264Enc->hMFCH264Handle.bSkypeBitrate = OMX_TRUE;
        }
        ret = OMX_ErrorNone;
    }
        break;
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_GetConfig_SkypeHD(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = NULL;

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

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pH264Enc  = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    switch ((int)nIndex) {
    case OMX_IndexSkypeConfigBasePid:
    {
        OMX_VIDEO_CONFIG_BASELAYERPID *pBaseLayerPid = (OMX_VIDEO_CONFIG_BASELAYERPID *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pBaseLayerPid, sizeof(OMX_VIDEO_CONFIG_BASELAYERPID));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pBaseLayerPid->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pBaseLayerPid->nPID = pH264Enc->hMFCH264Handle.nBaseLayerPid;
    }
        break;
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_SetConfig_SkypeHD(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = NULL;

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

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pH264Enc  = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    switch ((int)nIndex) {
    case OMX_IndexSkypeConfigMarkLTRFrame:
    {
        OMX_VIDEO_CONFIG_MARKLTRFRAME *pMarkLTRFrame = (OMX_VIDEO_CONFIG_MARKLTRFRAME *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pMarkLTRFrame, sizeof(OMX_VIDEO_CONFIG_MARKLTRFRAME));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pMarkLTRFrame->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeConfigUseLTRFrame:
    {
        OMX_VIDEO_CONFIG_USELTRFRAME *pUseLTRFrame = (OMX_VIDEO_CONFIG_USELTRFRAME *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pUseLTRFrame, sizeof(OMX_VIDEO_CONFIG_USELTRFRAME));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pUseLTRFrame->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeConfigQP:
    {
        OMX_VIDEO_CONFIG_QP *pConfigQp = (OMX_VIDEO_CONFIG_QP *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pConfigQp, sizeof(OMX_VIDEO_CONFIG_QP));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pConfigQp->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pVideoEnc->quantization.nQpI = pConfigQp->nQP;
        pVideoEnc->quantization.nQpP = pConfigQp->nQP;
        pVideoEnc->quantization.nQpB = pConfigQp->nQP;
    }
        break;
    case OMX_IndexSkypeConfigBasePid:
    {
        OMX_VIDEO_CONFIG_BASELAYERPID *pBaseLayerPid = (OMX_VIDEO_CONFIG_BASELAYERPID *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pBaseLayerPid, sizeof(OMX_VIDEO_CONFIG_BASELAYERPID));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pBaseLayerPid->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pH264Enc->hMFCH264Handle.nBaseLayerPid = pBaseLayerPid->nPID;
    }
        break;
    case OMX_IndexSkypeConfigEncoderInputTrigger:
    {
        OMX_CONFIG_ENC_TRIGGER_TS *pTriggerTS = (OMX_CONFIG_ENC_TRIGGER_TS *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pTriggerTS, sizeof(OMX_CONFIG_ENC_TRIGGER_TS));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pTriggerTS->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
        break;
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Change_H264Enc_SkypeHDParam(
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
    OMX_PTR                     pDynamicConfigCMD)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc         = NULL;
    EXYNOS_H264ENC_HANDLE         *pH264Enc          = NULL;
    EXYNOS_MFC_H264ENC_HANDLE     *pMFCH264Handle    = NULL;
    OMX_PTR                        pConfigData       = NULL;
    OMX_S32                        nCmdIndex         = 0;
    ExynosVideoEncOps             *pEncOps           = NULL;
    int                            nValue            = 0;

    int i;

    FunctionIn();

    pVideoEnc       = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pH264Enc        = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCH264Handle  = &pH264Enc->hMFCH264Handle;
    pEncOps         = pMFCH264Handle->pEncOps;

    if (pDynamicConfigCMD == NULL)
        goto EXIT;

    nCmdIndex   = *(OMX_S32 *)pDynamicConfigCMD;
    pConfigData = (OMX_PTR)((OMX_U8 *)pDynamicConfigCMD + sizeof(OMX_S32));

    switch ((int)nCmdIndex) {
    case OMX_IndexSkypeConfigMarkLTRFrame:
    {
        OMX_VIDEO_CONFIG_MARKLTRFRAME *pMarkLTRFrame = (OMX_VIDEO_CONFIG_MARKLTRFRAME *)pConfigData;

        ret = Exynos_OMX_Check_SizeVersion(pMarkLTRFrame, sizeof(OMX_VIDEO_CONFIG_MARKLTRFRAME));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pMarkLTRFrame->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Mark LTR : index(%d)",
                                            pExynosComponent, __FUNCTION__, pMarkLTRFrame->nLongTermFrmIdx + 1);

        pEncOps->Set_MarkLTRFrame(pMFCH264Handle->hMFCHandle, pMarkLTRFrame->nLongTermFrmIdx + 1);
    }
        break;
    case OMX_IndexSkypeConfigUseLTRFrame:
    {
        OMX_VIDEO_CONFIG_USELTRFRAME *pUseLTRFrame = (OMX_VIDEO_CONFIG_USELTRFRAME *)pConfigData;

        ret = Exynos_OMX_Check_SizeVersion(pUseLTRFrame, sizeof(OMX_VIDEO_CONFIG_USELTRFRAME));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pUseLTRFrame->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Use LTR : BM(%x)",
                                            pExynosComponent, __FUNCTION__, pUseLTRFrame->nUsedLTRFrameBM);

        pEncOps->Set_UsedLTRFrame(pMFCH264Handle->hMFCHandle, pUseLTRFrame->nUsedLTRFrameBM);
    }
        break;
    case OMX_IndexSkypeConfigQP:
    {
        OMX_VIDEO_CONFIG_QP *pConfigQp = (OMX_VIDEO_CONFIG_QP *)pConfigData;

        ret = Exynos_OMX_Check_SizeVersion(pConfigQp, sizeof(OMX_VIDEO_CONFIG_QP));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pConfigQp->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] QP : %d", pExynosComponent, __FUNCTION__, pConfigQp->nQP);

        pEncOps->Set_DynamicQpControl(pMFCH264Handle->hMFCHandle, pConfigQp->nQP);
    }
        break;
    case OMX_IndexSkypeConfigBasePid:
    {
        OMX_VIDEO_CONFIG_BASELAYERPID *pBaseLayerPid = (OMX_VIDEO_CONFIG_BASELAYERPID *)pConfigData;

        ret = Exynos_OMX_Check_SizeVersion(pBaseLayerPid, sizeof(OMX_VIDEO_CONFIG_BASELAYERPID));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pBaseLayerPid->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] BasePID : %d", pExynosComponent, __FUNCTION__, pBaseLayerPid->nPID);

        pEncOps->Set_BasePID(pMFCH264Handle->hMFCHandle, pBaseLayerPid->nPID);
    }
        break;
    case OMX_IndexSkypeConfigEncoderInputTrigger:
    {
        OMX_CONFIG_ENC_TRIGGER_TS *pTriggerTS = (OMX_CONFIG_ENC_TRIGGER_TS *)pConfigData;
        EXYNOS_OMX_BASEPORT       *pInputPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);

        ret = Exynos_OMX_Check_SizeVersion(pTriggerTS, sizeof(OMX_CONFIG_ENC_TRIGGER_TS));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pTriggerTS->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pMFCH264Handle->nTriggerTS = pTriggerTS->nTimestamp;

        if (pInputPort->processData.timeStamp == pMFCH264Handle->nTriggerTS) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input trigger is matched input timestamp : %lld",
                                                    pExynosComponent, __FUNCTION__, pTriggerTS->nTimestamp);
            ret = OMX_ErrorNoMore;
            goto EXIT;
        }

        if (Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ) <= 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] no more input trigger", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNoMore;
            goto EXIT;
        }
    }
        break;
    default:
        break;
    }

EXIT:

    FunctionOut();

    return ret;
}
#endif


/* DECODE_ONLY */
#ifdef BUILD_DEC
/* video dec */
OMX_ERRORTYPE Exynos_H264Dec_GetParameter_SkypeHD(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_H264DEC_HANDLE           *pH264Dec          = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentParameterStructure == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
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

    if (pVideoDec->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pH264Dec = (EXYNOS_H264DEC_HANDLE *)pVideoDec->hCodecHandle;

    switch ((int)nParamIndex) {
    case OMX_IndexSkypeParamDriverVersion:
    {
        OMX_VIDEO_PARAM_DRIVERVER *pDriverVer = (OMX_VIDEO_PARAM_DRIVERVER *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDriverVer, sizeof(OMX_VIDEO_PARAM_DRIVERVER));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDriverVer->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDriverVer->nDriverVersion = (OMX_U64)(pH264Dec->hMFCH264Handle.videoInstInfo.SwVersion);
    }
        break;
    case OMX_IndexSkypeParamLowLatency:
    {
        OMX_PARAM_U32TYPE *pLowLatency = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pLowLatency, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pLowLatency->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pLowLatency->nU32 = (pH264Dec->hMFCH264Handle.bLowLatency == OMX_TRUE)? 1:0;
    }
        break;
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Dec_SetParameter_SkypeHD(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec         = NULL;
    EXYNOS_H264DEC_HANDLE           *pH264Dec          = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
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

    if (pVideoDec->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pH264Dec = (EXYNOS_H264DEC_HANDLE *)pVideoDec->hCodecHandle;

    switch ((int)nIndex) {
    case OMX_IndexSkypeParamLowLatency:
    {
        OMX_PARAM_U32TYPE *pLowLatency = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pLowLatency, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pLowLatency->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pH264Dec->hMFCH264Handle.bLowLatency = (pLowLatency->nU32 == 0)? OMX_FALSE:OMX_TRUE;
    }
        break;
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;

    }

EXIT:

    FunctionOut();

    return ret;
}
#endif

#ifdef __cplusplus
}
#endif
