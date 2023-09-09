/*
 *
 * Copyright 2017 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OMX_HEVCenc_wfd.c
 * @brief
 * @author      Taehwan Kim    (t_h.kim@samsung.com)
 *              ByungGwan Kang (bk0917.kang@samsung.com)
 * @version     2.0.0
 * @history
 *   2018.07.13 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_VencControl.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OMX_HEVCenc_wfd.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OSAL_Queue.h"

#include "Exynos_OSAL_Platform.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_HEVC_WFD_ENC"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

typedef struct _OMX_BUFFERHEADER_ARRAY {
    OMX_BUFFERHEADERTYPE *pBuffer;
    OMX_BOOL              bInOMX;
} OMX_BUFFERHEADER_ARRAY;

static OMX_BUFFERHEADER_ARRAY inputBufArray[MAX_BUFFER_NUM];
static OMX_BUFFERHEADER_ARRAY outputBufArray[MAX_BUFFER_NUM];

static OMX_ERRORTYPE SetProfileLevel(
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_ERRORTYPE                    ret            = OMX_ErrorNone;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc       = NULL;

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

    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
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
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc       = NULL;

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

    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
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
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc   = NULL;

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

    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
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

static OMX_U32 OMXHevcProfileToProfileIDC(OMX_VIDEO_HEVCPROFILETYPE profile)
{
    OMX_U32 ret = 0;

    switch (profile) {
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

static OMX_U32 OMXHevcLevelToTierIDC(OMX_VIDEO_HEVCLEVELTYPE level)
{
    OMX_U32 ret = 0; //default Main tier

    switch (level) {
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

/* need to add to header file */
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

static void Print_HEVCWFDEnc_Param(ExynosVideoEncParam *pEncParam)
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

    /* rate control related parameters */
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableFRMRateControl    : %d", pCommonParam->EnableFRMRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "EnableMBRateControl     : %d", pCommonParam->EnableMBRateControl);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "CBRPeriodRf             : %d", pCommonParam->CBRPeriodRf);
}

static void Set_HEVCWFDEnc_Param(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    EXYNOS_OMX_BASEPORT            *pInputPort        = NULL;
    EXYNOS_OMX_BASEPORT            *pOutputPort       = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT  *pVideoEnc         = NULL;
    EXYNOS_HEVCWFDENC_HANDLE       *pHevcEnc          = NULL;
    EXYNOS_MFC_HEVCWFDENC_HANDLE   *pMFCHevcWFDHandle = NULL;
    OMX_COLOR_FORMATTYPE            eColorFormat      = OMX_COLOR_FormatUnused;

    ExynosVideoEncParam       *pEncParam    = NULL;
    ExynosVideoEncInitParam   *pInitParam   = NULL;
    ExynosVideoEncCommonParam *pCommonParam = NULL;
    ExynosVideoEncHevcParam   *pHevcParam   = NULL;

    int i;

    pVideoEnc           = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    pHevcEnc            = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
    pMFCHevcWFDHandle   = &pHevcEnc->hMFCHevcHandle;
    pInputPort          = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pOutputPort         = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    pEncParam       = &pMFCHevcWFDHandle->encParam;
    pInitParam      = &pEncParam->initParam;
    pCommonParam    = &pEncParam->commonParam;
    pHevcParam      = &pEncParam->codecParam.hevc;
    pEncParam->eCompressionFormat = VIDEO_CODING_HEVC;

    /* common parameters */
    if ((pVideoEnc->eRotationType == ROTATE_0) ||
        (pVideoEnc->eRotationType == ROTATE_180)) {
        pCommonParam->SourceWidth  = pOutputPort->portDefinition.format.video.nFrameWidth;
        pCommonParam->SourceHeight = pOutputPort->portDefinition.format.video.nFrameHeight;
    } else {
        pCommonParam->SourceWidth  = pOutputPort->portDefinition.format.video.nFrameHeight;
        pCommonParam->SourceHeight = pOutputPort->portDefinition.format.video.nFrameWidth;
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

    /* HEVC specific parameters */
    pHevcParam->ProfileIDC = OMXHevcProfileToProfileIDC(pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].eProfile);
    pHevcParam->TierIDC    = OMXHevcLevelToTierIDC(pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].eLevel);
    pHevcParam->LevelIDC   = OMXHevcLevelToLevelIDC(pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].eLevel);

    pHevcParam->FrameQp_B  = pVideoEnc->quantization.nQpB;
    pHevcParam->FrameRate  = (pInputPort->portDefinition.format.video.xFramerate) >> 16;

    /* there is no interface at OMX IL component */
    pHevcParam->NumberBFrames         = 0;    /* 0 ~ 2 */
    pHevcParam->NumberRefForPframes   = 1;    /* 1 ~ 2 */

    pHevcParam->MaxPartitionDepth     = 1;
    pHevcParam->LoopFilterDisable     = 0;    /* 1: Loop Filter Disable, 0: Filter Enable */
    pHevcParam->LoopFilterSliceFlag   = 1;
    pHevcParam->LoopFilterTcOffset    = 0;
    pHevcParam->LoopFilterBetaOffset  = 0;

    pHevcParam->LongtermRefEnable     = 0;
    pHevcParam->LongtermUserRef       = 0;
    pHevcParam->LongtermStoreRef      = 0;

    pHevcParam->DarkDisable           = 1;    /* disable adaptive rate control on dark region */
    pHevcParam->SmoothDisable         = 1;    /* disable adaptive rate control on smooth region */
    pHevcParam->StaticDisable         = 1;    /* disable adaptive rate control on static region */
    pHevcParam->ActivityDisable       = 1;    /* disable adaptive rate control on high activity region */

    /* Temporal SVC */
    /* If MaxTemporalLayerCount value is 0, codec supported max value will be set */
    pHevcParam->MaxTemporalLayerCount           = 0;
    pHevcParam->TemporalSVC.nTemporalLayerCount = (unsigned int)pHevcEnc->nTemporalLayerCount;

    if (pHevcEnc->bUseTemporalLayerBitrateRatio == OMX_TRUE) {
        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++)
            pHevcParam->TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)pHevcEnc->nTemporalLayerBitrateRatio[i];
    } else {
        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++)
            pHevcParam->TemporalSVC.nTemporalLayerBitrateRatio[i] = pOutputPort->portDefinition.format.video.nBitrate;
    }

    pHevcParam->ROIEnable = (pMFCHevcWFDHandle->bRoiInfo == OMX_TRUE)? 1:0;

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

OMX_BOOL CheckFormatHWSupport(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_COLOR_FORMATTYPE         eColorFormat)
{
    OMX_BOOL                         ret            = OMX_FALSE;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc       = NULL;
    EXYNOS_OMX_BASEPORT             *pInputPort     = NULL;
    ExynosVideoColorFormatType       eVideoFormat   = VIDEO_COLORFORMAT_UNKNOWN;
    int i;

    if (pExynosComponent == NULL)
        goto EXIT;

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoEnc == NULL)
        goto EXIT;

    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
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

OMX_ERRORTYPE HEVCWFDCodecOpen(
    EXYNOS_HEVCWFDENC_HANDLE   *pHevcEnc,
    ExynosVideoInstInfo        *pVideoInstInfo)
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

OMX_ERRORTYPE HEVCWFDCodecClose(EXYNOS_HEVCWFDENC_HANDLE *pHevcEnc)
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

OMX_ERRORTYPE HEVCWFDCodecStart(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;
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

    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
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

OMX_ERRORTYPE HEVCWFDCodecStop(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;
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

    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
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

OMX_ERRORTYPE Exynos_HEVCWFDEnc_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;
    int                              i;

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
    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;

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

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_WFD_ENC_ROLE);
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
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHevcComponent = NULL;

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
        pDstProfileLevel->eLevel = pSrcHevcComponent->eLevel;
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
    case OMX_IndexParamVideoInit:
    {
        OMX_PORT_PARAM_TYPE *pPortParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pPortParam->nPorts           = pExynosComponent->portParam.nPorts;
        pPortParam->nStartPortNumber = pExynosComponent->portParam.nStartPortNumber;
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex  = pPortFormat->nPortIndex;
        OMX_U32                         nIndex      = pPortFormat->nIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef    = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (nPortIndex == INPUT_PORT_INDEX) {
            if (nIndex > (INPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1)) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
            pPortDef = &pExynosPort->portDefinition;

            pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;

            if (pExynosPort->supportFormat[nIndex] == OMX_COLOR_FormatUnused) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }
            pPortFormat->eColorFormat = pExynosPort->supportFormat[nIndex];
        } else if (nPortIndex == OUTPUT_PORT_INDEX) {
            if (nIndex > (OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1)) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            pPortDef = &pExynosPort->portDefinition;

            pPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
            pPortFormat->eColorFormat       = pPortDef->format.video.eColorFormat;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE     *pVideoBitrate  = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32                          nPortIndex     = pVideoBitrate->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE    *pPortDef       = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
            if (pVideoEnc == NULL) {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoBitrate->eControlRate = pVideoEnc->eControlRate[nPortIndex];
            pVideoBitrate->nTargetBitrate = pPortDef->format.video.nBitrate;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoQuantization:
    {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE  *pVideoQuantization = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;
        OMX_U32                            nPortIndex         = pVideoQuantization->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE      *pPortDef           = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoQuantization->nQpI = pVideoEnc->quantization.nQpI;
            pVideoQuantization->nQpP = pVideoEnc->quantization.nQpP;
            pVideoQuantization->nQpB = pVideoEnc->quantization.nQpB;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                       portIndex      = portDefinition->nPortIndex;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        if (portIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = Exynos_OMX_Check_SizeVersion(portDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[portIndex];
        Exynos_OSAL_Memcpy(((char *)portDefinition) + nOffset,
                           ((char *)&pExynosPort->portDefinition) + nOffset,
                           portDefinition->nSize - nOffset);

        ret = Exynos_OSAL_GetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        if (ret != OMX_ErrorNone)
            goto EXIT;
    }
        break;
    case OMX_IndexVendorNeedContigMemory:
    {
        EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *pPortMemType = (EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *)pComponentParameterStructure;
        OMX_U32                             nPortIndex   = pPortMemType->nPortIndex;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPortMemType, sizeof(EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        pPortMemType->bNeedContigMem = pExynosPort->bNeedContigMem;
    }
        break;
    case OMX_IndexParamVideoIntraRefresh:
    {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
        OMX_U32                           nPortIndex    = pIntraRefresh->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pIntraRefresh, sizeof(OMX_VIDEO_PARAM_INTRAREFRESHTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pIntraRefresh->eRefreshMode = pVideoEnc->intraRefresh.eRefreshMode;
        pIntraRefresh->nAirMBs      = pVideoEnc->intraRefresh.nAirMBs;
        pIntraRefresh->nAirRef      = pVideoEnc->intraRefresh.nAirRef;
        pIntraRefresh->nCirMBs      = pVideoEnc->intraRefresh.nCirMBs;

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamRotationInfo:
    {
        EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *pRotationInfo = (EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *)pComponentParameterStructure;
        OMX_U32                               nPortIndex    = pRotationInfo->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pRotationInfo, sizeof(EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pRotationInfo->eRotationType = pVideoEnc->eRotationType;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamConsumerUsageBits:
    {
        ret = Exynos_OSAL_GetParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
#endif
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit:
    {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        portParam->nPorts               = 0;
        portParam->nStartPortNumber     = 0;
    }
        break;
    case OMX_IndexParamPriorityMgmt:
    {
        OMX_PRIORITYMGMTTYPE *compPriority = (OMX_PRIORITYMGMTTYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(compPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        compPriority->nGroupID       = pExynosComponent->compPriority.nGroupID;
        compPriority->nGroupPriority = pExynosComponent->compPriority.nGroupPriority;
    }
        break;

    case OMX_IndexParamCompBufferSupplier:
    {
        OMX_PARAM_BUFFERSUPPLIERTYPE *bufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pComponentParameterStructure;
        OMX_U32                       portIndex = bufferSupplier->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort;

        if ((pExynosComponent->currentState == OMX_StateLoaded) ||
            (pExynosComponent->currentState == OMX_StateWaitForResources)) {
            if (portIndex >= pExynosComponent->portParam.nPorts) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
            ret = Exynos_OMX_Check_SizeVersion(bufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
            if (ret != OMX_ErrorNone) {
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[portIndex];


            if (pExynosPort->portDefinition.eDir == OMX_DirInput) {
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;
                } else if (CHECK_PORT_TUNNELED(pExynosPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;
                } else {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            } else {
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;
                } else if (CHECK_PORT_TUNNELED(pExynosPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;
                } else {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            }
        }
        else
        {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
    }
        break;
    default:
    {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCWFDEnc_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;
    int                              i;

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
    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;

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

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_WFD_ENC_ROLE)) {
            pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
        } else {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE          *pDstHevcComponent = NULL;

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
        pDstHevcComponent->eLevel = pSrcProfileLevel->eLevel;
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
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] MFC D/D doesn't support Roi Info", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        pHevcEnc->hMFCHevcHandle.bRoiInfo = pSrcEnableRoiInfo->bEnableRoiInfo;
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
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] pOutputPort->portDefinition.nBufferSize: %d", pExynosComponent, __FUNCTION__, pExynosPort->portDefinition.nBufferSize);
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

        if ((pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bPVCSupport == VIDEO_FALSE) &&
            (pEnablePVC->nU32 != 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] PVC mode is not supported", pExynosComponent, __FUNCTION__);
        }

        pVideoEnc->bPVCMode = (pEnablePVC->nU32)? OMX_TRUE:OMX_FALSE;
    }
        break;

    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex  = pPortFormat->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef    = NULL;

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
            ((pPortFormat->xFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] xFramerate is invalid(%d)",
                                              pExynosComponent, __FUNCTION__, pPortFormat->xFramerate >> 16);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pPortDef->format.video.eColorFormat       = pPortFormat->eColorFormat;
        pPortDef->format.video.eCompressionFormat = pPortFormat->eCompressionFormat;
        pPortDef->format.video.xFramerate         = pPortFormat->xFramerate;
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
            pPortDef = &(pExynosComponent->pExynosPort[nPortIndex].portDefinition);
            pVideoEnc->eControlRate[nPortIndex] = pVideoBitrate->eControlRate;
            pPortDef->format.video.nBitrate = pVideoBitrate->nTargetBitrate;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoQuantization:
    {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE *pVideoQuantization = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;
        OMX_U32                           nPortIndex         = pVideoQuantization->nPortIndex;
        OMX_PARAM_PORTDEFINITIONTYPE     *pPortDef           = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoEnc->quantization.nQpI = pVideoQuantization->nQpI;
            pVideoEnc->quantization.nQpP = pVideoQuantization->nQpP;
            pVideoEnc->quantization.nQpB = pVideoQuantization->nQpB;
        }
        ret = OMX_ErrorNone;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexParamStoreMetaDataBuffer:
    case OMX_IndexParamAllocateNativeHandle:
    {
        ret = Exynos_OSAL_SetParameter(hComponent, nIndex, pComponentParameterStructure);
    }
        break;
#endif
    case OMX_IndexVendorNeedContigMemory:
    {
        EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *pPortMemType = (EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE *)pComponentParameterStructure;
        OMX_U32                             nPortIndex   = pPortMemType->nPortIndex;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPortMemType, sizeof(EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE));
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

        pExynosPort->bNeedContigMem = pPortMemType->bNeedContigMem;
    }
        break;
    case OMX_IndexParamVideoIntraRefresh:
    {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
        OMX_U32                           nPortIndex    = pIntraRefresh->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pIntraRefresh, sizeof(OMX_VIDEO_PARAM_INTRAREFRESHTYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (pIntraRefresh->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic) {
            pVideoEnc->intraRefresh.eRefreshMode    = pIntraRefresh->eRefreshMode;
            pVideoEnc->intraRefresh.nCirMBs         = pIntraRefresh->nCirMBs;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "OMX_VIDEO_IntraRefreshCyclic Enable, nCirMBs: %d",
                            pVideoEnc->intraRefresh.nCirMBs);
        } else {
            ret = OMX_ErrorUnsupportedSetting;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamEnableBlurFilter:
    {
        EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER *pBlurMode  = (EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER *)pComponentParameterStructure;
        OMX_U32                                   nPortIndex = pBlurMode->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pBlurMode, sizeof(EXYNOS_OMX_VIDEO_PARAM_ENABLE_BLURFILTER));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pVideoEnc->bUseBlurFilter = pBlurMode->bUseBlurFilter;
    }
        break;
    case OMX_IndexParamRotationInfo:
    {
        EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *pRotationInfo = (EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO *)pComponentParameterStructure;
        OMX_U32                               nPortIndex    = pRotationInfo->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pRotationInfo, sizeof(EXYNOS_OMX_VIDEO_PARAM_ROTATION_INFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pRotationInfo->eRotationType != ROTATE_0) &&
                (pRotationInfo->eRotationType != ROTATE_90) &&
                (pRotationInfo->eRotationType != ROTATE_180) &&
                (pRotationInfo->eRotationType != ROTATE_270)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] can't accecpt a rotation value(%d)", pExynosComponent, __FUNCTION__,
                    pRotationInfo->eRotationType);
            ret = OMX_ErrorUnsupportedSetting;
            goto EXIT;
        }

        pVideoEnc->eRotationType = pRotationInfo->eRotationType;
    }
        break;

    case OMX_IndexParamVideoInit:
    {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        /* ret = OMX_ErrorUndefined; */
        /* Exynos_OSAL_Memcpy(&pExynosComponent->portParam, portParam, sizeof(OMX_PORT_PARAM_TYPE)); */
    }
        break;
    case OMX_IndexParamPriorityMgmt:
    {
        OMX_PRIORITYMGMTTYPE *compPriority = (OMX_PRIORITYMGMTTYPE *)pComponentParameterStructure;

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(compPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pExynosComponent->compPriority.nGroupID = compPriority->nGroupID;
        pExynosComponent->compPriority.nGroupPriority = compPriority->nGroupPriority;
    }
        break;
    case OMX_IndexParamCompBufferSupplier:
    {
        OMX_PARAM_BUFFERSUPPLIERTYPE *bufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pComponentParameterStructure;
        OMX_U32           portIndex = bufferSupplier->nPortIndex;
        EXYNOS_OMX_BASEPORT *pExynosPort = NULL;


        if (portIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = Exynos_OMX_Check_SizeVersion(bufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[portIndex];
        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            if (pExynosPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyUnspecified) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
        if (CHECK_PORT_TUNNELED(pExynosPort) == 0) {
            ret = OMX_ErrorNone; /*OMX_ErrorNone ?????*/
            goto EXIT;
        }

        if (pExynosPort->portDefinition.eDir == OMX_DirInput) {
            if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyInput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    ret = OMX_ErrorNone;
                }
                */
                pExynosPort->tunnelFlags |= EXYNOS_TUNNEL_IS_SUPPLIER;
                bufferSupplier->nPortIndex = pExynosPort->tunneledPort;
                ret = OMX_SetParameter(pExynosPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, bufferSupplier);
                goto EXIT;
            } else if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) {
                ret = OMX_ErrorNone;
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    pExynosPort->tunnelFlags &= ~EXYNOS_TUNNEL_IS_SUPPLIER;
                    bufferSupplier->nPortIndex = pExynosPort->tunneledPort;
                    ret = OMX_SetParameter(pExynosPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, bufferSupplier);
                }
                goto EXIT;
            }
        } else if (pExynosPort->portDefinition.eDir == OMX_DirOutput) {
            if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyInput) {
                ret = OMX_ErrorNone;
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    pExynosPort->tunnelFlags &= ~EXYNOS_TUNNEL_IS_SUPPLIER;
                    ret = OMX_ErrorNone;
                }
                goto EXIT;
            } else if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
                    ret = OMX_ErrorNone;
                }
                */
                pExynosPort->tunnelFlags |= EXYNOS_TUNNEL_IS_SUPPLIER;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }
        break;
    default:
    {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCWFDEnc_GetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;
    int                              i;

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
    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;

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
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       nPortIndex     = pConfigBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort    = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pConfigBitrate->nEncodeBitrate = pExynosPort->portDefinition.format.video.nBitrate;
        }
    }
        break;
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
        pConfigFramerate->xEncodeFramerate = pExynosPort->portDefinition.format.video.xFramerate;
    }
        break;
#ifdef USE_ANDROID
    case OMX_IndexConfigVideoHdrStaticInfo:
    case OMX_IndexConfigVideoColorAspects:
    {
        ret = Exynos_OSAL_GetConfig(hComponent, nIndex, pComponentConfigStructure);
    }
        break;
#endif
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCWFDEnc_SetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;
    EXYNOS_MFC_HEVCWFDENC_HANDLE    *pMFCHevcWFDHandle  = NULL;
    ExynosVideoEncOps               *pEncOps            = NULL;
    int                              i;

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
    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;

    if (&(pHevcEnc->hMFCHevcHandle) == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pMFCHevcWFDHandle = (EXYNOS_MFC_HEVCWFDENC_HANDLE *)&pHevcEnc->hMFCHevcHandle;

    if (pMFCHevcWFDHandle->pEncOps == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pEncOps = (ExynosVideoEncOps *)pMFCHevcWFDHandle->pEncOps;

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] nIndex %x", pExynosComponent, __FUNCTION__, (int)nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigVideoIntraPeriod:
    {
        OMX_U32 nPFrames = (*((OMX_U32 *)pComponentConfigStructure)) - 1;
        pHevcEnc->HevcComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval = nPFrames;
        pEncOps->Set_IDRPeriod(pHevcEnc->hMFCHevcHandle.hMFCHandle, nPFrames + 1);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] IDR period: %d", pExynosComponent, __FUNCTION__, nPFrames + 1);
        ret = OMX_ErrorNone ;
    }
        break;
    case OMX_IndexConfigVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE *pQpRange   = (OMX_VIDEO_QPRANGETYPE *)pComponentConfigStructure;
        OMX_U32                nPortIndex = pQpRange->nPortIndex;
        ExynosVideoQPRange     qpRange;

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

        qpRange.QpMin_I = pQpRange->qpRangeI.nMinQP;
        qpRange.QpMax_I = pQpRange->qpRangeI.nMaxQP;
        qpRange.QpMin_P = pQpRange->qpRangeP.nMinQP;
        qpRange.QpMax_P = pQpRange->qpRangeP.nMaxQP;
        qpRange.QpMin_B = pQpRange->qpRangeB.nMinQP;
        qpRange.QpMax_B = pQpRange->qpRangeB.nMaxQP;

        pEncOps->Set_QpRange(pMFCHevcWFDHandle->hMFCHandle, qpRange);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] qp range: I(%d, %d), P(%d, %d), B(%d, %d)",
                                                pExynosComponent, __FUNCTION__,
                                                qpRange.QpMin_I, qpRange.QpMax_I,
                                                qpRange.QpMin_P, qpRange.QpMax_P,
                                                qpRange.QpMin_B, qpRange.QpMax_B);
    }
        break;
    case OMX_IndexConfigVideoTemporalSVC:
    {
        EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *pSrcTemporalSVC = (EXYNOS_OMX_VIDEO_CONFIG_TEMPORALSVC *)pComponentConfigStructure;
        ExynosVideoQPRange qpRange;
        TemporalLayerShareBuffer TemporalSVC;

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

        qpRange.QpMin_I = pSrcTemporalSVC->nMinQuantizer;
        qpRange.QpMax_I = pSrcTemporalSVC->nMaxQuantizer;
        qpRange.QpMin_P = pSrcTemporalSVC->nMinQuantizer;
        qpRange.QpMax_P = pSrcTemporalSVC->nMaxQuantizer;
        qpRange.QpMin_B = pSrcTemporalSVC->nMinQuantizer;
        qpRange.QpMax_B = pSrcTemporalSVC->nMaxQuantizer;

        pEncOps->Set_QpRange(pMFCHevcWFDHandle->hMFCHandle, qpRange);
        pEncOps->Set_IDRPeriod(pMFCHevcWFDHandle->hMFCHandle, pSrcTemporalSVC->nKeyFrameInterval);

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // qp range: I(%d, %d), P(%d, %d), B(%d, %d)",
                                                pExynosComponent, __FUNCTION__,
                                                qpRange.QpMin_I, qpRange.QpMax_I,
                                                qpRange.QpMin_P, qpRange.QpMax_P,
                                                qpRange.QpMin_B, qpRange.QpMax_B);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // IDR period: %d", pExynosComponent, __FUNCTION__, pSrcTemporalSVC->nKeyFrameInterval);

        /* Temporal SVC */
        Exynos_OSAL_Memset(&TemporalSVC, 0, sizeof(TemporalLayerShareBuffer));

        TemporalSVC.nTemporalLayerCount = (unsigned int)pSrcTemporalSVC->nTemporalLayerCount;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // layer count: %d", pExynosComponent, __FUNCTION__, TemporalSVC.nTemporalLayerCount);

        for (i = 0; i < OMX_VIDEO_MAX_HEVC_TEMPORAL_LAYERS; i++) {
            TemporalSVC.nTemporalLayerBitrateRatio[i] = (unsigned int)pSrcTemporalSVC->nTemporalLayerBitrateRatio[i];
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] temporal svc // bitrate ratio[%d]: %d",
                                                    pExynosComponent, __FUNCTION__,
                                                    i, TemporalSVC.nTemporalLayerBitrateRatio[i]);
        }
        if (pEncOps->Set_LayerChange(pMFCHevcWFDHandle->hMFCHandle, TemporalSVC) != VIDEO_ERROR_NONE)
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Not supported control: Set_LayerChange", pExynosComponent, __FUNCTION__);
    }
        break;
    case OMX_IndexConfigVideoRoiInfo:
    {
        EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *pRoiInfo = (EXYNOS_OMX_VIDEO_CONFIG_ROIINFO *)pComponentConfigStructure;
        RoiInfoShareBuffer               RoiInfo;

        if (pRoiInfo->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pHevcEnc->hMFCHevcHandle.bRoiInfo == OMX_FALSE) ||
            ((pRoiInfo->bUseRoiInfo == OMX_TRUE) &&
            ((pRoiInfo->nRoiMBInfoSize <= 0) ||
            (pRoiInfo->pRoiMBInfo == NULL)))) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: %d: bUseRoiInfo %d nRoiMBInfoSize %d pRoiMBInfo %p", __FUNCTION__, __LINE__,
                                                    pRoiInfo->bUseRoiInfo, pRoiInfo->nRoiMBInfoSize, pRoiInfo->pRoiMBInfo);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
        }

        Exynos_OSAL_Memset(&RoiInfo, 0, sizeof(RoiInfo));
        RoiInfo.pRoiMBInfo     = (OMX_U64)(unsigned long)(((OMX_U8 *)pComponentConfigStructure) + sizeof(EXYNOS_OMX_VIDEO_CONFIG_ROIINFO));
        RoiInfo.nRoiMBInfoSize = pRoiInfo->nRoiMBInfoSize;
        RoiInfo.nUpperQpOffset = pRoiInfo->nUpperQpOffset;
        RoiInfo.nLowerQpOffset = pRoiInfo->nLowerQpOffset;
        RoiInfo.bUseRoiInfo    = (pRoiInfo->bUseRoiInfo == OMX_TRUE)? VIDEO_TRUE:VIDEO_FALSE;
        if (pEncOps->Set_RoiInfo != NULL) {
            pEncOps->Set_RoiInfo(pMFCHevcWFDHandle->hMFCHandle, &RoiInfo);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] roi info: %s", pExynosComponent, __FUNCTION__,
                                                    (RoiInfo.bUseRoiInfo == VIDEO_TRUE)? "enabled":"disabled");
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s]: Not supported control: Set_RoiInfo",
                                                    pExynosComponent, __FUNCTION__);
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

        pEncOps->Set_IFrameRatio(pMFCHevcWFDHandle->hMFCHandle, pIFrameRatio->nU32);
    }
        break;
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       nPortIndex     = pConfigBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort    = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            if (pVideoEnc->eControlRate[nPortIndex] == OMX_Video_ControlRateDisable) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Rate control(eControlRate) is disable. can not change a bitrate",
                                                    pExynosComponent, __FUNCTION__);
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pExynosPort->portDefinition.format.video.nBitrate = pConfigBitrate->nEncodeBitrate;
            pEncOps->Set_BitRate(pHevcEnc->hMFCHevcHandle.hMFCHandle, pConfigBitrate->nEncodeBitrate);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bitrate: %d", pExynosComponent, __FUNCTION__, pConfigBitrate->nEncodeBitrate);
        }
    }
        break;
    case OMX_IndexConfigVideoFramerate:
    {
        OMX_CONFIG_FRAMERATETYPE *pConfigFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   nPortIndex       = pConfigFramerate->nPortIndex;
        EXYNOS_OMX_BASEPORT      *pExynosPort      = NULL;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((nPortIndex == INPUT_PORT_INDEX) &&
            ((pConfigFramerate->xEncodeFramerate >> 16) <= 0)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] xFramerate is invalid(%d)",
                                              pExynosComponent, __FUNCTION__, pConfigFramerate->xEncodeFramerate >> 16);
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        pExynosPort->portDefinition.format.video.xFramerate = pConfigFramerate->xEncodeFramerate;

        pEncOps->Set_FrameRate(pHevcEnc->hMFCHevcHandle.hMFCHandle, (pConfigFramerate->xEncodeFramerate) >> 16);
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] framerate: %d", pExynosComponent, __FUNCTION__, (pConfigFramerate->xEncodeFramerate) >> 16);
    }
        break;
    case OMX_IndexConfigOperatingRate:  /* since M version */
    {
        OMX_PARAM_U32TYPE *pConfigRate = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
        OMX_U32            xFramerate  = 0;

        ret = Exynos_OMX_Check_SizeVersion(pConfigRate, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        xFramerate = pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.xFramerate;
        pVideoEnc->nQosRatio = pConfigRate->nU32 >> 16;

        if (pVideoEnc->nQosRatio == (((OMX_U32)INT_MAX) >> 16)) {
            pVideoEnc->nQosRatio = 1000;
        } else {
            pVideoEnc->nQosRatio = ((xFramerate >> 16) == 0)? 100:(OMX_U32)((pConfigRate->nU32 / (double)xFramerate) * 100);
        }

        pEncOps->Set_QosRatio(pMFCHevcWFDHandle->hMFCHandle, pVideoEnc->nQosRatio);

        pVideoEnc->bQosChanged = OMX_FALSE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] qos ratio: 0x%x", pExynosComponent, __FUNCTION__, pVideoEnc->nQosRatio);

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexConfigBlurInfo:
    {
        EXYNOS_OMX_VIDEO_CONFIG_BLURINFO *pBlurMode   = (EXYNOS_OMX_VIDEO_CONFIG_BLURINFO *)pComponentConfigStructure;
        OMX_U32                           nPortIndex  = pBlurMode->nPortIndex;
        EXYNOS_OMX_BASEPORT              *pExynosPort = NULL;

        int nEncResol;

        ret = Exynos_OMX_Check_SizeVersion(pBlurMode, sizeof(EXYNOS_OMX_VIDEO_CONFIG_BLURINFO));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        nEncResol   = pExynosPort->portDefinition.format.video.nFrameWidth * pExynosPort->portDefinition.format.video.nFrameHeight;

        if (pVideoEnc->bUseBlurFilter == OMX_TRUE) {
            if ((pBlurMode->eBlurMode & BLUR_MODE_DOWNUP) &&
                (nEncResol < (int)pBlurMode->eTargetResol)) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Resolution(%d x %d) is smaller than target resolution",
                                                    pExynosComponent, __FUNCTION__,
                                                    pExynosPort->portDefinition.format.video.nFrameWidth,
                                                    pExynosPort->portDefinition.format.video.nFrameHeight);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            pVideoEnc->eBlurMode = pBlurMode->eBlurMode;
            pVideoEnc->eBlurResol = pBlurMode->eTargetResol;
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Blur Filter is not enabled, it will be discard",
                                                    pExynosComponent, __FUNCTION__);
        }

        ret = (OMX_ERRORTYPE)OMX_ErrorNoneExpiration;
    }
        break;
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    if (ret == (OMX_ERRORTYPE)OMX_ErrorNoneExpiration)
        ret = OMX_ErrorNone;

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCWFDEnc_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;

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
    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;

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

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_INTRAPERIOD) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigVideoIntraPeriod;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_NEED_CONTIG_MEMORY) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorNeedContigMemory;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_GET_BUFFER_FD) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorGetBufferFD;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_VIDEO_QPRANGE_TYPE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamVideoQPRange;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_QPRANGE_TYPE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoQPRange;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ENABLE_BLUR_FILTER) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamEnableBlurFilter;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_BLUR_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigBlurInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ROATION_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamRotationInfo;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

#ifdef USE_ANDROID
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamStoreMetaDataBuffer;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_CONFIG_VIDEO_COLOR_ASPECTS_INFO) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexConfigVideoColorAspects;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ALLOCATE_NATIVE_HANDLE) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamAllocateNativeHandle;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    ret = OMX_ErrorBadParameter;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HEVCWFDEnc_ComponentRoleEnum(
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
        Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_HEVC_WFD_ENC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
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
    EXYNOS_OMX_BASEPORT           *pInputPort       = NULL;
    EXYNOS_OMX_BASEPORT           *pOutputPort      = NULL;
    EXYNOS_HEVCWFDENC_HANDLE      *pHevcEnc         = NULL;
    OMX_BOOL                       bSecureMode      = OMX_FALSE;

    ExynosVideoInstInfo           *pVideoInstInfo = NULL;

    int i = 0;

    Exynos_OSAL_Get_Log_Property(); // For debuging
    FunctionIn();

    if ((hComponent == NULL) ||
        (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_HEVC_WFD_ENC, componentName) == 0) {
        bSecureMode = OMX_FALSE;
    } else if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_HEVC_WFD_DRM_ENC, componentName) == 0) {
        bSecureMode = OMX_TRUE;
    } else {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unsupported component name(%s)", __FUNCTION__, componentName);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    ret = Exynos_OMX_VideoEncodeComponentInit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to VideoDecodeComponentInit for WFD (0x%x)", componentName, __FUNCTION__, ret);
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

    pHevcEnc = Exynos_OSAL_Malloc(sizeof(EXYNOS_HEVCWFDENC_HANDLE));
    if (pHevcEnc == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pHevcEnc, 0, sizeof(EXYNOS_HEVCWFDENC_HANDLE));

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

    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_HEVC_WFD_DRM_ENC);
    else
        Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_HEVC_WFD_ENC);

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

    pHevcEnc->hMFCHevcHandle.videoInstInfo.bOTFMode = VIDEO_TRUE;

    pOMXComponent->GetParameter      = &Exynos_HEVCWFDEnc_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_HEVCWFDEnc_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_HEVCWFDEnc_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_HEVCWFDEnc_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_HEVCWFDEnc_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_HEVCWFDEnc_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pVideoEnc->exynos_codec_start         = &HEVCWFDCodecStart;
    pVideoEnc->exynos_codec_stop          = &HEVCWFDCodecStop;

    pVideoEnc->exynos_codec_checkFormatSupport = &CheckFormatHWSupport;

    pHevcEnc->hMFCHevcHandle.videoInstInfo.eCodecType = VIDEO_CODING_HEVC;
    if (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)
        pHevcEnc->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_SECURE;
    else
        pHevcEnc->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_NORMAL;

    if (Exynos_Video_GetInstInfo(&(pHevcEnc->hMFCHevcHandle.videoInstInfo), VIDEO_FALSE /* enc */) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to GetInstInfo", pExynosComponent, __FUNCTION__);
        Exynos_OSAL_Free(pHevcEnc);
        pHevcEnc = pVideoEnc->hCodecHandle = NULL;
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] GetInstInfo for enc : temporal-svc(%d)/roi(%d)/qp-range(%d)",
                                            pExynosComponent, __FUNCTION__,
                                            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bTemporalSvcSupport),
                                            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bRoiInfoSupport),
                                            (pHevcEnc->hMFCHevcHandle.videoInstInfo.supportInfo.enc.bQpRangePBSupport));

    Exynos_Input_SetSupportFormat(pExynosComponent);
    SetProfileLevel(pExynosComponent);

#ifdef USE_ANDROID
    Exynos_OSAL_AddVendorExt(hComponent, "sec-ext-enc-qp-range", (OMX_INDEXTYPE)OMX_IndexConfigVideoQPRange);
#endif

    pInputPort     = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pOutputPort    = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    pVideoInstInfo = &(pHevcEnc->hMFCHevcHandle.videoInstInfo);

    pVideoInstInfo->nSize        = sizeof(ExynosVideoInstInfo);
    pVideoInstInfo->nWidth       = pInputPort->portDefinition.format.video.nFrameWidth;
    pVideoInstInfo->nHeight      = pInputPort->portDefinition.format.video.nFrameHeight;
    pVideoInstInfo->nBitrate     = pInputPort->portDefinition.format.video.nBitrate;
    pVideoInstInfo->xFramerate   = pInputPort->portDefinition.format.video.xFramerate;

    /* HEVC Codec Open */
    ret = HEVCWFDCodecOpen(pHevcEnc, pVideoInstInfo);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

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
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc           = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent       = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent    = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc           = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;

    pHevcEnc = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;
    if (pHevcEnc != NULL) {
        HEVCWFDCodecClose(pHevcEnc);
        Exynos_OSAL_Free(pHevcEnc);
        pHevcEnc = pVideoEnc->hCodecHandle = NULL;
    }

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

OMX_ERRORTYPE Exynos_OMX_Port_Constructor(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = NULL;
    int i = 0, j = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] OMX_ErrorBadParameter (0x%x) Line:%d", __FUNCTION__, ret, __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] OMX_ErrorBadParameter (0x%x) Line:%d", __FUNCTION__, ret, __LINE__);
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    INIT_SET_SIZE_VERSION(&pExynosComponent->portParam, OMX_PORT_PARAM_TYPE);
    pExynosComponent->portParam.nPorts = ALL_PORT_NUM;
    pExynosComponent->portParam.nStartPortNumber = INPUT_PORT_INDEX;

    pExynosPort = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_BASEPORT) * ALL_PORT_NUM);
    if (pExynosPort == NULL) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort, 0, sizeof(EXYNOS_OMX_BASEPORT) * ALL_PORT_NUM);
    pExynosComponent->pExynosPort = pExynosPort;

    /* Input Port */
    pExynosInputPort = &pExynosPort[INPUT_PORT_INDEX];

    pExynosInputPort->extendBufferHeader = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);
    if (pExynosInputPort->extendBufferHeader == NULL) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosInputPort->extendBufferHeader, 0, sizeof(EXYNOS_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);

    pExynosInputPort->bufferStateAllocate = Exynos_OSAL_Malloc(sizeof(OMX_U32) * MAX_BUFFER_NUM);
    if (pExynosInputPort->bufferStateAllocate == NULL) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosInputPort->bufferStateAllocate, 0, sizeof(OMX_U32) * MAX_BUFFER_NUM);

    pExynosInputPort->bufferSemID = NULL;
    pExynosInputPort->assignedBufferNum = 0;
    pExynosInputPort->portState = EXYNOS_OMX_PortStateLoaded;
    pExynosInputPort->tunneledComponent = NULL;
    pExynosInputPort->tunneledPort = 0;
    pExynosInputPort->tunnelBufferNum = 0;
    pExynosInputPort->bufferSupplier = OMX_BufferSupplyUnspecified;
    pExynosInputPort->tunnelFlags = 0;
    pExynosInputPort->supportFormat = NULL;
    pExynosInputPort->bNeedContigMem = OMX_FALSE;
    pExynosInputPort->latestTimeStamp = DEFAULT_TIMESTAMP_VAL;

    INIT_SET_SIZE_VERSION(&pExynosInputPort->portDefinition, OMX_PARAM_PORTDEFINITIONTYPE);
    pExynosInputPort->portDefinition.nPortIndex = INPUT_PORT_INDEX;
    pExynosInputPort->portDefinition.eDir = OMX_DirInput;
    pExynosInputPort->portDefinition.nBufferCountActual = 0;
    pExynosInputPort->portDefinition.nBufferCountMin = 0;
    pExynosInputPort->portDefinition.nBufferSize = 0;
    pExynosInputPort->portDefinition.bEnabled = OMX_FALSE;
    pExynosInputPort->portDefinition.bPopulated = OMX_FALSE;
    pExynosInputPort->portDefinition.eDomain = OMX_PortDomainMax;
    pExynosInputPort->portDefinition.bBuffersContiguous = OMX_FALSE;
    pExynosInputPort->portDefinition.nBufferAlignment = 0;

    /* Output Port */
    pExynosOutputPort = &pExynosPort[OUTPUT_PORT_INDEX];

    pExynosOutputPort->extendBufferHeader = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);
    if (pExynosOutputPort->extendBufferHeader == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosOutputPort->extendBufferHeader, 0, sizeof(EXYNOS_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);

    pExynosOutputPort->bufferStateAllocate = Exynos_OSAL_Malloc(sizeof(OMX_U32) * MAX_BUFFER_NUM);
    if (pExynosOutputPort->bufferStateAllocate == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Malloc (0x%x) Line:%d", pExynosComponent, __FUNCTION__, ret, __LINE__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosOutputPort->bufferStateAllocate, 0, sizeof(OMX_U32) * MAX_BUFFER_NUM);

    pExynosOutputPort->bufferSemID = NULL;
    pExynosOutputPort->assignedBufferNum = 0;
    pExynosOutputPort->portState = EXYNOS_OMX_PortStateLoaded;
    pExynosOutputPort->tunneledComponent = NULL;
    pExynosOutputPort->tunneledPort = 0;
    pExynosOutputPort->tunnelBufferNum = 0;
    pExynosOutputPort->bufferSupplier = OMX_BufferSupplyUnspecified;
    pExynosOutputPort->tunnelFlags = 0;
    pExynosOutputPort->supportFormat = NULL;
    pExynosOutputPort->bNeedContigMem = OMX_FALSE;
    pExynosOutputPort->latestTimeStamp = DEFAULT_TIMESTAMP_VAL;

    INIT_SET_SIZE_VERSION(&pExynosOutputPort->portDefinition, OMX_PARAM_PORTDEFINITIONTYPE);
    pExynosOutputPort->portDefinition.nPortIndex = OUTPUT_PORT_INDEX;
    pExynosOutputPort->portDefinition.eDir = OMX_DirOutput;
    pExynosOutputPort->portDefinition.nBufferCountActual = 0;
    pExynosOutputPort->portDefinition.nBufferCountMin = 0;
    pExynosOutputPort->portDefinition.nBufferSize = 0;
    pExynosOutputPort->portDefinition.bEnabled = OMX_FALSE;
    pExynosOutputPort->portDefinition.bPopulated = OMX_FALSE;
    pExynosOutputPort->portDefinition.eDomain = OMX_PortDomainMax;
    pExynosOutputPort->portDefinition.bBuffersContiguous = OMX_FALSE;
    pExynosOutputPort->portDefinition.nBufferAlignment = 0;

    pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
    pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
    pExynosComponent->checkTimeStamp.startTimeStamp = 0;
    pExynosComponent->checkTimeStamp.nStartFlags = 0x0;

    pOMXComponent->EmptyThisBuffer = &Exynos_OMX_EmptyThisBuffer;
    pOMXComponent->FillThisBuffer  = &Exynos_OMX_FillThisBuffer;

    ret = OMX_ErrorNone;

EXIT:
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent != NULL) &&
        (pExynosComponent->pExynosPort != NULL)) {
        for (i = 0; i < ALL_PORT_NUM; i++) {
            pExynosPort = &pExynosComponent->pExynosPort[i];

            Exynos_OSAL_Free(pExynosPort->bufferStateAllocate);
            pExynosPort->bufferStateAllocate = NULL;
            Exynos_OSAL_Free(pExynosPort->extendBufferHeader);
            pExynosPort->extendBufferHeader = NULL;
        }

        Exynos_OSAL_Free(pExynosComponent->pExynosPort);
        pExynosComponent->pExynosPort = NULL;
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_Port_Destructor(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;

    int i = 0, j = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    for (i = 0; i < ALL_PORT_NUM; i++) {
        pExynosPort = &pExynosComponent->pExynosPort[i];

        Exynos_OSAL_Free(pExynosPort->bufferStateAllocate);
        pExynosPort->bufferStateAllocate = NULL;
        Exynos_OSAL_Free(pExynosPort->extendBufferHeader);
        pExynosPort->extendBufferHeader = NULL;
    }

    Exynos_OSAL_Free(pExynosComponent->pExynosPort);
    pExynosComponent->pExynosPort = NULL;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *pExynosPort      = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_U32                   i                = 0;

    pExynosComponent->pCallbacks->EmptyBufferDone(pOMXComponent,
                                                pExynosComponent->callbackData,
                                                bufferHeader);

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bufferHeader: %p", pExynosComponent, __FUNCTION__, bufferHeader);

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (bufferHeader == inputBufArray[i].pBuffer) {
            inputBufArray[i].bInOMX = OMX_FALSE;
            break;
        }
    }

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *pExynosPort      = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                   i                = 0;

    pExynosComponent->pCallbacks->FillBufferDone(pOMXComponent,
                                                pExynosComponent->callbackData,
                                                bufferHeader);

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] bufferHeader: %p", pExynosComponent, __FUNCTION__, bufferHeader);

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (bufferHeader == outputBufArray[i].pBuffer) {
            outputBufArray[i].bInOMX = OMX_FALSE;
            break;
        }
    }

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferFlushProcess(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_S32             nPortIndex,
    OMX_BOOL            bEvent)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    OMX_S32                   nIndex            = 0;

    OMX_U32 i = 0, cnt = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    cnt = (nPortIndex == ALL_PORT_INDEX) ? ALL_PORT_NUM : 1;
    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            nIndex = i;
        else
            nIndex = nPortIndex;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Flush %s Port", pExynosComponent, __FUNCTION__,
                                                (nIndex == INPUT_PORT_INDEX)? "input":"output");

        ret = pExynosComponent->exynos_BufferFlush(pOMXComponent, nIndex, bEvent);

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port is flushed", pExynosComponent, __FUNCTION__,
                                                (nIndex == INPUT_PORT_INDEX)? "input":"output");
        if (ret == OMX_ErrorNone) {
            pExynosComponent->pExynosPort[nIndex].portState = EXYNOS_OMX_PortStateIdle;

            if (bEvent == OMX_TRUE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(EventCmdComplete/Flush/%s port)",
                                                        pExynosComponent, __FUNCTION__, (nIndex == INPUT_PORT_INDEX)? "input":"output");
                pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                        pExynosComponent->callbackData,
                                                        OMX_EventCmdComplete,
                                                        OMX_CommandFlush, nIndex, NULL);
            }
        }
    }

EXIT:
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] (0x%x)", pExynosComponent, __FUNCTION__, ret);
        if ((pOMXComponent != NULL) &&
            (pExynosComponent != NULL)) {
            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                    pExynosComponent->callbackData,
                                                    OMX_EventError,
                                                    ret, 0, NULL);
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_EnablePort(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_S32             nPortIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;

    OMX_U32 i = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    pExynosPort->portDefinition.bEnabled = OMX_TRUE;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_PortEnableProcess(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_S32             nPortIndex)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    OMX_S32                   nIndex           = 0;

    OMX_U32 i = 0, cnt = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    cnt = (nPortIndex == ALL_PORT_INDEX) ? ALL_PORT_NUM : 1;
    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            nIndex = i;
        else
            nIndex = nPortIndex;
    }

EXIT:
    if ((ret != OMX_ErrorNone) &&
        (pOMXComponent != NULL) &&
        (pExynosComponent != NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] (0x%x)", pExynosComponent, __FUNCTION__, ret);
        pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError,
                                                ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_DisablePort(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_S32             nPortIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    pExynosPort->portDefinition.bEnabled = OMX_FALSE;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_PortDisableProcess(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_S32             nPortIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    OMX_S32                   nIndex            = 0;

    OMX_U32 i = 0, cnt = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    cnt = (nPortIndex == ALL_PORT_INDEX ) ? ALL_PORT_NUM : 1;

    if ((pExynosComponent->currentState == OMX_StateExecuting) ||
        (pExynosComponent->currentState == OMX_StatePause)) {
        /* port flush*/
        for(i = 0; i < cnt; i++) {
            if (nPortIndex == ALL_PORT_INDEX)
                nIndex = i;
            else
                nIndex = nPortIndex;

            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] Before disabling %s port, do flush",
                                                pExynosComponent, __FUNCTION__,
                                                (nIndex == INPUT_PORT_INDEX)? "input":"output");
            pExynosComponent->pExynosPort[nIndex].portState = EXYNOS_OMX_PortStateFlushingForDisable;
            ret = pExynosComponent->exynos_BufferFlush(pOMXComponent, nIndex, OMX_FALSE);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s Port is flushed", pExynosComponent, __FUNCTION__,
                                                    (nIndex == INPUT_PORT_INDEX)? "input":"output");
            if (ret != OMX_ErrorNone)
                goto EXIT;
        }
    }

    for(i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            nIndex = i;
        else
            nIndex = nPortIndex;

        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] Disable %s Port",
                                            pExynosComponent, __FUNCTION__,
                                            (nIndex == INPUT_PORT_INDEX)? "input":"output");
        pExynosComponent->pExynosPort[nIndex].portState = EXYNOS_OMX_PortStateDisabling;
        ret = Exynos_OMX_DisablePort(pOMXComponent, nIndex);
    }

EXIT:
    if ((ret != OMX_ErrorNone) &&
        (pOMXComponent != NULL) &&
        (pExynosComponent != NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] (0x%x)", pExynosComponent, __FUNCTION__, ret);
        pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError,
                                                ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

/* Change CHECK_SIZE_VERSION Macro */
OMX_ERRORTYPE Exynos_OMX_Check_SizeVersion(OMX_PTR header, OMX_U32 size)
{
    OMX_ERRORTYPE        ret        = OMX_ErrorNone;
    OMX_VERSIONTYPE     *version    = NULL;

    if (header == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    version = (OMX_VERSIONTYPE*)((char*)header + sizeof(OMX_U32));
    if (*((OMX_U32*)header) != size) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_FUNC_TRACE, "[%s] nVersionMajor:%d, nVersionMinor:%d", __FUNCTION__, version->s.nVersionMajor, version->s.nVersionMinor);

    if ((version->s.nVersionMajor != VERSIONMAJOR_NUMBER) ||
        (version->s.nVersionMinor > VERSIONMINOR_NUMBER)) {
        ret = OMX_ErrorVersionMismatch;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    return ret;
}

int Exynos_GetPlaneFromPort(EXYNOS_OMX_BASEPORT *pPort)
{
    int ret = 0;

    if (pPort == NULL)
        goto EXIT;

    ret = pPort->processData.buffer.nPlanes;

EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_SetPlaneToPort(EXYNOS_OMX_BASEPORT *pPort, int nPlaneNum)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (pPort == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pPort->processData.buffer.nPlanes = nPlaneNum;

EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_EmptyThisBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pExynosPort        = NULL;
    unsigned int                 i                  = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    if (pExynosComponent->pExynosPort == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pBuffer->nInputPortIndex != INPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    ret = Exynos_OMX_Check_SizeVersion(pBuffer, sizeof(OMX_BUFFERHEADERTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if ((pExynosComponent->currentState != OMX_StateIdle) &&
        (pExynosComponent->currentState != OMX_StateExecuting) &&
        (pExynosComponent->currentState != OMX_StatePause)) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (inputBufArray[i].bInOMX == OMX_FALSE) {
            inputBufArray[i].pBuffer  = pBuffer;
            inputBufArray[i].bInOMX   = OMX_TRUE;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] pBuffer[%d]: %p",
                                                    pExynosComponent, __FUNCTION__, i, pBuffer);
            break;
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FillThisBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pExynosPort        = NULL;
    unsigned int                 i                  = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    if (pExynosComponent->pExynosPort == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pBuffer->nOutputPortIndex != OUTPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    ret = Exynos_OMX_Check_SizeVersion(pBuffer, sizeof(OMX_BUFFERHEADERTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if ((pExynosComponent->currentState != OMX_StateIdle) &&
        (pExynosComponent->currentState != OMX_StateExecuting) &&
        (pExynosComponent->currentState != OMX_StatePause)) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (outputBufArray[i].bInOMX == OMX_FALSE) {
            outputBufArray[i].pBuffer  = pBuffer;
            outputBufArray[i].bInOMX   = OMX_TRUE;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] pBuffer[%d]: %p",
                                                    pExynosComponent, __FUNCTION__, i, pBuffer);
            break;
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_GetComponentVersion(
    OMX_IN  OMX_HANDLETYPE   hComponent,
    OMX_OUT OMX_STRING       pComponentName,
    OMX_OUT OMX_VERSIONTYPE *pComponentVersion,
    OMX_OUT OMX_VERSIONTYPE *pSpecVersion,
    OMX_OUT OMX_UUIDTYPE    *pComponentUUID)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    unsigned long             compUUID[3];

    FunctionIn();

    /* check parameters */
    if (hComponent     == NULL ||
        pComponentName == NULL || pComponentVersion == NULL ||
        pSpecVersion   == NULL || pComponentUUID    == NULL) {
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
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    Exynos_OSAL_Strcpy(pComponentName, pExynosComponent->componentName);
    Exynos_OSAL_Memcpy(pComponentVersion, &(pExynosComponent->componentVersion), sizeof(OMX_VERSIONTYPE));
    Exynos_OSAL_Memcpy(pSpecVersion, &(pExynosComponent->specVersion), sizeof(OMX_VERSIONTYPE));

    /* Fill UUID with handle address, PID and UID.
     * This should guarantee uiniqness */
    compUUID[0] = (unsigned long)pOMXComponent;
    compUUID[1] = (unsigned long)getpid();
    compUUID[2] = (unsigned long)getuid();
    Exynos_OSAL_Memcpy(*pComponentUUID, compUUID, 3 * sizeof(*compUUID));

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_GetState(
        OMX_IN OMX_HANDLETYPE  hComponent,
        OMX_OUT OMX_STATETYPE *pState)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pState == NULL) {
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

    *pState = pExynosComponent->currentState;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_SetPortFlush(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret         = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort = NULL;
    OMX_S32              nPortIndex  = nParam;
    unsigned int         i;

    if ((pExynosComponent->currentState == OMX_StateExecuting) ||
        (pExynosComponent->currentState == OMX_StatePause)) {
        if ((nPortIndex != ALL_PORT_INDEX) &&
           ((OMX_S32)nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (nPortIndex == ALL_PORT_INDEX) {
            for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
                pExynosPort = &pExynosComponent->pExynosPort[i];
                if (!CHECK_PORT_ENABLED(pExynosPort)) {
                    ret = OMX_ErrorIncorrectStateOperation;
                    goto EXIT;
                }

                pExynosPort->portState = EXYNOS_OMX_PortStateFlushing;
            }
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            if (!CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }

            pExynosPort->portState = EXYNOS_OMX_PortStateFlushing;
        }
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_SetPortDisable(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;
    OMX_S32              nPortIndex     = nParam;
    unsigned int         i;

    FunctionIn();

    if ((nPortIndex != ALL_PORT_INDEX) &&
            ((OMX_S32)nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((nPortIndex != ALL_PORT_INDEX) &&
        ((OMX_S32)nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (nPortIndex == ALL_PORT_INDEX) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosPort = &pExynosComponent->pExynosPort[i];
            if (!CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }

            pExynosPort->portState = EXYNOS_OMX_PortStateDisabling;
        }
    } else {
        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        if (!CHECK_PORT_ENABLED(pExynosPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        pExynosPort->portState = EXYNOS_OMX_PortStateDisabling;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_SetPortEnable(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT *pExynosPort    = NULL;
    OMX_S32              nPortIndex     = nParam;

    OMX_U16 i = 0;

    FunctionIn();

    if ((nPortIndex != ALL_PORT_INDEX) &&
            ((OMX_S32)nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (nPortIndex == ALL_PORT_INDEX) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosPort = &pExynosComponent->pExynosPort[i];
            if (CHECK_PORT_ENABLED(pExynosPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }

            pExynosPort->portState = EXYNOS_OMX_PortStateEnabling;
        }
    } else {
        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        if (CHECK_PORT_ENABLED(pExynosPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        pExynosPort->portState = EXYNOS_OMX_PortStateEnabling;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;

}

static OMX_ERRORTYPE Exynos_SetStateSet(
        EXYNOS_OMX_BASECOMPONENT   *pExynosComponent,
        OMX_U32                     nParam)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    OMX_U32 i = 0;

    FunctionIn();

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    switch ((OMX_STATETYPE)nParam) {
    case OMX_StateIdle:
    {
        /* Loaded to Idle */
        if (pExynosComponent->currentState == OMX_StateLoaded) {
            pExynosComponent->transientState = EXYNOS_OMX_TransStateLoadedToIdle;

            for(i = 0; i < pExynosComponent->portParam.nPorts; i++)
                pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateEnabling;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateLoaded to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        /* Executing to Idle */
        if (pExynosComponent->currentState == OMX_StateExecuting) {
            EXYNOS_OMX_BASEPORT *pExynosPort = NULL;

            pExynosComponent->transientState = EXYNOS_OMX_TransStateExecutingToIdle;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateExecuting to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        /* Pause to Idle */
        if (pExynosComponent->currentState == OMX_StatePause) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StatePause to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        if (pExynosComponent->currentState == OMX_StateInvalid) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        ret = OMX_ErrorNone;
    }
        break;
    case OMX_StateExecuting:
    {
        /* Idle to Executing */
        if (pExynosComponent->currentState == OMX_StateIdle) {
            pExynosComponent->transientState = EXYNOS_OMX_TransStateIdleToExecuting;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateIdle to OMX_StateExecuting", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        /* Pause to Executing */
        if (pExynosComponent->currentState == OMX_StatePause) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StatePause to OMX_StateIdle", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorNone;
            goto EXIT;
        }

        ret = OMX_ErrorBadParameter;
    }
        break;
    case OMX_StateLoaded:
    {
        /* Idle to Loaded */
        if (pExynosComponent->currentState == OMX_StateIdle) {
            pExynosComponent->transientState = EXYNOS_OMX_TransStateIdleToLoaded;

            for (i = 0; i < pExynosComponent->portParam.nPorts; i++)
                pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateDisabling;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateIdle to OMX_StateLoaded", pExynosComponent, __FUNCTION__);
        }
    }
        break;
    case OMX_StateInvalid:
    {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++)
            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateInvalid;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s to OMX_StateInvalid",
                                                pExynosComponent, __FUNCTION__, stateString(pExynosComponent->currentState));
    }
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s to %s", pExynosComponent, __FUNCTION__,
                                                stateString(pExynosComponent->currentState), stateString(nParam));
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_StateSet(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nParam)
{
    OMX_U32 destState = nParam;
    OMX_U32 i = 0;

    if ((destState == OMX_StateIdle) && (pExynosComponent->currentState == OMX_StateLoaded)) {
        for(i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateEnabling;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateLoaded to OMX_StateIdle", pExynosComponent, __FUNCTION__);
    } else if ((destState == OMX_StateLoaded) && (pExynosComponent->currentState == OMX_StateIdle)) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateDisabling;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateIdle to OMX_StateLoaded", pExynosComponent, __FUNCTION__);
    } else if ((destState == OMX_StateIdle) && (pExynosComponent->currentState == OMX_StateExecuting)) {
        EXYNOS_OMX_BASEPORT *pExynosPort = NULL;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateExecuting to OMX_StateIdle", pExynosComponent, __FUNCTION__);
    } else if ((destState == OMX_StateIdle) && (pExynosComponent->currentState == OMX_StatePause)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StatePause to OMX_StateIdle", pExynosComponent, __FUNCTION__);
    } else if ((destState == OMX_StateExecuting) && (pExynosComponent->currentState == OMX_StateIdle)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] OMX_StateIdle to OMX_StateExecuting", pExynosComponent, __FUNCTION__);
    } else if (destState == OMX_StateInvalid) {
        for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateInvalid;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Exynos_OMX_ComponentStateSet(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE                    ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    OMX_STATETYPE                    destState        = nParam;
    OMX_STATETYPE                    currentState     = pExynosComponent->currentState;
    EXYNOS_OMX_BASEPORT             *pExynosPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCWFDENC_HANDLE        *pHevcEnc         = (EXYNOS_HEVCWFDENC_HANDLE *)pVideoEnc->hCodecHandle;


    ExynosVideoEncOps       *pEncOps    = pHevcEnc->hMFCHevcHandle.pEncOps;
    ExynosVideoEncBufferOps *pInbufOps  = pHevcEnc->hMFCHevcHandle.pInbufOps;
    ExynosVideoEncBufferOps *pOutbufOps = pHevcEnc->hMFCHevcHandle.pOutbufOps;
    ExynosVideoEncParam     *pEncParam  = NULL;
    ExynosVideoGeometry      bufferConf;

    unsigned int  i = 0, j = 0;
    int           k = 0;
    int           nOutBufSize = 0, nOutputBufferCnt = 0;

    FunctionIn();

    /* check parameters */
    if (currentState == destState) {
         ret = OMX_ErrorSameState;
            goto EXIT;
    }
    if (currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] current:(%s) dest:(%s)", pExynosComponent, __FUNCTION__, stateString(currentState), stateString(destState));
    switch (destState) {
    case OMX_StateInvalid:
        switch (currentState) {
        case OMX_StateWaitForResources:
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
        case OMX_StateLoaded:
            pExynosComponent->currentState = OMX_StateInvalid;

            if (currentState != OMX_StateLoaded)
                Exynos_ResetAllPortConfig(pOMXComponent);

            ret = OMX_ErrorInvalidState;
            break;
        default:
            ret = OMX_ErrorInvalidState;
            break;
        }
        break;
    case OMX_StateLoaded:
        switch (currentState) {
        case OMX_StateIdle:
            for(i = 0; i < pExynosComponent->portParam.nPorts; i++)
                pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateDisabling;

            ret = Exynos_ResetAllPortConfig(pOMXComponent);
            if (ret != OMX_ErrorNone)
                goto EXIT;
            else
                goto NO_EVENT_EXIT;

            break;
        case OMX_StateWaitForResources:
            pExynosComponent->currentState = OMX_StateLoaded;
            break;
        case OMX_StateExecuting:
        case OMX_StatePause:
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateIdle:
        switch (currentState) {
        case OMX_StateLoaded:
            Exynos_OSAL_Get_Log_Property(); // For debuging, Function called when GetHandle function is success

            for (i = 0; i < pExynosComponent->portParam.nPorts; i++) {
                pExynosPort = &(pExynosComponent->pExynosPort[i]);
                pExynosPort->portState = EXYNOS_OMX_PortStateEnabling;

                ret = Exynos_SetPlaneToPort(pExynosPort, (i == INPUT_PORT_INDEX) ? MFC_DEFAULT_INPUT_BUFFER_PLANE : MFC_DEFAULT_OUTPUT_BUFFER_PLANE);
                if (ret != OMX_ErrorNone)
                    break;
            }

            if (ret != OMX_ErrorNone)
                goto EXIT;
            else
                goto NO_EVENT_EXIT;

            break;
        case OMX_StateExecuting:
            Exynos_SetPortFlush(pExynosComponent, ALL_PORT_INDEX);
            Exynos_OMX_BufferFlushProcess(pOMXComponent, ALL_PORT_INDEX, OMX_FALSE);
            pExynosComponent->currentState = OMX_StateIdle;
            break;
        case OMX_StatePause:
            Exynos_SetPortFlush(pExynosComponent, ALL_PORT_INDEX);
            Exynos_OMX_BufferFlushProcess(pOMXComponent, ALL_PORT_INDEX, OMX_FALSE);
            pExynosComponent->currentState = OMX_StateIdle;
            break;
        case OMX_StateWaitForResources:
            pExynosComponent->currentState = OMX_StateIdle;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateExecuting:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
            pExynosComponent->currentState = OMX_StateExecuting;

            /* Set EncParam for input (src) */
            Set_HEVCWFDEnc_Param(pExynosComponent);
            pEncParam = &(pHevcEnc->hMFCHevcHandle.encParam);
            if (pEncOps->Set_EncParam) {
                if(pEncOps->Set_EncParam(pHevcEnc->hMFCHevcHandle.hMFCHandle, pEncParam) != VIDEO_ERROR_NONE) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set encParam", pExynosComponent, __FUNCTION__);
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
            }
            Print_HEVCWFDEnc_Param(pEncParam);

            /* set geometry for output (dst) */
            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            nOutBufSize = pExynosPort->portDefinition.nBufferSize;
            if ((pExynosPort->bufferProcessType & BUFFER_COPY) ||
                    (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
                /* OMX buffer is not used directly : CODEC buffer or MetaData */
                nOutBufSize = ALIGN(pExynosPort->portDefinition.format.video.nFrameWidth *
                        pExynosPort->portDefinition.format.video.nFrameHeight * 3 / 2, 512);

#ifdef USE_CUSTOM_COMPONENT_SUPPORT
                nOutBufSize = Exynos_OSAL_GetOutBufferSize(pExynosPort->portDefinition.format.video.nFrameWidth,
                                                        pExynosPort->portDefinition.format.video.nFrameHeight,
                                                        pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.nBufferSize);
#endif

            }
            if (pOutbufOps->Set_Geometry) {
                /* output buffer info: only 2 config values needed */
                bufferConf.eCompressionFormat = VIDEO_CODING_HEVC;
                bufferConf.nSizeImage = nOutBufSize;
                bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pExynosPort);

                if (pOutbufOps->Set_Geometry(pHevcEnc->hMFCHevcHandle.hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to set geometry about output", pExynosComponent, __FUNCTION__);
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
            }
            break;
        case OMX_StatePause:
            pExynosComponent->currentState = OMX_StateExecuting;
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StatePause:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
            pExynosComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateExecuting:
            pExynosComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateWaitForResources:
        switch (currentState) {
        case OMX_StateLoaded:
            pExynosComponent->currentState = OMX_StateWaitForResources;
            break;
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    default:
        ret = OMX_ErrorIncorrectStateTransition;
        break;
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pExynosComponent->pCallbacks != NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete", pExynosComponent, __FUNCTION__);
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
            pExynosComponent->callbackData,
            OMX_EventCmdComplete, OMX_CommandStateSet,
            destState, NULL);
        }
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] (0x%x)", pExynosComponent, __FUNCTION__, ret);
        if (pExynosComponent->pCallbacks != NULL) {
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
            pExynosComponent->callbackData,
            OMX_EventError, ret, 0, NULL);
        }
    }

NO_EVENT_EXIT: /* postpone to send event */
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SendCommand(
        OMX_IN OMX_HANDLETYPE  hComponent,
        OMX_IN OMX_COMMANDTYPE Cmd,
        OMX_IN OMX_U32         nParam,
        OMX_IN OMX_PTR         pCmdData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if (hComponent == NULL) {
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
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (Cmd) {
    case OMX_CommandStateSet :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Command: OMX_CommandStateSet", pExynosComponent, __FUNCTION__);
        ret = Exynos_StateSet(pExynosComponent, nParam);
        if (ret == OMX_ErrorNone)
           Exynos_SetStateSet(pExynosComponent, nParam);
        ret = Exynos_OMX_ComponentStateSet(pOMXComponent, nParam);
        break;
    case OMX_CommandFlush :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Command: OMX_CommandFlush", pExynosComponent, __FUNCTION__);
        ret = Exynos_SetPortFlush(pExynosComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        ret = Exynos_OMX_BufferFlushProcess(pOMXComponent, nParam, OMX_TRUE);
        break;
    case OMX_CommandPortDisable :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Command: OMX_CommandPortDisable", pExynosComponent, __FUNCTION__);
        ret = Exynos_SetPortDisable(pExynosComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        ret = Exynos_OMX_PortDisableProcess(pOMXComponent, nParam);
        break;
    case OMX_CommandPortEnable :
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Command: OMX_CommandPortEnable", pExynosComponent, __FUNCTION__);
        ret = Exynos_SetPortEnable(pExynosComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        ret = Exynos_OMX_PortEnableProcess(pOMXComponent, nParam);
        break;
    default:
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SetCallbacks(
    OMX_IN OMX_HANDLETYPE    hComponent,
    OMX_IN OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN OMX_PTR           pAppData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if (hComponent == NULL) {
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

    if (pCallbacks == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }
    if (pExynosComponent->currentState != OMX_StateLoaded) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pExynosComponent->pCallbacks   = pCallbacks;
    pExynosComponent->callbackData = pAppData;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BaseComponent_Constructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] lib version is %s", __FUNCTION__, IS_64BIT_OS? "64bit":"32bit");

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] OMX_ErrorBadParameter (0x%x)", __FUNCTION__, ret);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_BASECOMPONENT));
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Malloc (0x%x)", __FUNCTION__, ret);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent, 0, sizeof(EXYNOS_OMX_BASECOMPONENT));
    pOMXComponent->pComponentPrivate = (OMX_PTR)pExynosComponent;

    pOMXComponent->GetComponentVersion = &Exynos_OMX_GetComponentVersion;
    pOMXComponent->SendCommand         = &Exynos_OMX_SendCommand;
    pOMXComponent->GetState            = &Exynos_OMX_GetState;
    pOMXComponent->SetCallbacks        = &Exynos_OMX_SetCallbacks;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BaseComponent_Destructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if (hComponent == NULL) {
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

    Exynos_OSAL_Free(pExynosComponent);
    pExynosComponent = NULL;

    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}

void Exynos_UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *exynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    exynosInputPort->portDefinition.nBufferSize = ALIGN(exynosInputPort->portDefinition.format.video.nFrameWidth, 16) *
                                                  ALIGN(exynosInputPort->portDefinition.format.video.nFrameHeight, 16) * 3 / 2;

    if ((exynosOutputPort->portDefinition.format.video.nFrameWidth !=
            exynosInputPort->portDefinition.format.video.nFrameWidth) ||
        (exynosOutputPort->portDefinition.format.video.nFrameHeight !=
            exynosInputPort->portDefinition.format.video.nFrameHeight)) {
        OMX_U32 width = 0, height = 0;

        exynosOutputPort->portDefinition.format.video.nFrameWidth =
            exynosInputPort->portDefinition.format.video.nFrameWidth;
        exynosOutputPort->portDefinition.format.video.nFrameHeight =
            exynosInputPort->portDefinition.format.video.nFrameHeight;
        width = exynosOutputPort->portDefinition.format.video.nStride =
            exynosInputPort->portDefinition.format.video.nStride;
        height = exynosOutputPort->portDefinition.format.video.nSliceHeight =
            exynosInputPort->portDefinition.format.video.nSliceHeight;

        if (width && height)
            exynosOutputPort->portDefinition.nBufferSize = ALIGN((ALIGN(width, 16) * ALIGN(height, 16) * 3) / 2, 512);
    }

    return;
}

void Exynos_Input_SetSupportFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_COLOR_FORMATTYPE             ret        = OMX_COLOR_FormatUnused;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc  = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    if ((pVideoEnc == NULL) || (pInputPort == NULL))
        return ;

    if (pInputPort->supportFormat != NULL) {
        OMX_BOOL ret = OMX_FALSE;
        int nLastIndex = INPUT_PORT_SUPPORTFORMAT_DEFAULT_NUM;
        int i;

        /* default supported formats */
        pInputPort->supportFormat[0] = OMX_COLOR_FormatYUV420Planar;
        pInputPort->supportFormat[1] = OMX_COLOR_FormatYUV420SemiPlanar;
        pInputPort->supportFormat[2] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear;
        pInputPort->supportFormat[3] = OMX_COLOR_Format32bitARGB8888;
        pInputPort->supportFormat[4] = (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888;
#ifdef USE_ANDROID
        pInputPort->supportFormat[nLastIndex++] = OMX_COLOR_FormatAndroidOpaque;
#endif

        /* add extra formats, if It is supported by H/W. (CSC doesn't exist) */
        /* OMX_SEC_COLOR_FormatNV12Tiled */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled;

        /* OMX_SEC_COLOR_FormatYVU420Planar */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar;

        /* OMX_COLOR_Format32bitBGRA8888 */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent, OMX_COLOR_Format32bitBGRA8888);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = OMX_COLOR_Format32bitBGRA8888;

        for (i = 0; i < nLastIndex; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] Supported Format[%d] : 0x%x",
                                                pExynosComponent, __FUNCTION__, i, pInputPort->supportFormat[i]);
        }

        pInputPort->supportFormat[nLastIndex] = OMX_COLOR_FormatUnused;
    }

    return ;
}

OMX_COLOR_FORMATTYPE Exynos_Input_GetActualColorFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_COLOR_FORMATTYPE             ret                = OMX_COLOR_FormatUnused;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE             eColorFormat       = pInputPort->portDefinition.format.video.eColorFormat;

#ifdef USE_ANDROID
    if (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque)
        eColorFormat = pVideoEnc->surfaceFormat;
#endif

    if (pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent, eColorFormat) == OMX_TRUE) {
        ret = eColorFormat;
        goto EXIT;
    }

    switch ((int)eColorFormat) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_COLOR_FormatYUV420Planar:       /* converted to NV12 using CSC */
    case OMX_COLOR_Format32bitARGB8888:      /* converted to NV12 using CSC */
    case OMX_COLOR_Format32BitRGBA8888:      /* converted to NV12 using CSC */
        ret = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
    case OMX_SEC_COLOR_FormatNV21Linear:
    case OMX_SEC_COLOR_FormatNV12Tiled:
        ret = eColorFormat;
        break;
    default:
        ret = OMX_COLOR_FormatUnused;
        break;
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_ResetAllPortConfig(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT           *pInputPort        = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort       = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    /* Input port */
    pInputPort->portDefinition.format.video.nFrameWidth             = DEFAULT_FRAME_WIDTH;
    pInputPort->portDefinition.format.video.nFrameHeight            = DEFAULT_FRAME_HEIGHT;
    pInputPort->portDefinition.format.video.nStride                 = 0; /*DEFAULT_FRAME_WIDTH;*/
    pInputPort->portDefinition.format.video.nSliceHeight            = 0;
    pInputPort->portDefinition.format.video.pNativeRender           = 0;
    pInputPort->portDefinition.format.video.bFlagErrorConcealment   = OMX_FALSE;
    pInputPort->portDefinition.format.video.eColorFormat            = OMX_COLOR_FormatYUV420SemiPlanar;

    pInputPort->portDefinition.nBufferSize  = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pInputPort->portDefinition.bEnabled     = OMX_TRUE;

    pInputPort->bufferProcessType   = BUFFER_COPY;
    pInputPort->portWayType         = WAY2_PORT;
    Exynos_SetPlaneToPort(pInputPort, MFC_DEFAULT_INPUT_BUFFER_PLANE);

    /* Output port */
    pOutputPort->portDefinition.format.video.nFrameWidth            = DEFAULT_FRAME_WIDTH;
    pOutputPort->portDefinition.format.video.nFrameHeight           = DEFAULT_FRAME_HEIGHT;
    pOutputPort->portDefinition.format.video.nStride                = 0; /*DEFAULT_FRAME_WIDTH;*/
    pOutputPort->portDefinition.format.video.nSliceHeight           = 0;
    pOutputPort->portDefinition.format.video.pNativeRender          = 0;
    pOutputPort->portDefinition.format.video.bFlagErrorConcealment  = OMX_FALSE;
    pOutputPort->portDefinition.format.video.eColorFormat           = OMX_COLOR_FormatUnused;

    pOutputPort->portDefinition.nBufferCountActual  = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pOutputPort->portDefinition.nBufferCountMin     = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pOutputPort->portDefinition.nBufferSize  = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pOutputPort->portDefinition.bEnabled     = OMX_TRUE;

    pOutputPort->bufferProcessType  = BUFFER_SHARE;
    pOutputPort->portWayType        = WAY2_PORT;
    pOutputPort->latestTimeStamp    = DEFAULT_TIMESTAMP_VAL;
    Exynos_SetPlaneToPort(pOutputPort, Exynos_OSAL_GetPlaneCount(OMX_COLOR_FormatYUV420Planar, pOutputPort->ePlaneType));

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    ret = Exynos_OMX_BaseComponent_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to construct BaseComponent for WFD", __FUNCTION__);
        goto EXIT;
    }

    ret = Exynos_OMX_Port_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Port_Constructor", __FUNCTION__);
        Exynos_OMX_BaseComponent_Destructor(pOMXComponent);
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pVideoEnc = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_VIDEOENC_COMPONENT));
    if (pVideoEnc == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_Port_Destructor(pOMXComponent);
        Exynos_OMX_BaseComponent_Destructor(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Memset(pVideoEnc, 0, sizeof(EXYNOS_OMX_VIDEOENC_COMPONENT));
    pExynosComponent->hComponentHandle = (OMX_HANDLETYPE)pVideoEnc;

    pVideoEnc->nQosRatio         = 0;
    pVideoEnc->quantization.nQpI = 4; // I frame quantization parameter
    pVideoEnc->quantization.nQpP = 5; // P frame quantization parameter
    pVideoEnc->quantization.nQpB = 5; // B frame quantization parameter

    pVideoEnc->bUseBlurFilter   = OMX_FALSE;
    pVideoEnc->eBlurMode        = BLUR_MODE_NONE;
    pVideoEnc->eBlurResol       = BLUR_RESOL_240;

    pVideoEnc->eRotationType    = ROTATE_0;

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort->supportFormat, 0, (sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX));

    pExynosPort->portDefinition.nBufferCountActual = MAX_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferCountMin = MAX_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferSize = 0;
    pExynosPort->portDefinition.eDomain = OMX_PortDomainVideo;

    pExynosPort->portDefinition.format.video.cMIMEType = Exynos_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    if (pExynosPort->portDefinition.format.video.cMIMEType == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pExynosPort->portDefinition.format.video.nFrameWidth = 0;
    pExynosPort->portDefinition.format.video.nFrameHeight= 0;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.format.video.nBitrate = 1000000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pVideoEnc->eControlRate[INPUT_PORT_INDEX] = OMX_Video_ControlRateVariable;

    pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort->supportFormat, 0, (sizeof(OMX_COLOR_FORMATTYPE) * OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX));

    pExynosPort->portDefinition.nBufferCountActual = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferCountMin = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.eDomain = OMX_PortDomainVideo;

    pExynosPort->portDefinition.format.video.cMIMEType = Exynos_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    if (pExynosPort->portDefinition.format.video.cMIMEType == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pExynosPort->portDefinition.format.video.nFrameWidth = 0;
    pExynosPort->portDefinition.format.video.nFrameHeight= 0;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.format.video.nBitrate = 1000000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] = OMX_Video_ControlRateVariable;

    pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;

    pOMXComponent->UseBuffer              = &Exynos_OMX_UseBuffer;
    pOMXComponent->AllocateBuffer         = &Exynos_OMX_AllocateBuffer;
    pOMXComponent->FreeBuffer             = &Exynos_OMX_FreeBuffer;

    pExynosComponent->exynos_BufferFlush  = &Exynos_OMX_BufferFlush;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentDeinit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort      = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = NULL;

    int i = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_Free(pVideoEnc);
    pExynosComponent->hComponentHandle = pVideoEnc = NULL;

    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    if (pExynosPort->processData.extInfo != NULL) {
        Exynos_OSAL_Free(pExynosPort->processData.extInfo);
        pExynosPort->processData.extInfo = NULL;
    }

    for(i = 0; i < ALL_PORT_NUM; i++) {
        pExynosPort = &pExynosComponent->pExynosPort[i];
        Exynos_OSAL_Free(pExynosPort->portDefinition.format.video.cMIMEType);
        pExynosPort->portDefinition.format.video.cMIMEType = NULL;

        Exynos_OSAL_Free(pExynosPort->supportFormat);
        pExynosPort->supportFormat = NULL;
    }

    ret = Exynos_OMX_Port_Destructor(pOMXComponent);

    ret = Exynos_OMX_BaseComponent_Destructor(hComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_AllocateBuffer(
        OMX_IN OMX_HANDLETYPE            hComponent,
        OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
        OMX_IN OMX_U32                   nPortIndex,
        OMX_IN OMX_PTR                   pAppPrivate,
        OMX_IN OMX_U32                   nSizeBytes)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *pTempBufferHdr     = NULL;
    OMX_U8                          *pTempBuffer        = NULL;
    OMX_U32                          i                  = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, nPortIndex);
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (pExynosPort->portState != EXYNOS_OMX_PortStateEnabling) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] %s port : invalid state : comp state(0x%x), port state(0x%x), enabled(0x%x)",
                                            pExynosComponent, __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                            pExynosComponent->currentState, pExynosPort->portState, pExynosPort->portDefinition.bEnabled);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    pTempBuffer = (OMX_U8 *)Exynos_OSAL_Malloc(64);
    if (pTempBuffer == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBuffer, 0, 64);

    pTempBufferHdr = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (pTempBufferHdr == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = pTempBufferHdr;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ALLOCATED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(pTempBufferHdr, OMX_BUFFERHEADERTYPE);

            pTempBufferHdr->pBuffer        = pTempBuffer;
            pTempBufferHdr->nAllocLen      = nSizeBytes;
            pTempBufferHdr->pAppPrivate    = pAppPrivate;

            if (nPortIndex == INPUT_PORT_INDEX)
                pTempBufferHdr->nInputPortIndex = INPUT_PORT_INDEX;
            else
                pTempBufferHdr->nOutputPortIndex = OUTPUT_PORT_INDEX;

            pExynosPort->assignedBufferNum++;

            *ppBufferHdr = pTempBufferHdr;

            if (pExynosPort->assignedBufferNum == (OMX_S32)pExynosPort->portDefinition.nBufferCountActual) {
                pExynosPort->portDefinition.bPopulated = OMX_TRUE;

                if ((pExynosComponent->currentState == OMX_StateLoaded) &&
                    (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle)) {
                    if (CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]) &&
                        CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX])) {
                        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete", pExynosComponent, __FUNCTION__);
                        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                pExynosComponent->callbackData,
                                                                OMX_EventCmdComplete, OMX_CommandStateSet,
                                                                OMX_StateIdle, NULL);

                        for (i = 0; i < pExynosComponent->portParam.nPorts; i++)
                            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateIdle;

                        pExynosComponent->transientState = EXYNOS_OMX_TransStateMax;
                        pExynosComponent->currentState = OMX_StateIdle;
                    }
                } else if(!(CHECK_PORT_ENABLED(pExynosPort)) &&
                        (pExynosPort->portState == EXYNOS_OMX_PortStateEnabling)) {
                    pExynosPort->portState = EXYNOS_OMX_PortStateIdle;

                    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] Enable %s Port", pExynosComponent, __FUNCTION__,
                                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output");
                    ret = Exynos_OMX_EnablePort(pOMXComponent, nPortIndex);

                    if ((ret == OMX_ErrorNone) && (pExynosComponent->pCallbacks != NULL)) {
                        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(Enable/%s port)",
                                                            pExynosComponent, __FUNCTION__,
                                                            (nPortIndex == INPUT_PORT_INDEX) ? "input" : "output");

                        pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                                pExynosComponent->callbackData,
                                                                OMX_EventCmdComplete,
                                                                OMX_CommandPortEnable,
                                                                (nPortIndex == INPUT_PORT_INDEX) ? INPUT_PORT_INDEX : OUTPUT_PORT_INDEX,
                                                                NULL);

                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(EventCmdComplete/Enable/%s port)",
                                                                pExynosComponent, __FUNCTION__,
                                                                (nPortIndex == INPUT_PORT_INDEX)? "input":"output");

                    }
                }
            }

            ret = OMX_ErrorNone;

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port: buffer header(%p), size(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                    (*ppBufferHdr), nSizeBytes);
            goto EXIT;
        }
    }

    ret = OMX_ErrorInsufficientResources;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pTempBufferHdr != NULL)
            Exynos_OSAL_Free(pTempBufferHdr);

        if (pTempBuffer != NULL)
            Exynos_OSAL_Free(pTempBuffer);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes,
    OMX_IN OMX_U8                   *pBuffer)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE     *pTempBufferHdr    = NULL;
    OMX_U32                   i                 = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, nPortIndex);
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (pExynosPort->portState != EXYNOS_OMX_PortStateEnabling) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] %s port : invalid state : comp state(0x%x), port state(0x%x), enabled(0x%x)",
                                            pExynosComponent, __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                            pExynosComponent->currentState, pExynosPort->portState, pExynosPort->portDefinition.bEnabled);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    pTempBufferHdr = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (pTempBufferHdr == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = pTempBufferHdr;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(pTempBufferHdr, OMX_BUFFERHEADERTYPE);

            pTempBufferHdr->pBuffer        = pBuffer;
            pTempBufferHdr->nAllocLen      = nSizeBytes;
            pTempBufferHdr->pAppPrivate    = pAppPrivate;

            if (nPortIndex == INPUT_PORT_INDEX)
                pTempBufferHdr->nInputPortIndex = INPUT_PORT_INDEX;
            else
                pTempBufferHdr->nOutputPortIndex = OUTPUT_PORT_INDEX;

            pExynosPort->assignedBufferNum++;

            *ppBufferHdr = pTempBufferHdr;

            if (pExynosPort->assignedBufferNum == (OMX_S32)pExynosPort->portDefinition.nBufferCountActual) {
                pExynosPort->portDefinition.bPopulated = OMX_TRUE;

                if ((pExynosComponent->currentState == OMX_StateLoaded) &&
                    (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle)) {
                    if (CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]) &&
                        CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX])) {
                        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete", pExynosComponent, __FUNCTION__);
                        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                pExynosComponent->callbackData,
                                                                OMX_EventCmdComplete, OMX_CommandStateSet,
                                                                OMX_StateIdle, NULL);

                        for (i = 0; i < pExynosComponent->portParam.nPorts; i++)
                            pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateIdle;

                        pExynosComponent->transientState = EXYNOS_OMX_TransStateMax;
                        pExynosComponent->currentState = OMX_StateIdle;
                    }
                } else if(!(CHECK_PORT_ENABLED(pExynosPort)) &&
                        (pExynosPort->portState == EXYNOS_OMX_PortStateEnabling)) {
                    pExynosPort->portState = EXYNOS_OMX_PortStateIdle;

                    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] Enable %s Port", pExynosComponent, __FUNCTION__,
                                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output");
                    ret = Exynos_OMX_EnablePort(pOMXComponent, nPortIndex);

                    if ((ret == OMX_ErrorNone) && (pExynosComponent->pCallbacks != NULL)) {
                        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete(Enable/%s port)",
                                                            pExynosComponent, __FUNCTION__,
                                                            (nPortIndex == INPUT_PORT_INDEX) ? "input" : "output");

                        pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                                pExynosComponent->callbackData,
                                                                OMX_EventCmdComplete,
                                                                OMX_CommandPortEnable,
                                                                (nPortIndex == INPUT_PORT_INDEX) ? INPUT_PORT_INDEX : OUTPUT_PORT_INDEX,
                                                                NULL);

                        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(EventCmdComplete/Enable/%s port)",
                                                                pExynosComponent, __FUNCTION__,
                                                                (nPortIndex == INPUT_PORT_INDEX)? "input":"output");
                    }
                }
            }

            ret = OMX_ErrorNone;
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port: buffer header(%p), size(%d)",
                                                    pExynosComponent, __FUNCTION__,
                                                    (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                    (*ppBufferHdr), nSizeBytes);
            goto EXIT;
        }
    }

    Exynos_OSAL_Free(pTempBufferHdr);
    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_U32               nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *pOMXBufferHdr      = NULL;
    OMX_U32                          i                  = 0;

    FunctionIn();

    if (hComponent == NULL) {
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

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter(0x%x)", pExynosComponent, __FUNCTION__, nPortIndex);
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pExynosPort->portState != EXYNOS_OMX_PortStateDisabling) &&
        (pExynosPort->portState != EXYNOS_OMX_PortStateFlushingForDisable) &&
        (pExynosPort->portState != EXYNOS_OMX_PortStateInvalid)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] %s port : invalid state : comp state(0x%x), port state(0x%x), enabled(0x%x)",
                                            pExynosComponent, __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                            pExynosComponent->currentState, pExynosPort->portState, pExynosPort->portDefinition.bEnabled);
        ret = OMX_ErrorIncorrectStateOperation;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] send event(OMX_EventError)", pExynosComponent, __FUNCTION__);

        (*(pExynosComponent->pCallbacks->EventHandler)) (pOMXComponent,
                                                    pExynosComponent->callbackData,
                                                    (OMX_U32)OMX_EventError,
                                                    (OMX_U32)OMX_ErrorPortUnpopulated,
                                                    nPortIndex, NULL);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    for (i = 0; i < /*pExynosPort->portDefinition.nBufferCountActual*/MAX_BUFFER_NUM; i++) {
        if ((pExynosPort->bufferStateAllocate[i] != BUFFER_STATE_FREE) &&
                (pExynosPort->extendBufferHeader[i].OMXBufferHeader != NULL)) {
            pOMXBufferHdr = pExynosPort->extendBufferHeader[i].OMXBufferHeader;

            if (pOMXBufferHdr->pBuffer == pBufferHdr->pBuffer) {
                Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] %s port: buffer header(%p)",
                                                        pExynosComponent, __FUNCTION__,
                                                        (nPortIndex == INPUT_PORT_INDEX)? "input":"output",
                                                        pOMXBufferHdr);
                if (pExynosPort->bufferStateAllocate[i] & BUFFER_STATE_ALLOCATED) {
                    pOMXBufferHdr->pBuffer  = NULL;
                    pBufferHdr->pBuffer     = NULL;
                }

                pExynosPort->assignedBufferNum--;

                if (pExynosPort->bufferStateAllocate[i] & HEADER_STATE_ALLOCATED) {
                    Exynos_OSAL_Free(pOMXBufferHdr);
                    pExynosPort->extendBufferHeader[i].OMXBufferHeader = NULL;
                    pBufferHdr = NULL;
                }

                pExynosPort->bufferStateAllocate[i] = BUFFER_STATE_FREE;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pExynosPort->assignedBufferNum < (OMX_S32)pExynosPort->portDefinition.nBufferCountActual)
            pExynosPort->portDefinition.bPopulated = OMX_FALSE;

        if (pExynosPort->assignedBufferNum == 0) {
            if ((pExynosComponent->currentState == OMX_StateIdle) &&
                (pExynosComponent->transientState == EXYNOS_OMX_TransStateIdleToLoaded)) {
                if (!CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[INPUT_PORT_INDEX]) &&
                    !CHECK_PORT_POPULATED(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX])) {
                    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] OMX_EventCmdComplete", pExynosComponent, __FUNCTION__);
                    pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                            pExynosComponent->callbackData,
                                                            OMX_EventCmdComplete, OMX_CommandStateSet,
                                                            OMX_StateLoaded, NULL);

                    for (i = 0; i < pExynosComponent->portParam.nPorts; i++)
                        pExynosComponent->pExynosPort[i].portState = EXYNOS_OMX_PortStateLoaded;

                    pExynosComponent->transientState = EXYNOS_OMX_TransStateMax;
                    pExynosComponent->currentState = OMX_StateLoaded;
                }
            }

            if (pExynosPort->portState == EXYNOS_OMX_PortStateDisabling) {
                if (!CHECK_PORT_ENABLED(pExynosPort)) {
                    pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                                            pExynosComponent->callbackData,
                                                            OMX_EventCmdComplete,
                                                            OMX_CommandPortDisable, nPortIndex, NULL);

                    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s]send event(EventCmdComplete/Disable/%s port)",
                                                            pExynosComponent, __FUNCTION__,
                                                            (nPortIndex == INPUT_PORT_INDEX)? "input":"output");

                    pExynosPort->portState = EXYNOS_OMX_PortStateLoaded;
                }
            }
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FlushPort(
        OMX_COMPONENTTYPE   *pOMXComponent,
        OMX_S32              nPortIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE     *pBufferHdr        = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    unsigned int              i                 = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if ((nPortIndex < 0) ||
            (nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (nPortIndex == INPUT_PORT_INDEX) {
            if (inputBufArray[i].bInOMX == OMX_TRUE) {
                if (inputBufArray[i].pBuffer != NULL) {
                    pBufferHdr = inputBufArray[i].pBuffer;
                    pBufferHdr->nFilledLen = 0;
                    Exynos_OMX_InputBufferReturn(pOMXComponent, pBufferHdr);
                }
            }
        } else {
            if (outputBufArray[i].bInOMX == OMX_TRUE) {
                if (outputBufArray[i].pBuffer != NULL) {
                    pBufferHdr = outputBufArray[i].pBuffer;
                    pBufferHdr->nFilledLen = 0;
                    Exynos_OMX_OutputBufferReturn(pOMXComponent, pBufferHdr);
                }
            }
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferFlush(
        OMX_COMPONENTTYPE   *pOMXComponent,
        OMX_S32              nPortIndex,
        OMX_BOOL             bEvent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if ((nPortIndex < 0) ||
            (nPortIndex >= (OMX_S32)pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] OMX_CommandFlush start, port:%d, event:%d",
                                        pExynosComponent, __FUNCTION__, nPortIndex, bEvent);

    ret = Exynos_OMX_FlushPort(pOMXComponent, nPortIndex);
    if (ret != OMX_ErrorNone)
        goto EXIT;

    Exynos_ResetCodecData(&pExynosPort->processData);

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%p][%s] OMX_CommandFlush end, port:%d, event:%d",
                                        pExynosComponent, __FUNCTION__, nPortIndex, bEvent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_ResetCodecData(EXYNOS_OMX_DATA *pData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (pData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /* caution: nPlanes in buffer structure might be used all times */
    Exynos_OSAL_Memset(&(pData->buffer.fd), 0, sizeof(pData->buffer.fd));
    Exynos_OSAL_Memset(&(pData->buffer.addr), 0, sizeof(pData->buffer.addr));

    pData->dataLen       = 0;
    pData->usedDataLen   = 0;
    pData->remainDataLen = 0;
    pData->nFlags        = 0;
    pData->timeStamp     = 0;
    pData->pPrivate      = NULL;
    pData->bufferHeader  = NULL;

EXIT:
    return ret;
}
