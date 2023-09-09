/*
 *
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
 * @file        Exynos_OMX_Vp8enc.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Taehwan Kim (t_h.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2013.02.14 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_VencControl.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "library_register.h"
#include "Exynos_OMX_Vp8enc.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OSAL_Queue.h"

#include "Exynos_OSAL_Platform.h"

#include "VendorVideoAPI.h"

/* To use CSC_METHOD_HW in EXYNOS OMX, gralloc should allocate physical memory using FIMC */
/* It means GRALLOC_USAGE_HW_FIMC1 should be set on Native Window usage */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VP8_ENC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

#define VP8_QP_INDEX_RANGE 64

static OMX_ERRORTYPE SetProfileLevel(
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc        = NULL;

    int nProfileCnt = 0;

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVp8Enc->hMFCVp8Handle.profiles[nProfileCnt++] = OMX_VIDEO_VP8ProfileMain;
    pVp8Enc->hMFCVp8Handle.nProfileCnt = nProfileCnt;
    pVp8Enc->hMFCVp8Handle.maxLevel = OMX_VIDEO_VP8Level_Version3;

EXIT:
    return ret;
}

static OMX_ERRORTYPE GetIndexToProfileLevel(
    EXYNOS_OMX_BASECOMPONENT         *pExynosComponent,
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevelType)
{
    OMX_ERRORTYPE                    ret           = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc     = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc       = NULL;

    int nLevelCnt = 0;
    OMX_U32 nMaxIndex = 0;

    FunctionIn();

    if ((pExynosComponent == NULL) ||
        (pProfileLevelType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (pVp8Enc->hMFCVp8Handle.nProfileCnt <= (int)pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pVp8Enc->hMFCVp8Handle.profiles[pProfileLevelType->nProfileIndex];
    pProfileLevelType->eLevel   = pVp8Enc->hMFCVp8Handle.maxLevel;
#else
    while ((pVp8Enc->hMFCVp8Handle.maxLevel >> nLevelCnt) > 0) {
        nLevelCnt++;
    }

    if ((pVp8Enc->hMFCVp8Handle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    nMaxIndex = pVp8Enc->hMFCVp8Handle.nProfileCnt * nLevelCnt;
    if (nMaxIndex <= pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pVp8Enc->hMFCVp8Handle.profiles[pProfileLevelType->nProfileIndex / nLevelCnt];
    pProfileLevelType->eLevel = 0x1 << (pProfileLevelType->nProfileIndex % nLevelCnt);
#endif

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] : supported profile(%x), level(%x)",
                                            pExynosComponent, __FUNCTION__, pProfileLevelType->eProfile, pProfileLevelType->eLevel);

EXIT:
    return ret;
}

static OMX_BOOL CheckProfileLevelSupport(
    EXYNOS_OMX_BASECOMPONENT         *pExynosComponent,
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevelType)
{
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc  = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc    = NULL;

    OMX_BOOL bProfileSupport = OMX_FALSE;
    OMX_BOOL bLevelSupport   = OMX_FALSE;

    int nLevelCnt = 0;
    int i;

    FunctionIn();

    if ((pExynosComponent == NULL) ||
        (pProfileLevelType == NULL)) {
        goto EXIT;
    }

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL)
        goto EXIT;

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc == NULL)
        goto EXIT;

    while ((pVp8Enc->hMFCVp8Handle.maxLevel >> nLevelCnt++) > 0);

    if ((pVp8Enc->hMFCVp8Handle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pVp8Enc->hMFCVp8Handle.nProfileCnt; i++) {
        if (pVp8Enc->hMFCVp8Handle.profiles[i] == pProfileLevelType->eProfile) {
            bProfileSupport = OMX_TRUE;
            break;
        }
    }

    if (bProfileSupport != OMX_TRUE)
        goto EXIT;

    while (nLevelCnt >= 0) {
        if ((int)pProfileLevelType->eLevel == (0x1 << nLevelCnt)) {
            bLevelSupport = OMX_TRUE;
            break;
        }

        nLevelCnt--;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] : profile(%x)/level(%x) is %ssupported", pExynosComponent, __FUNCTION__,
                                            pProfileLevelType->eProfile, pProfileLevelType->eLevel,
                                            (bProfileSupport && bLevelSupport)? "":"not ");

EXIT:
    return (bProfileSupport && bLevelSupport);
}

static OMX_U32 OMXVP8ProfileToProfileIDC(OMX_VIDEO_VP8PROFILETYPE eProfile)
{
    OMX_U32 ret;

    switch (eProfile) {
    case OMX_VIDEO_VP8ProfileMain:
        ret = 0;
        break;
    default:
        ret = 0;
        break;
    }

    return ret;
}

static OMX_U32 OMXVP8LevelToMFCVersion(OMX_VIDEO_VP8LEVELTYPE eLevel)
{
    OMX_U32 ret;

    switch (eLevel) {
    case OMX_VIDEO_VP8Level_Version0:
        ret = 0;
        break;
    case OMX_VIDEO_VP8Level_Version1:
        ret = 1;
        break;
    case OMX_VIDEO_VP8Level_Version2:
        ret = 2;
        break;
    case OMX_VIDEO_VP8Level_Version3:
        ret = 3;
        break;
    default:
        ret = 0;
        break;
    }

    return ret;
}

static void Print_VP8Enc_Param(ExynosVideoEncParam *pEncParam)
{
    ExynosVideoEncCommonParam *pCommonParam = &pEncParam->commonParam;
    ExynosVideoEncVp8Param    *pVp8Param    = &pEncParam->codecParam.vp8;

    /* common parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "SourceWidth             : %d", pCommonParam->SourceWidth);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "SourceHeight            : %d", pCommonParam->SourceHeight);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "IDRPeriod               : %d", pCommonParam->IDRPeriod);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "SliceMode               : %d", pCommonParam->SliceMode);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "RandomIntraMBRefresh    : %d", pCommonParam->RandomIntraMBRefresh);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "Bitrate                 : %d", pCommonParam->Bitrate);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameQp                 : %d", pCommonParam->FrameQp);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameQp_P               : %d", pCommonParam->FrameQp_P);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "QP(I) ranege            : %d / %d", pCommonParam->QpRange.QpMin_I, pCommonParam->QpRange.QpMax_I);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "QP(P) ranege            : %d / %d", pCommonParam->QpRange.QpMin_P, pCommonParam->QpRange.QpMax_P);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "PadControlOn            : %d", pCommonParam->PadControlOn);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LumaPadVal              : %d", pCommonParam->LumaPadVal);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "CbPadVal                : %d", pCommonParam->CbPadVal);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "CrPadVal                : %d", pCommonParam->CrPadVal);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameMap                : %d", pCommonParam->FrameMap);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "DropControl             : %d", pCommonParam->bDropControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "DisableDFR              : %d", pCommonParam->bDisableDFR);

    /* Vp8 specific parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameRate               : %d", pVp8Param->FrameRate);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "Vp8Version              : %d", pVp8Param->Vp8Version);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "Vp8NumberOfPartitions   : %d", pVp8Param->Vp8NumberOfPartitions);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "Vp8FilterLevel          : %d", pVp8Param->Vp8FilterLevel);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "Vp8FilterSharpness      : %d", pVp8Param->Vp8FilterSharpness);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "Vp8GoldenFrameSel       : %d", pVp8Param->Vp8GoldenFrameSel);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "Vp8GFRefreshPeriod      : %d", pVp8Param->Vp8GFRefreshPeriod);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "RefNumberForPFrame      : %d", pVp8Param->RefNumberForPFrame);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "DisableIntraMd4x4       : %d", pVp8Param->DisableIntraMd4x4);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "NumTemporalLayer        : %d", pVp8Param->TemporalSVC.nTemporalLayerCount);

    /* rate control related parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableFRMRateControl    : %d", pCommonParam->EnableFRMRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableMBRateControl     : %d", pCommonParam->EnableMBRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "CBRPeriodRf             : %d", pCommonParam->CBRPeriodRf);
}

static void Set_VP8Enc_Param(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    EXYNOS_OMX_BASEPORT           *pInputPort       = NULL;
    EXYNOS_OMX_BASEPORT           *pOutputPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc          = NULL;
    EXYNOS_MFC_VP8ENC_HANDLE      *pMFCVp8Handle    = NULL;
    OMX_COLOR_FORMATTYPE           eColorFormat     = OMX_COLOR_FormatUnused;

    ExynosVideoEncParam       *pEncParam    = NULL;
    ExynosVideoEncCommonParam *pCommonParam = NULL;
    ExynosVideoEncVp8Param    *pVp8Param    = NULL;

    OMX_S32 nWidth, nHeight;
    int i;

    pVideoEnc       = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVp8Enc         = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCVp8Handle   = &pVp8Enc->hMFCVp8Handle;
    pInputPort      = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pOutputPort     = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pEncParam       = &pMFCVp8Handle->encParam;
    pCommonParam    = &pEncParam->commonParam;
    pVp8Param       = &pEncParam->codecParam.vp8;

    pEncParam->eCompressionFormat = VIDEO_CODING_VP8;

    /* uses specified target size instead of size on output port */
    if (pOutputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_TRUE) {
        nWidth  = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
        nHeight = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
    } else {
        nWidth  = pOutputPort->portDefinition.format.video.nFrameWidth;
        nHeight = pOutputPort->portDefinition.format.video.nFrameHeight;
    }

    /* common parameters */
    if ((pVideoEnc->eRotationType == ROTATE_0) ||
        (pVideoEnc->eRotationType == ROTATE_180)) {
        pCommonParam->SourceWidth  = nWidth;
        pCommonParam->SourceHeight = nHeight;
    } else {
        pCommonParam->SourceWidth  = nHeight;
        pCommonParam->SourceHeight = nWidth;
    }
    pCommonParam->IDRPeriod    = pVp8Enc->nPFrames + 1;
    pCommonParam->SliceMode    = 0;
    pCommonParam->Bitrate      = pOutputPort->portDefinition.format.video.nBitrate;
    pCommonParam->FrameQp      = pVideoEnc->quantization.nQpI;
    pCommonParam->FrameQp_P    = pVideoEnc->quantization.nQpP;

    pCommonParam->QpRange.QpMin_I = pVp8Enc->qpRangeI.nMinQP;
    pCommonParam->QpRange.QpMax_I = pVp8Enc->qpRangeI.nMaxQP;
    pCommonParam->QpRange.QpMin_P = pVp8Enc->qpRangeP.nMinQP;
    pCommonParam->QpRange.QpMax_P = pVp8Enc->qpRangeP.nMaxQP;

    pCommonParam->PadControlOn = 0;    /* 0: Use boundary pixel, 1: Use the below setting value */
    pCommonParam->LumaPadVal   = 0;
    pCommonParam->CbPadVal     = 0;
    pCommonParam->CrPadVal     = 0;

    if (pVideoEnc->intraRefresh.eRefreshMode == OMX_VIDEO_IntraRefreshCyclic) {
        /* Cyclic Mode */
        pCommonParam->RandomIntraMBRefresh = pVideoEnc->intraRefresh.nCirMBs;
    } else {
        /* Don't support "Adaptive" and "Cyclic + Adaptive" */
        pCommonParam->RandomIntraMBRefresh = 0;
    }

    /* Perceptual Mode */
    pCommonParam->PerceptualMode = (pVideoEnc->bPVCMode)? VIDEO_TRUE:VIDEO_FALSE;

    eColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
    pCommonParam->FrameMap = Exynos_OSAL_OMX2VideoFormat(eColorFormat, pInputPort->ePlaneType);

    pCommonParam->bDropControl = (pVideoEnc->bDropControl)? VIDEO_TRUE:VIDEO_FALSE;
    pCommonParam->bDisableDFR = (pVideoEnc->bDisableDFR)? VIDEO_TRUE:VIDEO_FALSE;

    /* Vp8 specific parameters */
    pVp8Param->Vp8Version = OMXVP8LevelToMFCVersion(pVp8Enc->VP8Component[OUTPUT_PORT_INDEX].eLevel);
    pVp8Param->FrameRate = (pInputPort->portDefinition.format.video.xFramerate) >> 16;
    if (pVp8Param->FrameRate <= 0)
        pVp8Param->FrameRate = 30;  /* default : 30fps, zero means that DynamicFramerateChange mode is set */

    pVp8Param->RefNumberForPFrame = pMFCVp8Handle->nRefForPframes;

    /* there is no interface at OMX IL component */
    pVp8Param->Vp8NumberOfPartitions    = pVp8Enc->VP8Component[OUTPUT_PORT_INDEX].nDCTPartitions;
    pVp8Param->Vp8FilterLevel           = 28;
    pVp8Param->Vp8FilterSharpness       = 6;
    pVp8Param->Vp8GoldenFrameSel        = 0;
    pVp8Param->Vp8GFRefreshPeriod       = 10;
    pVp8Param->DisableIntraMd4x4        = 0;

    /* Temporal SVC */
    pVp8Param->TemporalSVC.nTemporalLayerCount = (unsigned int)pVp8Enc->AndroidVp8EncoderType.nTemporalLayerCount;
    pVp8Param->TemporalSVC.nTemporalLayerBitrateRatio[0] = (unsigned int)(pOutputPort->portDefinition.format.video.nBitrate *
                                                                          pVp8Enc->AndroidVp8EncoderType.nTemporalLayerBitrateRatio[0] / 100);
    for (i = 1; i < OMX_VIDEO_ANDROID_MAXVP8TEMPORALLAYERS; i++) {
        pVp8Param->TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)(pOutputPort->portDefinition.format.video.nBitrate *
                                                                              (pVp8Enc->AndroidVp8EncoderType.nTemporalLayerBitrateRatio[i] -
                                                                               pVp8Enc->AndroidVp8EncoderType.nTemporalLayerBitrateRatio[i - 1])
                                                                              / 100);
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] eControlRate: 0x%x", pExynosComponent, __FUNCTION__, pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
    /* rate control related parameters */
    switch (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]) {
    case OMX_Video_ControlRateDisable:
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode DBR");
        pCommonParam->EnableFRMRateControl = 0;    /* 0: Disable, 1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 0;    /* 0: Disable, 1:MB level RC */
        pCommonParam->CBRPeriodRf          = 200;
        break;
    case OMX_Video_ControlRateConstant:
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode CBR");
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable, 1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable, 1:MB level RC */
        pCommonParam->CBRPeriodRf          = 9;
        break;
    case OMX_Video_ControlRateVariable:
    default: /*Android default */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode VBR");
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable, 1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable, 1:MB level RC */
        pCommonParam->CBRPeriodRf          = 200;
        break;
    }

//    Print_VP8Enc_Param(pEncParam);
}

static void Change_VP8Enc_Param(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc         = NULL;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc           = NULL;
    EXYNOS_MFC_VP8ENC_HANDLE      *pMFCVp8Handle     = NULL;
    OMX_PTR                        pDynamicConfigCMD = NULL;
    OMX_PTR                        pConfigData       = NULL;
    OMX_S32                        nCmdIndex         = 0;
    ExynosVideoEncOps             *pEncOps           = NULL;
    int                            nValue            = 0;

    int i;

    pVideoEnc       = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVp8Enc         = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCVp8Handle   = &pVp8Enc->hMFCVp8Handle;
    pEncOps         = pMFCVp8Handle->pEncOps;

    pDynamicConfigCMD = (OMX_PTR)Exynos_OSAL_Dequeue(&pExynosComponent->dynamicConfigQ);
    if (pDynamicConfigCMD == NULL)
        goto EXIT;

    nCmdIndex   = *(OMX_S32 *)pDynamicConfigCMD;
    pConfigData = (OMX_PTR)((OMX_U8 *)pDynamicConfigCMD + sizeof(OMX_S32));

    switch ((int)nCmdIndex) {
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        nValue = VIDEO_FRAME_I;
        pEncOps->Set_FrameType(pMFCVp8Handle->hMFCHandle, nValue);
        pVideoEnc->IntraRefreshVOP = OMX_FALSE;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] VOP Refresh", pExynosComponent, __FUNCTION__);
    }
        break;
    case OMX_IndexConfigVideoIntraPeriod:
    {
        OMX_S32 nPFrames = (*((OMX_U32 *)pConfigData)) - 1;
        nValue = nPFrames + 1;
        pEncOps->Set_IDRPeriod(pMFCVp8Handle->hMFCHandle, nValue);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] IDR period: %d", pExynosComponent, __FUNCTION__, nValue);
    }
        break;
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pConfigData;

        if (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] != OMX_Video_ControlRateDisable) {
            /* bitrate : main */
            nValue = pConfigBitrate->nEncodeBitrate;
            pEncOps->Set_BitRate(pMFCVp8Handle->hMFCHandle, nValue);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bitrate: %d", pExynosComponent, __FUNCTION__, nValue);
            /* bitrate : layer */
            TemporalLayerShareBuffer TemporalSVC;
            Exynos_OSAL_Memset(&TemporalSVC, 0, sizeof(TemporalLayerShareBuffer));
            TemporalSVC.nTemporalLayerCount = (unsigned int)pVp8Enc->AndroidVp8EncoderType.nTemporalLayerCount;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "nTemporalLayerCount     : %d", TemporalSVC.nTemporalLayerCount);
            TemporalSVC.nTemporalLayerBitrateRatio[0] = (unsigned int)(pConfigBitrate->nEncodeBitrate *
                                                                   pVp8Enc->AndroidVp8EncoderType.nTemporalLayerBitrateRatio[0] / 100);
            for (i = 1; i < OMX_VIDEO_ANDROID_MAXVP8TEMPORALLAYERS; i++) {
                TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)(pConfigBitrate->nEncodeBitrate *
                                                                           (pVp8Enc->AndroidVp8EncoderType.nTemporalLayerBitrateRatio[i] -
                                                                            pVp8Enc->AndroidVp8EncoderType.nTemporalLayerBitrateRatio[i - 1])
                                                                           / 100);
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "nTempBitrateRatio[%d]   : %d", i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
            }
            pEncOps->Set_LayerChange(pMFCVp8Handle->hMFCHandle, TemporalSVC);
        }
    }
        break;
    case OMX_IndexConfigVideoFramerate:
    {
        OMX_CONFIG_FRAMERATETYPE *pConfigFramerate = (OMX_CONFIG_FRAMERATETYPE *)pConfigData;
        OMX_U32                   nPortIndex       = pConfigFramerate->nPortIndex;

        if (nPortIndex == INPUT_PORT_INDEX) {
            nValue = (pConfigFramerate->xEncodeFramerate) >> 16;
            pEncOps->Set_FrameRate(pMFCVp8Handle->hMFCHandle, nValue);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] framerate: %d", pExynosComponent, __FUNCTION__, nValue);
        }
    }
        break;
    case OMX_IndexConfigVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE *pQpRange = (OMX_VIDEO_QPRANGETYPE *)pConfigData;
        ExynosVideoQPRange     qpRange;

        Exynos_OSAL_Memset(&qpRange, 0, sizeof(ExynosVideoQPRange));

        qpRange.QpMin_I = pQpRange->qpRangeI.nMinQP;
        qpRange.QpMax_I = pQpRange->qpRangeI.nMaxQP;
        qpRange.QpMin_P = pQpRange->qpRangeP.nMinQP;
        qpRange.QpMax_P = pQpRange->qpRangeP.nMaxQP;

        pEncOps->Set_QpRange(pMFCVp8Handle->hMFCHandle, qpRange);

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] qp range: I(%d, %d), P(%d, %d)",
                                    pExynosComponent, __FUNCTION__,
                                    qpRange.QpMin_I, qpRange.QpMax_I,
                                    qpRange.QpMin_P, qpRange.QpMax_P);
    }
        break;
    case OMX_IndexConfigOperatingRate:
    {
        OMX_PARAM_U32TYPE *pConfigRate = (OMX_PARAM_U32TYPE *)pConfigData;
        OMX_U32            xFramerate  = pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.xFramerate;

        nValue = pConfigRate->nU32 >> 16;

        if (pMFCVp8Handle->videoInstInfo.supportInfo.enc.bOperatingRateSupport == VIDEO_TRUE) {
            if (nValue == ((OMX_U32)INT_MAX >> 16)) {
                nValue = (OMX_U32)INT_MAX;
            } else {
                nValue = nValue * 1000;
            }

            pEncOps->Set_OperatingRate(pMFCVp8Handle->hMFCHandle, nValue);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] operating rate: 0x%x", pExynosComponent, __FUNCTION__, nValue);
        } else {
            if (nValue == (((OMX_U32)INT_MAX) >> 16)) {
                nValue = 1000;
            } else {
                nValue = ((xFramerate >> 16) == 0)? 100:(OMX_U32)((pConfigRate->nU32 / (double)xFramerate) * 100);
            }

            pEncOps->Set_QosRatio(pMFCVp8Handle->hMFCHandle, nValue);

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] qos ratio: %d", pExynosComponent, __FUNCTION__, nValue);
        }

        pVideoEnc->bQosChanged = OMX_FALSE;
    }
        break;
    case OMX_IndexConfigCommonOutputSize:
    {
        OMX_FRAMESIZETYPE   *pFrameSize     = (OMX_FRAMESIZETYPE *)pConfigData;
        OMX_CONFIG_RECTTYPE *pTargetFrame   = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].cropRectangle[IMG_CROP_OUTPUT_PORT]);

        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] resolution(%d x %d) is changed to (%d x %d)", pExynosComponent, __FUNCTION__,
                                            pTargetFrame->nWidth, pTargetFrame->nHeight,
                                            pFrameSize->nWidth, pFrameSize->nHeight);

        /* update target size */
        pTargetFrame->nTop      = 0;
        pTargetFrame->nLeft     = 0;
        pTargetFrame->nWidth    = pFrameSize->nWidth;
        pTargetFrame->nHeight   = pFrameSize->nHeight;

        pVideoEnc->bEncDRC = OMX_TRUE;
    }
        break;
    default:
        break;
    }

    Exynos_OSAL_Free(pDynamicConfigCMD);

    Set_VP8Enc_Param(pExynosComponent);

EXIT:
    return;
}

OMX_ERRORTYPE GetCodecOutputPrivateData(
    OMX_PTR  pCodecBuffer,
    OMX_PTR *pVirtAddr,
    OMX_U32 *pDataSize)
{
    OMX_ERRORTYPE      ret          = OMX_ErrorNone;
    ExynosVideoBuffer *pVideoBuffer = NULL;

    if (pCodecBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoBuffer = (ExynosVideoBuffer *)pCodecBuffer;

    if (pVirtAddr != NULL)
        *pVirtAddr = pVideoBuffer->planes[0].addr;

    if (pDataSize != NULL)
        *pDataSize = pVideoBuffer->planes[0].allocSize;

EXIT:
    return ret;
}

OMX_BOOL CheckFormatHWSupport(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_COLOR_FORMATTYPE         eColorFormat)
{
    OMX_BOOL                         ret            = OMX_FALSE;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc        = NULL;
    EXYNOS_OMX_BASEPORT             *pInputPort     = NULL;
    ExynosVideoColorFormatType       eVideoFormat   = VIDEO_COLORFORMAT_UNKNOWN;
    int i;

    if (pExynosComponent == NULL)
        goto EXIT;

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL)
        goto EXIT;

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc == NULL)
        goto EXIT;
    pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    eVideoFormat = (ExynosVideoColorFormatType)Exynos_OSAL_OMX2VideoFormat(eColorFormat, pInputPort->ePlaneType);

    for (i = 0; i < VIDEO_COLORFORMAT_MAX; i++) {
        if (pVp8Enc->hMFCVp8Handle.videoInstInfo.supportFormat[i] == VIDEO_COLORFORMAT_UNKNOWN)
            break;

        if (pVp8Enc->hMFCVp8Handle.videoInstInfo.supportFormat[i] == eVideoFormat) {
            ret = OMX_TRUE;
            break;
        }
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE VP8CodecOpen(
    EXYNOS_VP8ENC_HANDLE    *pVp8Enc,
    ExynosVideoInstInfo     *pVideoInstInfo)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    ExynosVideoEncOps       *pEncOps    = NULL;
    ExynosVideoEncBufferOps *pInbufOps  = NULL;
    ExynosVideoEncBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if ((pVp8Enc == NULL) ||
        (pVideoInstInfo == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    /* alloc ops structure */
    pEncOps     = (ExynosVideoEncOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoEncOps));
    pInbufOps   = (ExynosVideoEncBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoEncBufferOps));
    pOutbufOps  = (ExynosVideoEncBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoEncBufferOps));

    if ((pEncOps == NULL) ||
        (pInbufOps == NULL) ||
        (pOutbufOps == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to allocate decoder ops buffer", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pVp8Enc->hMFCVp8Handle.pEncOps    = pEncOps;
    pVp8Enc->hMFCVp8Handle.pInbufOps  = pInbufOps;
    pVp8Enc->hMFCVp8Handle.pOutbufOps = pOutbufOps;

    /* function pointer mapping */
    pEncOps->nSize    = sizeof(ExynosVideoEncOps);
    pInbufOps->nSize  = sizeof(ExynosVideoEncBufferOps);
    pOutbufOps->nSize = sizeof(ExynosVideoEncBufferOps);

    if (Exynos_Video_Register_Encoder(pEncOps, pInbufOps, pOutbufOps) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to get decoder ops", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* check mandatory functions for encoder ops */
    if ((pEncOps->Init == NULL) ||
        (pEncOps->Finalize == NULL) ||
        (pEncOps->Set_FrameTag == NULL) ||
        (pEncOps->Get_FrameTag == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Mandatory functions must be supplied", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* check mandatory functions for buffer ops */
    if ((pInbufOps->Setup == NULL) || (pOutbufOps->Setup == NULL) ||
        (pInbufOps->Run == NULL) || (pOutbufOps->Run == NULL) ||
        (pInbufOps->Stop == NULL) || (pOutbufOps->Stop == NULL) ||
        (pInbufOps->Enqueue == NULL) || (pOutbufOps->Enqueue == NULL) ||
        (pInbufOps->Dequeue == NULL) || (pOutbufOps->Dequeue == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Mandatory functions must be supplied", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* alloc context, open, querycap */
#ifdef USE_DMA_BUF
    pVideoInstInfo->nMemoryType = VIDEO_MEMORY_DMABUF;
#else
    pVideoInstInfo->nMemoryType = VIDEO_MEMORY_USERPTR;
#endif
    pVp8Enc->hMFCVp8Handle.hMFCHandle = pVp8Enc->hMFCVp8Handle.pEncOps->Init(pVideoInstInfo);
    if (pVp8Enc->hMFCVp8Handle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to init", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pEncOps != NULL) {
            Exynos_OSAL_Free(pEncOps);
            pVp8Enc->hMFCVp8Handle.pEncOps = NULL;
        }

        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pVp8Enc->hMFCVp8Handle.pInbufOps = NULL;
        }

        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pVp8Enc->hMFCVp8Handle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP8CodecClose(EXYNOS_VP8ENC_HANDLE *pVp8Enc)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoEncOps       *pEncOps    = NULL;
    ExynosVideoEncBufferOps *pInbufOps  = NULL;
    ExynosVideoEncBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pVp8Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pVp8Enc->hMFCVp8Handle.hMFCHandle;
    pEncOps    = pVp8Enc->hMFCVp8Handle.pEncOps;
    pInbufOps  = pVp8Enc->hMFCVp8Handle.pInbufOps;
    pOutbufOps = pVp8Enc->hMFCVp8Handle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pEncOps->Finalize(hMFCHandle);
        hMFCHandle = pVp8Enc->hMFCVp8Handle.hMFCHandle = NULL;
        pVp8Enc->hMFCVp8Handle.bConfiguredMFCSrc = OMX_FALSE;
        pVp8Enc->hMFCVp8Handle.bConfiguredMFCDst = OMX_FALSE;
    }

    /* Unregister function pointers */
    Exynos_Video_Unregister_Encoder(pEncOps, pInbufOps, pOutbufOps);

    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pOutbufOps = pVp8Enc->hMFCVp8Handle.pOutbufOps = NULL;
    }

    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pInbufOps = pVp8Enc->hMFCVp8Handle.pInbufOps = NULL;
    }

    if (pEncOps != NULL) {
        Exynos_OSAL_Free(pEncOps);
        pEncOps = pVp8Enc->hMFCVp8Handle.pEncOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP8CodecStart(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    void                            *hMFCHandle         = NULL;
    ExynosVideoEncBufferOps         *pInbufOps          = NULL;
    ExynosVideoEncBufferOps         *pOutbufOps         = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pVp8Enc->hMFCVp8Handle.hMFCHandle;
    pInbufOps  = pVp8Enc->hMFCVp8Handle.pInbufOps;
    pOutbufOps = pVp8Enc->hMFCVp8Handle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX)
        pInbufOps->Run(hMFCHandle);
    else if (nPortIndex == OUTPUT_PORT_INDEX)
        pOutbufOps->Run(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP8CodecStop(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    void                            *hMFCHandle         = NULL;
    ExynosVideoEncBufferOps         *pInbufOps          = NULL;
    ExynosVideoEncBufferOps         *pOutbufOps         = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pVp8Enc->hMFCVp8Handle.hMFCHandle;
    pInbufOps  = pVp8Enc->hMFCVp8Handle.pInbufOps;
    pOutbufOps = pVp8Enc->hMFCVp8Handle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) && (pInbufOps != NULL))
        pInbufOps->Stop(hMFCHandle);
    else if ((nPortIndex == OUTPUT_PORT_INDEX) && (pOutbufOps != NULL))
        pOutbufOps->Stop(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP8CodecOutputBufferProcessRun(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pVp8Enc->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pVp8Enc->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pVp8Enc->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pVp8Enc->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pVp8Enc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP8CodecEnqueueAllBuffer(
    OMX_COMPONENTTYPE *pOMXComponent,
    OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                            *hMFCHandle         = pVp8Enc->hMFCVp8Handle.hMFCHandle;
    int i;

    ExynosVideoEncBufferOps *pInbufOps  = pVp8Enc->hMFCVp8Handle.pInbufOps;
    ExynosVideoEncBufferOps *pOutbufOps = pVp8Enc->hMFCVp8Handle.pOutbufOps;

    FunctionIn();

    if ((nPortIndex != INPUT_PORT_INDEX) &&
        (nPortIndex != OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pVp8Enc->bSourceStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)  {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] CodecBuffer(input) [%d]: FD(0x%x), VA(0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                i, pVideoEnc->pMFCEncInputBuffer[i]->fd[0], pVideoEnc->pMFCEncInputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnqueue(pExynosComponent, INPUT_PORT_INDEX, pVideoEnc->pMFCEncInputBuffer[i]);
        }

        pInbufOps->Clear_Queue(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pVp8Enc->bDestinationStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);

        for (i = 0; i < MFC_OUTPUT_BUFFER_NUM_MAX; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] CodecBuffer(output) [%d]: FD(0x%x), VA(0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                i, pVideoEnc->pMFCEncOutputBuffer[i]->fd[0], pVideoEnc->pMFCEncOutputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnqueue(pExynosComponent, OUTPUT_PORT_INDEX, pVideoEnc->pMFCEncOutputBuffer[i]);
        }

        pOutbufOps->Clear_Queue(hMFCHandle);
    }

EXIT:
    FunctionOut();

    return ret;
}


void VP8CodecSetHdrInfo(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc          = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_VP8ENC_HANDLE      *pMFCVp8Handle    = &pVp8Enc->hMFCVp8Handle;
    void                          *hMFCHandle       = pMFCVp8Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoEncOps *pEncOps = pVp8Enc->hMFCVp8Handle.pEncOps;

    OMX_COLOR_FORMATTYPE eColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);

    if (pMFCVp8Handle->videoInstInfo.supportInfo.enc.bColorAspectsSupport == VIDEO_TRUE) {
        ExynosVideoColorAspects BSCA;
        Exynos_OSAL_Memset(&BSCA, 0, sizeof(BSCA));

#ifdef USE_ANDROID
        if (pVideoEnc->surfaceFormat == eColorFormat) {
            Exynos_OSAL_GetColorAspectsForBitstream(&(pInputPort->ColorAspects), &(pOutputPort->ColorAspects));

            /* in case of RGBA, re-update suitable values(transfer) */
            if (pVideoEnc->surfaceFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888) {
                /* Eventhough frameworks already sent ColorAspects,
                 * sometimes dataspace has not set to OMX component because of timing.
                 * So get dataspace from frameworks ColorAspects.
                 */
                if (pInputPort->ColorAspects.nDataSpace == 0) {
                    Exynos_OSAL_UpdateDataSpaceFromAspects(&(pInputPort->ColorAspects));
                }
                Exynos_OSAL_GetRGBColorTypeForBitStream(&pOutputPort->ColorAspects, pInputPort->ColorAspects.nDataSpace, eColorFormat);
            }
        } else {
            /* in case of RGBA->YUV, color aspects for bitstream is already updated */
            if (pVideoEnc->surfaceFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888)
                Exynos_OSAL_GetColorAspectsForBitstream(&(pInputPort->ColorAspects), &(pOutputPort->ColorAspects));
        }
#endif

        BSCA.eRangeType     = (ExynosRangeType)pOutputPort->ColorAspects.nRangeType;
        BSCA.ePrimariesType = (ExynosPrimariesType)pOutputPort->ColorAspects.nPrimaryType;
        BSCA.eTransferType  = (ExynosTransferType)pOutputPort->ColorAspects.nTransferType;
        BSCA.eCoeffType     = (ExynosMatrixCoeffType)pOutputPort->ColorAspects.nCoeffType;

        pEncOps->Set_ColorAspects(hMFCHandle, &BSCA);
    }

    return ;
}

OMX_ERRORTYPE Vp8CodecUpdateResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc          = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_VP8ENC_HANDLE      *pMFCVp8Handle    = &pVp8Enc->hMFCVp8Handle;
    void                          *hMFCHandle       = pMFCVp8Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE           eColorFormat     = Exynos_Input_GetActualColorFormat(pExynosComponent);

    ExynosVideoEncOps       *pEncOps    = pVp8Enc->hMFCVp8Handle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps  = pVp8Enc->hMFCVp8Handle.pInbufOps;
    ExynosVideoGeometry      bufferConf;

    FunctionIn();

    pVideoEnc->bEncDRCSync = OMX_TRUE;

    /* stream off */
    ret = VP8CodecStop(pOMXComponent, INPUT_PORT_INDEX);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to VP8CodecStop() about input", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    Exynos_OSAL_SignalWait(pVideoEnc->hEncDRCSyncEvent, DEF_MAX_WAIT_TIME);
    Exynos_OSAL_SignalReset(pVideoEnc->hEncDRCSyncEvent);

    /* get input buffer geometry */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));

    if (pInbufOps->Get_Geometry) {
        if (pInbufOps->Get_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to get geometry about input", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }

    /* clear information */
    pInbufOps->Clear_RegisteredBuffer(hMFCHandle);
    pInbufOps->Cleanup_Buffer(hMFCHandle);

    /* set input buffer geometry */
    if ((pVideoEnc->eRotationType == ROTATE_0) ||
            (pVideoEnc->eRotationType == ROTATE_180)) {
        bufferConf.nFrameWidth  = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
        bufferConf.nFrameHeight = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
        bufferConf.nStride      = ALIGN(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth, 16);
    } else {
        bufferConf.nFrameWidth  = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
        bufferConf.nFrameHeight = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
        bufferConf.nStride      = ALIGN(pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight, 16);
    }

    if (pInbufOps->Set_Geometry) {
        if (pInbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set geometry about input", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }

    /* setup input buffer */
    if (pInbufOps->Setup(hMFCHandle, MAX_INPUTBUFFER_NUM_DYNAMIC) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to setup input buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    /* header with IDR */
    if (pEncOps->Set_HeaderMode(hMFCHandle, VIDEO_FALSE) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set header mode", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    /* stream on */
    VP8CodecStart(pOMXComponent, INPUT_PORT_INDEX);

    /* reset buffer queue */
    VP8CodecEnqueueAllBuffer(pOMXComponent, INPUT_PORT_INDEX);

    /* clear timestamp */
    Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);

    /* reinitialize a value for reconfiguring CSC */
    pVideoEnc->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP8CodecSrcSetup(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_VP8ENC_HANDLE        *pMFCVp8Handle      = &pVp8Enc->hMFCVp8Handle;
    void                            *hMFCHandle         = pMFCVp8Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                          oneFrameSize       = pSrcInputData->dataLen;

    ExynosVideoEncOps       *pEncOps    = pVp8Enc->hMFCVp8Handle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps  = pVp8Enc->hMFCVp8Handle.pInbufOps;
    ExynosVideoEncParam     *pEncParam  = NULL;

    ExynosVideoGeometry      bufferConf;
    OMX_U32                  inputBufferNumber = 0;

    FunctionIn();

    if ((oneFrameSize <= 0) && (pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] first frame has only EOS flag. EOS flag will be returned through FBD",
                                                pExynosComponent, __FUNCTION__);

        BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Malloc(sizeof(BYPASS_BUFFER_INFO));
        if (pBufferInfo == NULL) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        pBufferInfo->nFlags     = pSrcInputData->nFlags;
        pBufferInfo->timeStamp  = pSrcInputData->timeStamp;
        ret = Exynos_OSAL_Queue(&pVp8Enc->bypassBufferInfoQ, (void *)pBufferInfo);

        if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
            Exynos_OSAL_SignalSet(pVp8Enc->hDestinationInStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        } else if (pOutputPort->bufferProcessType & BUFFER_COPY) {
            Exynos_OSAL_SignalSet(pVp8Enc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    Set_VP8Enc_Param(pExynosComponent);

    pEncParam = &pMFCVp8Handle->encParam;
    if (pEncOps->Set_EncParam) {
        if(pEncOps->Set_EncParam(pVp8Enc->hMFCVp8Handle.hMFCHandle, pEncParam) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to set geometry for input buffer");
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    }
    Print_VP8Enc_Param(pEncParam);

#ifdef USE_ANDROID
    VP8CodecSetHdrInfo(pOMXComponent);
#endif

    if (pMFCVp8Handle->videoInstInfo.supportInfo.enc.bPrioritySupport == VIDEO_TRUE)
        pEncOps->Set_Priority(hMFCHandle, pVideoEnc->nPriority);

    if (pEncParam->commonParam.bDisableDFR == VIDEO_TRUE)
        pEncOps->Disable_DynamicFrameRate(pMFCVp8Handle->hMFCHandle, VIDEO_TRUE);

    /* input buffer info: only 3 config values needed */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));
    bufferConf.eColorFormat = pEncParam->commonParam.FrameMap;

    {
        OMX_S32 nTargetWidth, nTargetHeight;

        /* uses specified target size instead of size on output port */
        if (pOutputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_TRUE) {
            /* use specified resolution */
            nTargetWidth    = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
            nTargetHeight   = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
        } else {
            nTargetWidth    = pOutputPort->portDefinition.format.video.nFrameWidth;
            nTargetHeight   = pOutputPort->portDefinition.format.video.nFrameHeight;
        }

        if ((pVideoEnc->eRotationType == ROTATE_0) ||
            (pVideoEnc->eRotationType == ROTATE_180)) {
            bufferConf.nFrameWidth  = nTargetWidth;
            bufferConf.nFrameHeight = nTargetHeight;
            bufferConf.nStride      = ALIGN(nTargetWidth, 16);
        } else {
            bufferConf.nFrameWidth  = nTargetHeight;
            bufferConf.nFrameHeight = nTargetWidth;
            bufferConf.nStride      = ALIGN(nTargetHeight, 16);
        }
    }

    bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pInputPort);
    pInbufOps->Set_Shareable(hMFCHandle);
    inputBufferNumber = MAX_INPUTBUFFER_NUM_DYNAMIC;

    if (pInputPort->bufferProcessType & BUFFER_COPY) {
        /* should be done before prepare input buffer */
        if (pInbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    }

    /* set input buffer geometry */
    if (pInbufOps->Set_Geometry) {
        if (pInbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set geometry about input", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    }

    /* setup input buffer */
    if (pInbufOps->Setup(hMFCHandle, inputBufferNumber) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to setup input buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if ((pInputPort->bufferProcessType & BUFFER_SHARE) &&
        (pInputPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
        /* data buffer */
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
    }

    pVp8Enc->hMFCVp8Handle.bConfiguredMFCSrc = OMX_TRUE;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE VP8CodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_VP8ENC_HANDLE        *pMFCVp8Handle      = &pVp8Enc->hMFCVp8Handle;
    void                            *hMFCHandle         = pMFCVp8Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pOutbufOps = pVp8Enc->hMFCVp8Handle.pOutbufOps;
    ExynosVideoGeometry      bufferConf;

    unsigned int nAllocLen[VIDEO_BUFFER_MAX_PLANES] = {0, 0, 0};
    unsigned int nDataLen[VIDEO_BUFFER_MAX_PLANES]  = {0, 0, 0};
    int i, nOutBufSize = 0, nOutputBufferCnt = 0;

    FunctionIn();

    nOutBufSize = pOutputPort->portDefinition.nBufferSize;
    if ((pOutputPort->bufferProcessType & BUFFER_COPY) ||
        (pOutputPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
        /* OMX buffer is not used directly : CODEC buffer or MetaData */
        nOutBufSize = ALIGN(pOutputPort->portDefinition.format.video.nFrameWidth *
                            pOutputPort->portDefinition.format.video.nFrameHeight * 3 / 2, 512);

#ifdef USE_CUSTOM_COMPONENT_SUPPORT
        nOutBufSize = Exynos_OSAL_GetOutBufferSize(pOutputPort->portDefinition.format.video.nFrameWidth,
                                                pOutputPort->portDefinition.format.video.nFrameHeight,
                                                pInputPort->portDefinition.nBufferSize);
#endif

    }

    /* set geometry for output (dst) */
    if (pOutbufOps->Set_Geometry) {
        /* only 2 config values needed */
        bufferConf.eCompressionFormat   = VIDEO_CODING_VP8;
        bufferConf.nSizeImage           = nOutBufSize;
        bufferConf.nPlaneCnt            = Exynos_GetPlaneFromPort(pOutputPort);

        if (pOutbufOps->Set_Geometry(pVp8Enc->hMFCVp8Handle.hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set geometry about output", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    }

    /* should be done before prepare output buffer */
    if (pOutbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pOutbufOps->Set_Shareable(hMFCHandle);

    if (pOutputPort->bufferProcessType & BUFFER_COPY)
        nOutputBufferCnt = MFC_OUTPUT_BUFFER_NUM_MAX;
    else
        nOutputBufferCnt = pOutputPort->portDefinition.nBufferCountActual;

    if (pOutbufOps->Setup(pVp8Enc->hMFCVp8Handle.hMFCHandle, nOutputBufferCnt) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to setup output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (pOutputPort->bufferProcessType & BUFFER_COPY) {
        nAllocLen[0] = nOutBufSize;
        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX, MFC_OUTPUT_BUFFER_NUM_MAX, nAllocLen);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Allocate_CodecBuffers for output", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        /* Enqueue output buffer */
        for (i = 0; i < MFC_OUTPUT_BUFFER_NUM_MAX; i++) {
            pOutbufOps->ExtensionEnqueue(hMFCHandle,
                                (void **)pVideoEnc->pMFCEncOutputBuffer[i]->pVirAddr,
                                (unsigned long *)pVideoEnc->pMFCEncOutputBuffer[i]->fd,
                                pVideoEnc->pMFCEncOutputBuffer[i]->bufferSize,
                                nDataLen,
                                Exynos_GetPlaneFromPort(pOutputPort),
                                NULL);
        }
    } else if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
        /* Register output buffer */
        /*************/
        /*    TBD    */
        /*************/
    }

    pVp8Enc->hMFCVp8Handle.bConfiguredMFCDst = OMX_TRUE;

    if (VP8CodecStart(pOMXComponent, OUTPUT_PORT_INDEX) != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to run output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc        = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc          = NULL;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nParamIndex %x", pExynosComponent, __FUNCTION__, (int)nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoVp8:
    {
        OMX_VIDEO_PARAM_VP8TYPE *pDstVP8Component = (OMX_VIDEO_PARAM_VP8TYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_VP8TYPE *pSrcVP8Component = NULL;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstVP8Component, sizeof(OMX_VIDEO_PARAM_VP8TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstVP8Component->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcVP8Component = &pVp8Enc->VP8Component[pDstVP8Component->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstVP8Component) + nOffset,
                           ((char *)pSrcVP8Component) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_VP8TYPE) - nOffset);
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_VP8_ENC_ROLE);
    }
        break;
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = GetIndexToProfileLevel(pExynosComponent, pDstProfileLevel);
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pComponentParameterStructure;
        OMX_VIDEO_PARAM_VP8TYPE          *pSrcVP8Component = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcVP8Component = &pVp8Enc->VP8Component[pDstProfileLevel->nPortIndex];
        pDstProfileLevel->eProfile  = pSrcVP8Component->eProfile;
        pDstProfileLevel->eLevel    = pSrcVP8Component->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstErrorCorrectionType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcErrorCorrectionType = &pVp8Enc->errorCorrectionType[OUTPUT_PORT_INDEX];
        pDstErrorCorrectionType->bEnableHEC              = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync           = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing   = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC             = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    case OMX_IndexParamVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE *pQpRange   = (OMX_VIDEO_QPRANGETYPE *)pComponentParameterStructure;
        OMX_U32                nPortIndex = pQpRange->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pQpRange, sizeof(OMX_VIDEO_QPRANGETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pQpRange->qpRangeI.nMinQP = pVp8Enc->qpRangeI.nMinQP;
        pQpRange->qpRangeI.nMaxQP = pVp8Enc->qpRangeI.nMaxQP;
        pQpRange->qpRangeP.nMinQP = pVp8Enc->qpRangeP.nMinQP;
        pQpRange->qpRangeP.nMaxQP = pVp8Enc->qpRangeP.nMaxQP;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamVideoAndroidVp8Encoder:
    {

        OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE *pDstVp8EncoderType = (OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE *pSrcVp8EncoderType = &pVp8Enc->AndroidVp8EncoderType;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstVp8EncoderType, sizeof(OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstVp8EncoderType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcVp8EncoderType->nKeyFrameInterval = pVp8Enc->nPFrames + 1;
        pSrcVp8EncoderType->nMinQuantizer = pVp8Enc->qpRangeI.nMinQP;
        pSrcVp8EncoderType->nMaxQuantizer = pVp8Enc->qpRangeI.nMaxQP;

        Exynos_OSAL_Memcpy(((char *)pDstVp8EncoderType) + nOffset,
                           ((char *)pSrcVp8EncoderType) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE) - nOffset);
    }
        break;
#endif
    case OMX_IndexParamVideoEnablePVC:
    {
        OMX_PARAM_U32TYPE *pEnablePVC  = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pEnablePVC, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pEnablePVC->nU32 = pVideoEnc->bPVCMode;
    }
        break;
    case OMX_IndexParamNumberRefPframes:
    {
        OMX_PARAM_U32TYPE *pNumberRefPframes  = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pNumberRefPframes, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pNumberRefPframes->nU32 = pVp8Enc->hMFCVp8Handle.nRefForPframes;
    }
        break;
    default:
        ret = Exynos_OMX_VideoEncodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc        = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc          = NULL;

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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamVideoVp8:
    {
        OMX_VIDEO_PARAM_VP8TYPE *pDstVP8Component = NULL;
        OMX_VIDEO_PARAM_VP8TYPE *pSrcVP8Component = (OMX_VIDEO_PARAM_VP8TYPE *)pComponentParameterStructure;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcVP8Component, sizeof(OMX_VIDEO_PARAM_VP8TYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pSrcVP8Component->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstVP8Component = &pVp8Enc->VP8Component[pSrcVP8Component->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstVP8Component) + nOffset,
                           ((char *)pSrcVP8Component) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_VP8TYPE) - nOffset);
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_VP8_ENC_ROLE)) {
            pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
        } else {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_VP8TYPE          *pDstVP8Component = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstVP8Component = &pVp8Enc->VP8Component[pSrcProfileLevel->nPortIndex];

        if (OMX_FALSE == CheckProfileLevelSupport(pExynosComponent, pSrcProfileLevel)) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pDstVP8Component->eProfile  = pSrcProfileLevel->eProfile;
        pDstVP8Component->eLevel    = pSrcProfileLevel->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcErrorCorrectionType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstErrorCorrectionType = &pVp8Enc->errorCorrectionType[OUTPUT_PORT_INDEX];
        pDstErrorCorrectionType->bEnableHEC                 = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync              = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing      = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning    = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC                = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat     = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex      = pPortFormat->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef        = NULL;

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
            ((pPortDef->format.video.xFramerate >> 16) > 0) &&
            ((pPortFormat->xFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] xFramerate is changed to invalid value(%d)",
                                              pExynosComponent, __FUNCTION__, pPortFormat->xFramerate >> 16);
        }

        pPortDef->format.video.eColorFormat       = pPortFormat->eColorFormat;
        pPortDef->format.video.eCompressionFormat = pPortFormat->eCompressionFormat;
        pPortDef->format.video.xFramerate         = pPortFormat->xFramerate;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef      = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                       nPortIndex    = pPortDef->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort   = NULL;
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
            ((pExynosPort->portDefinition.format.video.xFramerate >> 16) > 0) &&
            ((pPortDef->format.video.xFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] xFramerate is invalid(%d)",
                                              pExynosComponent, __FUNCTION__, pPortDef->format.video.xFramerate >> 16);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        Exynos_OSAL_Memcpy(((char *)&pExynosPort->portDefinition) + nOffset,
                           ((char *)pPortDef) + nOffset,
                           pPortDef->nSize - nOffset);
        if (nPortIndex == INPUT_PORT_INDEX) {
            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            Exynos_UpdateFrameSize(pOMXComponent);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pOutputPort->portDefinition.nBufferSize: %d",
                            pExynosPort->portDefinition.nBufferSize);
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE *pQpRange   = (OMX_VIDEO_QPRANGETYPE *)pComponentParameterStructure;
        OMX_U32                nPortIndex = pQpRange->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pQpRange, sizeof(OMX_VIDEO_QPRANGETYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pQpRange->qpRangeI.nMinQP > pQpRange->qpRangeI.nMaxQP) ||
            (pQpRange->qpRangeP.nMinQP > pQpRange->qpRangeP.nMaxQP)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: QP value is invalid(I[min:%d, max:%d], P[min:%d, max:%d])", __FUNCTION__,
                            pQpRange->qpRangeI.nMinQP, pQpRange->qpRangeI.nMaxQP,
                            pQpRange->qpRangeP.nMinQP, pQpRange->qpRangeP.nMaxQP);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pVp8Enc->qpRangeI.nMinQP = pQpRange->qpRangeI.nMinQP;
        pVp8Enc->qpRangeI.nMaxQP = pQpRange->qpRangeI.nMaxQP;
        pVp8Enc->qpRangeP.nMinQP = pQpRange->qpRangeP.nMinQP;
        pVp8Enc->qpRangeP.nMaxQP = pQpRange->qpRangeP.nMaxQP;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamVideoAndroidVp8Encoder:
    {
        OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE *pSrcVp8EncoderType = (OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE *pDstVp8EncoderType = &pVp8Enc->AndroidVp8EncoderType;


        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcVp8EncoderType, sizeof(OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pSrcVp8EncoderType->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pVp8Enc->hMFCVp8Handle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_FALSE) &&
            (pSrcVp8EncoderType->eTemporalPattern != OMX_VIDEO_VPXTemporalLayerPatternNone)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Temporal SVC is not supported", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        if ((pSrcVp8EncoderType->nMinQuantizer > pSrcVp8EncoderType->nMaxQuantizer) ||
            (pSrcVp8EncoderType->nMaxQuantizer >= VP8_QP_INDEX_RANGE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] QP value is invalid(min:%d, max:%d)", pExynosComponent, __FUNCTION__,
                            pSrcVp8EncoderType->nMinQuantizer, pSrcVp8EncoderType->nMaxQuantizer);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pVp8Enc->nPFrames = pSrcVp8EncoderType->nKeyFrameInterval - 1;

        pVp8Enc->qpRangeI.nMinQP = pSrcVp8EncoderType->nMinQuantizer;
        pVp8Enc->qpRangeI.nMaxQP = pSrcVp8EncoderType->nMaxQuantizer;

        Exynos_OSAL_Memcpy(((char *)pDstVp8EncoderType) + nOffset,
                           ((char *)pSrcVp8EncoderType) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE) - nOffset);
    }
        break;
#endif
    case OMX_IndexParamVideoEnablePVC:
    {
        OMX_PARAM_U32TYPE *pEnablePVC  = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pEnablePVC, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVideoEnc->bPVCMode = (pEnablePVC->nU32)? OMX_TRUE:OMX_FALSE;
    }
        break;
    case OMX_IndexParamNumberRefPframes:
    {
        OMX_PARAM_U32TYPE *pNumberRefPframes  = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pNumberRefPframes, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pVp8Enc->hMFCVp8Handle.nRefForPframes = pNumberRefPframes->nU32;
    }
        break;
    default:
        ret = Exynos_OMX_VideoEncodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_GetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc        = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc          = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentConfigStructure == NULL)) {
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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE *pQpRange   = (OMX_VIDEO_QPRANGETYPE *)pComponentConfigStructure;
        OMX_U32                nPortIndex = pQpRange->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pQpRange, sizeof(OMX_VIDEO_QPRANGETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pQpRange->qpRangeI.nMinQP = pVp8Enc->qpRangeI.nMinQP;
        pQpRange->qpRangeI.nMaxQP = pVp8Enc->qpRangeI.nMaxQP;
        pQpRange->qpRangeP.nMinQP = pVp8Enc->qpRangeP.nMinQP;
        pQpRange->qpRangeP.nMaxQP = pVp8Enc->qpRangeP.nMaxQP;
    }
        break;
    default:
        ret = Exynos_OMX_VideoEncodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_SetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc          = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentConfigStructure == NULL)) {
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
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (pVideoEnc->hCodecHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
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

        if ((nPortIndex == INPUT_PORT_INDEX) &&
            ((pExynosPort->portDefinition.format.video.xFramerate >> 16) > 0) &&
            ((pConfigFramerate->xEncodeFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "%s[%x] xFramerate is changed to invalid value(%d)",
                                              __FUNCTION__, OMX_IndexConfigVideoFramerate, pConfigFramerate->xEncodeFramerate >> 16);
        }
        pExynosPort->portDefinition.format.video.xFramerate = pConfigFramerate->xEncodeFramerate;
    }
        break;
    case OMX_IndexConfigVideoIntraPeriod:
    {
        OMX_U32 nPFrames = (*((OMX_U32 *)pComponentConfigStructure)) - 1;

        pVp8Enc->nPFrames = nPFrames;

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexConfigVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE *pQpRange   = (OMX_VIDEO_QPRANGETYPE *)pComponentConfigStructure;
        OMX_U32                nPortIndex = pQpRange->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pQpRange, sizeof(OMX_VIDEO_QPRANGETYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pQpRange->qpRangeI.nMinQP > pQpRange->qpRangeI.nMaxQP) ||
            (pQpRange->qpRangeP.nMinQP > pQpRange->qpRangeP.nMaxQP)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] QP value is invalid(I[min:%d, max:%d], P[min:%d, max:%d])",
                            pExynosComponent, __FUNCTION__,
                            pQpRange->qpRangeI.nMinQP, pQpRange->qpRangeI.nMaxQP,
                            pQpRange->qpRangeP.nMinQP, pQpRange->qpRangeP.nMaxQP);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if (pVp8Enc->hMFCVp8Handle.videoInstInfo.supportInfo.enc.bQpRangePBSupport == VIDEO_FALSE)
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] only I-frame's QP range is applied", pExynosComponent, __FUNCTION__);

        pVp8Enc->qpRangeI.nMinQP = pQpRange->qpRangeI.nMinQP;
        pVp8Enc->qpRangeI.nMaxQP = pQpRange->qpRangeI.nMaxQP;
        pVp8Enc->qpRangeP.nMinQP = pQpRange->qpRangeP.nMinQP;
        pVp8Enc->qpRangeP.nMaxQP = pQpRange->qpRangeP.nMaxQP;
    }
        break;
    default:
        ret = Exynos_OMX_VideoEncodeSetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        OMX_PTR pDynamicConfigCMD = NULL;
        pDynamicConfigCMD = Exynos_OMX_MakeDynamicConfig(nIndex, pComponentConfigStructure);
        Exynos_OSAL_Queue(&pExynosComponent->dynamicConfigQ, (void *)pDynamicConfigCMD);
    }

    if (ret == (OMX_ERRORTYPE)OMX_ErrorNoneExpiration)
        ret = OMX_ErrorNone;

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

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

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ENABLE_PVC) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamVideoEnablePVC;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_REF_NUM_PFRAMES) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamNumberRefPframes;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    ret = Exynos_OMX_VideoEncodeGetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_ComponentRoleEnum(
    OMX_HANDLETYPE   hComponent,
    OMX_U8          *cRole,
    OMX_U32          nIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if ((hComponent == NULL) ||
        (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nIndex == (MAX_COMPONENT_ROLE_NUM-1)) {
        Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_VP8_ENC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_VP8Enc_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc            = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort           = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort          = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc              = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    OMX_COLOR_FORMATTYPE           eColorFormat         = pInputPort->portDefinition.format.video.eColorFormat;

    ExynosVideoInstInfo *pVideoInstInfo = &(pVp8Enc->hMFCVp8Handle.videoInstInfo);

    CSC_METHOD csc_method = CSC_METHOD_SW;

    FunctionIn();

    pVp8Enc->hMFCVp8Handle.bConfiguredMFCSrc = OMX_FALSE;
    pVp8Enc->hMFCVp8Handle.bConfiguredMFCDst = OMX_FALSE;
    pVideoEnc->bFirstInput         = OMX_TRUE;
    pVideoEnc->bFirstOutput        = OMX_FALSE;
    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

    if (pInputPort->eMetaDataType != METADATA_TYPE_DISABLED) {
        /* metadata buffer */
        pInputPort->bufferProcessType = BUFFER_SHARE;

#ifdef USE_ANDROID
        if ((pInputPort->eMetaDataType == METADATA_TYPE_DATA) &&
            (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque)) {
            pInputPort->eMetaDataType     = METADATA_TYPE_GRAPHIC;  /* AndoridOpaque means GrallocSource */
            pInputPort->bufferProcessType = BUFFER_COPY;  /* will determine a process type after getting a hal format at handle */
        }
#else
        if (pInputPort->eMetaDataType == METADATA_TYPE_UBM_BUFFER) {
            pInputPort->bufferProcessType = BUFFER_COPY;
        }
#endif
    } else {
        /* data buffer */
        pInputPort->bufferProcessType = BUFFER_COPY;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] CodecOpen W:%d H:%d Bitrate:%d FPS:%d", pExynosComponent, __FUNCTION__,
                                                                                            pInputPort->portDefinition.format.video.nFrameWidth,
                                                                                            pInputPort->portDefinition.format.video.nFrameHeight,
                                                                                            pInputPort->portDefinition.format.video.nBitrate,
                                                                                            pInputPort->portDefinition.format.video.xFramerate);
    pVideoInstInfo->nSize        = sizeof(ExynosVideoInstInfo);
    pVideoInstInfo->nWidth       = pInputPort->portDefinition.format.video.nFrameWidth;
    pVideoInstInfo->nHeight      = pInputPort->portDefinition.format.video.nFrameHeight;
    pVideoInstInfo->nBitrate     = pInputPort->portDefinition.format.video.nBitrate;
    pVideoInstInfo->xFramerate   = pInputPort->portDefinition.format.video.xFramerate;

    /* VP8 Codec Open */
    ret = VP8CodecOpen(pVp8Enc, pVideoInstInfo);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    Exynos_SetPlaneToPort(pInputPort, MFC_DEFAULT_INPUT_BUFFER_PLANE);
    Exynos_SetPlaneToPort(pOutputPort, MFC_DEFAULT_OUTPUT_BUFFER_PLANE);

    Exynos_OSAL_SemaphoreCreate(&pInputPort->codecSemID);
    Exynos_OSAL_QueueCreate(&pInputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);

    if (pOutputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_OSAL_SemaphoreCreate(&pOutputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pOutputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);
    } else if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    pVp8Enc->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pVp8Enc->hSourceStartEvent);
    pVp8Enc->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pVp8Enc->hDestinationInStartEvent);
    Exynos_OSAL_SignalCreate(&pVp8Enc->hDestinationOutStartEvent);

    Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);
    INIT_ARRAY_TO_VAL(pExynosComponent->timeStamp, DEFAULT_TIMESTAMP_VAL, MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pVp8Enc->hMFCVp8Handle.indexTimestamp       = 0;
    pVp8Enc->hMFCVp8Handle.outputIndexTimestamp = 0;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    Exynos_OSAL_QueueCreate(&pVp8Enc->bypassBufferInfoQ, QUEUE_ELEMENTS);

#ifdef USE_CSC_HW
    csc_method = CSC_METHOD_HW;
#endif

    pVideoEnc->csc_handle = csc_init(csc_method);
    if (pVideoEnc->csc_handle == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pVideoEnc->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Terminate */
OMX_ERRORTYPE Exynos_VP8Enc_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE               ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent  = NULL;

    FunctionIn();

    if (pOMXComponent == NULL)
        goto EXIT;

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent != NULL) {
        EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc   = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
        EXYNOS_OMX_BASEPORT     *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
        EXYNOS_OMX_BASEPORT     *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

        if (pVideoEnc != NULL) {
            EXYNOS_VP8ENC_HANDLE *pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;

            if (pVideoEnc->csc_handle != NULL) {
                csc_deinit(pVideoEnc->csc_handle);
                pVideoEnc->csc_handle = NULL;
            }

            if (pVp8Enc != NULL) {
                Exynos_OSAL_QueueTerminate(&pVp8Enc->bypassBufferInfoQ);

                Exynos_OSAL_SignalTerminate(pVp8Enc->hDestinationInStartEvent);
                pVp8Enc->hDestinationInStartEvent = NULL;
                Exynos_OSAL_SignalTerminate(pVp8Enc->hDestinationOutStartEvent);
                pVp8Enc->hDestinationOutStartEvent = NULL;
                pVp8Enc->bDestinationStart = OMX_FALSE;

                Exynos_OSAL_SignalTerminate(pVp8Enc->hSourceStartEvent);
                pVp8Enc->hSourceStartEvent = NULL;
                pVp8Enc->bSourceStart = OMX_FALSE;
            }

            if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
                Exynos_Free_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX);
                Exynos_OSAL_QueueTerminate(&pExynosOutputPort->codecBufferQ);
                Exynos_OSAL_SemaphoreTerminate(pExynosOutputPort->codecSemID);
                pExynosOutputPort->codecSemID = NULL;
            } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
                /*************/
                /*    TBD    */
                /*************/
                /* Does not require any actions. */
            }

            if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
                Exynos_Free_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX);
            } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
                /*************/
                /*    TBD    */
                /*************/
                /* Does not require any actions. */
            }

            Exynos_OSAL_QueueTerminate(&pExynosInputPort->codecBufferQ);
            Exynos_OSAL_SemaphoreTerminate(pExynosInputPort->codecSemID);
            pExynosInputPort->codecSemID = NULL;

            if (pVp8Enc != NULL) {
                VP8CodecClose(pVp8Enc);
            }
        }
    }

    Exynos_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_SrcIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc          = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle       = pVp8Enc->hMFCVp8Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE           inputColorFormat = OMX_COLOR_FormatUnused;

    ExynosVideoEncOps       *pEncOps     = pVp8Enc->hMFCVp8Handle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps   = pVp8Enc->hMFCVp8Handle.pInbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    OMX_BUFFERHEADERTYPE tempBufferHeader;
    void *pPrivate = NULL;

    unsigned int nAllocLen[MAX_BUFFER_PLANE]  = {0, 0, 0};
    unsigned int nDataLen[MAX_BUFFER_PLANE]   = {0, 0, 0};
    int i, nPlaneCnt, nConfigCnt;

    FunctionIn();

#ifdef USE_ANDROID
    {
        int fps = (pInputPort->portDefinition.format.video.xFramerate) >> 16;

        if (pVideoEnc->pPerfHandle != NULL)
            Exynos_OSAL_Performance(pVideoEnc->pPerfHandle, pVp8Enc->hMFCVp8Handle.indexTimestamp, fps);
    }
#endif

    if (pVp8Enc->hMFCVp8Handle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = VP8CodecSrcSetup(pOMXComponent, pSrcInputData);
        if ((ret != OMX_ErrorNone) ||
            ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to VP8CodecSrcSetup(0x%x)",
                                                pExynosComponent, __FUNCTION__);
            goto EXIT;
        }
    }

    if (pVp8Enc->hMFCVp8Handle.bConfiguredMFCDst == OMX_FALSE) {
        ret = VP8CodecDstSetup(pOMXComponent);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to VP8CodecDstSetup(0x%x)",
                                                pExynosComponent, __FUNCTION__);
            goto EXIT;
        }
    }

    nConfigCnt = Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ);
    if (nConfigCnt > 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] has config message(%d)", pExynosComponent, __FUNCTION__, nConfigCnt);
        while (Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ) > 0) {
            Change_VP8Enc_Param(pExynosComponent);
        }
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] all config messages were handled", pExynosComponent, __FUNCTION__);
    }

    /* DRC on Executing state through OMX_IndexConfigCommonOutputSize */
    if (pVideoEnc->bEncDRC == OMX_TRUE) {
        ret = Vp8CodecUpdateResolution(pOMXComponent);
        pVideoEnc->bEncDRC = OMX_FALSE;
        goto EXIT;
    }

    if ((pSrcInputData->dataLen > 0) ||
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        pExynosComponent->timeStamp[pVp8Enc->hMFCVp8Handle.indexTimestamp]              = pSrcInputData->timeStamp;
        pExynosComponent->bTimestampSlotUsed[pVp8Enc->hMFCVp8Handle.indexTimestamp]     = OMX_TRUE;
        pExynosComponent->nFlags[pVp8Enc->hMFCVp8Handle.indexTimestamp]                 = pSrcInputData->nFlags;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input / buffer header(%p), nFlags: 0x%x, timestamp %lld us (%.2f secs), Tag: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        pSrcInputData->bufferHeader, pSrcInputData->nFlags,
                                                        pSrcInputData->timeStamp, (double)(pSrcInputData->timeStamp / 1E6),
                                                        pVp8Enc->hMFCVp8Handle.indexTimestamp);
        pEncOps->Set_FrameTag(hMFCHandle, pVp8Enc->hMFCVp8Handle.indexTimestamp);
        pVp8Enc->hMFCVp8Handle.indexTimestamp++;
        pVp8Enc->hMFCVp8Handle.indexTimestamp %= MAX_TIMESTAMP;

#ifdef PERFORMANCE_DEBUG
        Exynos_OSAL_V4L2CountIncrease(pInputPort->hBufferCount, pSrcInputData->bufferHeader, INPUT_PORT_INDEX);
#endif

        inputColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);

        {
            OMX_S32 nTargetWidth, nTargetHeight;

            if (pOutputPort->bUseImgCrop[IMG_CROP_OUTPUT_PORT] == OMX_TRUE) {
                nTargetWidth    = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nWidth;
                nTargetHeight   = pOutputPort->cropRectangle[IMG_CROP_OUTPUT_PORT].nHeight;
            } else {
                nTargetWidth    = pOutputPort->portDefinition.format.video.nFrameWidth;
                nTargetHeight   = pOutputPort->portDefinition.format.video.nFrameHeight;
            }
            /* rotation */
            if ((pVideoEnc->eRotationType == ROTATE_0) ||
                (pVideoEnc->eRotationType == ROTATE_180)) {
                Exynos_OSAL_GetPlaneSize(inputColorFormat,
                                         pInputPort->ePlaneType,
                                         nTargetWidth, nTargetHeight,
                                         nDataLen, nAllocLen);
            } else {
                Exynos_OSAL_GetPlaneSize(inputColorFormat,
                                         pInputPort->ePlaneType,
                                         nTargetHeight, nTargetWidth,
                                         nDataLen, nAllocLen);
            }
        }

        if (pInputPort->bufferProcessType == BUFFER_COPY) {
            tempBufferHeader.nFlags     = pSrcInputData->nFlags;
            tempBufferHeader.nTimeStamp = pSrcInputData->timeStamp;
            pPrivate = (void *)&tempBufferHeader;
        } else {
            pPrivate = (void *)pSrcInputData->bufferHeader;
        }

        nPlaneCnt = Exynos_GetPlaneFromPort(pInputPort);
        if (pSrcInputData->dataLen == 0) {
            for (i = 0; i < nPlaneCnt; i++)
                nDataLen[i] = 0;
        } else {
            /* when having valid input */
            if (pSrcInputData->buffer.addr[2] != NULL) {
                ExynosVideoMeta *pVideoMeta = (ExynosVideoMeta *)pSrcInputData->buffer.addr[2];

                if ((pVideoMeta->eType & VIDEO_INFO_TYPE_CHECK_PIXEL_FORMAT) &&
                    ((pVideoEnc->surfaceFormat >= OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC) &&
                     (pVideoEnc->surfaceFormat <= OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L80))) {
                    int nOMXFormat   = OMX_COLOR_FormatUnused;
                    int nVideoFormat = VIDEO_COLORFORMAT_UNKNOWN;

                    nOMXFormat = Exynos_OSAL_HAL2OMXColorFormat(pVideoMeta->nPixelFormat);
                    if (nOMXFormat != OMX_COLOR_FormatUnused) {
                        nVideoFormat = Exynos_OSAL_OMX2VideoFormat(nOMXFormat, pInputPort->ePlaneType);

                        if (nVideoFormat != VIDEO_COLORFORMAT_UNKNOWN) {
                            if (pEncOps->Set_ActualFormat(hMFCHandle, nVideoFormat) != VIDEO_ERROR_NONE) {
                                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] set change format is failed", pExynosComponent, __FUNCTION__);
                                ret = OMX_ErrorBadParameter;
                                goto EXIT;
                            }
                            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] video format is changed: %d", pExynosComponent, __FUNCTION__, nVideoFormat);
                        }
                    }
                }
            }
        }

        if ((pVp8Enc->hMFCVp8Handle.videoInstInfo.supportInfo.enc.bDropControlSupport == VIDEO_TRUE) &&
            (pVideoEnc->bDropControl == OMX_TRUE)) {
            pEncOps->Set_DropControl(hMFCHandle, VIDEO_TRUE);
        }

        codecReturn = pInbufOps->ExtensionEnqueue(hMFCHandle,
                                    (void **)pSrcInputData->buffer.addr,
                                    (unsigned long *)pSrcInputData->buffer.fd,
                                    nAllocLen,
                                    nDataLen,
                                    nPlaneCnt,
                                    pPrivate);
        if (codecReturn != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to ExtensionEnqueue about input (0x%x)",
                                                pExynosComponent, __FUNCTION__, codecReturn);
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecEncode;
            goto EXIT;
        }

        VP8CodecStart(pOMXComponent, INPUT_PORT_INDEX);

        if (pVp8Enc->bSourceStart == OMX_FALSE) {
            pVp8Enc->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pVp8Enc->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        if ((pVp8Enc->bDestinationStart == OMX_FALSE) &&
            (pVp8Enc->hMFCVp8Handle.bConfiguredMFCDst == OMX_TRUE)) {
            pVp8Enc->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pVp8Enc->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pVp8Enc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_SrcOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc          = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle       = pVp8Enc->hMFCVp8Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pInbufOps      = pVp8Enc->hMFCVp8Handle.pInbufOps;
    ExynosVideoBuffer       *pVideoBuffer   = NULL;
    ExynosVideoBuffer        videoBuffer;

    FunctionIn();

    if (pInbufOps->ExtensionDequeue(hMFCHandle, &videoBuffer) == VIDEO_ERROR_NONE) {
        pVideoBuffer = &videoBuffer;
    } else {
        if (pVideoEnc->bEncDRCSync == OMX_TRUE) {
            Exynos_OSAL_SignalSet(pVideoEnc->hEncDRCSyncEvent);
            pVideoEnc->bEncDRCSync = OMX_FALSE;
        }
        pVideoBuffer = NULL;
    }

    pSrcOutputData->dataLen       = 0;
    pSrcOutputData->usedDataLen   = 0;
    pSrcOutputData->remainDataLen = 0;
    pSrcOutputData->nFlags        = 0;
    pSrcOutputData->timeStamp     = 0;
    pSrcOutputData->allocSize     = 0;
    pSrcOutputData->bufferHeader  = NULL;

    if (pVideoBuffer == NULL) {
        pSrcOutputData->buffer.addr[0] = NULL;
        pSrcOutputData->pPrivate = NULL;
    } else {
        int plane = 0, nPlaneCnt;
        nPlaneCnt = Exynos_GetPlaneFromPort(pInputPort);
        for (plane = 0; plane < nPlaneCnt; plane++) {
            pSrcOutputData->buffer.addr[plane]  = pVideoBuffer->planes[plane].addr;
            pSrcOutputData->buffer.fd[plane]    = pVideoBuffer->planes[plane].fd;

            pSrcOutputData->allocSize += pVideoBuffer->planes[plane].allocSize;
        }

        if (pInputPort->bufferProcessType & BUFFER_COPY) {
            int i;
            for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
                if (pSrcOutputData->buffer.addr[0] ==
                        pVideoEnc->pMFCEncInputBuffer[i]->pVirAddr[0]) {
                    pVideoEnc->pMFCEncInputBuffer[i]->dataSize = 0;
                    pSrcOutputData->pPrivate = pVideoEnc->pMFCEncInputBuffer[i];
                    break;
                }
            }

            if (i >= MFC_INPUT_BUFFER_NUM_MAX) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Can not find a codec buffer", pExynosComponent, __FUNCTION__);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecEncode;
                goto EXIT;
            }
        }

        /* For Share Buffer */
        if (pInputPort->bufferProcessType == BUFFER_SHARE) {
            pSrcOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE*)pVideoBuffer->pPrivate;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input / buffer header(%p)",
                                                pExynosComponent, __FUNCTION__, pSrcOutputData->bufferHeader);
        }

#ifdef PERFORMANCE_DEBUG
        Exynos_OSAL_V4L2CountDecrease(pInputPort->hBufferCount, pSrcOutputData->bufferHeader, INPUT_PORT_INDEX);
#endif
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_DstIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc            = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc              = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle           = pVp8Enc->hMFCVp8Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort          = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pInputPort           = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pOutbufOps  = pVp8Enc->hMFCVp8Handle.pOutbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    unsigned int nAllocLen[VIDEO_BUFFER_MAX_PLANES] = {0, 0, 0};
    unsigned int nDataLen[VIDEO_BUFFER_MAX_PLANES]  = {0, 0, 0};

    FunctionIn();

    if (pDstInputData->buffer.addr[0] == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to find output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

#ifdef PERFORMANCE_DEBUG
    Exynos_OSAL_V4L2CountIncrease(pOutputPort->hBufferCount, pDstInputData->bufferHeader, OUTPUT_PORT_INDEX);
#endif

    nAllocLen[0] = pOutputPort->portDefinition.nBufferSize;
    if ((pOutputPort->bufferProcessType & BUFFER_COPY) ||
        (pOutputPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
        /* OMX buffer is not used directly : CODEC buffer or MetaData */
        nAllocLen[0] = ALIGN(pOutputPort->portDefinition.format.video.nFrameWidth * pOutputPort->portDefinition.format.video.nFrameHeight * 3 / 2, 512);
#ifdef USE_CUSTOM_COMPONENT_SUPPORT
        nAllocLen[0] = Exynos_OSAL_GetOutBufferSize(pOutputPort->portDefinition.format.video.nFrameWidth,
                                                pOutputPort->portDefinition.format.video.nFrameHeight,
                                                pInputPort->portDefinition.nBufferSize);
#endif

    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output / buffer header(%p)",
                                        pExynosComponent, __FUNCTION__, pDstInputData->bufferHeader);

    codecReturn = pOutbufOps->ExtensionEnqueue(hMFCHandle,
                                (void **)pDstInputData->buffer.addr,
                                (unsigned long *)&pDstInputData->buffer.fd,
                                nAllocLen,
                                nDataLen,
                                Exynos_GetPlaneFromPort(pOutputPort),
                                pDstInputData->bufferHeader);
    if (codecReturn != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to ExtensionEnqueue about output (0x%x)",
                                            pExynosComponent, __FUNCTION__, codecReturn);
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecEncode;
        goto EXIT;
    }

    VP8CodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_DstOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc            = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc              = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort          = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    void                          *hMFCHandle           = pVp8Enc->hMFCVp8Handle.hMFCHandle;

    ExynosVideoEncOps           *pEncOps        = pVp8Enc->hMFCVp8Handle.pEncOps;
    ExynosVideoEncBufferOps     *pOutbufOps     = pVp8Enc->hMFCVp8Handle.pOutbufOps;
    ExynosVideoBuffer           *pVideoBuffer   = NULL;
    ExynosVideoBuffer            videoBuffer;
    ExynosVideoErrorType         codecReturn    = VIDEO_ERROR_NONE;

    OMX_S32 indexTimestamp = 0;

    FunctionIn();

    if (pVp8Enc->bDestinationStart == OMX_FALSE) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    codecReturn = pOutbufOps->ExtensionDequeue(hMFCHandle, &videoBuffer);
    if (codecReturn == VIDEO_ERROR_NONE) {
        pVideoBuffer = &videoBuffer;
    } else if (codecReturn == VIDEO_ERROR_DQBUF_EIO) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] HW is not available(EIO)", pExynosComponent, __FUNCTION__);
        pVideoBuffer = NULL;
        ret = OMX_ErrorHardware;
        goto EXIT;
    } else {
        pVideoBuffer = NULL;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    pDstOutputData->buffer.addr[0]  = pVideoBuffer->planes[0].addr;
    pDstOutputData->buffer.fd[0]    = pVideoBuffer->planes[0].fd;

    pDstOutputData->allocSize       = pVideoBuffer->planes[0].allocSize;
    pDstOutputData->dataLen         = pVideoBuffer->planes[0].dataSize;
    pDstOutputData->remainDataLen   = pVideoBuffer->planes[0].dataSize;
    pDstOutputData->usedDataLen     = 0;

    pDstOutputData->pPrivate        = pVideoBuffer;

    if (pOutputPort->bufferProcessType & BUFFER_COPY) {
        int i = 0;
        pDstOutputData->pPrivate = NULL;
        for (i = 0; i < MFC_OUTPUT_BUFFER_NUM_MAX; i++) {
            if (pDstOutputData->buffer.addr[0] ==
                pVideoEnc->pMFCEncOutputBuffer[i]->pVirAddr[0]) {
                pDstOutputData->pPrivate = pVideoEnc->pMFCEncOutputBuffer[i];
                break;
            }
        }

        if (pDstOutputData->pPrivate == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Can not find a codec buffer", pExynosComponent, __FUNCTION__);
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecEncode;
            goto EXIT;
        }
    }

    /* For Share Buffer */
    pDstOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE *)pVideoBuffer->pPrivate;
    indexTimestamp = pEncOps->Get_FrameTag(pVp8Enc->hMFCVp8Handle.hMFCHandle);

    if (pVideoEnc->bFirstOutput == OMX_FALSE) {
        /* Because of Codec2 requirement, Driver doesn't send only CSD buffer in VP8, VP9 encoders. */
        if (indexTimestamp == INDEX_HEADER_DATA) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] codec specific data is generated", pExynosComponent, __FUNCTION__);

            pDstOutputData->nFlags     |= OMX_BUFFERFLAG_CODECCONFIG;
            pDstOutputData->timeStamp   = 0;
        } else {
            pDstOutputData->timeStamp   = pExynosComponent->timeStamp[pVp8Enc->hMFCVp8Handle.outputIndexTimestamp];
        }

        pDstOutputData->nFlags     |= OMX_BUFFERFLAG_ENDOFFRAME;
        pVideoEnc->bFirstOutput     = OMX_TRUE;
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] out indexTimestamp: %d(0x%x)", pExynosComponent, __FUNCTION__, indexTimestamp, indexTimestamp);

        if ((indexTimestamp < 0) || (indexTimestamp >= MAX_TIMESTAMP)) {
            if (indexTimestamp == INDEX_AFTER_DRC) {
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] invalid frame, buffer will be discard and reused", pExynosComponent, __FUNCTION__);
                pDstOutputData->timeStamp   = 0x00;
                pDstOutputData->nFlags      = 0x00;
                ret = (OMX_ERRORTYPE)OMX_ErrorNoneReuseBuffer;
                goto EXIT;
            }

            if (indexTimestamp == INDEX_HEADER_DATA) {
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] codec specific data is generated", pExynosComponent, __FUNCTION__);
                indexTimestamp              = pVp8Enc->hMFCVp8Handle.outputIndexTimestamp;
                pDstOutputData->timeStamp   = pExynosComponent->nFlags[indexTimestamp];
                pDstOutputData->nFlags      = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;

                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output / buffer header(%p), nFlags: 0x%x, frameType: %d, dataLen: %d, timestamp %lld us (%.2f secs), Tag: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        pDstOutputData->bufferHeader, pDstOutputData->nFlags,
                                                        pVideoBuffer->frameType, pDstOutputData->dataLen,
                                                        pDstOutputData->timeStamp, (double)(pDstOutputData->timeStamp / 1E6),
                                                        indexTimestamp);
                ret = OMX_ErrorNone;
                goto EXIT;
            }
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Tag(%x) is invalid. changes to use outputIndexTimestamp(%d)",
                                                  pExynosComponent, __FUNCTION__,
                                                  indexTimestamp, pVp8Enc->hMFCVp8Handle.outputIndexTimestamp);
            indexTimestamp = pVp8Enc->hMFCVp8Handle.outputIndexTimestamp;
        } else {
            pVp8Enc->hMFCVp8Handle.outputIndexTimestamp = indexTimestamp;
        }

        /* In case of Video buffer batch mode, use timestamp from MFC device driver */
        if (pVideoBuffer->timestamp != 0)
            pDstOutputData->timeStamp = pVideoBuffer->timestamp;
        else
            pDstOutputData->timeStamp = pExynosComponent->timeStamp[indexTimestamp];

        pExynosComponent->bTimestampSlotUsed[indexTimestamp]    = OMX_FALSE;
        pDstOutputData->nFlags                                  = pExynosComponent->nFlags[indexTimestamp];
        pDstOutputData->nFlags                                 |= OMX_BUFFERFLAG_ENDOFFRAME;
    }

    if (pVideoBuffer->frameType == VIDEO_FRAME_I)
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output / buffer header(%p), nFlags: 0x%x, frameType: %d, dataLen: %d, timestamp %lld us (%.2f secs), Tag: %d",
                                            pExynosComponent, __FUNCTION__,
                                            pDstOutputData->bufferHeader, pDstOutputData->nFlags,
                                            pVideoBuffer->frameType, pDstOutputData->dataLen,
                                            pDstOutputData->timeStamp, (double)(pDstOutputData->timeStamp / 1E6),
                                            indexTimestamp);

#ifdef PERFORMANCE_DEBUG
    if (pDstOutputData->bufferHeader != NULL) {
        pDstOutputData->bufferHeader->nTimeStamp = pDstOutputData->timeStamp;
        Exynos_OSAL_V4L2CountDecrease(pOutputPort->hBufferCount, pDstOutputData->bufferHeader, OUTPUT_PORT_INDEX);
    }
#endif

    if ((pDstOutputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] got end of stream", pExynosComponent, __FUNCTION__);

        if (pExynosComponent->bBehaviorEOS == OMX_FALSE)
            pDstOutputData->remainDataLen = 0;
        else
            pExynosComponent->bBehaviorEOS = OMX_FALSE;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_srcInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT         *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pInputPort)) ||
        (!CHECK_PORT_POPULATED(pInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    ret = Exynos_VP8Enc_SrcIn(pOMXComponent, pSrcInputData);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                pExynosComponent, __FUNCTION__);
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_srcOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pInputPort)) ||
        (!CHECK_PORT_POPULATED(pInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pInputPort->bufferProcessType & BUFFER_COPY) {
        if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    if ((pVp8Enc->bSourceStart == OMX_FALSE) &&
        (!CHECK_PORT_BEING_FLUSHED(pInputPort))) {
        Exynos_OSAL_SignalWait(pVp8Enc->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get SourceStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pVp8Enc->hSourceStartEvent);
    }

    ret = Exynos_VP8Enc_SrcOut(pOMXComponent, pSrcOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                pExynosComponent, __FUNCTION__);
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_dstInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pOutputPort)) ||
        (!CHECK_PORT_POPULATED(pOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if ((pVp8Enc->bDestinationStart == OMX_FALSE) &&
        (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
        Exynos_OSAL_SignalWait(pVp8Enc->hDestinationInStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationInStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pVp8Enc->hDestinationInStartEvent);
    }

    if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
        if (Exynos_OSAL_GetElemNum(&pVp8Enc->bypassBufferInfoQ) > 0) {
            BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Dequeue(&pVp8Enc->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bypassBufferInfoQ has EOS buffer", pExynosComponent, __FUNCTION__);

            pDstInputData->bufferHeader->nFlags     = pBufferInfo->nFlags;
            pDstInputData->bufferHeader->nTimeStamp = pBufferInfo->timeStamp;

            Exynos_OMX_OutputBufferReturn(pOMXComponent, pDstInputData->bufferHeader);
            Exynos_OSAL_Free(pBufferInfo);

            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    if (pVp8Enc->hMFCVp8Handle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_VP8Enc_DstIn(pOMXComponent, pDstInputData);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                    pExynosComponent, __FUNCTION__);
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_VP8Enc_dstOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pOutputPort)) ||
        (!CHECK_PORT_POPULATED(pOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if ((pVp8Enc->bDestinationStart == OMX_FALSE) &&
        (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
        Exynos_OSAL_SignalWait(pVp8Enc->hDestinationOutStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

         Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationOutStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pVp8Enc->hDestinationOutStartEvent);
    }

    if (pOutputPort->bufferProcessType & BUFFER_COPY) {
        if (Exynos_OSAL_GetElemNum(&pVp8Enc->bypassBufferInfoQ) > 0) {
            EXYNOS_OMX_DATABUFFER *dstOutputUseBuffer   = &pOutputPort->way.port2WayDataBuffer.outputDataBuffer;
            OMX_BUFFERHEADERTYPE  *pOMXBuffer           = NULL;
            BYPASS_BUFFER_INFO    *pBufferInfo          = NULL;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bypassBufferInfoQ has EOS buffer", pExynosComponent, __FUNCTION__);

            if (dstOutputUseBuffer->dataValid == OMX_FALSE) {
                pOMXBuffer = Exynos_OutputBufferGetQueue_Direct(pExynosComponent);
                if (pOMXBuffer == NULL) {
                    ret = OMX_ErrorUndefined;
                    goto EXIT;
                }
            } else {
                pOMXBuffer = dstOutputUseBuffer->bufferHeader;
            }

            pBufferInfo = Exynos_OSAL_Dequeue(&pVp8Enc->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            pOMXBuffer->nFlags      = pBufferInfo->nFlags;
            pOMXBuffer->nTimeStamp  = pBufferInfo->timeStamp;
            Exynos_OMX_OutputBufferReturn(pOMXComponent, pOMXBuffer);
            Exynos_OSAL_Free(pBufferInfo);

            dstOutputUseBuffer->dataValid = OMX_FALSE;

            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    ret = Exynos_VP8Enc_DstOut(pOMXComponent, pDstOutputData);
    if (((ret != OMX_ErrorNone) && (ret != OMX_ErrorNoneReuseBuffer)) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)",
                                                pExynosComponent, __FUNCTION__);
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(
    OMX_HANDLETYPE hComponent,
    OMX_STRING     componentName)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;
    EXYNOS_VP8ENC_HANDLE          *pVp8Enc          = NULL;
    int i = 0;

    Exynos_OSAL_Get_Log_Property(); // For debuging
    FunctionIn();

    if ((hComponent == NULL) ||
        (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_VP8_ENC, componentName) != 0) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unsupported component name(%s)", __FUNCTION__, componentName);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_VideoEncodeComponentInit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to VideoDecodeComponentInit (0x%x)", componentName, __FUNCTION__, ret);
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosComponent->codecType = HW_VIDEO_ENC_CODEC;

    pExynosComponent->componentName = (OMX_STRING)Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pExynosComponent->componentName == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);

    pVp8Enc = Exynos_OSAL_Malloc(sizeof(EXYNOS_VP8ENC_HANDLE));
    if (pVp8Enc == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pVp8Enc, 0, sizeof(EXYNOS_VP8ENC_HANDLE));

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoEnc->hCodecHandle = (OMX_HANDLETYPE)pVp8Enc;
    pVp8Enc->qpRangeI.nMinQP = 2;   /* index = 2, value = 2 */
    pVp8Enc->qpRangeI.nMaxQP = 63;  /* index = 63, value = 127 */
    pVp8Enc->qpRangeP.nMinQP = 2;   /* index = 2, value = 2 */;
    pVp8Enc->qpRangeP.nMaxQP = 63;  /* index = 63, value = 127 */

    pVideoEnc->quantization.nQpI = 37;
    pVideoEnc->quantization.nQpP = 40;
    pVideoEnc->quantization.nQpB = 40;

    Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_VP8_ENC);

    /* Set componentVersion */
    pExynosComponent->componentVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->componentVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->componentVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->componentVersion.s.nStep         = STEP_NUMBER;
    /* Set specVersion */
    pExynosComponent->specVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->specVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->specVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->specVersion.s.nStep         = STEP_NUMBER;

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth  = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_COPY;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_MULTIPLE;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth  = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/avc");
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_SINGLE;

    for(i = 0; i < ALL_PORT_NUM; i++) {
        INIT_SET_SIZE_VERSION(&pVp8Enc->VP8Component[i], OMX_VIDEO_PARAM_VP8TYPE);
        pVp8Enc->VP8Component[i].nPortIndex = i;
        pVp8Enc->VP8Component[i].eProfile   = OMX_VIDEO_VP8ProfileMain;
        pVp8Enc->VP8Component[i].eLevel     = OMX_VIDEO_VP8Level_Version0;
    }
    pVp8Enc->nPFrames = -1;  /* for GTS issue */

    Exynos_OSAL_Memset(&pVp8Enc->AndroidVp8EncoderType, 0, sizeof(OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE));
    INIT_SET_SIZE_VERSION(&pVp8Enc->AndroidVp8EncoderType, OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE);
    pVp8Enc->AndroidVp8EncoderType.nKeyFrameInterval    = pVp8Enc->nPFrames + 1;
    pVp8Enc->AndroidVp8EncoderType.eTemporalPattern     = OMX_VIDEO_VPXTemporalLayerPatternNone;
    pVp8Enc->AndroidVp8EncoderType.nTemporalLayerCount  = 1;
    for (i = 0; i < OMX_VIDEO_ANDROID_MAXVP8TEMPORALLAYERS; i++)
        pVp8Enc->AndroidVp8EncoderType.nTemporalLayerBitrateRatio[i] = 100;

    pVp8Enc->AndroidVp8EncoderType.nMinQuantizer = pVp8Enc->qpRangeI.nMinQP;
    pVp8Enc->AndroidVp8EncoderType.nMaxQuantizer = pVp8Enc->qpRangeI.nMaxQP;

    pOMXComponent->GetParameter      = &Exynos_VP8Enc_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_VP8Enc_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_VP8Enc_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_VP8Enc_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_VP8Enc_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_VP8Enc_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_VP8Enc_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_VP8Enc_Terminate;

    pVideoEnc->exynos_codec_srcInputProcess  = &Exynos_VP8Enc_srcInputBufferProcess;
    pVideoEnc->exynos_codec_srcOutputProcess = &Exynos_VP8Enc_srcOutputBufferProcess;
    pVideoEnc->exynos_codec_dstInputProcess  = &Exynos_VP8Enc_dstInputBufferProcess;
    pVideoEnc->exynos_codec_dstOutputProcess = &Exynos_VP8Enc_dstOutputBufferProcess;

    pVideoEnc->exynos_codec_start            = &VP8CodecStart;
    pVideoEnc->exynos_codec_stop             = &VP8CodecStop;
    pVideoEnc->exynos_codec_bufferProcessRun = &VP8CodecOutputBufferProcessRun;
    pVideoEnc->exynos_codec_enqueueAllBuffer = &VP8CodecEnqueueAllBuffer;

    pVideoEnc->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;

    pVideoEnc->exynos_codec_checkFormatSupport = &CheckFormatHWSupport;

    pVideoEnc->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoEnc->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Open", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pVp8Enc);
        pVp8Enc = pVideoEnc->hCodecHandle = NULL;
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pVp8Enc->hMFCVp8Handle.videoInstInfo.eCodecType = VIDEO_CODING_VP8;
    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        pVp8Enc->hMFCVp8Handle.videoInstInfo.eSecurityType = VIDEO_SECURE;
    else
        pVp8Enc->hMFCVp8Handle.videoInstInfo.eSecurityType = VIDEO_NORMAL;

    pVp8Enc->hMFCVp8Handle.nRefForPframes = 1;

    if (Exynos_Video_GetInstInfo(&(pVp8Enc->hMFCVp8Handle.videoInstInfo), VIDEO_FALSE /* enc */) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to GetInstInfo", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pVp8Enc);
        pVp8Enc = ((EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle = NULL;
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] GetInstInfo for enc : temporal-svc(%d)/qp-range(%d)/pvc(%d)",
            pExynosComponent, __FUNCTION__,
            (pVp8Enc->hMFCVp8Handle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport),
            (pVp8Enc->hMFCVp8Handle.videoInstInfo.supportInfo.enc.bQpRangePBSupport),
            (pVp8Enc->hMFCVp8Handle.videoInstInfo.supportInfo.enc.bPVCSupport));

    Exynos_Input_SetSupportFormat(pExynosComponent);
    SetProfileLevel(pExynosComponent);

#ifdef USE_ANDROID
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-qp-range",     (OMX_INDEXTYPE)OMX_IndexConfigVideoQPRange);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-ref-pframes",  (OMX_INDEXTYPE)OMX_IndexParamNumberRefPframes);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-drop-control", (OMX_INDEXTYPE)OMX_IndexParamVideoDropControl);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-disable-dfr", (OMX_INDEXTYPE)OMX_IndexParamVideoDisableDFR);
#endif

    pExynosComponent->currentState = OMX_StateLoaded;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_VP8ENC_HANDLE            *pVp8Enc            = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (((pExynosComponent->currentState != OMX_StateInvalid) &&
         (pExynosComponent->currentState != OMX_StateLoaded)) ||
        ((pExynosComponent->currentState == OMX_StateLoaded) &&
         (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle))) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] in curState(0x%x), OMX_FreeHandle() is called. change to OMX_StateInvalid",
                                            pExynosComponent, __FUNCTION__, pExynosComponent->currentState);
        Exynos_OMX_Component_AbnormalTermination(hComponent);
    }

    Exynos_OSAL_SharedMemory_Close(pVideoEnc->hSharedMemory);

    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;

    pVp8Enc = (EXYNOS_VP8ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pVp8Enc != NULL) {
        Exynos_OSAL_Free(pVp8Enc);
        pVp8Enc = pVideoEnc->hCodecHandle = NULL;
    }

#ifdef USE_ANDROID
    Exynos_OSAL_DelVendorExts(hComponent);
#endif

    ret = Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to VideoDecodeComponentDeinit", pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}
