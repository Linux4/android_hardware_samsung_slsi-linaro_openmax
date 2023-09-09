/*
 *
 * Copyright 2014 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OMX_HEVCenc.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Taehwan Kim (t_h.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2014.05.22 : Create
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
#include "library_register.h"
#include "Exynos_OMX_HEVCenc.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OSAL_Queue.h"

#include "Exynos_OSAL_Platform.h"

#include "VendorVideoAPI.h"

/* To use CSC_METHOD_HW in EXYNOS OMX, gralloc should allocate physical memory using FIMC */
/* It means GRALLOC_USAGE_HW_FIMC1 should be set on Native Window usage */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_HEVC_ENC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

static OMX_ERRORTYPE SetProfileLevel(
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc       = NULL;

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

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcEnc->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain;
    pHevcEnc->hMFCHevcHandle.nProfileCnt = nProfileCnt;

    switch (pHevcEnc->hMFCHevcHandle.videoInstInfo.HwVersion) {
    case MFC_1501:
    case MFC_150:
    case MFC_1400:
    case MFC_1410:
    case MFC_140:
        pHevcEnc->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10;
        pHevcEnc->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10;
        pHevcEnc->hMFCHevcHandle.nProfileCnt = nProfileCnt;

        pHevcEnc->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel6;
        break;
    case MFC_130:
    case MFC_120:
    case MFC_1220:
        pHevcEnc->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10;
        pHevcEnc->hMFCHevcHandle.profiles[nProfileCnt++] = OMX_VIDEO_HEVCProfileMain10HDR10;
        pHevcEnc->hMFCHevcHandle.nProfileCnt = nProfileCnt;

        pHevcEnc->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel6;
        break;
    case MFC_110:
        pHevcEnc->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel6;
        break;
    case MFC_100:
    case MFC_101:
        pHevcEnc->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel51;
        break;
    case MFC_90:
    case MFC_1010:
    case MFC_1120:
        pHevcEnc->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel5;
        break;
    case MFC_1011:
    case MFC_1020:
    case MFC_1021:
        pHevcEnc->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel41;
        break;
    case MFC_92:
    default:
        pHevcEnc->hMFCHevcHandle.maxLevel = OMX_VIDEO_HEVCHighTierLevel4;
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
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc       = NULL;

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

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (pHevcEnc->hMFCHevcHandle.nProfileCnt <= (int)pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pHevcEnc->hMFCHevcHandle.profiles[pProfileLevelType->nProfileIndex];
    pProfileLevelType->eLevel   = pHevcEnc->hMFCHevcHandle.maxLevel;
#else
    while ((pHevcEnc->hMFCHevcHandle.maxLevel >> nLevelCnt) > 0) {
        nLevelCnt++;
    }

    if ((pHevcEnc->hMFCHevcHandle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    nMaxIndex = pHevcEnc->hMFCHevcHandle.nProfileCnt * nLevelCnt;
    if (nMaxIndex <= pProfileLevelType->nProfileIndex) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pProfileLevelType->eProfile = pHevcEnc->hMFCHevcHandle.profiles[pProfileLevelType->nProfileIndex / nLevelCnt];
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
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc   = NULL;

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

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc == NULL)
        goto EXIT;

    while ((pHevcEnc->hMFCHevcHandle.maxLevel >> nLevelCnt++) > 0);

    if ((pHevcEnc->hMFCHevcHandle.nProfileCnt == 0) ||
        (nLevelCnt == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] : there is no any profile/level",
                                            pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pHevcEnc->hMFCHevcHandle.nProfileCnt; i++) {
        if (pHevcEnc->hMFCHevcHandle.profiles[i] == pProfileLevelType->eProfile) {
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

static OMX_U32 OMXHevcProfileToProfileIDC(OMX_VIDEO_HEVCPROFILETYPE eProfile)
{
    OMX_U32 ret = 0;

    switch (eProfile) {
    case OMX_VIDEO_HEVCProfileMain:
        ret = 0;
        break;
    case OMX_VIDEO_HEVCProfileMain10:
    case OMX_VIDEO_HEVCProfileMain10HDR10:
    case OMX_VIDEO_HEVCProfileMain10HDR10Plus:
        ret = 3;
        break;
    default:
        ret = 0;
        break;
    }

    return ret;
}

static OMX_U32 OMXHevcLevelToTierIDC(OMX_VIDEO_HEVCLEVELTYPE eLevel)
{
    OMX_U32 ret = 0; //default Main tier

    switch (eLevel) {
    case OMX_VIDEO_HEVCMainTierLevel1:
    case OMX_VIDEO_HEVCMainTierLevel2:
    case OMX_VIDEO_HEVCMainTierLevel21:
    case OMX_VIDEO_HEVCMainTierLevel3:
    case OMX_VIDEO_HEVCMainTierLevel31:
    case OMX_VIDEO_HEVCMainTierLevel4:
    case OMX_VIDEO_HEVCMainTierLevel41:
    case OMX_VIDEO_HEVCMainTierLevel5:
    case OMX_VIDEO_HEVCMainTierLevel51:
    case OMX_VIDEO_HEVCMainTierLevel52:
    case OMX_VIDEO_HEVCMainTierLevel6:
    case OMX_VIDEO_HEVCMainTierLevel61:
    case OMX_VIDEO_HEVCMainTierLevel62:
        ret = 0;
        break;
    case OMX_VIDEO_HEVCHighTierLevel1:
    case OMX_VIDEO_HEVCHighTierLevel2:
    case OMX_VIDEO_HEVCHighTierLevel21:
    case OMX_VIDEO_HEVCHighTierLevel3:
    case OMX_VIDEO_HEVCHighTierLevel31:
    case OMX_VIDEO_HEVCHighTierLevel4:
    case OMX_VIDEO_HEVCHighTierLevel41:
    case OMX_VIDEO_HEVCHighTierLevel5:
    case OMX_VIDEO_HEVCHighTierLevel51:
    case OMX_VIDEO_HEVCHighTierLevel52:
    case OMX_VIDEO_HEVCHighTierLevel6:
    case OMX_VIDEO_HEVCHighTierLevel61:
    case OMX_VIDEO_HEVCHighTierLevel62:
        ret = 1;
        break;
    default:
        ret = 0;
        break;
    }

    return ret;
}

static OMX_U32 OMXHevcLevelToLevelIDC(OMX_VIDEO_HEVCLEVELTYPE eLevel)
{
    OMX_U32 ret = 40; //default OMX_VIDEO_HEVCLevel4

    switch (eLevel) {
    case OMX_VIDEO_HEVCMainTierLevel1:
    case OMX_VIDEO_HEVCHighTierLevel1:
        ret = 10;
        break;
    case OMX_VIDEO_HEVCMainTierLevel2:
    case OMX_VIDEO_HEVCHighTierLevel2:
        ret = 20;
        break;
    case OMX_VIDEO_HEVCMainTierLevel21:
    case OMX_VIDEO_HEVCHighTierLevel21:
        ret = 21;
        break;
    case OMX_VIDEO_HEVCMainTierLevel3:
    case OMX_VIDEO_HEVCHighTierLevel3:
        ret = 30;
        break;
    case OMX_VIDEO_HEVCMainTierLevel31:
    case OMX_VIDEO_HEVCHighTierLevel31:
        ret = 31;
        break;
    case OMX_VIDEO_HEVCMainTierLevel4:
    case OMX_VIDEO_HEVCHighTierLevel4:
        ret = 40;
        break;
    case OMX_VIDEO_HEVCMainTierLevel41:
    case OMX_VIDEO_HEVCHighTierLevel41:
        ret = 41;
        break;
    case OMX_VIDEO_HEVCMainTierLevel5:
    case OMX_VIDEO_HEVCHighTierLevel5:
        ret = 50;
        break;
    case OMX_VIDEO_HEVCMainTierLevel51:
    case OMX_VIDEO_HEVCHighTierLevel51:
        ret = 51;
        break;
    case OMX_VIDEO_HEVCMainTierLevel52:
    case OMX_VIDEO_HEVCHighTierLevel52:
        ret = 52;
        break;
    case OMX_VIDEO_HEVCMainTierLevel6:
    case OMX_VIDEO_HEVCHighTierLevel6:
        ret = 60;
        break;
    case OMX_VIDEO_HEVCMainTierLevel61:
    case OMX_VIDEO_HEVCHighTierLevel61:
        ret = 61;
        break;
    case OMX_VIDEO_HEVCMainTierLevel62:
    case OMX_VIDEO_HEVCHighTierLevel62:
        ret = 62;
        break;
    default:
        ret = 40;
        break;
    }

    return ret;
}

static void Print_HEVCEnc_Param(ExynosVideoEncParam *pEncParam)
{
    ExynosVideoEncCommonParam *pCommonParam = &pEncParam->commonParam;
    ExynosVideoEncHevcParam   *pHEVCParam   = &pEncParam->codecParam.hevc;

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

    /* HEVC specific parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "ProfileIDC              : %d", pHEVCParam->ProfileIDC);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "TierIDC                 : %d", pHEVCParam->TierIDC);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "LevelIDC                : %d", pHEVCParam->LevelIDC);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameQp_B               : %d", pHEVCParam->FrameQp_B);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "FrameRate               : %d", pHEVCParam->FrameRate);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "MaxPartitionDepth       : %d", pHEVCParam->MaxPartitionDepth);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "NumberBFrames           : %d", pHEVCParam->NumberBFrames);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "NumberRefForPframes     : %d", pHEVCParam->NumberRefForPframes);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LoopFilterDisable       : %d", pHEVCParam->LoopFilterDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LoopFilterSliceFlag     : %d", pHEVCParam->LoopFilterSliceFlag);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LoopFilterTcOffset      : %d", pHEVCParam->LoopFilterTcOffset);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LoopFilterBetaOffset    : %d", pHEVCParam->LoopFilterBetaOffset);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LongtermRefEnable       : %d", pHEVCParam->LongtermRefEnable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LongtermUserRef         : %d", pHEVCParam->LongtermUserRef);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "LongtermStoreRef        : %d", pHEVCParam->LongtermStoreRef);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "DarkDisable             : %d", pHEVCParam->DarkDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "SmoothDisable           : %d", pHEVCParam->SmoothDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "StaticDisable           : %d", pHEVCParam->StaticDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "ActivityDisable         : %d", pHEVCParam->ActivityDisable);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "ROIEnable:              : %d", pHEVCParam->ROIEnable);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "GPBEnable:              : %d", pHEVCParam->GPBEnable);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,     "HierarType:             : %d", pHEVCParam->HierarType);

    /* rate control related parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableFRMRateControl    : %d", pCommonParam->EnableFRMRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableMBRateControl     : %d", pCommonParam->EnableMBRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "CBRPeriodRf             : %d", pCommonParam->CBRPeriodRf);
}

static void Set_HEVCEnc_Param(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    EXYNOS_OMX_BASEPORT           *pInputPort       = NULL;
    EXYNOS_OMX_BASEPORT           *pOutputPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = NULL;
    EXYNOS_MFC_HEVCENC_HANDLE     *pMFCHevcHandle   = NULL;
    OMX_COLOR_FORMATTYPE           eColorFormat     = OMX_COLOR_FormatUnused;

    ExynosVideoEncParam       *pEncParam    = NULL;
    ExynosVideoEncInitParam   *pInitParam   = NULL;
    ExynosVideoEncCommonParam *pCommonParam = NULL;
    ExynosVideoEncHevcParam   *pHEVCParam   = NULL;

    OMX_S32 nWidth, nHeight;
    int i;

    pVideoEnc           = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pHevcEnc            = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCHevcHandle      = &pHevcEnc->hMFCHevcHandle;
    pInputPort          = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pOutputPort         = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    pEncParam       = &pMFCHevcHandle->encParam;
    pInitParam      = &pEncParam->initParam;
    pCommonParam    = &pEncParam->commonParam;
    pHEVCParam      = &pEncParam->codecParam.hevc;

    pEncParam->eCompressionFormat = VIDEO_CODING_HEVC;

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
    pCommonParam->IDRPeriod    = pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval;
    pCommonParam->SliceMode    = 0;
    pCommonParam->Bitrate      = pOutputPort->portDefinition.format.video.nBitrate;
    pCommonParam->FrameQp      = pVideoEnc->quantization.nQpI;
    pCommonParam->FrameQp_P    = pVideoEnc->quantization.nQpP;

    pCommonParam->QpRange.QpMin_I = pHevcEnc->qpRangeI.nMinQP;
    pCommonParam->QpRange.QpMax_I = pHevcEnc->qpRangeI.nMaxQP;
    pCommonParam->QpRange.QpMin_P = pHevcEnc->qpRangeP.nMinQP;
    pCommonParam->QpRange.QpMax_P = pHevcEnc->qpRangeP.nMaxQP;
    pCommonParam->QpRange.QpMin_B = pHevcEnc->qpRangeB.nMinQP;
    pCommonParam->QpRange.QpMax_B = pHevcEnc->qpRangeB.nMaxQP;

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

    /* HEVC specific parameters */
    pHEVCParam->ProfileIDC = OMXHevcProfileToProfileIDC(pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].eProfile);
    pHEVCParam->TierIDC    = OMXHevcLevelToTierIDC(pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].eLevel);
    pHEVCParam->LevelIDC   = OMXHevcLevelToLevelIDC(pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].eLevel);

    pHEVCParam->FrameQp_B  = pVideoEnc->quantization.nQpB;
    pHEVCParam->FrameRate  = (pInputPort->portDefinition.format.video.xFramerate) >> 16;

    if (pHevcEnc->nTemporalLayerCountForB > 0)
        pHEVCParam->NumberRefForPframes = 1;  /* Hierarchical-B enabled */
    else
        pHEVCParam->NumberRefForPframes = pMFCHevcHandle->nRefForPframes;

    /* there is no interface at OMX IL component */
    pHEVCParam->NumberBFrames         = 0;    /* 0 ~ 2 */

    pHEVCParam->MaxPartitionDepth     = 1;
    pHEVCParam->LoopFilterDisable     = 0;    /* 1: Loop Filter Disable, 0: Filter Enable */
    pHEVCParam->LoopFilterSliceFlag   = 1;
    pHEVCParam->LoopFilterTcOffset    = 0;
    pHEVCParam->LoopFilterBetaOffset  = 0;

    pHEVCParam->LongtermRefEnable     = 0;
    pHEVCParam->LongtermUserRef       = 0;
    pHEVCParam->LongtermStoreRef      = 0;

    pHEVCParam->DarkDisable           = 1;    /* disable adaptive rate control on dark region */
    pHEVCParam->SmoothDisable         = 1;    /* disable adaptive rate control on smooth region */
    pHEVCParam->StaticDisable         = 1;    /* disable adaptive rate control on static region */
    pHEVCParam->ActivityDisable       = 1;    /* disable adaptive rate control on high activity region */

    /* Chroma QP Offset */
    pHEVCParam->chromaQPOffset.Cr      = pHevcEnc->chromaQPOffset.nCr;
    pHEVCParam->chromaQPOffset.Cb      = pHevcEnc->chromaQPOffset.nCb;

    /* Temporal SVC */
    /* If MaxTemporalLayerCount value is 0, codec supported max value will be set */
    pHEVCParam->MaxTemporalLayerCount           = 0;
    pHEVCParam->TemporalSVC.nTemporalLayerCount = (unsigned int)pHevcEnc->nTemporalLayerCount;

    if (pHevcEnc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++)
            pHEVCParam->TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)pHevcEnc->nTemporalLayerBitrateRatio[i];
    } else {
        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++)
            pHEVCParam->TemporalSVC.nTemporalLayerBitrateRatio[i] = pOutputPort->portDefinition.format.video.nBitrate;
    }

    /* Hierarchal P & B */
    if (pHevcEnc->nTemporalLayerCountForB > 0)
        pHEVCParam->HierarType = EXYNOS_OMX_Hierarchical_B;
    else
        pHEVCParam->HierarType = EXYNOS_OMX_Hierarchical_P;

    pHEVCParam->ROIEnable = (pMFCHevcHandle->bRoiInfo == OMX_TRUE)? 1:0;
    pHEVCParam->GPBEnable = (ExynosVideoBoolType)pMFCHevcHandle->bGPBEnable;

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
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable, 1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable, 1:MB level RC */
        pCommonParam->CBRPeriodRf          = 9;
        break;
    case OMX_Video_ControlRateVariable:
    default: /*Android default */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Video Encode VBR");
        pCommonParam->EnableFRMRateControl = 1;    /* 0: Disable, 1: Frame level RC */
        pCommonParam->EnableMBRateControl  = 1;    /* 0: Disable, 1:MB level RC */
        pCommonParam->CBRPeriodRf          = 100;
        break;
    }

//    Print_HEVCEnc_Param(pEncParam);
}

static void Change_HEVCEnc_Param(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc         = NULL;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc          = NULL;
    EXYNOS_MFC_HEVCENC_HANDLE     *pMFCHevcHandle    = NULL;
    OMX_PTR                        pDynamicConfigCMD = NULL;
    OMX_PTR                        pConfigData       = NULL;
    OMX_S32                        nCmdIndex         = 0;
    ExynosVideoEncOps             *pEncOps           = NULL;
    int                            nValue            = 0;

    int i;

    pVideoEnc       = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pHevcEnc        = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCHevcHandle  = &pHevcEnc->hMFCHevcHandle;
    pEncOps         = pMFCHevcHandle->pEncOps;

    pDynamicConfigCMD = (OMX_PTR)Exynos_OSAL_Dequeue(&pExynosComponent->dynamicConfigQ);
    if (pDynamicConfigCMD == NULL)
        goto EXIT;

    nCmdIndex   = *(OMX_S32 *)pDynamicConfigCMD;
    pConfigData = (OMX_PTR)((OMX_U8 *)pDynamicConfigCMD + sizeof(OMX_S32));

    switch ((int)nCmdIndex) {
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        nValue = VIDEO_FRAME_I;
        pEncOps->Set_FrameType(pMFCHevcHandle->hMFCHandle, nValue);
        pVideoEnc->IntraRefreshVOP = OMX_FALSE;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] VOP Refresh", pExynosComponent, __FUNCTION__);
    }
        break;
    case OMX_IndexConfigVideoIntraPeriod:
    {
        OMX_S32 nPFrames = (*((OMX_U32 *)pConfigData)) - 1;
        nValue = nPFrames + 1;
        pEncOps->Set_IDRPeriod(pMFCHevcHandle->hMFCHandle, nValue);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] IDR period: %d", pExynosComponent, __FUNCTION__, nValue);
    }
        break;
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pConfigData;

        if (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] != OMX_Video_ControlRateDisable) {
            nValue = pConfigBitrate->nEncodeBitrate;
            pEncOps->Set_BitRate(pMFCHevcHandle->hMFCHandle, nValue);
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
            pEncOps->Set_FrameRate(pMFCHevcHandle->hMFCHandle, nValue);
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

        pEncOps->Set_QpRange(pMFCHevcHandle->hMFCHandle, qpRange);

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

        if (pMFCHevcHandle->videoInstInfo.supportInfo.enc.bOperatingRateSupport == VIDEO_TRUE) {
            if (nValue == ((OMX_U32)INT_MAX >> 16)) {
                nValue = (OMX_U32)INT_MAX;
            } else {
                nValue = nValue * 1000;
            }

            pEncOps->Set_OperatingRate(pMFCHevcHandle->hMFCHandle, nValue);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] operating rate: 0x%x", pExynosComponent, __FUNCTION__, nValue);
        } else {
            if (nValue == (((OMX_U32)INT_MAX) >> 16)) {
                nValue = 1000;
            } else {
                nValue = ((xFramerate >> 16) == 0)? 100:(OMX_U32)((pConfigRate->nU32 / (double)xFramerate) * 100);
            }

            pEncOps->Set_QosRatio(pMFCHevcHandle->hMFCHandle, nValue);

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

        pEncOps->Set_QpRange(pMFCHevcHandle->hMFCHandle, qpRange);
        pEncOps->Set_IDRPeriod(pMFCHevcHandle->hMFCHandle, pTemporalSVC->nKeyFrameInterval);

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

        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++) {
            TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)pTemporalSVC->nTemporalLayerBitrateRatio[i];
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // bitrate ratio[%d]: %d",
                                                    pExynosComponent, __FUNCTION__,
                                                    i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
        }
        if (pEncOps->Set_LayerChange(pMFCHevcHandle->hMFCHandle, TemporalSVC) != VIDEO_ERROR_NONE)
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

        if (pEncOps->Set_RoiInfo(pMFCHevcHandle->hMFCHandle, &RoiInfo) != VIDEO_ERROR_NONE)
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Set_RoiInfo()", pExynosComponent, __FUNCTION__);
    }
        break;
    case OMX_IndexConfigIFrameRatio:
    {
        OMX_PARAM_U32TYPE *pIFrameRatio = (OMX_PARAM_U32TYPE *)pConfigData;

        pEncOps->Set_IFrameRatio(pMFCHevcHandle->hMFCHandle, pIFrameRatio->nU32);
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
            for (i = 0; i < (int)pHevcEnc->nMaxTemporalLayerCount; i++) {

                TemporalSVC.nTemporalLayerBitrateRatio[i] = (pTemporalLayering->nBitrateRatios[i] >> 16);
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // bitrate ratio[%d]: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
            }
        } else {
            EXYNOS_OMX_BASEPORT *pOutputPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

            for (i = 0; i < (int)pHevcEnc->nMaxTemporalLayerCount; i++) {
                TemporalSVC.nTemporalLayerBitrateRatio[i] = pOutputPort->portDefinition.format.video.nBitrate;

                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // bitrate ratio[%d]: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
            }
        }

        if (pEncOps->Set_LayerChange(pMFCHevcHandle->hMFCHandle, TemporalSVC) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Not supported control: Set_LayerChange", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Max(%d), layer(%d), B-layer(%d)", pExynosComponent, __FUNCTION__,
                                               pHevcEnc->nMaxTemporalLayerCount, TemporalSVC.nTemporalLayerCount, pTemporalLayering->nBLayerCountActual);
    }
        break;
#endif
    default:
        break;
    }

    Exynos_OSAL_Free(pDynamicConfigCMD);

    Set_HEVCEnc_Param(pExynosComponent);

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
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc       = NULL;
    EXYNOS_OMX_BASEPORT             *pInputPort     = NULL;
    ExynosVideoColorFormatType       eVideoFormat   = VIDEO_COLORFORMAT_UNKNOWN;
    int i;

    if (pExynosComponent == NULL)
        goto EXIT;

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL)
        goto EXIT;

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc == NULL)
        goto EXIT;
    pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    eVideoFormat = (ExynosVideoColorFormatType)Exynos_OSAL_OMX2VideoFormat(eColorFormat, pInputPort->ePlaneType);

    for (i = 0; i < VIDEO_COLORFORMAT_MAX; i++) {
        if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportFormat[i] == VIDEO_COLORFORMAT_UNKNOWN)
            break;

        if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportFormat[i] == eVideoFormat) {
            ret = OMX_TRUE;
            break;
        }
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE HEVCCodecOpen(
    EXYNOS_HEVCENC_HANDLE   *pHevcEnc,
    ExynosVideoInstInfo     *pVideoInstInfo)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    ExynosVideoEncOps       *pEncOps    = NULL;
    ExynosVideoEncBufferOps *pInbufOps  = NULL;
    ExynosVideoEncBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if ((pHevcEnc == NULL) ||
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

    pHevcEnc->hMFCHevcHandle.pEncOps    = pEncOps;
    pHevcEnc->hMFCHevcHandle.pInbufOps  = pInbufOps;
    pHevcEnc->hMFCHevcHandle.pOutbufOps = pOutbufOps;

    /* function pointer mapping */
    pEncOps->nSize     = sizeof(ExynosVideoEncOps);
    pInbufOps->nSize   = sizeof(ExynosVideoEncBufferOps);
    pOutbufOps->nSize  = sizeof(ExynosVideoEncBufferOps);

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
    pHevcEnc->hMFCHevcHandle.hMFCHandle = pHevcEnc->hMFCHevcHandle.pEncOps->Init(pVideoInstInfo);
    if (pHevcEnc->hMFCHevcHandle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to init", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pEncOps != NULL) {
            Exynos_OSAL_Free(pEncOps);
            pHevcEnc->hMFCHevcHandle.pEncOps = NULL;
        }

        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pHevcEnc->hMFCHevcHandle.pInbufOps = NULL;
        }

        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pHevcEnc->hMFCHevcHandle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HEVCCodecClose(EXYNOS_HEVCENC_HANDLE *pHevcEnc)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoEncOps       *pEncOps    = NULL;
    ExynosVideoEncBufferOps *pInbufOps  = NULL;
    ExynosVideoEncBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pHevcEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcEnc->hMFCHevcHandle.hMFCHandle;
    pEncOps    = pHevcEnc->hMFCHevcHandle.pEncOps;
    pInbufOps  = pHevcEnc->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcEnc->hMFCHevcHandle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pEncOps->Finalize(hMFCHandle);
        hMFCHandle = pHevcEnc->hMFCHevcHandle.hMFCHandle = NULL;
        pHevcEnc->hMFCHevcHandle.bConfiguredMFCSrc = OMX_FALSE;
        pHevcEnc->hMFCHevcHandle.bConfiguredMFCDst = OMX_FALSE;
    }

    /* Unregister function pointers */
    Exynos_Video_Unregister_Encoder(pEncOps, pInbufOps, pOutbufOps);

    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pOutbufOps = pHevcEnc->hMFCHevcHandle.pOutbufOps = NULL;
    }

    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pInbufOps = pHevcEnc->hMFCHevcHandle.pInbufOps = NULL;
    }

    if (pEncOps != NULL) {
        Exynos_OSAL_Free(pEncOps);
        pEncOps = pHevcEnc->hMFCHevcHandle.pEncOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HEVCCodecStart(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = NULL;
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

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcEnc->hMFCHevcHandle.hMFCHandle;
    pInbufOps  = pHevcEnc->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcEnc->hMFCHevcHandle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX)
        pInbufOps->Run(hMFCHandle);
    else if (nPortIndex == OUTPUT_PORT_INDEX)
        pOutbufOps->Run(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HEVCCodecStop(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = NULL;
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

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcEnc->hMFCHevcHandle.hMFCHandle;
    pInbufOps  = pHevcEnc->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcEnc->hMFCHevcHandle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) && (pInbufOps != NULL))
        pInbufOps->Stop(hMFCHandle);
    else if ((nPortIndex == OUTPUT_PORT_INDEX) && (pOutbufOps != NULL))
        pOutbufOps->Stop(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HEVCCodecOutputBufferProcessRun(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = NULL;
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

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pHevcEnc->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pHevcEnc->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pHevcEnc->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pHevcEnc->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pHevcEnc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HEVCCodecEnqueueAllBuffer(
    OMX_COMPONENTTYPE *pOMXComponent,
    OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                            *hMFCHandle         = pHevcEnc->hMFCHevcHandle.hMFCHandle;

    ExynosVideoEncBufferOps *pInbufOps  = pHevcEnc->hMFCHevcHandle.pInbufOps;
    ExynosVideoEncBufferOps *pOutbufOps = pHevcEnc->hMFCHevcHandle.pOutbufOps;

    int i;

    FunctionIn();

    if ((nPortIndex != INPUT_PORT_INDEX) &&
        (nPortIndex != OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pHevcEnc->bSourceStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)  {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] CodecBuffer(input) [%d]: FD(0x%x), VA(0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                i, pVideoEnc->pMFCEncInputBuffer[i]->fd[0], pVideoEnc->pMFCEncInputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnqueue(pExynosComponent, INPUT_PORT_INDEX, pVideoEnc->pMFCEncInputBuffer[i]);
        }

        pInbufOps->Clear_Queue(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pHevcEnc->bDestinationStart == OMX_TRUE)) {
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

void HEVCCodecSetHdrInfo(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_HEVCENC_HANDLE     *pMFCHevcHandle   = &pHevcEnc->hMFCHevcHandle;
    void                          *hMFCHandle       = pMFCHevcHandle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoEncOps *pEncOps = pHevcEnc->hMFCHevcHandle.pEncOps;

    OMX_COLOR_FORMATTYPE eColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);

    /* color aspects */
    if (pMFCHevcHandle->videoInstInfo.supportInfo.enc.bColorAspectsSupport == VIDEO_TRUE) {
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
            if (pVideoEnc->surfaceFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888) {
                Exynos_OSAL_GetColorAspectsForBitstream(&(pInputPort->ColorAspects), &(pOutputPort->ColorAspects));
            }
        }
#endif

        BSCA.eRangeType     = (ExynosRangeType)pOutputPort->ColorAspects.nRangeType;
        BSCA.ePrimariesType = (ExynosPrimariesType)pOutputPort->ColorAspects.nPrimaryType;
        BSCA.eTransferType  = (ExynosTransferType)pOutputPort->ColorAspects.nTransferType;
        BSCA.eCoeffType     = (ExynosMatrixCoeffType)pOutputPort->ColorAspects.nCoeffType;

        pEncOps->Set_ColorAspects(hMFCHandle, &BSCA);
    }

    /* hdr info : it is supported on 10bit encoding only */
    if ((eColorFormat == OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar) ||
        (eColorFormat == OMX_COLOR_FormatYUV420Planar16)) {
        /* hdr static info */
        if (pMFCHevcHandle->videoInstInfo.supportInfo.enc.bHDRStaticInfoSupport == VIDEO_TRUE) {
            EXYNOS_OMX_VIDEO_HDRSTATICINFO *FWHDR = &(pInputPort->HDRStaticInfo);
            ExynosVideoHdrStatic            BSHDR;
            Exynos_OSAL_Memset(&BSHDR, 0, sizeof(BSHDR));

            BSHDR.max_pic_average_light = (int)FWHDR->nMaxPicAverageLight;
            BSHDR.max_content_light     = (int)FWHDR->nMaxContentLight;
            BSHDR.max_display_luminance = (int)FWHDR->nMaxDisplayLuminance;
            BSHDR.min_display_luminance = (int)FWHDR->nMinDisplayLuminance;

            BSHDR.red.x     = (unsigned short)FWHDR->red.x;
            BSHDR.red.y     = (unsigned short)FWHDR->red.y;
            BSHDR.green.x   = (unsigned short)FWHDR->green.x;
            BSHDR.green.y   = (unsigned short)FWHDR->green.y;
            BSHDR.blue.x    = (unsigned short)FWHDR->blue.x;
            BSHDR.blue.y    = (unsigned short)FWHDR->blue.y;
            BSHDR.white.x   = (unsigned short)FWHDR->white.x;
            BSHDR.white.y   = (unsigned short)FWHDR->white.y;

#ifdef USE_ANDROID
            if ((pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) &&
                ((pInputPort->bufferProcessType & BUFFER_SHARE) &&
                 (pSrcInputData->buffer.addr[2] != NULL))) {
                ExynosVideoMeta *pMeta = (ExynosVideoMeta *)pSrcInputData->buffer.addr[2];

                /* use HDRStaticInfo given from camera */
                if (pMeta->eType & VIDEO_INFO_TYPE_HDR_STATIC) {
                    ExynosHdrStaticInfo *pHdrStaticInfo = &(pMeta->sHdrStaticInfo);

                    BSHDR.max_pic_average_light = (int)pHdrStaticInfo->sType1.mMaxFrameAverageLightLevel;
                    BSHDR.max_content_light     = (int)pHdrStaticInfo->sType1.mMaxContentLightLevel;
                    BSHDR.max_display_luminance = (int)pHdrStaticInfo->sType1.mMaxDisplayLuminance;
                    BSHDR.min_display_luminance = (int)pHdrStaticInfo->sType1.mMinDisplayLuminance;

                    BSHDR.red.x     = (unsigned short)pHdrStaticInfo->sType1.mR.x;
                    BSHDR.red.y     = (unsigned short)pHdrStaticInfo->sType1.mR.y;
                    BSHDR.green.x   = (unsigned short)pHdrStaticInfo->sType1.mG.x;
                    BSHDR.green.y   = (unsigned short)pHdrStaticInfo->sType1.mG.y;
                    BSHDR.blue.x    = (unsigned short)pHdrStaticInfo->sType1.mB.x;
                    BSHDR.blue.y    = (unsigned short)pHdrStaticInfo->sType1.mB.y;
                    BSHDR.white.x   = (unsigned short)pHdrStaticInfo->sType1.mW.x;
                    BSHDR.white.y   = (unsigned short)pHdrStaticInfo->sType1.mW.y;
                }
            }
#endif
            pEncOps->Set_HDRStaticInfo(hMFCHandle, &BSHDR);
        }

        /* hdr dynamic info */
        if (pMFCHevcHandle->videoInstInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_TRUE) {
            /* just enable a mode. hdr dynamic info will be updated in SrcIn */
            pMFCHevcHandle->bHDRDynamicInfo = OMX_TRUE;
        }
    }

    return ;
}

void HEVCCodecUpdateHdrDynamicInfo(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_HEVCENC_HANDLE     *pMFCHevcHandle   = &pHevcEnc->hMFCHevcHandle;
    void                          *hMFCHandle       = pMFCHevcHandle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncOps *pEncOps = pHevcEnc->hMFCHevcHandle.pEncOps;

    int i;

    if (pMFCHevcHandle->videoInstInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        ExynosVideoHdrDynamic BSHDR;
        Exynos_OSAL_Memset(&BSHDR, 0, sizeof(BSHDR));

#ifdef USE_ANDROID
        if ((pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) &&
            ((pInputPort->bufferProcessType & BUFFER_SHARE) &&
             (pSrcInputData->buffer.addr[2] != NULL))) {
            ExynosVideoMeta *pMeta = (ExynosVideoMeta *)pSrcInputData->buffer.addr[2];

            if (pMeta->eType & VIDEO_INFO_TYPE_HDR_DYNAMIC) {
                ExynosHdrDynamicInfo *pMetaHDRDynamic   = &(pMeta->sHdrDynamicInfo);

                BSHDR.valid = pMetaHDRDynamic->valid;

                if (pMetaHDRDynamic->valid != 0) {
                    BSHDR.itu_t_t35_country_code                    = pMetaHDRDynamic->data.country_code;
                    BSHDR.itu_t_t35_terminal_provider_code          = pMetaHDRDynamic->data.provider_code;
                    BSHDR.itu_t_t35_terminal_provider_oriented_code = pMetaHDRDynamic->data.provider_oriented_code;
                    BSHDR.application_identifier                    = pMetaHDRDynamic->data.application_identifier;
                    BSHDR.application_version                       = pMetaHDRDynamic->data.application_version;

#ifdef USE_FULL_ST2094_40
                    BSHDR.targeted_system_display_maximum_luminance = pMetaHDRDynamic->data.targeted_system_display_maximum_luminance;

                    /* save information on window-0 only */
                    BSHDR.num_windows = 1;
                    {
                        ExynosVideoHdrWindowInfo *pWindowInfo = &(BSHDR.window_info[0]);

                        /* maxscl */
                        for (i = 0; i < (int)(sizeof(pMetaHDRDynamic->data.maxscl)/sizeof(pMetaHDRDynamic->data.maxscl[0])); i++)
                            pWindowInfo->maxscl[i] = pMetaHDRDynamic->data.maxscl[0][i];

                        /* average_maxrgb */
                        pWindowInfo->average_maxrgb = pMetaHDRDynamic->data.average_maxrgb[0];

                        /* distribution maxrgb */
                        pWindowInfo->num_distribution_maxrgb_percentiles = pMetaHDRDynamic->data.num_maxrgb_percentiles[0];
                        for (i = 0; i < pMetaHDRDynamic->data.num_maxrgb_percentiles[0]; i++) {
                            pWindowInfo->distribution_maxrgb_percentages[i] = pMetaHDRDynamic->data.maxrgb_percentages[0][i];
                            pWindowInfo->distribution_maxrgb_percentiles[i] = pMetaHDRDynamic->data.maxrgb_percentiles[0][i];
                        }

                        /* tone mapping curve */
                        pWindowInfo->tone_mapping_flag = pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag[0];
                        if (pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag[0] != 0) {
                            pWindowInfo->knee_point_x = pMetaHDRDynamic->data.tone_mapping.knee_point_x[0];
                            pWindowInfo->knee_point_y = pMetaHDRDynamic->data.tone_mapping.knee_point_y[0];

                            pWindowInfo->num_bezier_curve_anchors = pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors[0];
                            for (i = 0; i < pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors[0]; i++)
                                pWindowInfo->bezier_curve_anchors[i] = pMetaHDRDynamic->data.tone_mapping.bezier_curve_anchors[0][i];
                        }

                        /* color saturation info */
                        pWindowInfo->color_saturation_mapping_flag = pMetaHDRDynamic->data.color_saturation_mapping_flag[0];
                        if (pMetaHDRDynamic->data.color_saturation_mapping_flag[0] != 0) {
                            pWindowInfo->color_saturation_weight = pMetaHDRDynamic->data.color_saturation_weight[0];
                        }
                    }
#else // USE_FULL_ST2094_40
                    BSHDR.targeted_system_display_maximum_luminance = pMetaHDRDynamic->data.display_maximum_luminance;

                    /* save information on window-0 only */
                    BSHDR.num_windows = 1;
                    {
                        ExynosVideoHdrWindowInfo *pWindowInfo = &(BSHDR.window_info[0]);

                        /* maxscl */
                        for (i = 0; i < (int)(sizeof(pMetaHDRDynamic->data.maxscl)/sizeof(pMetaHDRDynamic->data.maxscl[0])); i++)
                            pWindowInfo->maxscl[i] = pMetaHDRDynamic->data.maxscl[i];

                        /* distribution maxrgb */
                        pWindowInfo->num_distribution_maxrgb_percentiles = pMetaHDRDynamic->data.num_maxrgb_percentiles;
                        for (i = 0; i < pMetaHDRDynamic->data.num_maxrgb_percentiles; i++) {
                            pWindowInfo->distribution_maxrgb_percentages[i] = pMetaHDRDynamic->data.maxrgb_percentages[i];
                            pWindowInfo->distribution_maxrgb_percentiles[i] = pMetaHDRDynamic->data.maxrgb_percentiles[i];
                        }

                        /* tone mapping curve */
                        pWindowInfo->tone_mapping_flag = pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag;
                        if (pMetaHDRDynamic->data.tone_mapping.tone_mapping_flag != 0) {
                            pWindowInfo->knee_point_x = pMetaHDRDynamic->data.tone_mapping.knee_point_x;
                            pWindowInfo->knee_point_y = pMetaHDRDynamic->data.tone_mapping.knee_point_y;

                            pWindowInfo->num_bezier_curve_anchors = pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors;
                            for (i = 0; i < pMetaHDRDynamic->data.tone_mapping.num_bezier_curve_anchors; i++)
                                pWindowInfo->bezier_curve_anchors[i] = pMetaHDRDynamic->data.tone_mapping.bezier_curve_anchors[i];
                        }
                    }
#endif
                }
            }
        }
#endif

        pEncOps->Set_HDRDynamicInfo(hMFCHandle, &BSHDR);
    }

    return ;
}

OMX_ERRORTYPE HevcCodecUpdateResolution(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_HEVCENC_HANDLE     *pMFCHevcHandle   = &pHevcEnc->hMFCHevcHandle;
    void                          *hMFCHandle       = pMFCHevcHandle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE           eColorFormat     = Exynos_Input_GetActualColorFormat(pExynosComponent);

    ExynosVideoEncOps       *pEncOps    = pHevcEnc->hMFCHevcHandle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps  = pHevcEnc->hMFCHevcHandle.pInbufOps;
    ExynosVideoGeometry      bufferConf;

    FunctionIn();

    pVideoEnc->bEncDRCSync = OMX_TRUE;

    /* stream off */
    ret = HEVCCodecStop(pOMXComponent, INPUT_PORT_INDEX);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to HEVCCodecStop() about input", pExynosComponent, __FUNCTION__);
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
    HEVCCodecStart(pOMXComponent, INPUT_PORT_INDEX);

    /* reset buffer queue */
    HEVCCodecEnqueueAllBuffer(pOMXComponent, INPUT_PORT_INDEX);

    /* clear timestamp */
    Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);

    /* reinitialize a value for reconfiguring CSC */
    pVideoEnc->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HEVCCodecSrcSetup(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_HEVCENC_HANDLE     *pMFCHevcHandle   = &pHevcEnc->hMFCHevcHandle;
    void                          *hMFCHandle       = pMFCHevcHandle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize     = pSrcInputData->dataLen;

    ExynosVideoEncOps       *pEncOps    = pHevcEnc->hMFCHevcHandle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps  = pHevcEnc->hMFCHevcHandle.pInbufOps;
    ExynosVideoEncParam     *pEncParam    = NULL;

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
        ret = Exynos_OSAL_Queue(&pHevcEnc->bypassBufferInfoQ, (void *)pBufferInfo);

        if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
            Exynos_OSAL_SignalSet(pHevcEnc->hDestinationInStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        } else if (pOutputPort->bufferProcessType & BUFFER_COPY) {
            Exynos_OSAL_SignalSet(pHevcEnc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    Set_HEVCEnc_Param(pExynosComponent);

    pEncParam = &pMFCHevcHandle->encParam;
    if (pEncOps->Set_EncParam) {
        if(pEncOps->Set_EncParam(pHevcEnc->hMFCHevcHandle.hMFCHandle, pEncParam) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set encParam", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    }

    Print_HEVCEnc_Param(pEncParam);

     if (pMFCHevcHandle->bPrependSpsPpsToIdr == OMX_TRUE) {
         if (pEncOps->Enable_PrependSpsPpsToIdr)
             pEncOps->Enable_PrependSpsPpsToIdr(pHevcEnc->hMFCHevcHandle.hMFCHandle);
         else
             Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Not supported control: Enable_PrependSpsPpsToIdr", pExynosComponent, __FUNCTION__);
     }

    if ((pInputPort->eMetaDataType == METADATA_TYPE_GRAPHIC) &&
        ((pInputPort->bufferProcessType & BUFFER_SHARE) &&
         (pSrcInputData->buffer.addr[2] != NULL))) {
        ExynosVideoMeta *pMeta = (ExynosVideoMeta *)pSrcInputData->buffer.addr[2];

        if (pMeta->eType & VIDEO_INFO_TYPE_YSUM_DATA) {
            if (VIDEO_ERROR_NONE == pEncOps->Enable_WeightedPrediction(hMFCHandle))
                pMFCHevcHandle->bWeightedPrediction = OMX_TRUE;
        }
    }

    if (pMFCHevcHandle->videoInstInfo.supportInfo.enc.bAdaptiveLayerBitrateSupport == VIDEO_TRUE) {
        if (pHevcEnc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
            pEncOps->Enable_AdaptiveLayerBitrate(pMFCHevcHandle->hMFCHandle, 0); /* Disable : Use layer bitrate from framework */
        } else {
            pEncOps->Enable_AdaptiveLayerBitrate(pMFCHevcHandle->hMFCHandle, 1); /* Enable : Use adaptive layer bitrate from H/W */
        }
    }

#ifdef USE_ANDROID
    HEVCCodecSetHdrInfo(pOMXComponent, pSrcInputData);
#endif

    if (pMFCHevcHandle->videoInstInfo.supportInfo.enc.bPrioritySupport == VIDEO_TRUE)
        pEncOps->Set_Priority(hMFCHandle, pVideoEnc->nPriority);

    if (pEncParam->commonParam.bDisableDFR == VIDEO_TRUE)
        pEncOps->Disable_DynamicFrameRate(pMFCHevcHandle->hMFCHandle, VIDEO_TRUE);

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

    pHevcEnc->hMFCHevcHandle.bConfiguredMFCSrc = OMX_TRUE;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HEVCCodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc            = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc             = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_MFC_HEVCENC_HANDLE     *pMFCHevcHandle       = &pHevcEnc->hMFCHevcHandle;
    void                          *hMFCHandle           = pMFCHevcHandle->hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort          = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pInputPort           = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pOutbufOps = pHevcEnc->hMFCHevcHandle.pOutbufOps;
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
        bufferConf.eCompressionFormat = VIDEO_CODING_HEVC;
        bufferConf.nSizeImage = nOutBufSize;
        bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pOutputPort);

        if (pOutbufOps->Set_Geometry(pHevcEnc->hMFCHevcHandle.hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
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

    if (pOutbufOps->Setup(pHevcEnc->hMFCHevcHandle.hMFCHandle, nOutputBufferCnt) != VIDEO_ERROR_NONE) {
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

    pHevcEnc->hMFCHevcHandle.bConfiguredMFCDst = OMX_TRUE;

    if (HEVCCodecStart(pOMXComponent, OUTPUT_PORT_INDEX) != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to run output buffer", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCEnc_GetParameter(
    OMX_IN    OMX_HANDLETYPE hComponent,
    OMX_IN    OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc        = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc         = NULL;

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
    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nParamIndex %x", pExynosComponent, __FUNCTION__, (int)nParamIndex);
    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoHevc:
    {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHevcComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHevcComponent = NULL;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstHevcComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstHevcComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcHevcComponent = &pHevcEnc->HevcComponent[pDstHevcComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstHevcComponent) + nOffset,
                           ((char *)pSrcHevcComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_HEVCTYPE) - nOffset);
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
            Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_DRM_ENC_ROLE);
        else
            Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_ENC_ROLE);
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
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel  = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE         *pSrcHevcComponent = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcHevcComponent = &pHevcEnc->HevcComponent[pDstProfileLevel->nPortIndex];

        pDstProfileLevel->eProfile = pSrcHevcComponent->eProfile;
        pDstProfileLevel->eLevel   = pSrcHevcComponent->eLevel;
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

        pSrcErrorCorrectionType = &pHevcEnc->errorCorrectionType[OUTPUT_PORT_INDEX];

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

        pQpRange->qpRangeI.nMinQP = pHevcEnc->qpRangeI.nMinQP;
        pQpRange->qpRangeI.nMaxQP = pHevcEnc->qpRangeI.nMaxQP;
        pQpRange->qpRangeP.nMinQP = pHevcEnc->qpRangeP.nMinQP;
        pQpRange->qpRangeP.nMaxQP = pHevcEnc->qpRangeP.nMaxQP;
        pQpRange->qpRangeB.nMinQP = pHevcEnc->qpRangeB.nMinQP;
        pQpRange->qpRangeB.nMaxQP = pHevcEnc->qpRangeB.nMaxQP;
    }
        break;
    case OMX_IndexParamVideoHevcEnableTemporalSVC:
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

        pDstEnableTemporalSVC->bEnableTemporalSVC = pHevcEnc->hMFCHevcHandle.bTemporalSVC;
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

        pDstEnableRoiInfo->bEnableRoiInfo = pHevcEnc->hMFCHevcHandle.bRoiInfo;
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

        pNumberRefPframes->nU32 = pHevcEnc->hMFCHevcHandle.nRefForPframes;
    }
        break;
    case OMX_IndexParamVideoEnableGPB:
    {
        OMX_CONFIG_BOOLEANTYPE *pEnableGPB  = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pEnableGPB, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pEnableGPB->bEnabled = pHevcEnc->hMFCHevcHandle.bGPBEnable;
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

        pChromaQP->nCr = pHevcEnc->chromaQPOffset.nCr;
        pChromaQP->nCb = pHevcEnc->chromaQPOffset.nCb;
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

        pDisableHBEncoding->bEnabled = pHevcEnc->bDisableHBEncoding;
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

        if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_FALSE) {
            pTemporalLayering->eSupportedPatterns = OMX_VIDEO_AndroidTemporalLayeringPatternNone;
            pTemporalLayering->nLayerCountMax     = 1;  /* not supported */
            pTemporalLayering->nBLayerCountMax    = 0;
            pTemporalLayering->ePattern           = OMX_VIDEO_AndroidTemporalLayeringPatternNone;
        } else {
            pTemporalLayering->eSupportedPatterns = OMX_VIDEO_AndroidTemporalLayeringPatternAndroid;

            if (pHevcEnc->hMFCHevcHandle.bTemporalSVC == OMX_TRUE) {
                pTemporalLayering->nLayerCountMax   = pHevcEnc->nMaxTemporalLayerCount;
                pTemporalLayering->nBLayerCountMax  = pHevcEnc->nMaxTemporalLayerCountForB;
                pTemporalLayering->ePattern         = OMX_VIDEO_AndroidTemporalLayeringPatternAndroid;
            } else {
                pTemporalLayering->nLayerCountMax   = OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS;
                pTemporalLayering->nBLayerCountMax  = OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS_FOR_B;
                pTemporalLayering->ePattern         = OMX_VIDEO_AndroidTemporalLayeringPatternNone;

                if (pHevcEnc->bDisableHBEncoding == OMX_TRUE) {
                    pTemporalLayering->nBLayerCountMax = 0;
                }
            }
        }

        pTemporalLayering->nPLayerCountActual = pHevcEnc->nTemporalLayerCount;
        pTemporalLayering->nBLayerCountActual = pHevcEnc->nTemporalLayerCountForB;

        pTemporalLayering->bBitrateRatiosSpecified = pHevcEnc->bUseTemporalLayerBitrateRatio;
        if (pTemporalLayering->bBitrateRatiosSpecified == OMX_TRUE) {
            for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++)
                pTemporalLayering->nBitrateRatios[i] = (pHevcEnc->nTemporalLayerBitrateRatio[i] << 16);
        }
    }
        break;
#endif
    default:
        ret = Exynos_OMX_VideoEncodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCEnc_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc        = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc         = NULL;

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
    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamVideoHevc:
    {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHevcComponent = NULL;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHevcComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)pComponentParameterStructure;

        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcHevcComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcHevcComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstHevcComponent = &pHevcEnc->HevcComponent[pSrcHevcComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstHevcComponent) + nOffset,
                           ((char *)pSrcHevcComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_HEVCTYPE) - nOffset);
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

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_ENC_ROLE) ||
            !Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_DRM_ENC_ROLE)) {
            pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
        } else {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel  = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE         *pDstHevcComponent = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pSrcProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstHevcComponent = &pHevcEnc->HevcComponent[pSrcProfileLevel->nPortIndex];
        if (OMX_FALSE == CheckProfileLevelSupport(pExynosComponent, pSrcProfileLevel)) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pDstHevcComponent->eProfile = pSrcProfileLevel->eProfile;
        pDstHevcComponent->eLevel   = pSrcProfileLevel->eLevel;
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

        pDstErrorCorrectionType = &pHevcEnc->errorCorrectionType[OUTPUT_PORT_INDEX];

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

        pHevcEnc->qpRangeI.nMinQP = pQpRange->qpRangeI.nMinQP;
        pHevcEnc->qpRangeI.nMaxQP = pQpRange->qpRangeI.nMaxQP;
        pHevcEnc->qpRangeP.nMinQP = pQpRange->qpRangeP.nMinQP;
        pHevcEnc->qpRangeP.nMaxQP = pQpRange->qpRangeP.nMaxQP;
        pHevcEnc->qpRangeB.nMinQP = pQpRange->qpRangeB.nMinQP;
        pHevcEnc->qpRangeB.nMaxQP = pQpRange->qpRangeB.nMaxQP;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamPrependSPSPPSToIDR:
    {

        ret = Exynos_OSAL_SetPrependSPSPPSToIDR(pComponentParameterStructure, &(pHevcEnc->hMFCHevcHandle.bPrependSpsPpsToIdr));
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
            if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_FALSE) {
                Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] Temporal SVC is not supported", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorUnsupportedIndex;
                goto EXIT;
            }

            if (pTemporalLayering->nLayerCountMax > OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] total layer : Max value(%d) > supportable Max value(%d)",
                                        pExynosComponent, __FUNCTION__, pTemporalLayering->nLayerCountMax, OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            if (pTemporalLayering->nBLayerCountMax > OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS_FOR_B) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] B layer : Max value(%d) > supportable Max value(%d)",
                                        pExynosComponent, __FUNCTION__, pTemporalLayering->nBLayerCountMax, OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS_FOR_B);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            pHevcEnc->hMFCHevcHandle.bTemporalSVC = OMX_TRUE;
            pHevcEnc->nMaxTemporalLayerCount      = pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual;
            pHevcEnc->nMaxTemporalLayerCountForB  = pTemporalLayering->nBLayerCountActual;
            pHevcEnc->nTemporalLayerCount         = pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual;
            pHevcEnc->nTemporalLayerCountForB     = pTemporalLayering->nBLayerCountActual;

            if (pHevcEnc->bDisableHBEncoding) {
                pHevcEnc->nMaxTemporalLayerCountForB  = 0;
                pHevcEnc->nTemporalLayerCountForB  = 0;
            }

            pHevcEnc->bUseTemporalLayerBitrateRatio = pTemporalLayering->bBitrateRatiosSpecified;
            if (pHevcEnc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
                for (i = 0; i < (int)pHevcEnc->nMaxTemporalLayerCount; i++)
                    pHevcEnc->nTemporalLayerBitrateRatio[i] = (pTemporalLayering->nBitrateRatios[i] >> 16);
            }

            /* HIGH_SPEED_ENCODING */
            if (pHevcEnc->nTemporalLayerCountForB > 0) {
                int minRequiredInputs = pow(2, pHevcEnc->nTemporalLayerCount - 1);
                if (minRequiredInputs > pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.nBufferCountActual) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Temporal Layering with B-frame, increase InputBufferNum(%d)", pExynosComponent, __FUNCTION__, minRequiredInputs);
                    pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.nBufferCountActual = minRequiredInputs;
                }
            }

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Max(%d), B-Max(%d), layer(%d), B-layer(%d)", pExynosComponent, __FUNCTION__,
                                                    pHevcEnc->nMaxTemporalLayerCount, pHevcEnc->nMaxTemporalLayerCountForB,
                                                    pHevcEnc->nTemporalLayerCount, pHevcEnc->nTemporalLayerCountForB);
        } else {
            /* DISABLE */
            pHevcEnc->hMFCHevcHandle.bTemporalSVC   = OMX_FALSE;
            pHevcEnc->nMaxTemporalLayerCount        = 0;
            pHevcEnc->nMaxTemporalLayerCountForB    = 0;
            pHevcEnc->nTemporalLayerCount           = 0;
            pHevcEnc->nTemporalLayerCountForB       = 0;
            pHevcEnc->bUseTemporalLayerBitrateRatio = OMX_FALSE;
            Exynos_OSAL_Memset(pHevcEnc->nTemporalLayerBitrateRatio, 0, sizeof(pHevcEnc->nTemporalLayerBitrateRatio));
        }
    }
        break;
#endif
    case OMX_IndexParamVideoHevcEnableTemporalSVC:
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

        if ((pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_FALSE) &&
            (pSrcEnableTemporalSVC->bEnableTemporalSVC == OMX_TRUE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Temporal SVC is not supported", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        if ((pSrcEnableTemporalSVC->bEnableTemporalSVC == OMX_TRUE) &&
            (pHevcEnc->hMFCHevcHandle.bTemporalSVC == OMX_FALSE)) {
            /* ENABLE : not initialized yet */
            pHevcEnc->hMFCHevcHandle.bTemporalSVC   = OMX_TRUE;
            pHevcEnc->nMaxTemporalLayerCount        = OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS;
            pHevcEnc->nMaxTemporalLayerCountForB    = OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS_FOR_B;
            pHevcEnc->nTemporalLayerCount           = 1;
            pHevcEnc->nTemporalLayerCountForB       = 0;
            pHevcEnc->bUseTemporalLayerBitrateRatio = OMX_TRUE;
            pHevcEnc->nTemporalLayerBitrateRatio[0] = pPortDef->format.video.nBitrate;
        } else if (pSrcEnableTemporalSVC->bEnableTemporalSVC == OMX_FALSE) {
            /* DISABLE */
            pHevcEnc->hMFCHevcHandle.bTemporalSVC   = OMX_FALSE;  /* DISABLE */
            pHevcEnc->nMaxTemporalLayerCount        = 0;
            pHevcEnc->nMaxTemporalLayerCountForB    = 0;
            pHevcEnc->nTemporalLayerCount           = 0;
            pHevcEnc->nTemporalLayerCountForB       = 0;
            pHevcEnc->bUseTemporalLayerBitrateRatio = OMX_FALSE;
            Exynos_OSAL_Memset(pHevcEnc->nTemporalLayerBitrateRatio, 0, sizeof(pHevcEnc->nTemporalLayerBitrateRatio));
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

        if ((pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bRoiInfoSupport == VIDEO_FALSE) &&
            (pSrcEnableRoiInfo->bEnableRoiInfo == OMX_TRUE)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Roi Info is not supported", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        pHevcEnc->hMFCHevcHandle.bRoiInfo = pSrcEnableRoiInfo->bEnableRoiInfo;
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

        if ((pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bPVCSupport == VIDEO_FALSE) &&
            (pEnablePVC->nU32 != 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] PVC mode is not supported", pExynosComponent, __FUNCTION__);
        }

        pVideoEnc->bPVCMode = (pEnablePVC->nU32 != 0)? OMX_TRUE:OMX_FALSE;
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

        pHevcEnc->hMFCHevcHandle.nRefForPframes = pNumberRefPframes->nU32;
    }
        break;
    case OMX_IndexParamVideoEnableGPB:
    {
        OMX_CONFIG_BOOLEANTYPE *pEnableGPB  = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pEnableGPB, sizeof(OMX_CONFIG_BOOLEANTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pHevcEnc->hMFCHevcHandle.bGPBEnable = pEnableGPB->bEnabled;
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
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] nCr 0x%x is invalid [min:-12, max:12]", pExynosComponent, __FUNCTION__, pChromaQP->nCr);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        if ((pChromaQP->nCb < -12) || (pChromaQP->nCb > 12)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] nCb 0x%x is invalid [min:-12, max:12]", pExynosComponent, __FUNCTION__, pChromaQP->nCb);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pHevcEnc->chromaQPOffset.nCr = pChromaQP->nCr;
        pHevcEnc->chromaQPOffset.nCb = pChromaQP->nCb;
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

        pHevcEnc->bDisableHBEncoding = pDisableHBEncoding->bEnabled;
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

OMX_ERRORTYPE Exynos_HEVCEnc_GetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc        = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc         = NULL;

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
    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;

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

        pQpRange->qpRangeI.nMinQP = pHevcEnc->qpRangeI.nMinQP;
        pQpRange->qpRangeI.nMaxQP = pHevcEnc->qpRangeI.nMaxQP;
        pQpRange->qpRangeP.nMinQP = pHevcEnc->qpRangeP.nMinQP;
        pQpRange->qpRangeP.nMaxQP = pHevcEnc->qpRangeP.nMaxQP;
        pQpRange->qpRangeB.nMinQP = pHevcEnc->qpRangeB.nMinQP;
        pQpRange->qpRangeB.nMaxQP = pHevcEnc->qpRangeB.nMaxQP;
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

        pDstTemporalSVC->nKeyFrameInterval   = pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval;
        pDstTemporalSVC->nMinQuantizer       = pHevcEnc->qpRangeI.nMinQP;
        pDstTemporalSVC->nMaxQuantizer       = pHevcEnc->qpRangeI.nMaxQP;
        pDstTemporalSVC->nTemporalLayerCount = pHevcEnc->nTemporalLayerCount;
        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++)
            pDstTemporalSVC->nTemporalLayerBitrateRatio[i] = pHevcEnc->nTemporalLayerBitrateRatio[i];
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoHdrStaticInfo:
        ret = Exynos_OSAL_GetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
#endif
    default:
        ret = Exynos_OMX_VideoEncodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCEnc_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE                   ret                 = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = NULL;

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
    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigVideoIntraPeriod:
    {
        pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval = *((OMX_U32 *)pComponentConfigStructure);
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

        if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bQpRangePBSupport == VIDEO_FALSE)
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] only I-frame's QP range is applied", pExynosComponent, __FUNCTION__);

        pHevcEnc->qpRangeI.nMinQP = pQpRange->qpRangeI.nMinQP;
        pHevcEnc->qpRangeI.nMaxQP = pQpRange->qpRangeI.nMaxQP;
        pHevcEnc->qpRangeP.nMinQP = pQpRange->qpRangeP.nMinQP;
        pHevcEnc->qpRangeP.nMaxQP = pQpRange->qpRangeP.nMaxQP;
        pHevcEnc->qpRangeB.nMinQP = pQpRange->qpRangeB.nMinQP;
        pHevcEnc->qpRangeB.nMaxQP = pQpRange->qpRangeB.nMaxQP;
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

        if ((pHevcEnc->hMFCHevcHandle.bTemporalSVC == OMX_FALSE) ||
            (pSrcTemporalSVC->nTemporalLayerCount > OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS)) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval = pSrcTemporalSVC->nKeyFrameInterval;

        pHevcEnc->qpRangeI.nMinQP = pSrcTemporalSVC->nMinQuantizer;
        pHevcEnc->qpRangeI.nMaxQP = pSrcTemporalSVC->nMaxQuantizer;

        pHevcEnc->nTemporalLayerCount = pSrcTemporalSVC->nTemporalLayerCount;
        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++)
            pHevcEnc->nTemporalLayerBitrateRatio[i] = pSrcTemporalSVC->nTemporalLayerBitrateRatio[i];
    }
        break;
    case OMX_IndexConfigVideoRoiInfo:
    {
        EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *pRoiInfo = (EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *)pComponentConfigStructure;

        if (pRoiInfo->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pHevcEnc->hMFCHevcHandle.bRoiInfo == OMX_FALSE) {
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

        if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bIFrameRatioSupport == VIDEO_FALSE) {
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
            (pHevcEnc->hMFCHevcHandle.bTemporalSVC == OMX_FALSE)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pTemporalLayering->ePattern == OMX_VIDEO_AndroidTemporalLayeringPatternNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can not disable TemporalSVC while encoding", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if (((pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual) > pHevcEnc->nMaxTemporalLayerCount) ||
            ((pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual) == 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] layer count is invalid(MAX(%d), layer(%d), B-layer(%d))",
                                    pExynosComponent, __FUNCTION__,
                                    pHevcEnc->nMaxTemporalLayerCount,
                                    pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual,
                                    pTemporalLayering->nBLayerCountActual);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pHevcEnc->nTemporalLayerCount     = pTemporalLayering->nPLayerCountActual + pTemporalLayering->nBLayerCountActual;
        if (pHevcEnc->bDisableHBEncoding == OMX_TRUE) {
            pHevcEnc->nTemporalLayerCountForB = 0;
        } else {
            pHevcEnc->nTemporalLayerCountForB = pTemporalLayering->nBLayerCountActual;
        }

        pHevcEnc->bUseTemporalLayerBitrateRatio = pTemporalLayering->bBitrateRatiosSpecified;
        if (pHevcEnc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
            for (i = 0; i < (int)pHevcEnc->nMaxTemporalLayerCount; i++)
                pHevcEnc->nTemporalLayerBitrateRatio[i] = (pTemporalLayering->nBitrateRatios[i] >> 16);
        } else {
            Exynos_OSAL_Memset(pHevcEnc->nTemporalLayerBitrateRatio, 0, sizeof(pHevcEnc->nTemporalLayerBitrateRatio));
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Max(%d), layer(%d), B-layer(%d)", pExynosComponent, __FUNCTION__,
                                               pHevcEnc->nMaxTemporalLayerCount, pHevcEnc->nTemporalLayerCount, pHevcEnc->nTemporalLayerCountForB);
    }
        break;
    case OMX_IndexConfigVideoHdrStaticInfo:
        ret = Exynos_OSAL_SetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
#endif
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

OMX_ERRORTYPE Exynos_HEVCEnc_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = NULL;

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
    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_TEMPORALSVC) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigVideoTemporalSVC;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_VIDEO_HEVC_ENABLE_TEMPORALSVC) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamVideoHevcEnableTemporalSVC;
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

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_REF_NUM_PFRAMES) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamNumberRefPframes;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_VIDEO_ENABLE_GPB) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamVideoEnableGPB;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_PREPEND_SPSPPS_TO_IDR) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamPrependSPSPPSToIDR;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_HDR_STATIC_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoHdrStaticInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    ret = Exynos_OMX_VideoEncodeGetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCEnc_ComponentRoleEnum(
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
            Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_HEVC_ENC_ROLE);
        else
            Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_HEVC_DRM_ENC_ROLE);

        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_HEVCEnc_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT             *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    OMX_COLOR_FORMATTYPE             eColorFormat       = pInputPort->portDefinition.format.video.eColorFormat;

    ExynosVideoInstInfo *pVideoInstInfo = &(pHevcEnc->hMFCHevcHandle.videoInstInfo);

    CSC_METHOD csc_method = CSC_METHOD_SW;

    FunctionIn();

    pHevcEnc->hMFCHevcHandle.bConfiguredMFCSrc = OMX_FALSE;
    pHevcEnc->hMFCHevcHandle.bConfiguredMFCDst = OMX_FALSE;
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

    /* HEVC Codec Open */
    ret = HEVCCodecOpen(pHevcEnc, pVideoInstInfo);
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

    pHevcEnc->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pHevcEnc->hSourceStartEvent);
    pHevcEnc->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pHevcEnc->hDestinationInStartEvent);
    Exynos_OSAL_SignalCreate(&pHevcEnc->hDestinationOutStartEvent);

    Exynos_OSAL_Memset(pExynosComponent->bTimestampSlotUsed, 0, sizeof(OMX_BOOL) * MAX_TIMESTAMP);
    INIT_ARRAY_TO_VAL(pExynosComponent->timeStamp, DEFAULT_TIMESTAMP_VAL, MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pHevcEnc->hMFCHevcHandle.indexTimestamp = 0;
    pHevcEnc->hMFCHevcHandle.outputIndexTimestamp = 0;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    Exynos_OSAL_QueueCreate(&pHevcEnc->bypassBufferInfoQ, QUEUE_ELEMENTS);

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
OMX_ERRORTYPE Exynos_HEVCEnc_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
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
            EXYNOS_HEVCENC_HANDLE *pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;

            if (pVideoEnc->csc_handle != NULL) {
                csc_deinit(pVideoEnc->csc_handle);
                pVideoEnc->csc_handle = NULL;
            }

            if (pHevcEnc != NULL) {
                Exynos_OSAL_QueueTerminate(&pHevcEnc->bypassBufferInfoQ);

                Exynos_OSAL_SignalTerminate(pHevcEnc->hDestinationInStartEvent);
                pHevcEnc->hDestinationInStartEvent = NULL;
                Exynos_OSAL_SignalTerminate(pHevcEnc->hDestinationOutStartEvent);
                pHevcEnc->hDestinationOutStartEvent = NULL;
                pHevcEnc->bDestinationStart = OMX_FALSE;

                Exynos_OSAL_SignalTerminate(pHevcEnc->hSourceStartEvent);
                pHevcEnc->hSourceStartEvent = NULL;
                pHevcEnc->bSourceStart = OMX_FALSE;
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

            if (pHevcEnc != NULL) {
                HEVCCodecClose(pHevcEnc);
            }
        }
    }

    Exynos_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCEnc_SrcIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle       = pHevcEnc->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort       = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE           inputColorFormat  = OMX_COLOR_FormatUnused;

    ExynosVideoEncOps       *pEncOps     = pHevcEnc->hMFCHevcHandle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps   = pHevcEnc->hMFCHevcHandle.pInbufOps;
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
            Exynos_OSAL_Performance(pVideoEnc->pPerfHandle, pHevcEnc->hMFCHevcHandle.indexTimestamp, fps);
    }
#endif

    if (pHevcEnc->hMFCHevcHandle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = HEVCCodecSrcSetup(pOMXComponent, pSrcInputData);
        if ((ret != OMX_ErrorNone) ||
            ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to HEVCCodecSrcSetup(0x%x)",
                                                pExynosComponent, __FUNCTION__);
            goto EXIT;
        }
    }

    if (pHevcEnc->hMFCHevcHandle.bConfiguredMFCDst == OMX_FALSE) {
        ret = HEVCCodecDstSetup(pOMXComponent);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to HEVCCodecDstSetup(0x%x)",
                                                pExynosComponent, __FUNCTION__);
            goto EXIT;
        }
    }

    nConfigCnt = Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ);
    if (nConfigCnt > 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] has config message(%d)", pExynosComponent, __FUNCTION__, nConfigCnt);
        while (Exynos_OSAL_GetElemNum(&pExynosComponent->dynamicConfigQ) > 0) {
            Change_HEVCEnc_Param(pExynosComponent);
        }
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] all config messages were handled", pExynosComponent, __FUNCTION__);
    }

    /* DRC on Executing state through OMX_IndexConfigCommonOutputSize */
    if (pVideoEnc->bEncDRC == OMX_TRUE) {
        ret = HevcCodecUpdateResolution(pOMXComponent);
        pVideoEnc->bEncDRC = OMX_FALSE;
        goto EXIT;
    }

    if ((pSrcInputData->dataLen > 0) ||
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        pExynosComponent->timeStamp[pHevcEnc->hMFCHevcHandle.indexTimestamp]            = pSrcInputData->timeStamp;
        pExynosComponent->bTimestampSlotUsed[pHevcEnc->hMFCHevcHandle.indexTimestamp]   = OMX_TRUE;
        pExynosComponent->nFlags[pHevcEnc->hMFCHevcHandle.indexTimestamp]               = pSrcInputData->nFlags;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] input / buffer header(%p), nFlags: 0x%x, timestamp %lld us (%.2f secs), Tag: %d",
                                                        pExynosComponent, __FUNCTION__,
                                                        pSrcInputData->bufferHeader, pSrcInputData->nFlags,
                                                        pSrcInputData->timeStamp, (double)(pSrcInputData->timeStamp / 1E6),
                                                        pHevcEnc->hMFCHevcHandle.indexTimestamp);

        pEncOps->Set_FrameTag(hMFCHandle, pHevcEnc->hMFCHevcHandle.indexTimestamp);
        pHevcEnc->hMFCHevcHandle.indexTimestamp++;
        pHevcEnc->hMFCHevcHandle.indexTimestamp %= MAX_TIMESTAMP;

#ifdef PERFORMANCE_DEBUG
        Exynos_OSAL_V4L2CountIncrease(pExynosInputPort->hBufferCount, pSrcInputData->bufferHeader, INPUT_PORT_INDEX);
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

                /* y-sum */
                if (pHevcEnc->hMFCHevcHandle.bWeightedPrediction == OMX_TRUE) {
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

            /* update hdr dynamic info */
            if (pHevcEnc->hMFCHevcHandle.bHDRDynamicInfo == OMX_TRUE)
                HEVCCodecUpdateHdrDynamicInfo(pOMXComponent, pSrcInputData);
        }

        if ((pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bDropControlSupport == VIDEO_TRUE) &&
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

        HEVCCodecStart(pOMXComponent, INPUT_PORT_INDEX);
        if (pHevcEnc->bSourceStart == OMX_FALSE) {
            pHevcEnc->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pHevcEnc->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }

        if ((pHevcEnc->bDestinationStart == OMX_FALSE) &&
            (pHevcEnc->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE)) {
            pHevcEnc->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pHevcEnc->hDestinationInStartEvent);
            Exynos_OSAL_SignalSet(pHevcEnc->hDestinationOutStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCEnc_SrcOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                            *hMFCHandle         = pHevcEnc->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pInbufOps      = pHevcEnc->hMFCHevcHandle.pInbufOps;
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

OMX_ERRORTYPE Exynos_HEVCEnc_DstIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc            = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc             = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    void                          *hMFCHandle           = pHevcEnc->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort          = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pInputPort           = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoEncBufferOps *pOutbufOps  = pHevcEnc->hMFCHevcHandle.pOutbufOps;
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

    HEVCCodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCEnc_DstOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    EXYNOS_OMX_BASEPORT           *pOutputPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    void                          *hMFCHandle        = pHevcEnc->hMFCHevcHandle.hMFCHandle;

    ExynosVideoEncOps          *pEncOps        = pHevcEnc->hMFCHevcHandle.pEncOps;
    ExynosVideoEncBufferOps    *pOutbufOps     = pHevcEnc->hMFCHevcHandle.pOutbufOps;
    ExynosVideoBuffer          *pVideoBuffer   = NULL;
    ExynosVideoBuffer           videoBuffer;
    ExynosVideoErrorType        codecReturn    = VIDEO_ERROR_NONE;

    OMX_S32 indexTimestamp = 0;

    FunctionIn();

    if (pHevcEnc->bDestinationStart == OMX_FALSE) {
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
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] codec specific data is generated", pExynosComponent, __FUNCTION__);
        pDstOutputData->timeStamp = 0;
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
        pDstOutputData->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
        pVideoEnc->bFirstOutput = OMX_TRUE;
    } else {
        indexTimestamp = pEncOps->Get_FrameTag(pHevcEnc->hMFCHevcHandle.hMFCHandle);
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
                indexTimestamp              = pHevcEnc->hMFCHevcHandle.outputIndexTimestamp;
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
                                                indexTimestamp, pHevcEnc->hMFCHevcHandle.outputIndexTimestamp);
            indexTimestamp = pHevcEnc->hMFCHevcHandle.outputIndexTimestamp;
        } else {
            pHevcEnc->hMFCHevcHandle.outputIndexTimestamp = indexTimestamp;
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
        Exynos_OSAL_V4L2CountDecrease(pExynosOutputPort->hBufferCount, pDstOutputData->bufferHeader, OUTPUT_PORT_INDEX);
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

OMX_ERRORTYPE Exynos_HEVCEnc_srcInputBufferProcess(
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

    ret = Exynos_HEVCEnc_SrcIn(pOMXComponent, pSrcInputData);
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

OMX_ERRORTYPE Exynos_HEVCEnc_srcOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
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

    if ((pHevcEnc->bSourceStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pInputPort))) {
        Exynos_OSAL_SignalWait(pHevcEnc->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get SourceStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pHevcEnc->hSourceStartEvent);
    }

    ret = Exynos_HEVCEnc_SrcOut(pOMXComponent, pSrcOutputData);
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

OMX_ERRORTYPE Exynos_HEVCEnc_dstInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
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

    if ((pHevcEnc->bDestinationStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
        Exynos_OSAL_SignalWait(pHevcEnc->hDestinationInStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationInStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pHevcEnc->hDestinationInStartEvent);
    }

    if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
        if (Exynos_OSAL_GetElemNum(&pHevcEnc->bypassBufferInfoQ) > 0) {
            BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Dequeue(&pHevcEnc->bypassBufferInfoQ);
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

    if (pHevcEnc->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_HEVCEnc_DstIn(pOMXComponent, pDstInputData);
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

OMX_ERRORTYPE Exynos_HEVCEnc_dstOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
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

    if ((pHevcEnc->bDestinationStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
        Exynos_OSAL_SignalWait(pHevcEnc->hDestinationOutStartEvent, DEF_MAX_WAIT_TIME);
        if (pVideoEnc->bExitBufferProcessThread)
            goto EXIT;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] get DestinationOutStartEvent", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_SignalReset(pHevcEnc->hDestinationOutStartEvent);
    }

    if (pOutputPort->bufferProcessType & BUFFER_COPY) {
        if (Exynos_OSAL_GetElemNum(&pHevcEnc->bypassBufferInfoQ) > 0) {
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

            pBufferInfo = Exynos_OSAL_Dequeue(&pHevcEnc->bypassBufferInfoQ);
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

    ret = Exynos_HEVCEnc_DstOut(pOMXComponent, pDstOutputData);
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
    EXYNOS_HEVCENC_HANDLE         *pHevcEnc         = NULL;
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

    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_HEVC_ENC, componentName) == 0) {
        bSecureMode = OMX_FALSE;
    } else if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_HEVC_DRM_ENC, componentName) == 0) {
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

    pHevcEnc = Exynos_OSAL_Malloc(sizeof(EXYNOS_HEVCENC_HANDLE));
    if (pHevcEnc == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pHevcEnc, 0, sizeof(EXYNOS_HEVCENC_HANDLE));

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoEnc->hCodecHandle = (OMX_HANDLETYPE)pHevcEnc;
    pHevcEnc->qpRangeI.nMinQP = 0;
    pHevcEnc->qpRangeI.nMaxQP = 50;
    pHevcEnc->qpRangeP.nMinQP = 0;
    pHevcEnc->qpRangeP.nMaxQP = 50;
    pHevcEnc->qpRangeB.nMinQP = 0;
    pHevcEnc->qpRangeB.nMaxQP = 50;

    pVideoEnc->quantization.nQpI = 29;
    pVideoEnc->quantization.nQpP = 30;
    pVideoEnc->quantization.nQpB = 32;

    pHevcEnc->chromaQPOffset.nCr = 0;
    pHevcEnc->chromaQPOffset.nCb = 0;

    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_HEVC_DRM_ENC);
    else
        Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_HEVC_ENC);

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
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/avc");
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->portWayType = WAY2_PORT;
    pExynosPort->ePlaneType = PLANE_SINGLE;

    for(i = 0; i < ALL_PORT_NUM; i++) {
        INIT_SET_SIZE_VERSION(&pHevcEnc->HevcComponent[i], OMX_VIDEO_PARAM_HEVCTYPE);
        pHevcEnc->HevcComponent[i].nPortIndex = i;
        pHevcEnc->HevcComponent[i].eProfile   = OMX_VIDEO_HEVCProfileMain;
        pHevcEnc->HevcComponent[i].eLevel     = OMX_VIDEO_HEVCMainTierLevel4;
    }
    pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval = 30;

    pHevcEnc->hMFCHevcHandle.bTemporalSVC   = OMX_FALSE;
    pHevcEnc->nMaxTemporalLayerCount        = 0;
    pHevcEnc->nTemporalLayerCount           = 0;
    pHevcEnc->bUseTemporalLayerBitrateRatio = 0;
    Exynos_OSAL_Memset(pHevcEnc->nTemporalLayerBitrateRatio, 0, sizeof(pHevcEnc->nTemporalLayerBitrateRatio));

    pOMXComponent->GetParameter      = &Exynos_HEVCEnc_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_HEVCEnc_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_HEVCEnc_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_HEVCEnc_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_HEVCEnc_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_HEVCEnc_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_HEVCEnc_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_HEVCEnc_Terminate;

    pVideoEnc->exynos_codec_srcInputProcess  = &Exynos_HEVCEnc_srcInputBufferProcess;
    pVideoEnc->exynos_codec_srcOutputProcess = &Exynos_HEVCEnc_srcOutputBufferProcess;
    pVideoEnc->exynos_codec_dstInputProcess  = &Exynos_HEVCEnc_dstInputBufferProcess;
    pVideoEnc->exynos_codec_dstOutputProcess = &Exynos_HEVCEnc_dstOutputBufferProcess;

    pVideoEnc->exynos_codec_start            = &HEVCCodecStart;
    pVideoEnc->exynos_codec_stop             = &HEVCCodecStop;
    pVideoEnc->exynos_codec_bufferProcessRun = &HEVCCodecOutputBufferProcessRun;
    pVideoEnc->exynos_codec_enqueueAllBuffer = &HEVCCodecEnqueueAllBuffer;

    pVideoEnc->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;

    pVideoEnc->exynos_codec_checkFormatSupport = &CheckFormatHWSupport;

    pVideoEnc->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoEnc->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to SharedMemory_Open", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pHevcEnc);
        pHevcEnc = pVideoEnc->hCodecHandle = NULL;
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pHevcEnc->hMFCHevcHandle.videoInstInfo.eCodecType = VIDEO_CODING_HEVC;
    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        pHevcEnc->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_SECURE;
    else
        pHevcEnc->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_NORMAL;

    pHevcEnc->hMFCHevcHandle.nRefForPframes = 1;

    if (Exynos_Video_GetInstInfo(&(pHevcEnc->hMFCHevcHandle.videoInstInfo), VIDEO_FALSE /* enc */) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to GetInstInfo", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pHevcEnc);
        pHevcEnc = pVideoEnc->hCodecHandle = NULL;
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] GetInstInfo for enc : temporal-svc(%d)/roi(%d)/qp-range(%d)/pvc(%d)/I-ratio(%d)/CA(%d)AdaptiveBR(%d)/HDR-ST(%d)/HDR-DY(%d)/ChromaQP(%d)",
            pExynosComponent, __FUNCTION__,
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bRoiInfoSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bQpRangePBSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bPVCSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bIFrameRatioSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bColorAspectsSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bAdaptiveLayerBitrateSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bHDRStaticInfoSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bHDRDynamicInfoSupport),
            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bChromaQpSupport));

    Exynos_Input_SetSupportFormat(pExynosComponent);
    SetProfileLevel(pExynosComponent);

#ifdef USE_ANDROID
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-qp-range",     (OMX_INDEXTYPE)OMX_IndexConfigVideoQPRange);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-ref-pframes",  (OMX_INDEXTYPE)OMX_IndexParamNumberRefPframes);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-gpb",          (OMX_INDEXTYPE)OMX_IndexParamVideoEnableGPB);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-drop-control", (OMX_INDEXTYPE)OMX_IndexParamVideoDropControl);
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-disable-dfr", (OMX_INDEXTYPE)OMX_IndexParamVideoDisableDFR);
    if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bChromaQpSupport == VIDEO_TRUE)
        Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-chroma-qp-offset", (OMX_INDEXTYPE)OMX_IndexParamVideoChromaQP);
    if (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_TRUE)
        Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-disable-hierarchical-b-encoding", (OMX_INDEXTYPE)OMX_IndexParamVideoDisableHBEncoding);
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
    EXYNOS_HEVCENC_HANDLE           *pHevcEnc           = NULL;

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

    pHevcEnc = (EXYNOS_HEVCENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc != NULL) {
        Exynos_OSAL_Free(pHevcEnc);
        pHevcEnc = pVideoEnc->hCodecHandle = NULL;
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
