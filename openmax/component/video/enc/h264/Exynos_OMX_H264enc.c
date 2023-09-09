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
 * @file        Exynos_OMX_H264enc.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_VencControl.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OMX_H264enc.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OSAL_Queue.h"

#include "Exynos_OSAL_Platform.h"

#include "VendorVideoAPI.h"

#ifdef USE_SKYPE_HD
#include "Exynos_OSAL_SkypeHD.h"
#endif

/* To use CSC_METHOD_HW in EXYNOS OMX, gralloc should allocate physical memory using FIMC */
/* It means GRALLOC_USAGE_HW_FIMC1 should be set on Native Window usage */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_H264_ENC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

static OMX_ERRORTYPE SetProfileLevel(
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc       = NULL;

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

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pH264Enc->hMFCH264Handle.profiles[nProfileCnt++] = OMX_VIDEO_AVCProfileBaseline;
    pH264Enc->hMFCH264Handle.profiles[nProfileCnt++] = OMX_VIDEO_AVCProfileMain;
    pH264Enc->hMFCH264Handle.profiles[nProfileCnt++] = OMX_VIDEO_AVCProfileHigh;
    pH264Enc->hMFCH264Handle.profiles[nProfileCnt++] = (OMX_VIDEO_AVCPROFILETYPE)OMX_VIDEO_AVCProfileConstrainedBaseline;
    pH264Enc->hMFCH264Handle.profiles[nProfileCnt++] = (OMX_VIDEO_AVCPROFILETYPE)OMX_VIDEO_AVCProfileConstrainedHigh;
    pH264Enc->hMFCH264Handle.nProfileCnt = nProfileCnt;

    switch (pH264Enc->hMFCH264Handle.videoInstInfo.HwVersion) {
    case MFC_1501:
    case MFC_150:
        pH264Enc->hMFCH264Handle.maxLevel = OMX_VIDEO_AVCLevel6;
        break;
    case MFC_1400:
    case MFC_1410:
    case MFC_140:
    case MFC_130:
    case MFC_120:
    case MFC_1220:
    case MFC_110:
    case MFC_100:
    case MFC_101:
        pH264Enc->hMFCH264Handle.maxLevel = OMX_VIDEO_AVCLevel52;
        break;
    case MFC_80:
    case MFC_90:
    case MFC_1010:
    case MFC_1120:
        pH264Enc->hMFCH264Handle.maxLevel = OMX_VIDEO_AVCLevel51;
        break;
    case MFC_61:
    case MFC_65:
    case MFC_72:
    case MFC_723:
    case MFC_77:
    case MFC_1011:
    case MFC_1021:
        pH264Enc->hMFCH264Handle.maxLevel = OMX_VIDEO_AVCLevel42;
        break;
    case MFC_51:
    case MFC_78:
    case MFC_78D:
    case MFC_92:
    case MFC_1020:
    default:
        pH264Enc->hMFCH264Handle.maxLevel = OMX_VIDEO_AVCLevel4;
        break;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE GetIndexToProfileLevel(
    EXYNOS_OMX_BASECOMPONENT         *pExynosComponent,
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevelType)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc       = NULL;

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

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (pH264Enc->hMFCH264Handle.nProfileCnt <= (int)pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pH264Enc->hMFCH264Handle.profiles[pProfileLevelType->nProfileIndex];
    pProfileLevelType->eLevel   = pH264Enc->hMFCH264Handle.maxLevel;
#else
    while ((pH264Enc->hMFCH264Handle.maxLevel >> nLevelCnt) > 0) {
        nLevelCnt++;
    }

    if ((pH264Enc->hMFCH264Handle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    nMaxIndex = pH264Enc->hMFCH264Handle.nProfileCnt * nLevelCnt;
    if (nMaxIndex <= pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pH264Enc->hMFCH264Handle.profiles[pProfileLevelType->nProfileIndex / nLevelCnt];
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
    EXYNOS_H264ENC_HANDLE           *pH264Enc   = NULL;

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

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc == NULL)
        goto EXIT;

    while ((pH264Enc->hMFCH264Handle.maxLevel >> nLevelCnt++) > 0);

    if ((pH264Enc->hMFCH264Handle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pH264Enc->hMFCH264Handle.nProfileCnt; i++) {
        if (pH264Enc->hMFCH264Handle.profiles[i] == pProfileLevelType->eProfile) {
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

static OMX_U32 OMXAVCProfileToProfileIDC(OMX_VIDEO_AVCPROFILETYPE profile)
{
    OMX_U32 ret = 0;

    if (profile == OMX_VIDEO_AVCProfileBaseline)
        ret = 0;
    else if ((OMX_VIDEO_AVCPROFILEEXTTYPE)profile == OMX_VIDEO_AVCProfileConstrainedBaseline)
        ret = 1;
    else if (profile == OMX_VIDEO_AVCProfileMain)
        ret = 2;
    else if (profile == OMX_VIDEO_AVCProfileHigh)
        ret = 4;
    else if ((OMX_VIDEO_AVCPROFILEEXTTYPE)profile == OMX_VIDEO_AVCProfileConstrainedHigh)
        ret = 17;

    return ret;
}

static OMX_U32 OMXAVCLevelToLevelIDC(OMX_VIDEO_AVCLEVELTYPE level)
{
    OMX_U32 ret = 11; //default OMX_VIDEO_AVCLevel4

    if (level == OMX_VIDEO_AVCLevel1)
        ret = 0;
    else if (level == OMX_VIDEO_AVCLevel1b)
        ret = 1;
    else if (level == OMX_VIDEO_AVCLevel11)
        ret = 2;
    else if (level == OMX_VIDEO_AVCLevel12)
        ret = 3;
    else if (level == OMX_VIDEO_AVCLevel13)
        ret = 4;
    else if (level == OMX_VIDEO_AVCLevel2)
        ret = 5;
    else if (level == OMX_VIDEO_AVCLevel21)
        ret = 6;
    else if (level == OMX_VIDEO_AVCLevel22)
        ret = 7;
    else if (level == OMX_VIDEO_AVCLevel3)
        ret = 8;
    else if (level == OMX_VIDEO_AVCLevel31)
        ret = 9;
    else if (level == OMX_VIDEO_AVCLevel32)
        ret = 10;
    else if (level == OMX_VIDEO_AVCLevel4)
        ret = 11;
    else if (level == OMX_VIDEO_AVCLevel41)
        ret = 12;
    else if (level == OMX_VIDEO_AVCLevel42)
        ret = 13;
    else if (level == OMX_VIDEO_AVCLevel5)
        ret = 14;
    else if (level == OMX_VIDEO_AVCLevel51)
        ret = 15;
    else if (level == OMX_VIDEO_AVCLevel52)
        ret = 16;
    else if (level == OMX_VIDEO_AVCLevel6)
        ret = 17;

    return ret;
}

static OMX_U8 *FindDelimiter(OMX_U8 *pBuffer, OMX_U32 size)
{
    OMX_U32 i;

    for (i = 0; i < size - 3; i++) {
        if ((pBuffer[i] == 0x00)   &&
            (pBuffer[i + 1] == 0x00) &&
            (pBuffer[i + 2] == 0x00) &&
            (pBuffer[i + 3] == 0x01))
            return (pBuffer + i);
    }

    return NULL;
}

static void Print_H264Enc_Param(ExynosVideoEncParam *pEncParam)
{
    ExynosVideoEncCommonParam *pCommonParam = &pEncParam->commonParam;
    ExynosVideoEncH264Param   *pH264Param   = &pEncParam->codecParam.h264;

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
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "QP(B) ranege            : %d / %d", pCommonParam->QpRange.QpMin_B, pCommonParam->QpRange.QpMax_B);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "PadControlOn            : %d", pCommonParam->PadControlOn);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LumaPadVal              : %d", pCommonParam->LumaPadVal);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "CbPadVal                : %d", pCommonParam->CbPadVal);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "CrPadVal                : %d", pCommonParam->CrPadVal);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameMap                : %d", pCommonParam->FrameMap);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "DropControl             : %d", pCommonParam->bDropControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "DisableDFR              : %d", pCommonParam->bDisableDFR);

    /* H.264 specific parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "ProfileIDC              : %d", pH264Param->ProfileIDC);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "LevelIDC                : %d", pH264Param->LevelIDC);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameQp_B               : %d", pH264Param->FrameQp_B);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameRate               : %d", pH264Param->FrameRate);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SliceArgument           : %d", pH264Param->SliceArgument);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "NumberBFrames           : %d", pH264Param->NumberBFrames);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "NumberReferenceFrames   : %d", pH264Param->NumberReferenceFrames);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "NumberRefForPframes     : %d", pH264Param->NumberRefForPframes);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LoopFilterDisable       : %d", pH264Param->LoopFilterDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LoopFilterAlphaC0Offset : %d", pH264Param->LoopFilterAlphaC0Offset);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LoopFilterBetaOffset    : %d", pH264Param->LoopFilterBetaOffset);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SymbolMode              : %d", pH264Param->SymbolMode);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "PictureInterlace        : %d", pH264Param->PictureInterlace);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "Transform8x8Mode        : %d", pH264Param->Transform8x8Mode);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "DarkDisable             : %d", pH264Param->DarkDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SmoothDisable           : %d", pH264Param->SmoothDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "StaticDisable           : %d", pH264Param->StaticDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "ActivityDisable         : %d", pH264Param->ActivityDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "HierarType:             : %d", pH264Param->HierarType);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "VuiRestrictionEnable:   : %d", pH264Param->VuiRestrictionEnable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "HeaderWithIFrame:       : %d", pH264Param->HeaderWithIFrame);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SarEnable:              : %d", pH264Param->SarEnable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SarIndex:               : %d", pH264Param->SarIndex);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SarWidth:               : %d", pH264Param->SarWidth);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SarHeight:              : %d", pH264Param->SarHeight);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "LTRFrames:              : %d", pH264Param->LTRFrames);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "ROIEnable:              : %d", pH264Param->ROIEnable);

    /* rate control related parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableFRMRateControl    : %d", pCommonParam->EnableFRMRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableMBRateControl     : %d", pCommonParam->EnableMBRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "CBRPeriodRf             : %d", pCommonParam->CBRPeriodRf);
}

static void Set_H264Enc_Param(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    EXYNOS_OMX_BASEPORT           *pInputPort       = NULL;
    EXYNOS_OMX_BASEPORT           *pOutputPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = NULL;
    EXYNOS_MFC_H264ENC_HANDLE     *pMFCH264Handle   = NULL;
    OMX_COLOR_FORMATTYPE           eColorFormat     = OMX_COLOR_FormatUnused;

    ExynosVideoEncParam       *pEncParam    = NULL;
    ExynosVideoEncInitParam   *pInitParam   = NULL;
    ExynosVideoEncCommonParam *pCommonParam = NULL;
    ExynosVideoEncH264Param   *pH264Param   = NULL;

    OMX_S32 nWidth, nHeight;
    int i;

    pVideoEnc           = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pH264Enc            = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCH264Handle      = &pH264Enc->hMFCH264Handle;
    pInputPort          = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pOutputPort         = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    pEncParam       = &pMFCH264Handle->encParam;
    pInitParam      = &pEncParam->initParam;
    pCommonParam    = &pEncParam->commonParam;
    pH264Param      = &pEncParam->codecParam.h264;
    pEncParam->eCompressionFormat = VIDEO_CODING_AVC;

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
    pCommonParam->IDRPeriod    = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames + 1;
    pCommonParam->SliceMode    = pH264Enc->AVCSliceFmo.eSliceMode;
    pCommonParam->Bitrate      = pOutputPort->portDefinition.format.video.nBitrate;
    pCommonParam->FrameQp      = pVideoEnc->quantization.nQpI;
    pCommonParam->FrameQp_P    = pVideoEnc->quantization.nQpP;

    pCommonParam->QpRange.QpMin_I = pH264Enc->qpRangeI.nMinQP;
    pCommonParam->QpRange.QpMax_I = pH264Enc->qpRangeI.nMaxQP;
    pCommonParam->QpRange.QpMin_P = pH264Enc->qpRangeP.nMinQP;
    pCommonParam->QpRange.QpMax_P = pH264Enc->qpRangeP.nMaxQP;
    pCommonParam->QpRange.QpMin_B = pH264Enc->qpRangeB.nMinQP;
    pCommonParam->QpRange.QpMax_B = pH264Enc->qpRangeB.nMaxQP;

    pCommonParam->PadControlOn = 0;    /* 0: disable, 1: enable */
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

    /* H.264 specific parameters */
    pH264Param->ProfileIDC   = OMXAVCProfileToProfileIDC(pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].eProfile);    /*0: OMX_VIDEO_AVCProfileMain */
    pH264Param->LevelIDC     = OMXAVCLevelToLevelIDC(pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].eLevel);    /*40: OMX_VIDEO_AVCLevel4 */
    pH264Param->FrameQp_B    = pVideoEnc->quantization.nQpB;
    pH264Param->FrameRate    = (pInputPort->portDefinition.format.video.xFramerate) >> 16;
    if (pH264Enc->AVCSliceFmo.eSliceMode == OMX_VIDEO_SLICEMODE_AVCDefault) {
        pH264Param->SliceArgument = 0;    /* Slice mb/byte size number */

        if (pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nSliceHeaderSpacing > 0) {
            pCommonParam->SliceMode   = OMX_VIDEO_SLICEMODE_AVCMBSlice;
            pH264Param->SliceArgument = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nSliceHeaderSpacing;
        }
    } else {
        pH264Param->SliceArgument = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nSliceHeaderSpacing;
    }

    pH264Param->NumberBFrames           = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nBFrames;   /* 0 ~ 2 */
    pH264Param->NumberRefForPframes     = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nRefFrames; /* 1 ~ 2 */
    pH264Param->NumberReferenceFrames   = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nRefFrames;

    pH264Param->LoopFilterDisable       = 0;    /* 1: Loop Filter Disable, 0: Filter Enable */
    pH264Param->LoopFilterAlphaC0Offset = 0;
    pH264Param->LoopFilterBetaOffset    = 0;
    pH264Param->SymbolMode       = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].bEntropyCodingCABAC;    /* 0: CAVLC, 1: CABAC */
    pH264Param->PictureInterlace = 0;
    pH264Param->Transform8x8Mode = 1;    /* 0: 4x4, 1: allow 8x8 */
    pH264Param->DarkDisable      = 1;
    pH264Param->SmoothDisable    = 1;
    pH264Param->StaticDisable    = 1;
    pH264Param->ActivityDisable  = 1;

    /* Chroma QP Offset */
    pH264Param->chromaQPOffset.Cr       = pH264Enc->chromaQPOffset.nCr;
    pH264Param->chromaQPOffset.Cb       = pH264Enc->chromaQPOffset.nCb;

    /* Temporal SVC */
    /* If MaxTemporalLayerCount value is 0, codec supported max value will be set */
    pH264Param->MaxTemporalLayerCount           = 0;
    pH264Param->TemporalSVC.nTemporalLayerCount = (unsigned int)pH264Enc->nTemporalLayerCount;

#ifdef USE_SKYPE_HD
    if (pH264Enc->hMFCH264Handle.bEnableSkypeHD == OMX_TRUE) {
        pH264Param->TemporalSVC.nTemporalLayerCount |= GENERAL_TSVC_ENABLE;  /* always uses short-term reference */
        pH264Param->MaxTemporalLayerCount            = (unsigned int)pH264Enc->nMaxTemporalLayerCount;  /* max layer count on short-term reference */
    }
#endif

    if (pH264Enc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
        for (i = 0; i < OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS; i++)
            pH264Param->TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)pH264Enc->nTemporalLayerBitrateRatio[i];
    } else {
        for (i = 0; i < OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS; i++)
            pH264Param->TemporalSVC.nTemporalLayerBitrateRatio[i] = pOutputPort->portDefinition.format.video.nBitrate;
    }

    /* Hierarchal P & B */
    if (pH264Enc->nTemporalLayerCountForB > 0)
        pH264Param->HierarType = EXYNOS_OMX_Hierarchical_B;
    else
        pH264Param->HierarType = pH264Enc->hMFCH264Handle.eHierarchicalType;

    /* SPS VUI */
    if (pH264Enc->hMFCH264Handle.stSarParam.SarEnable == OMX_TRUE) {
        pH264Param->SarEnable = 1;
        pH264Param->SarIndex  = pH264Enc->hMFCH264Handle.stSarParam.SarIndex;
        pH264Param->SarWidth  = pH264Enc->hMFCH264Handle.stSarParam.SarWidth;
        pH264Param->SarHeight = pH264Enc->hMFCH264Handle.stSarParam.SarHeight;
    } else {
        pH264Param->SarEnable = 0;
        pH264Param->SarIndex  = 0;
        pH264Param->SarWidth  = 0;
        pH264Param->SarHeight = 0;
    }

#ifdef USE_SKYPE_HD
    if (pH264Enc->hMFCH264Handle.bLowLatency == OMX_TRUE) {
        pH264Param->HeaderWithIFrame                = 0; /* 1: header + first frame */
        pH264Param->LoopFilterDisable               = 0; /* 1: disable, 0: enable */
        pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]  = OMX_Video_ControlRateDisable;
        pCommonParam->EnableFRMQpControl            = 1; /* 0: Disable,  1: Per frame QP */
    }
#endif
    pH264Param->VuiRestrictionEnable = (int)OMX_TRUE;

    pH264Param->LTRFrames = pMFCH264Handle->nLTRFrames;

    pH264Param->ROIEnable = (pMFCH264Handle->bRoiInfo == OMX_TRUE)? 1:0;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] eControlRate: 0x%x", pExynosComponent, __FUNCTION__, pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
    /* rate control related parameters */
    switch ((int)pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]) {
    case OMX_Video_ControlRateDisable:
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode DBR");
        pCommonParam->EnableFRMRateControl = 0;    /* 0: Disable,  1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 0;    /* 0: Disable,  1: MB level RC */
        pCommonParam->CBRPeriodRf          = 100;
        break;
    case OMX_Video_ControlRateConstantVTCall:
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode CBR VT Call");
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable,  1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable,  1: MB level RC */
        pCommonParam->CBRPeriodRf          = 5;
        pCommonParam->bFixedSlice          = VIDEO_TRUE;
        break;
    case OMX_Video_ControlRateConstantSkipFrames:
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode CBR VT Call with Skip frame");
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable,  1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable,  1: MB level RC */
        pCommonParam->CBRPeriodRf          = 5;
        pCommonParam->bFixedSlice          = VIDEO_TRUE;
        pInitParam->FrameSkip              = VIDEO_FRAME_SKIP_MODE_LEVEL_LIMIT;
        break;
    case OMX_Video_ControlRateConstant:
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode CBR");
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable,  1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable,  1: MB level RC */
        pCommonParam->CBRPeriodRf          = 10;
        break;
    case OMX_Video_ControlRateVariable:
    default: /*Android default */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode VBR");
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable,  1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable,  1: MB level RC */
        pCommonParam->CBRPeriodRf          = 100;
        break;
    }

//    Print_H264Enc_Param(pEncParam);
}

static OMX_ERRORTYPE Change_H264Enc_Param(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc         = NULL;
    EXYNOS_H264ENC_HANDLE         *pH264Enc          = NULL;
    EXYNOS_MFC_H264ENC_HANDLE     *pMFCH264Handle    = NULL;
    OMX_PTR                        pDynamicConfigCMD = NULL;
    OMX_PTR                        pConfigData       = NULL;
    OMX_S32                        nCmdIndex         = 0;
    ExynosVideoEncOps             *pEncOps           = NULL;
    int                            nValue            = 0;

    int i;

    pVideoEnc       = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pH264Enc        = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCH264Handle  = &pH264Enc->hMFCH264Handle;
    pEncOps         = pMFCH264Handle->pEncOps;

    pDynamicConfigCMD = (OMX_PTR)Exynos_OSAL_Dequeue(&pExynosComponent->dynamicConfigQ);
    if (pDynamicConfigCMD == NULL)
        goto EXIT;

    nCmdIndex   = *(OMX_S32 *)pDynamicConfigCMD;
    pConfigData = (OMX_PTR)((OMX_U8 *)pDynamicConfigCMD + sizeof(OMX_S32));

    switch ((int)nCmdIndex) {
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        nValue = VIDEO_FRAME_I;
        pEncOps->Set_FrameType(pMFCH264Handle->hMFCHandle, nValue);
        pVideoEnc->IntraRefreshVOP = OMX_FALSE;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] VOP Refresh", pExynosComponent, __FUNCTION__);
    }
        break;
    case OMX_IndexConfigVideoIntraPeriod:
    {
        OMX_S32 nPFrames = (*((OMX_S32 *)pConfigData)) - 1;

        nValue = nPFrames + 1;
        pEncOps->Set_IDRPeriod(pH264Enc->hMFCH264Handle.hMFCHandle, nValue);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] IDR period: %d", pExynosComponent, __FUNCTION__, nValue);
    }
        break;
    case OMX_IndexConfigVideoAVCIntraPeriod:
    {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pAVCIntraPeriod = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pConfigData;

        nValue = pAVCIntraPeriod->nIDRPeriod;
        pEncOps->Set_IDRPeriod(pMFCH264Handle->hMFCHandle, nValue);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] IDR period: %d", pExynosComponent, __FUNCTION__, nValue);
    }
        break;
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pConfigData;

        if (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] != OMX_Video_ControlRateDisable) {
            nValue = pConfigBitrate->nEncodeBitrate;
            pEncOps->Set_BitRate(pH264Enc->hMFCH264Handle.hMFCHandle, nValue);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bitrate: %d", pExynosComponent, __FUNCTION__, nValue);
        }
    }
        break;
    case OMX_IndexConfigVideoFramerate:
    {
        OMX_CONFIG_FRAMERATETYPE *pConfigFramerate = (OMX_CONFIG_FRAMERATETYPE *)pConfigData;
        OMX_U32                   nPortIndex       = pConfigFramerate->nPortIndex;

        if (nPortIndex == INPUT_PORT_INDEX) {
            nValue = (pConfigFramerate->xEncodeFramerate) >> 16;
            pEncOps->Set_FrameRate(pH264Enc->hMFCH264Handle.hMFCHandle, nValue);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] framerate: %d", pExynosComponent, __FUNCTION__, nValue);
        }
    }
        break;
    case OMX_IndexConfigVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE *pQpRange = (OMX_VIDEO_QPRANGETYPE *)pConfigData;
        ExynosVideoQPRange     qpRange;

        qpRange.QpMin_I = pQpRange->qpRangeI.nMinQP;
        qpRange.QpMax_I = pQpRange->qpRangeI.nMaxQP;
        qpRange.QpMin_P = pQpRange->qpRangeP.nMinQP;
        qpRange.QpMax_P = pQpRange->qpRangeP.nMaxQP;
        qpRange.QpMin_B = pQpRange->qpRangeB.nMinQP;
        qpRange.QpMax_B = pQpRange->qpRangeB.nMaxQP;

        pEncOps->Set_QpRange(pMFCH264Handle->hMFCHandle, qpRange);

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] qp range: I(%d, %d), P(%d, %d), B(%d, %d)",
                                    pExynosComponent, __FUNCTION__,
                                    qpRange.QpMin_I, qpRange.QpMax_I,
                                    qpRange.QpMin_P, qpRange.QpMax_P,
                                    qpRange.QpMin_B, qpRange.QpMax_B);
    }
        break;
    case OMX_IndexConfigOperatingRate:
    {
        OMX_PARAM_U32TYPE *pConfigRate = (OMX_PARAM_U32TYPE *)pConfigData;
        OMX_U32            xFramerate  = pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.xFramerate;

        nValue = pConfigRate->nU32 >> 16;

        if (pMFCH264Handle->videoInstInfo.supportInfo.enc.bOperatingRateSupport == VIDEO_TRUE) {
            if (nValue == ((OMX_U32)INT_MAX >> 16)) {
                nValue = (OMX_U32)INT_MAX;
            } else {
                nValue = nValue * 1000;
            }

            pEncOps->Set_OperatingRate(pMFCH264Handle->hMFCHandle, nValue);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] operating rate: 0x%x", pExynosComponent, __FUNCTION__, nValue);
        } else {
            if (nValue == (((OMX_U32)INT_MAX) >> 16)) {
                nValue = 1000;
            } else {
                nValue = ((xFramerate >> 16) == 0)? 100:(OMX_U32)((pConfigRate->nU32 / (double)xFramerate) * 100);
            }

            pEncOps->Set_QosRatio(pMFCH264Handle->hMFCHandle, nValue);

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] qos ratio: 0x%x", pExynosComponent, __FUNCTION__, nValue);
        }

        pVideoEnc->bQosChanged = OMX_FALSE;
    }
        break;
    case OMX_IndexConfigVideoTemporalSVC:
    {
        EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *pTemporalSVC = (EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *)pConfigData;
        ExynosVideoQPRange qpRange;
        TemporalLayerShareBuffer TemporalSVC;

        qpRange.QpMin_I = pTemporalSVC->nMinQuantizer;
        qpRange.QpMax_I = pTemporalSVC->nMaxQuantizer;
        qpRange.QpMin_P = pTemporalSVC->nMinQuantizer;
        qpRange.QpMax_P = pTemporalSVC->nMaxQuantizer;
        qpRange.QpMin_B = pTemporalSVC->nMinQuantizer;
        qpRange.QpMax_B = pTemporalSVC->nMaxQuantizer;

        pEncOps->Set_QpRange(pMFCH264Handle->hMFCHandle, qpRange);
        pEncOps->Set_IDRPeriod(pMFCH264Handle->hMFCHandle, pTemporalSVC->nKeyFrameInterval);

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // qp range: I(%d, %d), P(%d, %d), B(%d, %d)",
                                    pExynosComponent, __FUNCTION__,
                                    qpRange.QpMin_I, qpRange.QpMax_I,
                                    qpRange.QpMin_P, qpRange.QpMax_P,
                                    qpRange.QpMin_B, qpRange.QpMax_B);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // IDR period: %d", pExynosComponent, __FUNCTION__, nValue);

        /* Temporal SVC */
        Exynos_OSAL_Memset(&TemporalSVC, 0, sizeof(TemporalLayerShareBuffer));

        TemporalSVC.nTemporalLayerCount = (unsigned int)pTemporalSVC->nTemporalLayerCount;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // layer count: %d", pExynosComponent, __FUNCTION__, TemporalSVC.nTemporalLayerCount);

        for (i = 0; i < OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS; i++) {
            TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)pTemporalSVC->nTemporalLayerBitrateRatio[i];
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // bitrate ratio[%d]: %d",
                                                    pExynosComponent, __FUNCTION__,
                                                    i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
        }
        if (pEncOps->Set_LayerChange(pMFCH264Handle->hMFCHandle, TemporalSVC) != VIDEO_ERROR_NONE)
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Not supported control: Set_LayerChange", pExynosComponent, __FUNCTION__);
    }
        break;
    case OMX_IndexConfigVideoRoiInfo:
    {
        EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *pRoiInfo = (EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *)pConfigData;

        /* ROI INFO */
        RoiInfoShareBuffer RoiInfo;

        Exynos_OSAL_Memset(&RoiInfo, 0, sizeof(RoiInfo));
        RoiInfo.pRoiMBInfo     = (OMX_U64)(unsigned long)(((OMX_U8 *)pConfigData) + sizeof(EXYNOS_OMX_VIDEO_CONFIG_ROIINFO));
        RoiInfo.nRoiMBInfoSize = pRoiInfo->nRoiMBInfoSize;
        RoiInfo.nUpperQpOffset = pRoiInfo->nUpperQpOffset;
        RoiInfo.nLowerQpOffset = pRoiInfo->nLowerQpOffset;
        RoiInfo.bUseRoiInfo    = (pRoiInfo->bUseRoiInfo == OMX_TRUE)? VIDEO_TRUE:VIDEO_FALSE;

        if (pEncOps->Set_RoiInfo(pMFCH264Handle->hMFCHandle, &RoiInfo) != VIDEO_ERROR_NONE)
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Set_RoiInfo()", pExynosComponent, __FUNCTION__);
    }
        break;
    case OMX_IndexConfigIFrameRatio:
    {
        OMX_PARAM_U32TYPE *pIFrameRatio = (OMX_PARAM_U32TYPE *)pConfigData;

        pEncOps->Set_IFrameRatio(pMFCH264Handle->hMFCHandle, pIFrameRatio->nU32);
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
#ifdef USE_ANDROID
    case OMX_IndexConfigAndroidVideoTemporalLayering:
    {
        OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE *pTemporalLayering = (OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE *)pConfigData;

        TemporalLayerShareBuffer TemporalSVC;

        /* Temporal SVC */
        Exynos_OSAL_Memset(&TemporalSVC, 0, sizeof(TemporalLayerShareBuffer));

        TemporalSVC.nTemporalLayerCount = (unsigned int)(pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual);

        if (pTemporalLayering->bBitrateRatiosSpecified == OMX_TRUE) {
            for (i = 0; i < (int)pH264Enc->nMaxTemporalLayerCount; i++) {

                TemporalSVC.nTemporalLayerBitrateRatio[i] = (pTemporalLayering->nBitrateRatios[i] >> 16);
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // bitrate ratio[%d]: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
            }
        } else {
            EXYNOS_OMX_BASEPORT *pOutputPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

            for (i = 0; i < (int)pH264Enc->nMaxTemporalLayerCount; i++) {
                TemporalSVC.nTemporalLayerBitrateRatio[i] = pOutputPort->portDefinition.format.video.nBitrate;

                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // bitrate ratio[%d]: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
            }
        }

        if (pEncOps->Set_LayerChange(pMFCH264Handle->hMFCHandle, TemporalSVC) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Not supported control: Set_LayerChange", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Max(%d), layer(%d), B-layer(%d)", pExynosComponent, __FUNCTION__,
                                               pH264Enc->nMaxTemporalLayerCount, TemporalSVC.nTemporalLayerCount, pTemporalLayering->nBLayerCountActual);
    }
        break;
#endif
    default:
#ifdef USE_SKYPE_HD
        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bSkypeSupport == VIDEO_TRUE) {
            ret = Change_H264Enc_SkypeHDParam(pExynosComponent, pDynamicConfigCMD);
            if (ret != OMX_ErrorNone)
                goto EXIT;
        }
#endif
        break;
    }

    Exynos_OSAL_Free(pDynamicConfigCMD);

    Set_H264Enc_Param(pExynosComponent);

EXIT:
    return ret;
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
    EXYNOS_H264ENC_HANDLE           *pH264Enc       = NULL;
    EXYNOS_OMX_BASEPORT             *pInputPort     = NULL;
    ExynosVideoColorFormatType       eVideoFormat   = VIDEO_COLORFORMAT_UNKNOWN;
    int i;

    if (pExynosComponent == NULL)
        goto EXIT;

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL)
        goto EXIT;

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc == NULL)
        goto EXIT;
    pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    eVideoFormat = (ExynosVideoColorFormatType)Exynos_OSAL_OMX2VideoFormat(eColorFormat, pInputPort->ePlaneType);

    for (i = 0; i < VIDEO_COLORFORMAT_MAX; i++) {
        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportFormat[i] == VIDEO_COLORFORMAT_UNKNOWN)
            break;

        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportFormat[i] == eVideoFormat) {
            ret = OMX_TRUE;
            break;
        }
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE H264CodecOpen(
    EXYNOS_H264ENC_HANDLE   *pH264Enc,
    ExynosVideoInstInfo     *pVideoInstInfo)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    ExynosVideoEncOps       *pEncOps    = NULL;
    ExynosVideoEncBufferOps *pInbufOps  = NULL;
    ExynosVideoEncBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if ((pH264Enc == NULL) ||
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

    pH264Enc->hMFCH264Handle.pEncOps = pEncOps;
    pH264Enc->hMFCH264Handle.pInbufOps = pInbufOps;
    pH264Enc->hMFCH264Handle.pOutbufOps = pOutbufOps;

    /* function pointer mapping */
    pEncOps->nSize = sizeof(ExynosVideoEncOps);
    pInbufOps->nSize = sizeof(ExynosVideoEncBufferOps);
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
    pH264Enc->hMFCH264Handle.hMFCHandle = pH264Enc->hMFCH264Handle.pEncOps->Init(pVideoInstInfo);
    if (pH264Enc->hMFCH264Handle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to init", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pEncOps != NULL) {
            Exynos_OSAL_Free(pEncOps);
            pH264Enc->hMFCH264Handle.pEncOps = NULL;
        }

        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pH264Enc->hMFCH264Handle.pInbufOps = NULL;
        }

        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pH264Enc->hMFCH264Handle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE H264CodecClose(EXYNOS_H264ENC_HANDLE *pH264Enc)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoEncOps       *pEncOps    = NULL;
    ExynosVideoEncBufferOps *pInbufOps  = NULL;
    ExynosVideoEncBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pH264Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pH264Enc->hMFCH264Handle.hMFCHandle;
    pEncOps    = pH264Enc->hMFCH264Handle.pEncOps;
    pInbufOps  = pH264Enc->hMFCH264Handle.pInbufOps;
    pOutbufOps = pH264Enc->hMFCH264Handle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pEncOps->Finalize(hMFCHandle);
        hMFCHandle = pH264Enc->hMFCH264Handle.hMFCHandle = NULL;
        pH264Enc->hMFCH264Handle.bConfiguredMFCSrc = OMX_FALSE;
        pH264Enc->hMFCH264Handle.bConfiguredMFCDst = OMX_FALSE;
    }

    /* Unregister function pointers */
    Exynos_Video_Unregister_Encoder(pEncOps, pInbufOps, pOutbufOps);

    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pOutbufOps = pH264Enc->hMFCH264Handle.pOutbufOps = NULL;
    }

    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pInbufOps = pH264Enc->hMFCH264Handle.pInbufOps = NULL;
    }

    if (pEncOps != NULL) {
        Exynos_OSAL_Free(pEncOps);
        pEncOps = pH264Enc->hMFCH264Handle.pEncOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE H264CodecStart(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;
    void                            *hMFCHandle         = NULL;
    ExynosVideoEncBufferOps         *pInbufOps          = NULL;
    ExynosVideoEncBufferOps         *pOutbufOps         = NULL;

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

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pH264Enc->hMFCH264Handle.hMFCHandle;
    pInbufOps  = pH264Enc->hMFCH264Handle.pInbufOps;
    pOutbufOps = pH264Enc->hMFCH264Handle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX)
        pInbufOps->Run(hMFCHandle);
    else if (nPortIndex == OUTPUT_PORT_INDEX)
        pOutbufOps->Run(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE H264CodecStop(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;
    void                            *hMFCHandle         = NULL;
    ExynosVideoEncBufferOps         *pInbufOps          = NULL;
    ExynosVideoEncBufferOps         *pOutbufOps         = NULL;

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

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pH264Enc->hMFCH264Handle.hMFCHandle;
    pInbufOps  = pH264Enc->hMFCH264Handle.pInbufOps;
    pOutbufOps = pH264Enc->hMFCH264Handle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) && (pInbufOps != NULL))
        pInbufOps->Stop(hMFCHandle);
    else if ((nPortIndex == OUTPUT_PORT_INDEX) && (pOutbufOps != NULL))
        pOutbufOps->Stop(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE H264CodecOutputBufferProcessRun(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

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

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pH264Enc->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pH264Enc->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pH264Enc->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pH264Enc->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pH264Enc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE H264CodecEnqueueAllBuffer(
    OMX_COMPONENTTYPE *pOMXComponent,
    OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle       = pH264Enc->hMFCH264Handle.hMFCHandle;

    ExynosVideoEncBufferOps *pInbufOps              = pH264Enc->hMFCH264Handle.pInbufOps;
    ExynosVideoEncBufferOps *pOutbufOps             = pH264Enc->hMFCH264Handle.pOutbufOps;

    int i;

    FunctionIn();

    if ((nPortIndex != INPUT_PORT_INDEX) &&
        (nPortIndex != OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pH264Enc->bSourceStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)  {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] CodecBuffer(input) [%d]: FD(0x%x), VA(0x%x), size(%d)",
                                                pExynosComponent, __FUNCTION__,
                                                i, pVideoEnc->pMFCEncInputBuffer[i]->fd[0], pVideoEnc->pMFCEncInputBuffer[i]->pVirAddr[0],
                                                pVideoEnc->pMFCEncInputBuffer[i]->bufferSize[0]);

            Exynos_CodecBufferEnqueue(pExynosComponent, INPUT_PORT_INDEX, pVideoEnc->pMFCEncInputBuffer[i]);
        }

        pInbufOps->Clear_Queue(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pH264Enc->bDestinationStart == OMX_TRUE)) {
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

void H264CodecSetHdrInfo(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_H264ENC_HANDLE     *pMFCH264Handle   = &pH264Enc->hMFCH264Handle;
    void                          *hMFCHandle       = pMFCH264Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoEncOps *pEncOps = pH264Enc->hMFCH264Handle.pEncOps;

    OMX_COLOR_FORMATTYPE eColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);

    if (pMFCH264Handle->videoInstInfo.supportInfo.enc.bColorAspectsSupport == VIDEO_TRUE) {
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

OMX_ERRORTYPE H264CodecUpdateResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_H264ENC_HANDLE     *pMFCH264Handle   = &pH264Enc->hMFCH264Handle;
    void                          *hMFCHandle       = pMFCH264Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE           eColorFormat     = Exynos_Input_GetActualColorFormat(pExynosComponent);

    ExynosVideoEncOps       *pEncOps    = pH264Enc->hMFCH264Handle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps  = pH264Enc->hMFCH264Handle.pInbufOps;
    ExynosVideoGeometry      bufferConf;

    FunctionIn();

    pVideoEnc->bEncDRCSync = OMX_TRUE;

    /* stream off */
    ret = H264CodecStop(pOMXComponent, INPUT_PORT_INDEX);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to H264CodecStop() about input", pExynosComponent, __FUNCTION__);
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
    H264CodecStart(pOMXComponent, INPUT_PORT_INDEX);

    /* reset buffer queue */
    H264CodecEnqueueAllBuffer(pOMXComponent, INPUT_PORT_INDEX);

    /* clear timestamp */
    Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);

    /* reinitialize a value for reconfiguring CSC */
    pVideoEnc->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE H264CodecSrcSetup(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_H264ENC_HANDLE     *pMFCH264Handle   = &pH264Enc->hMFCH264Handle;
    void                          *hMFCHandle       = pMFCH264Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize     = pSrcInputData->dataLen;

    ExynosVideoEncOps       *pEncOps    = pH264Enc->hMFCH264Handle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps  = pH264Enc->hMFCH264Handle.pInbufOps;
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
        ret = Exynos_OSAL_Queue(&pH264Enc->bypassBufferInfoQ, (void *)pBufferInfo);

        if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
            Exynos_OSAL_SignalSet(pH264Enc->hDestinationInStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        } else if (pOutputPort->bufferProcessType & BUFFER_COPY) {
            Exynos_OSAL_SignalSet(pH264Enc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    Set_H264Enc_Param(pExynosComponent);

    pEncParam = &pMFCH264Handle->encParam;
    if (pEncOps->Set_EncParam) {
        if(pEncOps->Set_EncParam(pH264Enc->hMFCH264Handle.hMFCHandle, pEncParam) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set encParam", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    }

    Print_H264Enc_Param(pEncParam);

    if (pMFCH264Handle->bPrependSpsPpsToIdr == OMX_TRUE) {
        if (pEncOps->Enable_PrependSpsPpsToIdr)
            pEncOps->Enable_PrependSpsPpsToIdr(pH264Enc->hMFCH264Handle.hMFCHandle);
        else
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Not supported control: Enable_PrependSpsPpsToIdr", pExynosComponent, __FUNCTION__);
    }

    if ((pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) &&
        ((pInputPort->bufferProcessType & BUFFER_SHARE) &&
         (pSrcInputData->buffer.addr[2] != NULL))) {
        ExynosVideoMeta *pMeta = (ExynosVideoMeta *)pSrcInputData->buffer.addr[2];

        if (pMeta->eType & VIDEO_INFO_TYPE_YSUM_DATA) {
            if (VIDEO_ERROR_NONE == pEncOps->Enable_WeightedPrediction(hMFCHandle))
                pMFCH264Handle->bWeightedPrediction = OMX_TRUE;
        }
    }

    if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bAdaptiveLayerBitrateSupport == VIDEO_TRUE) {
        if (pH264Enc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
            pEncOps->Enable_AdaptiveLayerBitrate(pMFCH264Handle->hMFCHandle, 0); /* Disable : Use layer bitrate from framework */
        } else {
            pEncOps->Enable_AdaptiveLayerBitrate(pMFCH264Handle->hMFCHandle, 1); /* Enable : Use adaptive layer bitrate from H/W */
        }
    }

#ifdef USE_ANDROID
    H264CodecSetHdrInfo(pOMXComponent);
#endif

    if (pMFCH264Handle->videoInstInfo.supportInfo.enc.bPrioritySupport == VIDEO_TRUE)
        pEncOps->Set_Priority(hMFCHandle, pVideoEnc->nPriority);

    if (pEncParam->commonParam.bDisableDFR == VIDEO_TRUE)
        pEncOps->Disable_DynamicFrameRate(pMFCH264Handle->hMFCHandle, VIDEO_TRUE);

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

    pH264Enc->hMFCH264Handle.bConfiguredMFCSrc = OMX_TRUE;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE H264CodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc            = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc             = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_H264ENC_HANDLE     *pMFCH264Handle       = &pH264Enc->hMFCH264Handle;
    void                          *hMFCHandle           = pMFCH264Handle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort          = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pInputPort           = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pOutbufOps = pH264Enc->hMFCH264Handle.pOutbufOps;
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

#ifdef USE_SMALL_SECURE_MEMORY
        if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) {
            if (nOutBufSize > CUSTOM_LIMITED_DRM_OUTPUT_BUFFER_SIZE) {
                nOutBufSize = CUSTOM_LIMITED_DRM_OUTPUT_BUFFER_SIZE;
                Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] set limited output buffer size to MFC dst: %d",
                                                    pExynosComponent, __FUNCTION__, nOutBufSize);
            }
        }
#endif
    }

    /* set geometry for output (dst) */
    if (pOutbufOps->Set_Geometry) {
        /* output buffer info: only 2 config values needed */
        bufferConf.eCompressionFormat = VIDEO_CODING_AVC;
        bufferConf.nSizeImage = nOutBufSize;
        bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pOutputPort);

        if (pOutbufOps->Set_Geometry(pH264Enc->hMFCH264Handle.hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
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

    if (pOutbufOps->Setup(pH264Enc->hMFCH264Handle.hMFCHandle, nOutputBufferCnt) != VIDEO_ERROR_NONE) {
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

    pH264Enc->hMFCH264Handle.bConfiguredMFCDst = OMX_TRUE;

    if (H264CodecStart(pOMXComponent, OUTPUT_PORT_INDEX) != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to run output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

    int i;

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
    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nParamIndex %x", pExynosComponent, __FUNCTION__, (int)nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoAvc:
    {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = NULL;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcAVCComponent = &pH264Enc->AVCComponent[pDstAVCComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstAVCComponent) + nOffset,
                           ((char *)pSrcAVCComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_AVCTYPE) - nOffset);
    }
        break;
    case OMX_IndexParamVideoSliceFMO:
    {
        OMX_VIDEO_PARAM_AVCSLICEFMO *pDstSliceFmo = (OMX_VIDEO_PARAM_AVCSLICEFMO *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCSLICEFMO *pSrcSliceFmo = NULL;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstSliceFmo, sizeof(OMX_VIDEO_PARAM_AVCSLICEFMO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pSrcSliceFmo = &pH264Enc->AVCSliceFmo;

        Exynos_OSAL_Memcpy(((char *)pDstSliceFmo) + nOffset,
                           ((char *)pSrcSliceFmo) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_AVCSLICEFMO) - nOffset);
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

        if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
            Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_H264_DRM_ENC_ROLE);
        else
            Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_H264_ENC_ROLE);
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
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcAVCComponent = &pH264Enc->AVCComponent[pDstProfileLevel->nPortIndex];
        pDstProfileLevel->eProfile = pSrcAVCComponent->eProfile;
        pDstProfileLevel->eLevel = pSrcAVCComponent->eLevel;
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

        pSrcErrorCorrectionType = &pH264Enc->errorCorrectionType[OUTPUT_PORT_INDEX];
        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
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

        pQpRange->qpRangeI.nMinQP = pH264Enc->qpRangeI.nMinQP;
        pQpRange->qpRangeI.nMaxQP = pH264Enc->qpRangeI.nMaxQP;
        pQpRange->qpRangeP.nMinQP = pH264Enc->qpRangeP.nMinQP;
        pQpRange->qpRangeP.nMaxQP = pH264Enc->qpRangeP.nMaxQP;
        pQpRange->qpRangeB.nMinQP = pH264Enc->qpRangeB.nMinQP;
        pQpRange->qpRangeB.nMaxQP = pH264Enc->qpRangeB.nMaxQP;
    }
        break;
    case OMX_IndexParamVideoAVCEnableTemporalSVC:
    {
        EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC *pDstEnableTemporalSVC = (EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDstEnableTemporalSVC, sizeof(EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstEnableTemporalSVC->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstEnableTemporalSVC->bEnableTemporalSVC = pH264Enc->hMFCH264Handle.bTemporalSVC;
    }
        break;
    case OMX_IndexParamVideoEnableRoiInfo:
    {
        EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO *pDstEnableRoiInfo = (EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDstEnableRoiInfo, sizeof(EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstEnableRoiInfo->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstEnableRoiInfo->bEnableRoiInfo = pH264Enc->hMFCH264Handle.bRoiInfo;
    }
        break;
    case OMX_IndexParamVideoEnablePVC:
    {
        OMX_PARAM_U32TYPE *pEnablePVC = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pEnablePVC, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pEnablePVC->nU32 = pVideoEnc->bPVCMode;
    }
        break;
    case OMX_IndexParamVideoChromaQP:
    {
        OMX_VIDEO_PARAM_CHROMA_QP_OFFSET *pChromaQP = (OMX_VIDEO_PARAM_CHROMA_QP_OFFSET *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pChromaQP, sizeof(OMX_VIDEO_PARAM_CHROMA_QP_OFFSET));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pChromaQP->nCr = pH264Enc->chromaQPOffset.nCr;
        pChromaQP->nCb = pH264Enc->chromaQPOffset.nCb;
    }
        break;
    case OMX_IndexParamVideoDisableHBEncoding:
    {
        OMX_CONFIG_BOOLEANTYPE *pDisableHBEncoding = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDisableHBEncoding, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDisableHBEncoding->bEnabled = pH264Enc->bDisableHBEncoding;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamAndroidVideoTemporalLayering:
    {
        OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE *pTemporalLayering = (OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pTemporalLayering, sizeof(OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pTemporalLayering->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_FALSE) {
            pTemporalLayering->eSupportedPatterns = OMX_VIDEO_AndroidTemporalLayeringPatternNone;
            pTemporalLayering->nLayerCountMax     = 1;  /* not supported */
            pTemporalLayering->nBLayerCountMax    = 0;
            pTemporalLayering->ePattern           = OMX_VIDEO_AndroidTemporalLayeringPatternNone;
        } else {
            pTemporalLayering->eSupportedPatterns = OMX_VIDEO_AndroidTemporalLayeringPatternAndroid;

            if (pH264Enc->hMFCH264Handle.bTemporalSVC == OMX_TRUE) {  /* already enabled */
                pTemporalLayering->nLayerCountMax   = pH264Enc->nMaxTemporalLayerCount;
                pTemporalLayering->nBLayerCountMax  = pH264Enc->nMaxTemporalLayerCountForB;
                pTemporalLayering->ePattern         = OMX_VIDEO_AndroidTemporalLayeringPatternAndroid;
            } else {
                pTemporalLayering->nLayerCountMax   = OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS;
                pTemporalLayering->nBLayerCountMax  = OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS_FOR_B;
                pTemporalLayering->ePattern         = OMX_VIDEO_AndroidTemporalLayeringPatternNone;

                if (pH264Enc->bDisableHBEncoding == OMX_TRUE) {
                    pTemporalLayering->nBLayerCountMax = 0;
                }
            }
        }

        pTemporalLayering->nPLayerCountActual = pH264Enc->nTemporalLayerCount;
        pTemporalLayering->nBLayerCountActual = pH264Enc->nTemporalLayerCountForB;

        pTemporalLayering->bBitrateRatiosSpecified = pH264Enc->bUseTemporalLayerBitrateRatio;
        if (pTemporalLayering->bBitrateRatiosSpecified == OMX_TRUE) {
            for (i = 0; i < OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS; i++)
                pTemporalLayering->nBitrateRatios[i] = (pH264Enc->nTemporalLayerBitrateRatio[i] << 16);
        }
    }
        break;
#endif
    default:
#ifdef USE_SKYPE_HD
        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bSkypeSupport == VIDEO_TRUE) {
            ret = Exynos_H264Enc_GetParameter_SkypeHD(hComponent, nParamIndex, pComponentParameterStructure);
            if (ret == OMX_ErrorNone)
                goto EXIT;
        }
#endif
        ret = Exynos_OMX_VideoEncodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

    int i;

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
    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamVideoAvc:
    {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = NULL;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstAVCComponent = &pH264Enc->AVCComponent[pSrcAVCComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstAVCComponent) + nOffset,
                           ((char *)pSrcAVCComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_AVCTYPE) - nOffset);

        if (pDstAVCComponent->nBFrames > 2) {  /* 0 ~ 2 */
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] nBFrames(%d) is over a maximum value(2). it is limited to max",
                                                    pExynosComponent, __FUNCTION__, pDstAVCComponent->nBFrames);
            pDstAVCComponent->nBFrames = 2;
        }

        if (pDstAVCComponent->nRefFrames > 2) {  /* 1 ~ 2 */
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] nRefFrames(%d) is over a maximum value(2). it is limited to max",
                                                    pExynosComponent, __FUNCTION__, pDstAVCComponent->nRefFrames);
            pDstAVCComponent->nRefFrames = 2;
        } else if (pDstAVCComponent->nRefFrames == 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] nRefFrames(%d) is smaller than minimum value(1). it is limited to min",
                                                    pExynosComponent, __FUNCTION__, pDstAVCComponent->nRefFrames);
            pDstAVCComponent->nRefFrames = 1;
        }
    }
        break;
    case OMX_IndexParamVideoSliceFMO:
    {
        OMX_VIDEO_PARAM_AVCSLICEFMO *pSrcSliceFmo = (OMX_VIDEO_PARAM_AVCSLICEFMO *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCSLICEFMO *pDstSliceFmo = NULL;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcSliceFmo, sizeof(OMX_VIDEO_PARAM_AVCSLICEFMO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pDstSliceFmo = &pH264Enc->AVCSliceFmo;

        Exynos_OSAL_Memcpy(((char *)pDstSliceFmo) + nOffset,
                           ((char *)pSrcSliceFmo) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_AVCSLICEFMO) - nOffset);
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

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_H264_ENC_ROLE) ||
            !Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_H264_DRM_ENC_ROLE)) {
            pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
        } else {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCTYPE          *pDstAVCComponent = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstAVCComponent = &pH264Enc->AVCComponent[pSrcProfileLevel->nPortIndex];
        if (OMX_FALSE == CheckProfileLevelSupport(pExynosComponent, pSrcProfileLevel)) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pDstAVCComponent->eProfile = pSrcProfileLevel->eProfile;
        pDstAVCComponent->eLevel = pSrcProfileLevel->eLevel;
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

        pDstErrorCorrectionType = &pH264Enc->errorCorrectionType[OUTPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
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

        if ((pQpRange->qpRangeI.nMinQP > pQpRange->qpRangeI.nMaxQP) ||
            (pQpRange->qpRangeP.nMinQP > pQpRange->qpRangeP.nMaxQP) ||
            (pQpRange->qpRangeB.nMinQP > pQpRange->qpRangeB.nMaxQP)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: QP value is invalid(I[min:%d, max:%d], P[min:%d, max:%d], B[min:%d, max:%d])", __FUNCTION__,
                            pQpRange->qpRangeI.nMinQP, pQpRange->qpRangeI.nMaxQP,
                            pQpRange->qpRangeP.nMinQP, pQpRange->qpRangeP.nMaxQP,
                            pQpRange->qpRangeB.nMinQP, pQpRange->qpRangeB.nMaxQP);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pH264Enc->qpRangeI.nMinQP = pQpRange->qpRangeI.nMinQP;
        pH264Enc->qpRangeI.nMaxQP = pQpRange->qpRangeI.nMaxQP;
        pH264Enc->qpRangeP.nMinQP = pQpRange->qpRangeP.nMinQP;
        pH264Enc->qpRangeP.nMaxQP = pQpRange->qpRangeP.nMaxQP;
        pH264Enc->qpRangeB.nMinQP = pQpRange->qpRangeB.nMinQP;
        pH264Enc->qpRangeB.nMaxQP = pQpRange->qpRangeB.nMaxQP;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamPrependSPSPPSToIDR:
    {
        ret = Exynos_OSAL_SetPrependSPSPPSToIDR(pComponentParameterStructure, &(pH264Enc->hMFCH264Handle.bPrependSpsPpsToIdr));
    }
        break;
    case OMX_IndexParamAndroidVideoTemporalLayering:
    {
        OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE *pTemporalLayering = (OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pTemporalLayering, sizeof(OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pTemporalLayering->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pTemporalLayering->ePattern != OMX_VIDEO_AndroidTemporalLayeringPatternNone) {
            /* ENABLE */
            if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_FALSE) {
                Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] Temporal SVC is not supported", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorUnsupportedIndex;
                goto EXIT;
            }

            if (pTemporalLayering->nLayerCountMax > OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] total layer : Max value(%d) > supportable Max value(%d)",
                                        pExynosComponent, __FUNCTION__, pTemporalLayering->nLayerCountMax, OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            if (pTemporalLayering->nBLayerCountMax > OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS_FOR_B) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] B layer : Max value(%d) > supportable Max value(%d)",
                                        pExynosComponent, __FUNCTION__, pTemporalLayering->nBLayerCountMax, OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS_FOR_B);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            pH264Enc->hMFCH264Handle.bTemporalSVC = OMX_TRUE;
            pH264Enc->nMaxTemporalLayerCount      = pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual;
            pH264Enc->nMaxTemporalLayerCountForB  = pTemporalLayering->nBLayerCountActual;
            pH264Enc->nTemporalLayerCount         = pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual;
            pH264Enc->nTemporalLayerCountForB     = pTemporalLayering->nBLayerCountActual;

            if (pH264Enc->bDisableHBEncoding) {
                pH264Enc->nMaxTemporalLayerCountForB  = 0;
                pH264Enc->nTemporalLayerCountForB  = 0;
            }

            pH264Enc->bUseTemporalLayerBitrateRatio = pTemporalLayering->bBitrateRatiosSpecified;
            if (pH264Enc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
                for (i = 0; i < (int)pH264Enc->nMaxTemporalLayerCount; i++)
                    pH264Enc->nTemporalLayerBitrateRatio[i] = (pTemporalLayering->nBitrateRatios[i] >> 16);
            }

            /* HIGH_SPEED_ENCODING */
            if (pH264Enc->nTemporalLayerCountForB > 0) {
                int minRequiredInputs = pow(2, pH264Enc->nTemporalLayerCount - 1);
                if (minRequiredInputs > pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.nBufferCountActual) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Temporal Layering with B-frame, increase InputBufferNum(%d)", pExynosComponent, __FUNCTION__, minRequiredInputs);
                    pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.nBufferCountActual = minRequiredInputs;
                }
            }

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Max(%d), B-Max(%d), layer(%d), B-layer(%d)", pExynosComponent, __FUNCTION__,
                                                    pH264Enc->nMaxTemporalLayerCount, pH264Enc->nMaxTemporalLayerCountForB,
                                                    pH264Enc->nTemporalLayerCount, pH264Enc->nTemporalLayerCountForB);
        } else {
            /* DISABLE */
            pH264Enc->hMFCH264Handle.bTemporalSVC   = OMX_FALSE;
            pH264Enc->nMaxTemporalLayerCount        = 0;
            pH264Enc->nMaxTemporalLayerCountForB    = 0;
            pH264Enc->nTemporalLayerCount           = 0;
            pH264Enc->nTemporalLayerCountForB       = 0;
            pH264Enc->bUseTemporalLayerBitrateRatio = OMX_FALSE;
            Exynos_OSAL_Memset(pH264Enc->nTemporalLayerBitrateRatio, 0, sizeof(pH264Enc->nTemporalLayerBitrateRatio));
        }
    }
        break;
#endif
    case OMX_IndexParamVideoAVCEnableTemporalSVC:
    {
        EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC   *pSrcEnableTemporalSVC  = (EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC *)pComponentParameterStructure;
        OMX_PARAM_PORTDEFINITIONTYPE                *pPortDef               = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].portDefinition);
        int i;

        ret = Exynos_OMX_Check_SizeVersion(pSrcEnableTemporalSVC, sizeof(EXYNOS_OMX_VIDEO_PARAM_ENABLE_TEMPORALSVC));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcEnableTemporalSVC->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_FALSE) &&
            (pSrcEnableTemporalSVC->bEnableTemporalSVC == OMX_TRUE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Temporal SVC is not supported", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        if ((pSrcEnableTemporalSVC->bEnableTemporalSVC == OMX_TRUE) &&
            (pH264Enc->hMFCH264Handle.bTemporalSVC == OMX_FALSE)) {
            /* ENABLE : not initialized yet */
            pH264Enc->hMFCH264Handle.bTemporalSVC   = OMX_TRUE;
            pH264Enc->nMaxTemporalLayerCount        = OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS;
            pH264Enc->nMaxTemporalLayerCountForB    = OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS_FOR_B;
            pH264Enc->nTemporalLayerCount           = 1;
            pH264Enc->nTemporalLayerCountForB       = 0;
            pH264Enc->bUseTemporalLayerBitrateRatio = OMX_TRUE;
            pH264Enc->nTemporalLayerBitrateRatio[0] = pPortDef->format.video.nBitrate;
        } else if (pSrcEnableTemporalSVC->bEnableTemporalSVC == OMX_FALSE) {
            /* DISABLE */
            pH264Enc->hMFCH264Handle.bTemporalSVC   = OMX_FALSE;  /* DISABLE */
            pH264Enc->nMaxTemporalLayerCount        = 0;
            pH264Enc->nMaxTemporalLayerCountForB    = 0;
            pH264Enc->nTemporalLayerCount           = 0;
            pH264Enc->nTemporalLayerCountForB       = 0;
            pH264Enc->bUseTemporalLayerBitrateRatio = OMX_FALSE;
            Exynos_OSAL_Memset(pH264Enc->nTemporalLayerBitrateRatio, 0, sizeof(pH264Enc->nTemporalLayerBitrateRatio));
        }
    }
        break;
    case OMX_IndexParamVideoEnableRoiInfo:
    {
        EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO *pSrcEnableRoiInfo = (EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pSrcEnableRoiInfo, sizeof(EXYNOS_OMX_VIDEO_PARAM_ENABLE_ROIINFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcEnableRoiInfo->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bRoiInfoSupport == VIDEO_FALSE) &&
            (pSrcEnableRoiInfo->bEnableRoiInfo == OMX_TRUE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Roi Info is not supported", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        pH264Enc->hMFCH264Handle.bRoiInfo = pSrcEnableRoiInfo->bEnableRoiInfo;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE    *pPortDef       = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                          nPortIndex     = pPortDef->nPortIndex;
        EXYNOS_OMX_BASEPORT             *pExynosPort    = &pExynosComponent->pExynosPort[nPortIndex];;
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

            if (pVideoEnc->bFirstInput == OMX_FALSE)
                pVideoEnc->bEncDRC = OMX_TRUE;

            pVideoEnc->bFirstInput = OMX_TRUE;
            pH264Enc->hMFCH264Handle.bConfiguredMFCSrc = OMX_FALSE;

            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] pOutputPort->portDefinition.nBufferSize: %d", pExynosComponent, __FUNCTION__, pExynosPort->portDefinition.nBufferSize);
        } else if (nPortIndex == OUTPUT_PORT_INDEX) {
            pH264Enc->hMFCH264Handle.bConfiguredMFCDst = OMX_FALSE;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoEnablePVC:
    {
        OMX_PARAM_U32TYPE *pEnablePVC  = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pEnablePVC, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bPVCSupport == VIDEO_FALSE) &&
            (pEnablePVC->nU32 != 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] PVC mode is not supported", pExynosComponent, __FUNCTION__);
        }

        pVideoEnc->bPVCMode = (pEnablePVC->nU32 != 0)? OMX_TRUE:OMX_FALSE;
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
            if (pH264Enc->hMFCH264Handle.bSkypeBitrate == OMX_FALSE) {
                pPortDef = &(pExynosComponent->pExynosPort[nPortIndex].portDefinition);
                pVideoEnc->eControlRate[nPortIndex] = pVideoBitrate->eControlRate;
                pPortDef->format.video.nBitrate = pVideoBitrate->nTargetBitrate;
            }
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoChromaQP:
    {
        OMX_VIDEO_PARAM_CHROMA_QP_OFFSET *pChromaQP  = (OMX_VIDEO_PARAM_CHROMA_QP_OFFSET *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pChromaQP, sizeof(OMX_VIDEO_PARAM_CHROMA_QP_OFFSET));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pChromaQP->nCr < -12) || (pChromaQP->nCr > 12)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] ChromaQP Offset(Cr) 0x%x is invalid [min:-12, max:12]", pExynosComponent, __FUNCTION__, pChromaQP->nCr);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        if ((pChromaQP->nCb < -12) || (pChromaQP->nCb > 12)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] ChromaQP Offset(Cb) 0x%x is invalid [min:-12, max:12]", pExynosComponent, __FUNCTION__, pChromaQP->nCb);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pH264Enc->chromaQPOffset.nCr = pChromaQP->nCr;
        pH264Enc->chromaQPOffset.nCb = pChromaQP->nCb;
    }
        break;
    case OMX_IndexParamVideoDisableHBEncoding:
    {
        OMX_CONFIG_BOOLEANTYPE *pDisableHBEncoding = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pDisableHBEncoding, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pH264Enc->bDisableHBEncoding = pDisableHBEncoding->bEnabled;
    }
        break;
    default:
#ifdef USE_SKYPE_HD
        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bSkypeSupport == VIDEO_TRUE) {
            ret = Exynos_H264Enc_SetParameter_SkypeHD(hComponent, nIndex, pComponentParameterStructure);
            if (ret == OMX_ErrorNone)
                goto EXIT;
        }
#endif
        ret = Exynos_OMX_VideoEncodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_GetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

    int i;

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
    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigVideoAVCIntraPeriod:
    {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pAVCIntraPeriod = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentConfigStructure;
        OMX_U32                          portIndex       = pAVCIntraPeriod->nPortIndex;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pAVCIntraPeriod->nIDRPeriod = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames + 1;
            pAVCIntraPeriod->nPFrames = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames;
        }
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

        pQpRange->qpRangeI.nMinQP = pH264Enc->qpRangeI.nMinQP;
        pQpRange->qpRangeI.nMaxQP = pH264Enc->qpRangeI.nMaxQP;
        pQpRange->qpRangeP.nMinQP = pH264Enc->qpRangeP.nMinQP;
        pQpRange->qpRangeP.nMaxQP = pH264Enc->qpRangeP.nMaxQP;
        pQpRange->qpRangeB.nMinQP = pH264Enc->qpRangeB.nMinQP;
        pQpRange->qpRangeB.nMaxQP = pH264Enc->qpRangeB.nMaxQP;
    }
        break;
    case OMX_IndexConfigVideoTemporalSVC:
    {
        EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *pDstTemporalSVC = (EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *)pComponentConfigStructure;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstTemporalSVC, sizeof(EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstTemporalSVC->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstTemporalSVC->nKeyFrameInterval   = pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames + 1;
        pDstTemporalSVC->nMinQuantizer       = pH264Enc->qpRangeI.nMinQP;
        pDstTemporalSVC->nMaxQuantizer       = pH264Enc->qpRangeI.nMaxQP;
        pDstTemporalSVC->nTemporalLayerCount = pH264Enc->nTemporalLayerCount;
        for (i = 0; i < OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS; i++)
            pDstTemporalSVC->nTemporalLayerBitrateRatio[i] = pH264Enc->nTemporalLayerBitrateRatio[i];
    }
        break;
    default:
#ifdef USE_SKYPE_HD
        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bSkypeSupport == VIDEO_TRUE) {
            ret = Exynos_H264Enc_GetConfig_SkypeHD(hComponent, nIndex, pComponentConfigStructure);
            if (ret == OMX_ErrorNone)
                goto EXIT;
        }
#endif
        ret = Exynos_OMX_VideoEncodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_SetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

    int i;

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
    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigVideoIntraPeriod:
    {
        OMX_U32 nPFrames = (*((OMX_U32 *)pComponentConfigStructure)) - 1;
        pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames = nPFrames;

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexConfigVideoAVCIntraPeriod:
    {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pAVCIntraPeriod = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentConfigStructure;
        OMX_U32                          portIndex       = pAVCIntraPeriod->nPortIndex;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            if (pAVCIntraPeriod->nIDRPeriod == (pAVCIntraPeriod->nPFrames + 1))
                pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames = pAVCIntraPeriod->nPFrames;
            else {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
        }
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
            (pQpRange->qpRangeP.nMinQP > pQpRange->qpRangeP.nMaxQP) ||
            (pQpRange->qpRangeB.nMinQP > pQpRange->qpRangeB.nMaxQP)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] QP value is invalid(I[min:%d, max:%d], P[min:%d, max:%d], B[min:%d, max:%d])",
                            pExynosComponent, __FUNCTION__,
                            pQpRange->qpRangeI.nMinQP, pQpRange->qpRangeI.nMaxQP,
                            pQpRange->qpRangeP.nMinQP, pQpRange->qpRangeP.nMaxQP,
                            pQpRange->qpRangeB.nMinQP, pQpRange->qpRangeB.nMaxQP);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bQpRangePBSupport == VIDEO_FALSE)
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] only I-frame's QP range is applied", pExynosComponent, __FUNCTION__);

        pH264Enc->qpRangeI.nMinQP = pQpRange->qpRangeI.nMinQP;
        pH264Enc->qpRangeI.nMaxQP = pQpRange->qpRangeI.nMaxQP;
        pH264Enc->qpRangeP.nMinQP = pQpRange->qpRangeP.nMinQP;
        pH264Enc->qpRangeP.nMaxQP = pQpRange->qpRangeP.nMaxQP;
        pH264Enc->qpRangeB.nMinQP = pQpRange->qpRangeB.nMinQP;
        pH264Enc->qpRangeB.nMaxQP = pQpRange->qpRangeB.nMaxQP;
    }
        break;
    case OMX_IndexConfigVideoTemporalSVC:
    {
        EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *pSrcTemporalSVC = (EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *)pComponentConfigStructure;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcTemporalSVC, sizeof(EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcTemporalSVC->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pH264Enc->hMFCH264Handle.bTemporalSVC == OMX_FALSE) ||
            (pSrcTemporalSVC->nTemporalLayerCount > OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS)) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames = pSrcTemporalSVC->nKeyFrameInterval - 1;

        pH264Enc->qpRangeI.nMinQP = pSrcTemporalSVC->nMinQuantizer;
        pH264Enc->qpRangeI.nMaxQP = pSrcTemporalSVC->nMaxQuantizer;
        pH264Enc->qpRangeP.nMinQP = pSrcTemporalSVC->nMinQuantizer;
        pH264Enc->qpRangeP.nMaxQP = pSrcTemporalSVC->nMaxQuantizer;
        pH264Enc->qpRangeB.nMinQP = pSrcTemporalSVC->nMinQuantizer;
        pH264Enc->qpRangeB.nMaxQP = pSrcTemporalSVC->nMaxQuantizer;

        pH264Enc->nTemporalLayerCount = pSrcTemporalSVC->nTemporalLayerCount;
        for (i = 0; i < OMX_VIDEO_MAX_AVC_TEMPORAL_LAYERS; i++)
            pH264Enc->nTemporalLayerBitrateRatio[i] = pSrcTemporalSVC->nTemporalLayerBitrateRatio[i];

    }
        break;
    case OMX_IndexConfigVideoRoiInfo:
    {
        EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *pRoiInfo = (EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *)pComponentConfigStructure;

        if (pRoiInfo->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pH264Enc->hMFCH264Handle.bRoiInfo == OMX_FALSE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] RoiInfo is not enabled", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if ((pRoiInfo->bUseRoiInfo == OMX_TRUE) &&
            ((pRoiInfo->nRoiMBInfoSize <= 0) || (pRoiInfo->pRoiMBInfo == NULL))) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] parameter is invalid : nRoiMBInfoSize(%d)/pRoiMBInfo(%p)",
                                pExynosComponent, __FUNCTION__, pRoiInfo->nRoiMBInfoSize, pRoiInfo->pRoiMBInfo);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexConfigIFrameRatio:
    {
        OMX_PARAM_U32TYPE *pIFrameRatio = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pIFrameRatio, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pIFrameRatio->nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bIFrameRatioSupport == VIDEO_FALSE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] I-Frame ratio feature is not supported", pExynosComponent, __FUNCTION__);
            ret = (OMX_ERRORTYPE)OMX_ErrorNoneExpiration;
            goto EXIT;
        }
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigAndroidVideoTemporalLayering:
    {
        OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE *pTemporalLayering = (OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE *)pComponentConfigStructure;

        ret = Exynos_OMX_Check_SizeVersion(pTemporalLayering, sizeof(OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pTemporalLayering->nPortIndex != OUTPUT_PORT_INDEX) ||
            (pH264Enc->hMFCH264Handle.bTemporalSVC == OMX_FALSE)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pTemporalLayering->ePattern == OMX_VIDEO_AndroidTemporalLayeringPatternNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can not disable TemporalSVC while encoding", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if (((pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual) > pH264Enc->nMaxTemporalLayerCount) ||
            ((pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual) == 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] layer count is invalid(MAX(%d), layer(%d), B-layer(%d))",
                                    pExynosComponent, __FUNCTION__,
                                    pH264Enc->nMaxTemporalLayerCount,
                                    pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual,
                                    pTemporalLayering->nBLayerCountActual);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pH264Enc->nTemporalLayerCount     = pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual;
        if (pH264Enc->bDisableHBEncoding == OMX_TRUE) {
            pH264Enc->nTemporalLayerCountForB = 0;
        } else {
            pH264Enc->nTemporalLayerCountForB = pTemporalLayering->nBLayerCountActual;
        }

        pH264Enc->bUseTemporalLayerBitrateRatio = pTemporalLayering->bBitrateRatiosSpecified;
        if (pH264Enc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
            for (i = 0; i < (int)pH264Enc->nMaxTemporalLayerCount; i++)
                pH264Enc->nTemporalLayerBitrateRatio[i] = (pTemporalLayering->nBitrateRatios[i] >> 16);
        } else {
            Exynos_OSAL_Memset(pH264Enc->nTemporalLayerBitrateRatio, 0, sizeof(pH264Enc->nTemporalLayerBitrateRatio));
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Max(%d), layer(%d), B-layer(%d)", pExynosComponent, __FUNCTION__,
                                               pH264Enc->nMaxTemporalLayerCount, pH264Enc->nTemporalLayerCount, pH264Enc->nTemporalLayerCountForB);
    }
        break;
#endif
    default:
#ifdef USE_SKYPE_HD
        if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bSkypeSupport == VIDEO_TRUE) {
            ret = Exynos_H264Enc_SetConfig_SkypeHD(hComponent, nIndex, pComponentConfigStructure);
            if (ret == OMX_ErrorNone)
                goto EXIT;
        }
#endif
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

OMX_ERRORTYPE Exynos_H264Enc_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

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

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_TEMPORALSVC) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigVideoTemporalSVC;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_VIDEO_AVC_ENABLE_TEMPORALSVC) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamVideoAVCEnableTemporalSVC;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_ROIINFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigVideoRoiInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_VIDEO_ENABLE_ROIINFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamVideoEnableRoiInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ENABLE_PVC) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamVideoEnablePVC;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_IFRAME_RATIO) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigIFrameRatio;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_PREPEND_SPSPPS_TO_IDR) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamPrependSPSPPSToIDR;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    ret = Exynos_OMX_VideoEncodeGetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_ComponentRoleEnum(
    OMX_HANDLETYPE   hComponent,
    OMX_U8          *cRole,
    OMX_U32          nIndex)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
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

    if (nIndex == (MAX_COMPONENT_ROLE_NUM-1)) {
        if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
            Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_H264_DRM_ENC_ROLE);
        else
            Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_H264_ENC_ROLE);

        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_H264Enc_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_H264ENC_HANDLE       *pMFCH264Handle     = &pH264Enc->hMFCH264Handle;
    OMX_COLOR_FORMATTYPE             eColorFormat       = pInputPort->portDefinition.format.video.eColorFormat;

    ExynosVideoInstInfo *pVideoInstInfo = &(pH264Enc->hMFCH264Handle.videoInstInfo);

    CSC_METHOD csc_method = CSC_METHOD_SW;

    FunctionIn();

    pH264Enc->hMFCH264Handle.bConfiguredMFCSrc = OMX_FALSE;
    pH264Enc->hMFCH264Handle.bConfiguredMFCDst = OMX_FALSE;
    pVideoEnc->bFirstInput  = OMX_TRUE;
    pVideoEnc->bFirstOutput = OMX_FALSE;
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

    /* H.264 Codec Open */
    ret = H264CodecOpen(pH264Enc, pVideoInstInfo);
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

    pH264Enc->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pH264Enc->hSourceStartEvent);
    pH264Enc->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pH264Enc->hDestinationInStartEvent);
    Exynos_OSAL_SignalCreate(&pH264Enc->hDestinationOutStartEvent);

    Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);
    INIT_ARRAY_TO_VAL(pExynosComponent->timeStamp, DEFAULT_TIMESTAMP_VAL, MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pH264Enc->hMFCH264Handle.indexTimestamp       = 0;
    pH264Enc->hMFCH264Handle.outputIndexTimestamp = 0;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    Exynos_OSAL_QueueCreate(&pH264Enc->bypassBufferInfoQ, QUEUE_ELEMENTS);

#ifdef USE_CSC_HW
    csc_method = CSC_METHOD_HW;
#endif
    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) {
        pVideoEnc->csc_handle = csc_init(CSC_METHOD_HW);
        csc_set_hw_property(pVideoEnc->csc_handle, CSC_HW_PROPERTY_FIXED_NODE, 2);
        csc_set_hw_property(pVideoEnc->csc_handle, CSC_HW_PROPERTY_MODE_DRM, 1);
    } else {
        pVideoEnc->csc_handle = csc_init(csc_method);
    }
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
OMX_ERRORTYPE Exynos_H264Enc_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
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
            EXYNOS_H264ENC_HANDLE *pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;


            if (pVideoEnc->csc_handle != NULL) {
                csc_deinit(pVideoEnc->csc_handle);
                pVideoEnc->csc_handle = NULL;
            }

            if (pH264Enc != NULL) {
                Exynos_OSAL_QueueTerminate(&pH264Enc->bypassBufferInfoQ);

                Exynos_OSAL_SignalTerminate(pH264Enc->hDestinationInStartEvent);
                pH264Enc->hDestinationInStartEvent = NULL;
                Exynos_OSAL_SignalTerminate(pH264Enc->hDestinationOutStartEvent);
                pH264Enc->hDestinationOutStartEvent = NULL;
                pH264Enc->bDestinationStart = OMX_FALSE;

                Exynos_OSAL_SignalTerminate(pH264Enc->hSourceStartEvent);
                pH264Enc->hSourceStartEvent = NULL;
                pH264Enc->bSourceStart = OMX_FALSE;
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

            if (pH264Enc != NULL) {
                H264CodecClose(pH264Enc);
            }
        }
    }

    Exynos_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_SrcIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle       = pH264Enc->hMFCH264Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE           inputColorFormat = OMX_COLOR_FormatUnused;

    ExynosVideoEncOps       *pEncOps     = pH264Enc->hMFCH264Handle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps   = pH264Enc->hMFCH264Handle.pInbufOps;
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
            Exynos_OSAL_Performance(pVideoEnc->pPerfHandle, pH264Enc->hMFCH264Handle.indexTimestamp, fps);
    }
#endif

    if (pH264Enc->hMFCH264Handle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = H264CodecSrcSetup(pOMXComponent, pSrcInputData);
        if ((ret != OMX_ErrorNone) ||
            ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to H264CodecSrcSetup(0x%x)",
                                                pExynosComponent, __FUNCTION__);
            goto EXIT;
        }
    }
    if (pH264Enc->hMFCH264Handle.bConfiguredMFCDst == OMX_FALSE) {
        ret = H264CodecDstSetup(pOMXComponent);

        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to H264CodecDstSetup(0x%x)",
                                                pExynosComponent, __FUNCTION__);
            goto EXIT;
        }
    }

#ifdef USE_SKYPE_HD
    if ((pH264Enc->hMFCH264Handle.bEnableSkypeHD == OMX_TRUE) &&
        (pSrcInputData->dataLen > 0)) {
        do {
            /* process dynamic control until meet input trigger.
             * if there are several input triggers, do process until empty.
             */
            ret = Change_H264Enc_Param(pExynosComponent);
        } while (ret == OMX_ErrorNone);

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] all config messages were handled", pExynosComponent, __FUNCTION__);
    } else
#endif
    {
        nConfigCnt = Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ);
        if (nConfigCnt > 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] has config message(%d)", pExynosComponent, __FUNCTION__, nConfigCnt);
            while (Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ) > 0) {
                Change_H264Enc_Param(pExynosComponent);
            }
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] all config messages were handled", pExynosComponent, __FUNCTION__);
        }
    }

    /* DRC on Executing state through OMX_IndexConfigCommonOutputSize */
    if (pVideoEnc->bEncDRC == OMX_TRUE) {
        ret = H264CodecUpdateResolution(pOMXComponent);
        pVideoEnc->bEncDRC = OMX_FALSE;
        goto EXIT;
    }

    if ((pSrcInputData->dataLen > 0) ||
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        pExynosComponent->timeStamp[pH264Enc->hMFCH264Handle.indexTimestamp]            = pSrcInputData->timeStamp;
        pExynosComponent->bTimestampSlotUsed[pH264Enc->hMFCH264Handle.indexTimestamp]   = OMX_TRUE;
        pExynosComponent->nFlags[pH264Enc->hMFCH264Handle.indexTimestamp]               = pSrcInputData->nFlags;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input / buffer header(%p), nFlags: 0x%x, timestamp %lld us (%.2f secs), Tag: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        pSrcInputData->bufferHeader, pSrcInputData->nFlags,
                                                        pSrcInputData->timeStamp, (double)(pSrcInputData->timeStamp / 1E6),
                                                        pH264Enc->hMFCH264Handle.indexTimestamp);
        pEncOps->Set_FrameTag(hMFCHandle, pH264Enc->hMFCH264Handle.indexTimestamp);
        pH264Enc->hMFCH264Handle.indexTimestamp++;
        pH264Enc->hMFCH264Handle.indexTimestamp %= MAX_TIMESTAMP;

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

                if (pH264Enc->hMFCH264Handle.bWeightedPrediction == OMX_TRUE) {
                    if (pVideoMeta->eType & VIDEO_INFO_TYPE_YSUM_DATA) {
                        codecReturn = pEncOps->Set_YSumData(hMFCHandle, pVideoMeta->data.enc.sYsumData.high,
                                                            pVideoMeta->data.enc.sYsumData.low);
                        if (codecReturn != VIDEO_ERROR_NONE)
                            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Failed to Set_YSumData()", pExynosComponent, __FUNCTION__);
                    }
                }

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

        if ((pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bDropControlSupport == VIDEO_TRUE) &&
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

        H264CodecStart(pOMXComponent, INPUT_PORT_INDEX);
        if (pH264Enc->bSourceStart == OMX_FALSE) {
            pH264Enc->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pH264Enc->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        if ((pH264Enc->bDestinationStart == OMX_FALSE) &&
            (pH264Enc->hMFCH264Handle.bConfiguredMFCDst == OMX_TRUE)) {
            pH264Enc->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pH264Enc->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pH264Enc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_SrcOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                            *hMFCHandle         = pH264Enc->hMFCH264Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pInbufOps      = pH264Enc->hMFCH264Handle.pInbufOps;
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

OMX_ERRORTYPE Exynos_H264Enc_DstIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc            = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc             = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle           = pH264Enc->hMFCH264Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort          = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pInputPort           = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pOutbufOps  = pH264Enc->hMFCH264Handle.pOutbufOps;
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

#ifdef USE_SMALL_SECURE_MEMORY
        if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC) {
            if (nAllocLen[0] > CUSTOM_LIMITED_DRM_OUTPUT_BUFFER_SIZE) {
                nAllocLen[0] = CUSTOM_LIMITED_DRM_OUTPUT_BUFFER_SIZE;
            }
        }
#endif
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] output / buffer header(%p)",
                                        pExynosComponent, __FUNCTION__, pDstInputData->bufferHeader);

    codecReturn = pOutbufOps->ExtensionEnqueue(hMFCHandle,
                                (void **)pDstInputData->buffer.addr,
                                (unsigned long *)pDstInputData->buffer.fd,
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

    H264CodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_H264Enc_DstOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    void                          *hMFCHandle       = pH264Enc->hMFCH264Handle.hMFCHandle;

    ExynosVideoEncOps          *pEncOps        = pH264Enc->hMFCH264Handle.pEncOps;
    ExynosVideoEncBufferOps    *pOutbufOps     = pH264Enc->hMFCH264Handle.pOutbufOps;
    ExynosVideoBuffer          *pVideoBuffer   = NULL;
    ExynosVideoBuffer           videoBuffer;
    ExynosVideoErrorType        codecReturn    = VIDEO_ERROR_NONE;

    OMX_S32 indexTimestamp = 0;

    FunctionIn();

    if (pH264Enc->bDestinationStart == OMX_FALSE) {
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

    if (pVideoEnc->bFirstOutput == OMX_FALSE) {
        if (pExynosComponent->codecType != HW_VIDEO_ENC_SECURE_CODEC) {
            OMX_U8 *p = NULL;
            OMX_U32 iSpsSize = 0;
            OMX_U32 iPpsSize = 0;

            /* start header return */
            /* Calculate sps/pps size if needed */
            p = FindDelimiter((OMX_U8 *)((char *)pDstOutputData->buffer.addr[0] + 4),
                                pDstOutputData->dataLen - 4);

            iSpsSize = (unsigned int)(unsigned long)((p - (OMX_U8 *)pDstOutputData->buffer.addr[0]));
            pH264Enc->hMFCH264Handle.headerData.pHeaderSPS =
                (OMX_PTR)pDstOutputData->buffer.addr[0];
            pH264Enc->hMFCH264Handle.headerData.SPSLen = iSpsSize;

            iPpsSize = pDstOutputData->dataLen - iSpsSize;
            pH264Enc->hMFCH264Handle.headerData.pHeaderPPS =
                (OMX_U8 *)pDstOutputData->buffer.addr[0] + iSpsSize;
            pH264Enc->hMFCH264Handle.headerData.PPSLen = iPpsSize;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] codec specific data is generated", pExynosComponent, __FUNCTION__);
        pDstOutputData->timeStamp = 0;
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
        pVideoEnc->bFirstOutput = OMX_TRUE;
    } else {
        indexTimestamp = pEncOps->Get_FrameTag(pH264Enc->hMFCH264Handle.hMFCHandle);
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
                indexTimestamp              = pH264Enc->hMFCH264Handle.outputIndexTimestamp;
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
                                                  indexTimestamp, pH264Enc->hMFCH264Handle.outputIndexTimestamp);
            indexTimestamp = pH264Enc->hMFCH264Handle.outputIndexTimestamp;
        } else {
            pH264Enc->hMFCH264Handle.outputIndexTimestamp = indexTimestamp;
        }

        if (pH264Enc->AVCComponent[OUTPUT_PORT_INDEX].nBFrames > 0) {
            if ((pExynosComponent->nFlags[indexTimestamp] & OMX_BUFFERFLAG_EOS) &&
                (pVideoBuffer->frameType == VIDEO_FRAME_P)) {
                /* move an EOS flag to previous slot
                 * B1 B2 P(EOS) -> P B1 B2(EOS)
                 * B1 P(EOS) -> P B1(EOS)
                 */
                int index = ((indexTimestamp - 1) < 0)? (MAX_TIMESTAMP - 1):(indexTimestamp - 1);

                if (pExynosComponent->bTimestampSlotUsed[index] == OMX_TRUE) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] EOS flag is moved to %d from %d",
                                                            pExynosComponent, __FUNCTION__,
                                                            index, indexTimestamp);
                    pExynosComponent->nFlags[indexTimestamp] &= (~OMX_BUFFERFLAG_EOS);
                    pExynosComponent->nFlags[index] |= OMX_BUFFERFLAG_EOS;
                }
            }
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

OMX_ERRORTYPE Exynos_H264Enc_srcInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *pInputPort        = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

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

    ret = Exynos_H264Enc_SrcIn(pOMXComponent, pSrcInputData);
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

OMX_ERRORTYPE Exynos_H264Enc_srcOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
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

    if ((pH264Enc->bSourceStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pInputPort))) {
        Exynos_OSAL_SignalWait(pH264Enc->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get SourceStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pH264Enc->hSourceStartEvent);
    }

    ret = Exynos_H264Enc_SrcOut(pOMXComponent, pSrcOutputData);
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

OMX_ERRORTYPE Exynos_H264Enc_dstInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
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

    if ((pH264Enc->bDestinationStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
        Exynos_OSAL_SignalWait(pH264Enc->hDestinationInStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationInStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pH264Enc->hDestinationInStartEvent);
    }

    if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
        if (Exynos_OSAL_GetElemNum(&pH264Enc->bypassBufferInfoQ) > 0) {
            BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Dequeue(&pH264Enc->bypassBufferInfoQ);
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

    if (pH264Enc->hMFCH264Handle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_H264Enc_DstIn(pOMXComponent, pDstInputData);
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

OMX_ERRORTYPE Exynos_H264Enc_dstOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
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

    if ((pH264Enc->bDestinationStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
        Exynos_OSAL_SignalWait(pH264Enc->hDestinationOutStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationOutStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pH264Enc->hDestinationOutStartEvent);
    }

    if (pOutputPort->bufferProcessType & BUFFER_COPY) {
        if (Exynos_OSAL_GetElemNum(&pH264Enc->bypassBufferInfoQ) > 0) {
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

            pBufferInfo = Exynos_OSAL_Dequeue(&pH264Enc->bypassBufferInfoQ);
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

    ret = Exynos_H264Enc_DstOut(pOMXComponent, pDstOutputData);
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
    EXYNOS_H264ENC_HANDLE         *pH264Enc         = NULL;
    OMX_BOOL                       bSecureMode      = OMX_FALSE;
    int i = 0;

    Exynos_OSAL_Get_Log_Property(); // For debuging
    FunctionIn();

    if ((hComponent == NULL) ||
        (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_H264_ENC, componentName) == 0) {
        bSecureMode = OMX_FALSE;
    } else if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_H264_DRM_ENC, componentName) == 0) {
        bSecureMode = OMX_TRUE;
    } else {
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
    pExynosComponent->codecType = (bSecureMode == OMX_TRUE)? HW_VIDEO_ENC_SECURE_CODEC:HW_VIDEO_ENC_CODEC;

    pExynosComponent->componentName = (OMX_STRING)Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pExynosComponent->componentName == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);

    pH264Enc = Exynos_OSAL_Malloc(sizeof(EXYNOS_H264ENC_HANDLE));
    if (pH264Enc == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pH264Enc, 0, sizeof(EXYNOS_H264ENC_HANDLE));

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoEnc->hCodecHandle = (OMX_HANDLETYPE)pH264Enc;
    pH264Enc->qpRangeI.nMinQP = 5;
    pH264Enc->qpRangeI.nMaxQP = 50;
    pH264Enc->qpRangeP.nMinQP = 5;
    pH264Enc->qpRangeP.nMaxQP = 50;
    pH264Enc->qpRangeB.nMinQP = 5;
    pH264Enc->qpRangeB.nMaxQP = 50;

    pVideoEnc->quantization.nQpI = 29;
    pVideoEnc->quantization.nQpP = 30;
    pVideoEnc->quantization.nQpB = 32;

    pH264Enc->chromaQPOffset.nCr = 0;
    pH264Enc->chromaQPOffset.nCb = 0;

    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_H264_DRM_ENC);
    else
        Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_H264_ENC);

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
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_COPY;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_MULTIPLE;

#ifdef USE_SINGLE_PLANE_IN_DRM
    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        pExynosPort->ePlaneType = PLANE_SINGLE;
#endif

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/avc");
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_SINGLE;

    for(i = 0; i < ALL_PORT_NUM; i++) {
        INIT_SET_SIZE_VERSION(&pH264Enc->AVCComponent[i], OMX_VIDEO_PARAM_AVCTYPE);
        pH264Enc->AVCComponent[i].nPortIndex = i;
        pH264Enc->AVCComponent[i].eProfile   = OMX_VIDEO_AVCProfileBaseline;
        pH264Enc->AVCComponent[i].eLevel     = OMX_VIDEO_AVCLevel31;

        pH264Enc->AVCComponent[i].nPFrames      = 29;
        pH264Enc->AVCComponent[i].nBFrames      = 0;
        pH264Enc->AVCComponent[i].nRefFrames    = 1;
    }

    pH264Enc->hMFCH264Handle.bTemporalSVC   = OMX_FALSE;
    pH264Enc->nMaxTemporalLayerCount        = 0;
    pH264Enc->nTemporalLayerCount           = 0;
    pH264Enc->bUseTemporalLayerBitrateRatio = 0;
    Exynos_OSAL_Memset(pH264Enc->nTemporalLayerBitrateRatio, 0, sizeof(pH264Enc->nTemporalLayerBitrateRatio));

    pOMXComponent->GetParameter      = &Exynos_H264Enc_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_H264Enc_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_H264Enc_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_H264Enc_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_H264Enc_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_H264Enc_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_H264Enc_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_H264Enc_Terminate;

    pVideoEnc->exynos_codec_srcInputProcess  = &Exynos_H264Enc_srcInputBufferProcess;
    pVideoEnc->exynos_codec_srcOutputProcess = &Exynos_H264Enc_srcOutputBufferProcess;
    pVideoEnc->exynos_codec_dstInputProcess  = &Exynos_H264Enc_dstInputBufferProcess;
    pVideoEnc->exynos_codec_dstOutputProcess = &Exynos_H264Enc_dstOutputBufferProcess;

    pVideoEnc->exynos_codec_start         = &H264CodecStart;
    pVideoEnc->exynos_codec_stop          = &H264CodecStop;
    pVideoEnc->exynos_codec_bufferProcessRun = &H264CodecOutputBufferProcessRun;
    pVideoEnc->exynos_codec_enqueueAllBuffer = &H264CodecEnqueueAllBuffer;

    pVideoEnc->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;

    pVideoEnc->exynos_codec_checkFormatSupport = &CheckFormatHWSupport;

    pVideoEnc->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoEnc->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Open", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pH264Enc);
        pH264Enc = pVideoEnc->hCodecHandle = NULL;
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pH264Enc->hMFCH264Handle.videoInstInfo.eCodecType = VIDEO_CODING_AVC;
    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        pH264Enc->hMFCH264Handle.videoInstInfo.eSecurityType = VIDEO_SECURE;
    else
        pH264Enc->hMFCH264Handle.videoInstInfo.eSecurityType = VIDEO_NORMAL;

    pH264Enc->hMFCH264Handle.bSkypeBitrate = OMX_FALSE;

    if (Exynos_Video_GetInstInfo(&(pH264Enc->hMFCH264Handle.videoInstInfo), VIDEO_FALSE /* enc */) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to GetInstInfo", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pH264Enc);
        pH264Enc = pVideoEnc->hCodecHandle = NULL;
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] GetInstInfo for enc : temporal-svc(%d)/skype(%d)/roi(%d)/qp-range(%d)/fix(%d)/pvc(%d)/I-ratio(%d)/CA(%d)/AdaptiveBR(%d)/ChromaQP(%d)",
            pExynosComponent, __FUNCTION__,
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bSkypeSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bRoiInfoSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bQpRangePBSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bFixedSliceSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bPVCSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bIFrameRatioSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bColorAspectsSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bAdaptiveLayerBitrateSupport),
            (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bChromaQpSupport));

    Exynos_Input_SetSupportFormat(pExynosComponent);
    SetProfileLevel(pExynosComponent);

#ifdef USE_ANDROID
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-qp-range", (OMX_INDEXTYPE)OMX_IndexConfigVideoQPRange);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-drop-control", (OMX_INDEXTYPE)OMX_IndexParamVideoDropControl);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-disable-dfr", (OMX_INDEXTYPE)OMX_IndexParamVideoDisableDFR);
    if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bChromaQpSupport == VIDEO_TRUE)
        Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-chroma-qp-offset", (OMX_INDEXTYPE)OMX_IndexParamVideoChromaQP);
    if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_TRUE)
        Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-disable-hierarchical-b-encoding", (OMX_INDEXTYPE)OMX_IndexParamVideoDisableHBEncoding);
#ifdef USE_SKYPE_HD
    if (pH264Enc->hMFCH264Handle.videoInstInfo.supportInfo.enc.bSkypeSupport == VIDEO_TRUE) {
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-caps-vt-driver-version", (OMX_INDEXTYPE)OMX_IndexSkypeParamDriverVersion);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-low-latency", (OMX_INDEXTYPE)OMX_IndexSkypeParamLowLatency);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-caps-temporal-layers", (OMX_INDEXTYPE)OMX_IndexSkypeParamEncoderMaxTemporalLayerCount);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-caps-ltr", (OMX_INDEXTYPE)OMX_IndexSkypeParamEncoderMaxLTR);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-ltr-count", (OMX_INDEXTYPE)OMX_IndexSkypeParamEncoderLTR);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-caps-preprocess", (OMX_INDEXTYPE)OMX_IndexSkypeParamEncoderPreprocess);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-custom-profile-level", (OMX_INDEXTYPE)OMX_IndexParamVideoProfileLevelCurrent);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-sar", (OMX_INDEXTYPE)OMX_IndexSkypeParamEncoderSar);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-slice", (OMX_INDEXTYPE)OMX_IndexParamVideoAvc);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-ltr", (OMX_INDEXTYPE)OMX_IndexSkypeConfigEncoderLTR);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-frame-qp", (OMX_INDEXTYPE)OMX_IndexSkypeConfigQP);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-base-layer-pid", (OMX_INDEXTYPE)OMX_IndexSkypeConfigBasePid);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-app-input-control", (OMX_INDEXTYPE)OMX_IndexSkypeParamEncoderInputControl);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-input-trigger", (OMX_INDEXTYPE)OMX_IndexSkypeConfigEncoderInputTrigger);
        Exynos_OSAL_AddVendorExt(hComponent, "rtc-ext-enc-bitrate-mode", (OMX_INDEXTYPE)OMX_IndexSkypeParamVideoBitrate);
    }
#endif
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
    EXYNOS_H264ENC_HANDLE           *pH264Enc           = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent       = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent    = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc           = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

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

    pH264Enc = (EXYNOS_H264ENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pH264Enc != NULL) {
        Exynos_OSAL_Free(pH264Enc);
        pH264Enc = pVideoEnc->hCodecHandle = NULL;
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
