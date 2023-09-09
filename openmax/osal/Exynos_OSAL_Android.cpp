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
 * @file        Exynos_OSAL_Android.cpp
 * @brief
 * @author      Seungbeom Kim (sbcrux.kim@samsung.com)
 * @author      Hyeyeon Chung (hyeon.chung@samsung.com)
 * @author      Yunji Kim (yunji.kim@samsung.com)
 * @author      Jinsung Yang (jsgood.yang@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>

#include <cutils/properties.h>
#include <media/hardware/OMXPluginBase.h>
#include <media/hardware/HardwareAPI.h>
#include <media/hardware/MetadataBufferType.h>
#include <media/stagefright/foundation/ColorUtils.h>

#include <system/graphics.h>
#include <hardware/hardware.h>
#include <hidl/HidlSupport.h>
#include <memory>

#include "csc.h"
#include "ExynosGraphicBuffer.h"
#include <hardware/exynos/ion.h>

#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Macros.h"
#include "Exynos_OSAL_Platform.h"
#include "Exynos_OSAL_ETC.h"
#include "exynos_format.h"

#include "ExynosVideoApi.h"
#include "VendorVideoAPI.h"
#include <OperatorFactory.h>

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "Exynos_OSAL_Android"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

using namespace android;

using namespace ::vendor::graphics;
using namespace ::vendor::graphics::ion;

#define OMX_GRALLOC_USAGE_HW_VIDEO (ExynosGraphicBufferUsage::VIDEO_PRIVATE_DATA)
#define OMX_GRALLOC_USAGE_HW_VIDEO_DEC ((uint64_t)BufferUsage::VIDEO_DECODER)     // decoder(prod)
#define OMX_GRALLOC_USAGE_HW_VIDEO_ENC ((uint64_t)BufferUsage::VIDEO_ENCODER)     // encoder(cons)
#define OMX_GRALLOC_USAGE_DISP ((uint64_t)BufferUsage::GPU_TEXTURE)               // decoder(prod)
#define OMX_GRALLOC_USAGE_UVA ((uint64_t)(BufferUsage::CPU_READ_OFTEN | BufferUsage::CPU_WRITE_OFTEN))
#define OMX_GRALLOC_USAGE_LOCK (OMX_GRALLOC_USAGE_UVA | OMX_GRALLOC_USAGE_HW_VIDEO)
#define OMX_GRALLOC_USAGE_PROTECTED ((uint64_t)BufferUsage::PROTECTED)

#ifdef __cplusplus
extern "C" {
#endif

#define FD_NUM 1
#define EXT_DATA_NUM 2  /* alloc len, stream len for HDCP */

typedef struct _EXYNOS_OMX_SHARED_BUFFER {
    void *bufferHandle;
    unsigned long long bufferFd;
    unsigned long long bufferFd1;
    unsigned long long bufferFd2;

    ion_user_handle_t  ionHandle;
    ion_user_handle_t  ionHandle1;
    ion_user_handle_t  ionHandle2;

    OMX_U32 cnt;
} EXYNOS_OMX_SHARED_BUFFER;

typedef struct _EXYNOS_OMX_REF_HANDLE {
    OMX_HANDLETYPE           hMutex;
    EXYNOS_OMX_SHARED_BUFFER SharedBuffer[MAX_BUFFER_REF];
} EXYNOS_OMX_REF_HANDLE;

// for performance
typedef struct _PERFORMANCE_HANDLE {
    bool bIsEncoder;
    void *pEpic;
} PERFORMANCE_HANDLE;
static int lockCnt = 0;

#ifndef USE_WA_ION_BUF_REF
static int getIonFd()
{
    return get_ion_fd(); //gralloc4/libexynosgraphicbuffer/ion_helper.cpp
}
#endif

static OMX_ERRORTYPE setBufferProcessTypeForDecoder(EXYNOS_OMX_BASEPORT *pExynosPort)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if (pExynosPort == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ((pExynosPort->bufferProcessType & BUFFER_COPY) &&
        (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED)) {
        int i;
        if (pExynosPort->supportFormat == NULL) {
            ret = OMX_ErrorUndefined;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] supported format is empty", __FUNCTION__);
            goto EXIT;
        }

        pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        /* check to support NV12T format */
        for (i = 0; pExynosPort->supportFormat[i] != OMX_COLOR_FormatUnused; i++) {
            /* prefer to use NV12T */
            if (pExynosPort->supportFormat[i] == (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled) {
                pExynosPort->portDefinition.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled;
                break;
            }
        }

        if (pExynosPort->bufferProcessType & BUFFER_COPY_FORCE)
            pExynosPort->bufferProcessType = (EXYNOS_OMX_BUFFERPROCESS_TYPE)(BUFFER_COPY_FORCE | BUFFER_SHARE);
        else
            pExynosPort->bufferProcessType = BUFFER_SHARE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] output buffer mode(%x), format(%x)",
                                                __FUNCTION__, pExynosPort->bufferProcessType,
                                                pExynosPort->portDefinition.format.video.eColorFormat);
    }

EXIT:
    FunctionOut();

    return ret;
}

static void resetBufferProcessTypeForDecoder(EXYNOS_OMX_BASEPORT *pExynosPort)
{
    FunctionIn();

    if (pExynosPort == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (pExynosPort->bufferProcessType & BUFFER_COPY_FORCE)
        pExynosPort->bufferProcessType = (EXYNOS_OMX_BUFFERPROCESS_TYPE)(BUFFER_COPY_FORCE | BUFFER_COPY);
    else
        pExynosPort->bufferProcessType = BUFFER_COPY;

    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

EXIT:
    FunctionOut();

    return;
}

static OMX_COLOR_FORMATTYPE getBufferFormat(OMX_IN OMX_PTR handle)
{
    FunctionIn();

    OMX_COLOR_FORMATTYPE ret = OMX_COLOR_FormatUnused;
    ExynosGraphicBufferMeta graphicBuffer((buffer_handle_t)handle);

    ret = Exynos_OSAL_HAL2OMXColorFormat(graphicBuffer.format);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] HAL(0x%x), OMX(0x%x)", __FUNCTION__, graphicBuffer.format, ret);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE getDataSpaceFromAspects(OMX_PTR param)
{
    OMX_ERRORTYPE               ret         = OMX_ErrorNone;
    DescribeColorAspectsParams *pInfo       = (DescribeColorAspectsParams *)param;
    android_dataspace           dataSpace   = HAL_DATASPACE_UNKNOWN;

    FunctionIn();

    dataSpace = android::ColorUtils::getDataSpaceForColorAspects(pInfo->sAspects, false);

    if (dataSpace == HAL_DATASPACE_UNKNOWN) {
        ret = OMX_ErrorUnsupportedSetting;
        goto EXIT;
    } else  {
        switch ((int)dataSpace) {
        case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_HLG | HAL_DATASPACE_STANDARD_BT2020):
        case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT2020):
        case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_STANDARD_BT2020):
        case HAL_DATASPACE_BT2020:        /* HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT2020 */
        case HAL_DATASPACE_BT2020_PQ:     /* HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_STANDARD_BT2020 */
        case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT709):
        case HAL_DATASPACE_BT709:
        case HAL_DATASPACE_V0_BT709:      /* HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT709 */
        case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_625):
        case HAL_DATASPACE_BT601_625:
        case HAL_DATASPACE_V0_BT601_625:  /* HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_625 */
        case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_525):
        case HAL_DATASPACE_BT601_525:
        case HAL_DATASPACE_V0_BT601_525:  /* HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_525 */
        case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED):
        case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED):
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] guidance for the dataSpace is (0x%x) ", __FUNCTION__, dataSpace);
            pInfo->nDataSpace = dataSpace;
            break;
        default:
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] dataSpace(0x%x) is not supported", __FUNCTION__, dataSpace);
            ret = OMX_ErrorUnsupportedSetting;
            break;
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE getColorAspectsFromDataSpace(OMX_PTR param)
{
    OMX_ERRORTYPE               ret         = OMX_ErrorNone;
    DescribeColorAspectsParams *pInfo       = (DescribeColorAspectsParams *)param;

    int range, standard, transfer;
    unsigned int dataspace;

    FunctionIn();

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] dataSpace(0x%x), pixelformat(0x%x)", __FUNCTION__, pInfo->nDataSpace, pInfo->nPixelFormat);

    switch (pInfo->nDataSpace) {
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_HLG | HAL_DATASPACE_STANDARD_BT2020):
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT2020):
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_STANDARD_BT2020):
    case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_625):
    case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_525):
    case HAL_DATASPACE_BT709:
    case HAL_DATASPACE_V0_BT709:
    case HAL_DATASPACE_BT2020:
    case HAL_DATASPACE_BT2020_PQ:
    case HAL_DATASPACE_BT601_625:
    case HAL_DATASPACE_V0_BT601_625:
    case HAL_DATASPACE_BT601_525:
    case HAL_DATASPACE_V0_BT601_525:
        dataspace = (unsigned int)pInfo->nDataSpace;
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] dataSpace(0x%x) is not supported", __FUNCTION__, pInfo->nDataSpace);
        ret = OMX_ErrorUnsupportedSetting;
        goto EXIT;
    }

    switch (pInfo->nPixelFormat) {
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P:       /* I420 */
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_PN:
    case HAL_PIXEL_FORMAT_YV12:                     /* YV12 */
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP:      /* NV12 */
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_PRIV:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:    /* NV21 */
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_BGRA_8888:                /* BGRA */
    case HAL_PIXEL_FORMAT_RGBA_8888:                /* RGBA */
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:   /* NV21 : HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M */
        /* W/A for MCD : upper limitation of 8bit is SDR(BT.709) */
        if ((dataspace & HAL_DATASPACE_STANDARD_BT2020) == HAL_DATASPACE_STANDARD_BT2020) {
            dataspace = HAL_DATASPACE_BT709;
        }
        break;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_S10B:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M:      /* P010 */
        /* W/A for MCD : lower limitation of 10bit is HDR(BT.2020) */
        if ((dataspace & HAL_DATASPACE_STANDARD_BT2020) != HAL_DATASPACE_STANDARD_BT2020) {
            unsigned int mask = 0xFFFFFFFF ^ (0b111111 << HAL_DATASPACE_STANDARD_SHIFT);
            dataspace = (dataspace & mask) | HAL_DATASPACE_STANDARD_BT2020;
        }
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] pixelFormat(0x%x) is not supported", __FUNCTION__, pInfo->nPixelFormat);
        ret = OMX_ErrorUnsupportedSetting;
        goto EXIT;
    }

    switch (dataspace) {
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_HLG | HAL_DATASPACE_STANDARD_BT2020):
        pInfo->sAspects.mRange          = ColorAspects::RangeLimited;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT2020;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT2020;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferHLG;
        break;
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT2020):
        pInfo->sAspects.mRange          = ColorAspects::RangeLimited;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT2020;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT2020;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferSMPTE170M;
        break;
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_STANDARD_BT2020):
        pInfo->sAspects.mRange          = ColorAspects::RangeLimited;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT2020;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT2020;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferST2084;
        break;
    case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_625):
        pInfo->sAspects.mRange          = ColorAspects::RangeFull;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT601_6_625;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT601_6;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferSMPTE170M;
        break;
    case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_525):
        pInfo->sAspects.mRange          = ColorAspects::RangeFull;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT601_6_525;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT601_6;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferSMPTE170M;
        break;
    case HAL_DATASPACE_BT709:
    case HAL_DATASPACE_V0_BT709:
        pInfo->sAspects.mRange          = ColorAspects::RangeLimited;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT709_5;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT709_5;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferSMPTE170M;
        break;
    case HAL_DATASPACE_BT2020:
        pInfo->sAspects.mRange          = ColorAspects::RangeFull;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT2020;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT2020;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferSMPTE170M;
        break;
    case HAL_DATASPACE_BT2020_PQ:
        pInfo->sAspects.mRange          = ColorAspects::RangeFull;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT2020;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT2020;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferST2084;
        break;
    case HAL_DATASPACE_BT601_625:
    case HAL_DATASPACE_V0_BT601_625:
        pInfo->sAspects.mRange          = ColorAspects::RangeLimited;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT601_6_625;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT601_6;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferSMPTE170M;
        break;
    case HAL_DATASPACE_BT601_525:
    case HAL_DATASPACE_V0_BT601_525:
        pInfo->sAspects.mRange          = ColorAspects::RangeLimited;
        pInfo->sAspects.mPrimaries      = ColorAspects::PrimariesBT601_6_525;
        pInfo->sAspects.mMatrixCoeffs   = ColorAspects::MatrixBT601_6;
        pInfo->sAspects.mTransfer       = ColorAspects::TransferSMPTE170M;
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] dataSpace(0x%x) is not supported", __FUNCTION__, pInfo->nDataSpace);
        ret = OMX_ErrorUnsupportedSetting;
        break;
    };

EXIT:
    FunctionOut();

    return ret;
}

void getColorAspectsPreferBitstream(
    EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA,
    EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA,
    void                          *pParam)
{
    FunctionIn();

    if ((pBSCA == NULL) || (pFWCA == NULL) || (pParam == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid argument", __FUNCTION__);
        return;
    }

    DescribeColorAspectsParams *pParams = (DescribeColorAspectsParams *)pParam;

    pParams->sAspects.mRange = (enum ColorAspects::Range)pBSCA->nRangeType;
    if (pBSCA->nRangeType == RANGE_UNSPECIFIED)
        pParams->sAspects.mRange = (enum ColorAspects::Range)pFWCA->nRangeType;

    switch ((ExynosPrimariesType)pBSCA->nPrimaryType) {
    case PRIMARIES_RESERVED:
    case PRIMARIES_UNSPECIFIED:
        pParams->sAspects.mPrimaries = (enum ColorAspects::Primaries)pFWCA->nPrimaryType;
        break;
    case PRIMARIES_BT709_5:
        pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT709_5;
        break;
    case PRIMARIES_BT601_6_625:
        pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT601_6_625;
        break;
    case PRIMARIES_BT601_6_525:
    case PRIMARIES_SMPTE_240M:
        pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT601_6_525;
        break;
    case PRIMARIES_BT2020:
        pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT2020;
        break;
    case PRIMARIES_BT470_6M:
        pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT470_6M;
        break;
    case PRIMARIES_GENERIC_FILM:
        pParams->sAspects.mPrimaries = ColorAspects::PrimariesGenericFilm;
        break;
    default:
        pParams->sAspects.mPrimaries = ColorAspects::PrimariesOther;
        break;
    }

    switch ((ExynosTransferType)pBSCA->nTransferType) {
    case TRANSFER_RESERVED:
    case TRANSFER_UNSPECIFIED:
        pParams->sAspects.mTransfer = (enum ColorAspects::Transfer)pFWCA->nTransferType;
        break;
    case TRANSFER_BT709:
    case TRANSFER_SMPTE_170M:
    case TRANSFER_BT2020_1:
    case TRANSFER_BT2020_2:
        pParams->sAspects.mTransfer = ColorAspects::TransferSMPTE170M;
        break;
    case TRANSFER_ST2084:
        pParams->sAspects.mTransfer = ColorAspects::TransferST2084;
        break;
    /* unsupported value */
    case TRANSFER_GAMMA_22:
        pParams->sAspects.mTransfer = ColorAspects::TransferGamma22;
        break;
    case TRANSFER_GAMMA_28:
        pParams->sAspects.mTransfer = ColorAspects::TransferGamma28;
        break;
    case TRANSFER_SMPTE_240M:
        pParams->sAspects.mTransfer = ColorAspects::TransferSMPTE240M;
        break;
    case TRANSFER_LINEAR:
        pParams->sAspects.mTransfer = ColorAspects::TransferLinear;
        break;
    case TRANSFER_XvYCC:
        pParams->sAspects.mTransfer = ColorAspects::TransferXvYCC;
        break;
    case TRANSFER_BT1361:
        pParams->sAspects.mTransfer = ColorAspects::TransferBT1361;
        break;
    case TRANSFER_SRGB:
        pParams->sAspects.mTransfer = ColorAspects::TransferSRGB;
        break;
    case TRANSFER_ST428:
        pParams->sAspects.mTransfer = ColorAspects::TransferST428;
        break;
    case TRANSFER_HLG:
        pParams->sAspects.mTransfer = ColorAspects::TransferHLG;
        break;
    default:
        pParams->sAspects.mTransfer = ColorAspects::TransferOther;
        break;
    }

    switch ((ExynosMatrixCoeffType)pBSCA->nCoeffType) {
    case MATRIX_COEFF_IDENTITY:
    case MATRIX_COEFF_RESERVED:
    case MATRIX_COEFF_UNSPECIFIED:
        pParams->sAspects.mMatrixCoeffs = (enum ColorAspects::MatrixCoeffs)pFWCA->nCoeffType;
        break;
    case MATRIX_COEFF_REC709:
        pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT709_5;
        break;
    case MATRIX_COEFF_SMPTE170M:
    case MATRIX_COEFF_470_SYSTEM_BG:
        pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT601_6;
        break;
    case MATRIX_COEFF_SMPTE240M:
        pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixSMPTE240M;
        break;
    case MATRIX_COEFF_BT2020:
        pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT2020;
        break;
    case MATRIX_COEFF_BT2020_CONSTANT:
        pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT2020Constant;
        break;
    case MATRIX_COEFF_470_SYSTEM_M:
        pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT470_6M;
        break;
    default:
        pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixOther;
        break;
    }

    FunctionOut();

    return;
}

void getColorAspectsPreferFramework(
    EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA,
    EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA,
    void                          *pParam)
{
    FunctionIn();

    if ((pBSCA == NULL) || (pFWCA == NULL) || (pParam == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid argument", __FUNCTION__);
        return;
    }

    DescribeColorAspectsParams *pParams = (DescribeColorAspectsParams *)pParam;

    pParams->sAspects.mRange = (enum ColorAspects::Range)pFWCA->nRangeType;
    if (pParams->sAspects.mRange == ColorAspects::RangeUnspecified) {
        switch ((ExynosRangeType)pBSCA->nRangeType) {
        case RANGE_FULL:
            pParams->sAspects.mRange = ColorAspects::RangeFull;
            break;
        case RANGE_LIMITED:
            pParams->sAspects.mRange = ColorAspects::RangeLimited;
            break;
        default:
            pParams->sAspects.mRange = ColorAspects::RangeUnspecified;
            break;
        }
    }

    pParams->sAspects.mPrimaries = (enum ColorAspects::Primaries)pFWCA->nPrimaryType;
    if (pParams->sAspects.mPrimaries == ColorAspects::PrimariesUnspecified) {
        switch ((ExynosPrimariesType)pBSCA->nPrimaryType) {
        case PRIMARIES_RESERVED:
        case PRIMARIES_UNSPECIFIED:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesUnspecified;
            break;
        case PRIMARIES_BT709_5:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT709_5;
            break;
        case PRIMARIES_BT601_6_625:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT601_6_625;
            break;
        case PRIMARIES_BT601_6_525:
        case PRIMARIES_SMPTE_240M:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT601_6_525;
            break;
        case PRIMARIES_BT2020:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT2020;
            break;
        /* unsupported value */
        case PRIMARIES_BT470_6M:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesBT470_6M;
            break;
        case PRIMARIES_GENERIC_FILM:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesGenericFilm;
            break;
        default:
            pParams->sAspects.mPrimaries = ColorAspects::PrimariesOther;
            break;
        }
    }

    pParams->sAspects.mTransfer = (enum ColorAspects::Transfer)pFWCA->nTransferType;
    if (pParams->sAspects.mTransfer == ColorAspects::TransferUnspecified) {
        switch ((ExynosTransferType)pBSCA->nTransferType) {
        case TRANSFER_RESERVED:
        case TRANSFER_UNSPECIFIED:
            pParams->sAspects.mTransfer = ColorAspects::TransferUnspecified;
            break;
        case TRANSFER_BT709:
        case TRANSFER_SMPTE_170M:
        case TRANSFER_BT2020_1:
        case TRANSFER_BT2020_2:
            pParams->sAspects.mTransfer = ColorAspects::TransferSMPTE170M;
            break;
        case TRANSFER_ST2084:
            pParams->sAspects.mTransfer = ColorAspects::TransferST2084;
            break;
        /* unsupported value */
        case TRANSFER_GAMMA_22:
            pParams->sAspects.mTransfer = ColorAspects::TransferGamma22;
            break;
        case TRANSFER_GAMMA_28:
            pParams->sAspects.mTransfer = ColorAspects::TransferGamma28;
            break;
        case TRANSFER_SMPTE_240M:
            pParams->sAspects.mTransfer = ColorAspects::TransferSMPTE240M;
            break;
        case TRANSFER_LINEAR:
            pParams->sAspects.mTransfer = ColorAspects::TransferLinear;
            break;
        case TRANSFER_XvYCC:
            pParams->sAspects.mTransfer = ColorAspects::TransferXvYCC;
            break;
        case TRANSFER_BT1361:
            pParams->sAspects.mTransfer = ColorAspects::TransferBT1361;
            break;
        case TRANSFER_SRGB:
            pParams->sAspects.mTransfer = ColorAspects::TransferSRGB;
            break;
        case TRANSFER_ST428:
            pParams->sAspects.mTransfer = ColorAspects::TransferST428;
            break;
        case TRANSFER_HLG:
            pParams->sAspects.mTransfer = ColorAspects::TransferHLG;
            break;
        default:
            pParams->sAspects.mTransfer = ColorAspects::TransferOther;
            break;
        }
    }

    pParams->sAspects.mMatrixCoeffs = (enum ColorAspects::MatrixCoeffs)pFWCA->nCoeffType;
    if (pParams->sAspects.mMatrixCoeffs == ColorAspects::MatrixUnspecified) {
        switch ((ExynosMatrixCoeffType)pBSCA->nCoeffType) {
        case MATRIX_COEFF_IDENTITY:
        case MATRIX_COEFF_UNSPECIFIED:
            pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixUnspecified;
            break;
        case MATRIX_COEFF_REC709:
            pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT709_5;
            break;
        case MATRIX_COEFF_SMPTE170M:
        case MATRIX_COEFF_470_SYSTEM_BG:
            pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT601_6;
            break;
        case MATRIX_COEFF_SMPTE240M:
            pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixSMPTE240M;
            break;
        case MATRIX_COEFF_BT2020:
            pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT2020;
            break;
        case MATRIX_COEFF_BT2020_CONSTANT:
            pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixBT2020Constant;
            break;
        case MATRIX_COEFF_RESERVED:
        /* unsupported value */
        case MATRIX_COEFF_470_SYSTEM_M:
        default:
            pParams->sAspects.mMatrixCoeffs = ColorAspects::MatrixOther;
            break;
        }
    }

    FunctionOut();

    return;
}

OMX_ERRORTYPE Exynos_OSAL_UpdateDataSpaceFromAspects(EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA) {
    OMX_ERRORTYPE                   ret         = OMX_ErrorNone;
    android_dataspace               dataSpace   = HAL_DATASPACE_UNKNOWN;
    DescribeColorAspectsParams      colorInfo;
    EXYNOS_OMX_VIDEO_COLORASPECTS   BSCA;

    Exynos_OSAL_Memset(&BSCA, 0, sizeof(EXYNOS_OMX_VIDEO_COLORASPECTS));
    getColorAspectsPreferFramework(&BSCA, pFWCA, (void *)&colorInfo);

    ret = getDataSpaceFromAspects(&colorInfo);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%s] failed to get dataspace", __FUNCTION__);
        goto EXIT;
    }

    pFWCA->nDataSpace = colorInfo.nDataSpace;
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] dataspace changed to 0x%x from 0x0", __FUNCTION__, colorInfo.nDataSpace);
EXIT:
    return ret;
}

void Exynos_OSAL_GetColorAspectsForBitstream(
    EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA,
    EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA)
{
    FunctionIn();

    if ((pFWCA == NULL) || (pBSCA == NULL)) {
        return;
    }

    switch (pFWCA->nRangeType) {
    case ColorAspects::RangeFull:
        pBSCA->nRangeType = RANGE_FULL;
        break;
    case ColorAspects::RangeLimited:
        pBSCA->nRangeType = RANGE_LIMITED;
        break;
    default:
        pBSCA->nRangeType = RANGE_UNSPECIFIED;
        break;
    }

    switch (pFWCA->nPrimaryType) {
    case ColorAspects::PrimariesBT709_5:
        pBSCA->nPrimaryType = PRIMARIES_BT709_5;
        break;
    case ColorAspects::PrimariesUnspecified:
        pBSCA->nPrimaryType = PRIMARIES_UNSPECIFIED;
        break;
    case ColorAspects::PrimariesBT470_6M:
        pBSCA->nPrimaryType = PRIMARIES_BT470_6M;
        break;
    case ColorAspects::PrimariesBT601_6_625:
        pBSCA->nPrimaryType = PRIMARIES_BT601_6_625;
        break;
    case ColorAspects::PrimariesBT601_6_525:
        pBSCA->nPrimaryType = PRIMARIES_BT601_6_525;
        break;
    /* not declared in ColorAspects
    case ColorAspects::PrimariesSMPTE240M:
        pBSCA->nPrimaryType = PRIMARIES_SMPTE_240M;
        break;
    */
    case ColorAspects::PrimariesGenericFilm:
        pBSCA->nPrimaryType = PRIMARIES_GENERIC_FILM;
        break;
    case ColorAspects::PrimariesBT2020:
        pBSCA->nPrimaryType = PRIMARIES_BT2020;
        break;
    default:
        pBSCA->nPrimaryType = PRIMARIES_RESERVED;
        break;
    }

    switch (pFWCA->nTransferType) {
    case ColorAspects::TransferSMPTE170M:
        pBSCA->nTransferType = TRANSFER_SMPTE_170M;  /* same as 1, 6, 14, 15 */
        break;
    case ColorAspects::TransferUnspecified:
        pBSCA->nTransferType = TRANSFER_UNSPECIFIED;
        break;
    case ColorAspects::TransferGamma22:
        pBSCA->nTransferType = TRANSFER_GAMMA_22;
        break;
    case ColorAspects::TransferGamma28:
        pBSCA->nTransferType = TRANSFER_GAMMA_28;
        break;
    case ColorAspects::TransferSMPTE240M:
        pBSCA->nTransferType = TRANSFER_SMPTE_240M;
        break;
    case ColorAspects::TransferLinear:
        pBSCA->nTransferType = TRANSFER_LINEAR;
        break;
    case ColorAspects::TransferXvYCC:
        pBSCA->nTransferType = TRANSFER_XvYCC;
        break;
    case ColorAspects::TransferBT1361:
        pBSCA->nTransferType = TRANSFER_BT1361;
        break;
    case ColorAspects::TransferSRGB:
        pBSCA->nTransferType = TRANSFER_SRGB;
        break;
    case ColorAspects::TransferST2084:
        pBSCA->nTransferType = TRANSFER_ST2084;
        break;
    case ColorAspects::TransferST428:
        pBSCA->nTransferType = TRANSFER_ST428;
        break;
    case ColorAspects::TransferHLG:
        pBSCA->nTransferType = TRANSFER_HLG;
        break;
    default:
        pBSCA->nTransferType = TRANSFER_RESERVED;
        break;
    }

    switch (pFWCA->nCoeffType) {
    case ColorAspects::MatrixBT709_5:
        pBSCA->nCoeffType = MATRIX_COEFF_REC709;
        break;
    case ColorAspects::MatrixUnspecified:
        pBSCA->nCoeffType = MATRIX_COEFF_UNSPECIFIED;
        break;
    case ColorAspects::MatrixBT470_6M:
        pBSCA->nCoeffType = MATRIX_COEFF_470_SYSTEM_M;
        break;
    case ColorAspects::MatrixBT601_6:
        pBSCA->nCoeffType = MATRIX_COEFF_SMPTE170M;
        break;
    case ColorAspects::MatrixSMPTE240M:
        pBSCA->nCoeffType = MATRIX_COEFF_SMPTE240M;
        break;
    case ColorAspects::MatrixBT2020:
        pBSCA->nCoeffType = MATRIX_COEFF_BT2020;
        break;
    case ColorAspects::MatrixBT2020Constant:
        pBSCA->nCoeffType = MATRIX_COEFF_BT2020_CONSTANT;
        break;
    default:
        pBSCA->nCoeffType = MATRIX_COEFF_IDENTITY;
        break;
    }

    FunctionOut();

    return;
}

void Exynos_OSAL_UpdateDataSpaceFromBitstream(EXYNOS_OMX_BASECOMPONENT *pExynosComponent) {
    OMX_ERRORTYPE               ret                 = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT        *pInputPort          = NULL;
    EXYNOS_OMX_BASEPORT        *pOutputPort         = NULL;
    DescribeColorAspectsParams  colorInfo;

    if (pExynosComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    Exynos_OSAL_Memset(&colorInfo, 0, sizeof(DescribeColorAspectsParams));

    getColorAspectsPreferBitstream(&pInputPort->ColorAspects, &pOutputPort->ColorAspects, &colorInfo);

    ret = getDataSpaceFromAspects(&colorInfo);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    pOutputPort->ColorAspects.nDataSpace = colorInfo.nDataSpace;
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] dataspace is updated 0x%x from bitstream", pExynosComponent, __FUNCTION__, colorInfo.nDataSpace);

EXIT:
    return;
}

void Exynos_OSAL_UpdateDataspaceToGraphicMeta(OMX_PTR pBuf, int nDataSpace) {
    VideoGrallocMetadata *pMetaData = (VideoGrallocMetadata *)pBuf;

    if (pMetaData == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]  invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (nDataSpace == HAL_DATASPACE_UNKNOWN)
        goto EXIT;

    if (ExynosGraphicBufferMeta::set_dataspace(pMetaData->pHandle, (android_dataspace_t)nDataSpace) < 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] failed to set dataspace to graphic buffer", __FUNCTION__);
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] dataspace(0x%x)", __FUNCTION__, nDataSpace);

EXIT:
    return;
}

void Exynos_OSAL_ColorSpaceToColorAspects(
        int                            colorSpace,
        EXYNOS_OMX_VIDEO_COLORASPECTS *colorAspects)
{
    if (colorAspects == NULL) {
        return;
    }

    switch (colorSpace) {
    case CSC_EQ_COLORSPACE_REC709:
        colorAspects->nPrimaryType  = ColorAspects::PrimariesBT709_5;
        colorAspects->nTransferType = ColorAspects::TransferSMPTE170M;
        colorAspects->nCoeffType    = ColorAspects::MatrixBT709_5;
        break;
    case CSC_EQ_COLORSPACE_BT2020:
        colorAspects->nPrimaryType  = ColorAspects::PrimariesBT2020;
        colorAspects->nTransferType = ColorAspects::TransferSMPTE170M;
        colorAspects->nCoeffType    = ColorAspects::MatrixBT2020;
        break;
    case CSC_EQ_COLORSPACE_SMPTE170M:
    default:
        colorAspects->nPrimaryType  = ColorAspects::PrimariesBT601_6_625;
        colorAspects->nTransferType = ColorAspects::TransferSMPTE170M;
        colorAspects->nCoeffType    = ColorAspects::MatrixBT601_6;
        break;
    }

    return;
}

OMX_ERRORTYPE setHDR10PlusInfoForFramework(
    OMX_COMPONENTTYPE *pOMXComponent,
    void              *pHDRDynamicInfo)
{
    OMX_ERRORTYPE                ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    DescribeHDR10PlusInfoParams *pHDR10PlusInfo    = NULL; /* HDR10+ info for getConfig from framework */
    ExynosHdrDynamicInfo        *pMetaHDRDynamic   = NULL;
    OMX_U8                      *pTempBuffer       = NULL;
    int                          size;

    FunctionIn();

    if (pHDRDynamicInfo == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pMetaHDRDynamic = (ExynosHdrDynamicInfo *)pHDRDynamicInfo;

    pTempBuffer = (OMX_U8 *)Exynos_OSAL_Malloc(MAX_HDR10PLUS_SIZE);
    if (pTempBuffer == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBuffer, 0, MAX_HDR10PLUS_SIZE);

    size = Exynos_dynamic_meta_to_itu_t_t35(pMetaHDRDynamic, (char *)pTempBuffer);

    if (size > 0) {
        pHDR10PlusInfo = (DescribeHDR10PlusInfoParams *)Exynos_OSAL_Malloc(sizeof(DescribeHDR10PlusInfoParams) - 1 + size);
        if (pHDR10PlusInfo == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
            Exynos_OSAL_Free(pTempBuffer);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        Exynos_OSAL_Memset(pHDR10PlusInfo, 0, (sizeof(DescribeHDR10PlusInfoParams) - 1 + size));
        InitOMXParams(pHDR10PlusInfo, sizeof(DescribeHDR10PlusInfoParams));

        pHDR10PlusInfo->nParamSizeUsed = size;
        Exynos_OSAL_Memcpy(pHDR10PlusInfo->nValue, pTempBuffer, size);
        Exynos_OSAL_Free(pTempBuffer);

        if (Exynos_OSAL_Queue(&pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].HdrDynamicInfoQ, (void *)pHDR10PlusInfo) != 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Queue HDR10+ info", pExynosComponent, __FUNCTION__);
            Exynos_OSAL_Free(pHDR10PlusInfo);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%p][%s] send event(OMX_EventConfigUpdate)",
                                            pExynosComponent, __FUNCTION__);
        /** Send Port Settings changed call back **/
        (*(pExynosComponent->pCallbacks->EventHandler))
             (pOMXComponent,
              pExynosComponent->callbackData,
              OMX_EventConfigUpdate, /* The command was completed */
              OMX_DirOutput,         /* This is the port index */
              OMX_IndexConfigVideoHdr10PlusInfo,
              NULL);
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Dynamic meta to itu_t_t35 is failed (size: %d)", pExynosComponent, __FUNCTION__, size);
        Exynos_OSAL_Free(pTempBuffer);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE setHDR10PlusInfoForVendorPath(
    OMX_COMPONENTTYPE *pOMXComponent,
    void              *pExynosHDR10PlusInfo,
    void              *pHDRDynamicInfo)
{
    OMX_ERRORTYPE                ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    DescribeHDR10PlusInfoParams *pHDR10PlusInfo    = NULL; /* HDR10+ info for getConfig from framework */

    FunctionIn();

    if (pHDRDynamicInfo == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHDR10PlusInfo = (DescribeHDR10PlusInfoParams *)pHDRDynamicInfo;

    if (Exynos_parsing_user_data_registered_itu_t_t35((ExynosHdrDynamicInfo *)pExynosHDR10PlusInfo, (void *)pHDR10PlusInfo->nValue) != 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to parsing itu_t_t35", pExynosComponent, __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE lockBuffer(
    OMX_IN OMX_PTR handle,
    OMX_IN OMX_U32 width,
    OMX_IN OMX_U32 height,
    OMX_IN OMX_COLOR_FORMATTYPE format,
    OMX_OUT OMX_U32 *pStride,
    OMX_OUT EXYNOS_OMX_MULTIPLANE_BUFFER *pBufferInfo)
{
    FunctionIn();

    OMX_ERRORTYPE            ret           = OMX_ErrorNone;
    buffer_handle_t          bufferHandle  = (buffer_handle_t)handle;
    hardware::hidl_handle    acquireFenceHandle;

    static ExynosGraphicBufferMapper &mapper(ExynosGraphicBufferMapper::get());
    ExynosGraphicBufferMeta graphicBuffer(bufferHandle);
    Rect bounds(width, height);

    if ((handle == NULL) ||
        (pStride == NULL) ||
        (pBufferInfo == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] handle: 0x%x, format = %x", __FUNCTION__, handle, graphicBuffer.format);

    if (format == OMX_COLOR_FormatAndroidOpaque)
        format = getBufferFormat(handle);

    switch ((int)format) {
    case OMX_COLOR_Format32BitRGBA8888:
    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_COLOR_Format32bitARGB8888:
    {
        void *vaddr = NULL;

        auto err = mapper.lock64(bufferHandle, OMX_GRALLOC_USAGE_LOCK, bounds, &vaddr);

        if (err != ::android::NO_ERROR) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to mapper.lock()", __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        pBufferInfo->addr[0] = vaddr;
        pBufferInfo->addr[1] = NULL;
    }
        break;
    case OMX_SEC_COLOR_FormatNV21Linear:
    case OMX_SEC_COLOR_FormatYVU420Planar:
    {
        android_ycbcr outLayout;
        Exynos_OSAL_Memset(&outLayout, 0, sizeof(outLayout));

        auto err = mapper.lockYCbCr64(bufferHandle, OMX_GRALLOC_USAGE_LOCK, bounds, &outLayout);

        if (err != ::android::NO_ERROR) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to mapper.lock()", __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        pBufferInfo->addr[0] = outLayout.y;
        pBufferInfo->addr[1] = outLayout.cr;
    }
        break;
    default:
    {
        android_ycbcr outLayout;
        Exynos_OSAL_Memset(&outLayout, 0, sizeof(outLayout));

        auto err = mapper.lockYCbCr64(bufferHandle, OMX_GRALLOC_USAGE_LOCK, bounds, &outLayout);

        if (err != ::android::NO_ERROR) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to mapper.lock()", __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        pBufferInfo->addr[0] = outLayout.y;
        pBufferInfo->addr[1] = outLayout.cb;
    }
        break;
    }

    pBufferInfo->addr[2] = graphicBuffer.get_video_metadata(bufferHandle);

    lockCnt++;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] lockCnt:%d", __FUNCTION__, lockCnt);

    pBufferInfo->fd[0] = graphicBuffer.fd;
    pBufferInfo->fd[1] = graphicBuffer.fd1;
    pBufferInfo->fd[2] = graphicBuffer.fd2;

#ifndef GRALLOC_VERSION0
    if ((graphicBuffer.producer_usage & OMX_GRALLOC_USAGE_PROTECTED) ||
        (graphicBuffer.consumer_usage & OMX_GRALLOC_USAGE_PROTECTED))
#else
    if (priv_hnd->flags & OMX_GRALLOC_USAGE_PROTECTED)
#endif
    {
        /* in case of DRM,  VAs of ycbcr are invalid */
        pBufferInfo->addr[0] = INT_TO_PTR(graphicBuffer.fd);
        pBufferInfo->addr[1] = INT_TO_PTR(graphicBuffer.fd1);

        if ((pBufferInfo->addr[2] == NULL) && (graphicBuffer.fd2 > 0)) /* except for private data buffer */
            pBufferInfo->addr[2] = INT_TO_PTR(graphicBuffer.fd2);
    }

    *pStride = (OMX_U32)graphicBuffer.stride;

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] buffer locked: handle(%p), FD(%u, %u, %u)",
                                        __FUNCTION__, handle, pBufferInfo->fd[0], pBufferInfo->fd[1], pBufferInfo->fd[2]);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE unlockBuffer(OMX_IN OMX_PTR handle)
{
    FunctionIn();

    OMX_ERRORTYPE   ret             = OMX_ErrorNone;
    buffer_handle_t bufferHandle    = (buffer_handle_t) handle;

    auto buffer = const_cast<native_handle_t *>(bufferHandle);
    int  releaseFence = -1;

    static GraphicBufferMapper &mapper(GraphicBufferMapper::get());

    if (handle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] handle is NULL", __FUNCTION__);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] handle: 0x%x", __FUNCTION__, handle);

    {
        auto err = mapper.unlock(bufferHandle);
        if (err != ::android::NO_ERROR) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to mapper.lock()", __FUNCTION__);
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }

    lockCnt--;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] lockCnt:%d", __FUNCTION__, lockCnt);

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] buffer unlocked: handle(%p)", __FUNCTION__, handle);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_LockMetaData(
    OMX_IN OMX_PTR                          pBuffer,
    OMX_IN EXYNOS_OMX_LOCK_RANGE            range,
    OMX_OUT OMX_U32                        *pStride,
    OMX_OUT EXYNOS_OMX_MULTIPLANE_BUFFER   *pBufferInfo,
    OMX_IN EXYNOS_METADATA_TYPE             eMetaType)
{
    FunctionIn();

    OMX_ERRORTYPE   ret     = OMX_ErrorNone;
    OMX_PTR         pBuf    = NULL;

    if (eMetaType == METADATA_TYPE_GRAPHIC) {
        EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;

        ret = Exynos_OSAL_GetInfoFromMetaData(pBuffer, &bufferInfo, eMetaType);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]: Failed to Exynos_OSAL_GetInfoFromMetaData (err:0x%x)",
                                __FUNCTION__, ret);
            goto EXIT;
        }

        pBuf = bufferInfo.addr[0];
    } else {
        pBuf = pBuffer;
    }

    ret = lockBuffer(pBuf, range.nWidth, range.nHeight, range.eColorFormat, pStride, pBufferInfo);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]: Failed to lockBuffer (err:0x%x)", __FUNCTION__, ret);
        goto EXIT;
    }

    pBufferInfo->eColorFormat = getBufferFormat(pBuf);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_UnlockMetaData(
    OMX_IN OMX_PTR              pBuffer,
    OMX_IN EXYNOS_METADATA_TYPE eMetaType)
{
    OMX_ERRORTYPE   ret     = OMX_ErrorNone;
    OMX_PTR         pBuf    = NULL;

    FunctionIn();

    if (eMetaType == METADATA_TYPE_GRAPHIC) {
        EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;

        ret = Exynos_OSAL_GetInfoFromMetaData(pBuffer, &bufferInfo, eMetaType);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]: Failed to Exynos_OSAL_GetInfoFromMetaData (err:0x%x)",
                                __FUNCTION__, ret);
            goto EXIT;
        }

        pBuf = bufferInfo.addr[0];
    } else {
        pBuf = pBuffer;
    }

    ret = unlockBuffer(pBuf);
    if (ret != OMX_ErrorNone)
        goto EXIT;

EXIT:
    FunctionOut();

    return ret;
}

OMX_HANDLETYPE Exynos_OSAL_RefCount_Create()
{
    OMX_ERRORTYPE            ret    = OMX_ErrorNone;
    EXYNOS_OMX_REF_HANDLE   *phREF  = NULL;

    int i = 0;

    FunctionIn();

    phREF = (EXYNOS_OMX_REF_HANDLE *)Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_REF_HANDLE));
    if (phREF == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to allocate ref. handle", __FUNCTION__);
        goto EXIT;
    }

    Exynos_OSAL_Memset(phREF, 0, sizeof(EXYNOS_OMX_REF_HANDLE));

    ret = Exynos_OSAL_MutexCreate(&phREF->hMutex);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_MutexCreate", __FUNCTION__);
        Exynos_OSAL_Free(phREF);
        phREF = NULL;
    }

EXIT:
    FunctionOut();

    return ((OMX_HANDLETYPE)phREF);
}

OMX_ERRORTYPE Exynos_OSAL_RefCount_Reset(OMX_HANDLETYPE hREF)
{
    OMX_ERRORTYPE            ret    = OMX_ErrorNone;
    EXYNOS_OMX_REF_HANDLE   *phREF  = (EXYNOS_OMX_REF_HANDLE *)hREF;

    int i = 0;

    FunctionIn();

    if (phREF == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_MutexLock(phREF->hMutex);

#ifdef USE_WA_ION_BUF_REF
    for (i = 0; i < MAX_BUFFER_REF; i++) {
        if (phREF->SharedBuffer[i].cnt > 0) {
            if (phREF->SharedBuffer[i].ionHandle != -1)
                close(phREF->SharedBuffer[i].ionHandle);

            if (phREF->SharedBuffer[i].ionHandle1 != -1)
                close(phREF->SharedBuffer[i].ionHandle1);

            if (phREF->SharedBuffer[i].ionHandle2 != -1)
                close(phREF->SharedBuffer[i].ionHandle2);

            phREF->SharedBuffer[i].cnt = 0;

            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] SharedBuffer[%d] : fd:%llu dupfd:%llu cnt:%d", __FUNCTION__,
                                i, phREF->SharedBuffer[i].bufferFd,
                                phREF->SharedBuffer[i].ionHandle, phREF->SharedBuffer[i].cnt);

            phREF->SharedBuffer[i].bufferHandle = NULL;

            phREF->SharedBuffer[i].bufferFd    = 0;
            phREF->SharedBuffer[i].bufferFd1   = 0;
            phREF->SharedBuffer[i].bufferFd2   = 0;

            phREF->SharedBuffer[i].ionHandle   = -1;
            phREF->SharedBuffer[i].ionHandle1  = -1;
            phREF->SharedBuffer[i].ionHandle2  = -1;
        }
    }
#else
    for (i = 0; i < MAX_BUFFER_REF; i++) {
        if (phREF->SharedBuffer[i].bufferFd > 0) {
            while (phREF->SharedBuffer[i].cnt > 0) {
                if (phREF->SharedBuffer[i].ionHandle != -1)
                    exynos_ion_free_handle(getIonFd(), phREF->SharedBuffer[i].ionHandle);

                if (phREF->SharedBuffer[i].ionHandle1 != -1)
                    exynos_ion_free_handle(getIonFd(), phREF->SharedBuffer[i].ionHandle1);

                if (phREF->SharedBuffer[i].ionHandle2 != -1)
                    exynos_ion_free_handle(getIonFd(), phREF->SharedBuffer[i].ionHandle2);

                phREF->SharedBuffer[i].cnt--;

                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] SharedBuffer[%d] : fd:%llu cnt:%d", __FUNCTION__,
                                 i, phREF->SharedBuffer[i].bufferFd, phREF->SharedBuffer[i].cnt);
            }

            phREF->SharedBuffer[i].bufferHandle = NULL;

            phREF->SharedBuffer[i].bufferFd    = 0;
            phREF->SharedBuffer[i].bufferFd1   = 0;
            phREF->SharedBuffer[i].bufferFd2   = 0;

            phREF->SharedBuffer[i].ionHandle   = -1;
            phREF->SharedBuffer[i].ionHandle1  = -1;
            phREF->SharedBuffer[i].ionHandle2  = -1;

            phREF->SharedBuffer[i].cnt = 0;
        }
    }
#endif

    Exynos_OSAL_MutexUnlock(phREF->hMutex);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_RefCount_Terminate(OMX_HANDLETYPE hREF)
{
    OMX_ERRORTYPE            ret    = OMX_ErrorNone;
    EXYNOS_OMX_REF_HANDLE   *phREF  = (EXYNOS_OMX_REF_HANDLE *)hREF;

    FunctionIn();

    if (phREF == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_RefCount_Reset(phREF);

    ret = Exynos_OSAL_MutexTerminate(phREF->hMutex);
    if (ret != OMX_ErrorNone)
        goto EXIT;

    Exynos_OSAL_Free(phREF);
    phREF = NULL;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_RefCount_Increase(
    OMX_HANDLETYPE       hREF,
    OMX_PTR              pBuffer,
    EXYNOS_OMX_BASEPORT *pExynosPort)
{
    OMX_ERRORTYPE            ret            = OMX_ErrorNone;
    EXYNOS_OMX_REF_HANDLE   *phREF          = (EXYNOS_OMX_REF_HANDLE *)hREF;
    OMX_COLOR_FORMATTYPE     eColorFormat   = OMX_COLOR_FormatUnused;

    buffer_handle_t          bufferHandle  = NULL;

    ExynosGraphicBufferMeta graphicBuffer(bufferHandle);

    ion_user_handle_t ionHandle  = -1;
    ion_user_handle_t ionHandle1 = -1;
    ion_user_handle_t ionHandle2 = -1;
    int i, nPlaneCnt;

    FunctionIn();

    if ((phREF == NULL) ||
        (pBuffer == NULL) ||
        (pExynosPort == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE) {
        bufferHandle = (buffer_handle_t)pBuffer;
    } else {
        EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;

        ret = Exynos_OSAL_GetInfoFromMetaData(pBuffer, &bufferInfo, pExynosPort->eMetaDataType);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]: Failed to Exynos_OSAL_GetInfoFromMetaData (err:0x%x)",
                                __FUNCTION__, ret);
            goto EXIT;
        }

        bufferHandle = (buffer_handle_t)bufferInfo.addr[0];
    }

    eColorFormat = Exynos_OSAL_HAL2OMXColorFormat(graphicBuffer.format);
    if (eColorFormat == OMX_COLOR_FormatUnused) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] HAL format(0x%x) is invalid", __FUNCTION__, graphicBuffer.format);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    nPlaneCnt = Exynos_OSAL_GetPlaneCount(eColorFormat, pExynosPort->ePlaneType);

    Exynos_OSAL_MutexLock(phREF->hMutex);

#ifdef USE_WA_ION_BUF_REF
    for (i = 0; i < MAX_BUFFER_REF; i++) {
        if (phREF->SharedBuffer[i].bufferHandle == bufferHandle) {
            phREF->SharedBuffer[i].cnt++;
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] handle(%p) : fd:%llu dupfd:%d cnt:%d", __FUNCTION__,
                                    phREF->SharedBuffer[i].bufferHandle, phREF->SharedBuffer[i].bufferFd,
                                    phREF->SharedBuffer[i].ionHandle, phREF->SharedBuffer[i].cnt);
            goto EXIT;
        }
    }

    if ((graphicBuffer.fd > 0) &&
        (nPlaneCnt >= 1)) {
        ionHandle = dup(graphicBuffer.fd);
        if (ionHandle < 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to dup(fd:%d)", __FUNCTION__, graphicBuffer.fd);
            ionHandle = -1;
        }
    }

    if ((graphicBuffer.fd1 > 0) &&
        (nPlaneCnt >= 2)) {
        ionHandle1 = dup(graphicBuffer.fd1);
        if (ionHandle1 < 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to dup(fd:%d)", __FUNCTION__, graphicBuffer.fd1);
            ionHandle1 = -1;
        }
    }

    if ((graphicBuffer.fd2 > 0) &&
        (nPlaneCnt == 3)) {
        ionHandle2 = dup(graphicBuffer.fd2);
        if (ionHandle2 < 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to dup(fd:%d)", __FUNCTION__, graphicBuffer.fd2);
            ionHandle2 = -1;
        }
    }

    for (i = 0; i < MAX_BUFFER_REF; i++) {
        if (phREF->SharedBuffer[i].bufferFd == 0) {
            phREF->SharedBuffer[i].bufferHandle = (void *)bufferHandle;  /* mark that component owns it */

            phREF->SharedBuffer[i].bufferFd    = (unsigned long long)(graphicBuffer.fd > 0)? graphicBuffer.fd:0;
            phREF->SharedBuffer[i].bufferFd1   = (unsigned long long)(graphicBuffer.fd1 > 0)? graphicBuffer.fd1:0;
            phREF->SharedBuffer[i].bufferFd2   = (unsigned long long)(graphicBuffer.fd2 > 0)? graphicBuffer.fd2:0;

            phREF->SharedBuffer[i].ionHandle   = ionHandle;
            phREF->SharedBuffer[i].ionHandle1  = ionHandle1;
            phREF->SharedBuffer[i].ionHandle2  = ionHandle2;
            phREF->SharedBuffer[i].cnt = 1;
            break;
        }
    }

    if (i >=  MAX_BUFFER_REF) {
        ret = OMX_ErrorUndefined;
        Exynos_OSAL_MutexUnlock(phREF->hMutex);
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] handle(%p) : fd:%llu dupfd:%d cnt:%d", __FUNCTION__,
                            phREF->SharedBuffer[i].bufferHandle, phREF->SharedBuffer[i].bufferFd,
                            phREF->SharedBuffer[i].ionHandle, phREF->SharedBuffer[i].cnt);
#else
    if ((graphicBuffer.fd > 0) &&
        (nPlaneCnt >= 1)) {
        if (exynos_ion_import_handle(getIonFd(), graphicBuffer.fd, &ionHandle) < 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to exynos_ion_import_handle(client:%d, fd:%d)", __FUNCTION__, getIonFd(), graphicBuffer.fd);
            ionHandle = -1;
        }
    }

    if ((graphicBuffer.fd1 > 0) &&
        (nPlaneCnt >= 2)) {
        if (exynos_ion_import_handle(getIonFd(), graphicBuffer.fd1, &ionHandle1) < 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to exynos_ion_import_handle(client:%d, fd1:%d)", __FUNCTION__, getIonFd(), graphicBuffer.fd1);
            ionHandle1 = -1;
        }
    }

    if ((graphicBuffer.fd2 > 0) &&
        (nPlaneCnt == 3)) {
        if (exynos_ion_import_handle(getIonFd(), graphicBuffer.fd2, &ionHandle2) < 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to exynos_ion_import_handle(client:%d, fd2:%d)", __FUNCTION__, getIonFd(), graphicBuffer.fd2);
            ionHandle2 = -1;
        }
    }

    for (i = 0; i < MAX_BUFFER_REF; i++) {
        if (phREF->SharedBuffer[i].bufferFd == (unsigned long long)graphicBuffer.fd) {
            phREF->SharedBuffer[i].cnt++;
            break;
        }
    }

    if (i >=  MAX_BUFFER_REF) {
        for (i = 0; i < MAX_BUFFER_REF; i++) {
            if (phREF->SharedBuffer[i].bufferFd == 0) {
                phREF->SharedBuffer[i].bufferHandle = (void *)bufferHandle;

                phREF->SharedBuffer[i].bufferFd    = (unsigned long long)(graphicBuffer.fd > 0)? graphicBuffer.fd:0;
                phREF->SharedBuffer[i].bufferFd1   = (unsigned long long)(graphicBuffer.fd1 > 0)? graphicBuffer.fd1:0;
                phREF->SharedBuffer[i].bufferFd2   = (unsigned long long)(graphicBuffer.fd2 > 0)? graphicBuffer.fd2:0;

                phREF->SharedBuffer[i].ionHandle   = ionHandle;
                phREF->SharedBuffer[i].ionHandle1  = ionHandle1;
                phREF->SharedBuffer[i].ionHandle2  = ionHandle2;
                phREF->SharedBuffer[i].cnt++;
                break;
            }
        }
    }

    if (i >=  MAX_BUFFER_REF) {
        ret = OMX_ErrorUndefined;
        Exynos_OSAL_MutexUnlock(phREF->hMutex);
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] handle(%p) : fd:%llu cnt:%d", __FUNCTION__,
                            phREF->SharedBuffer[i].bufferHandle, phREF->SharedBuffer[i].bufferFd, phREF->SharedBuffer[i].cnt);
#endif

    Exynos_OSAL_MutexUnlock(phREF->hMutex);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_RefCount_Decrease(
    OMX_HANDLETYPE       hREF,
    OMX_PTR              pBuffer,
    ReleaseDPB           dpbFD[VIDEO_BUFFER_MAX_NUM],
    EXYNOS_OMX_BASEPORT *pExynosPort)
{
    OMX_ERRORTYPE            ret            = OMX_ErrorNone;
    EXYNOS_OMX_REF_HANDLE   *phREF          = (EXYNOS_OMX_REF_HANDLE *)hREF;
    OMX_COLOR_FORMATTYPE     eColorFormat   = OMX_COLOR_FormatUnused;

    buffer_handle_t      bufferHandle   = NULL;

    int i, j;

    FunctionIn();

    if ((phREF == NULL) ||
        (pBuffer == NULL) ||
        (dpbFD == NULL) ||
        (pExynosPort == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE) {
        bufferHandle = (buffer_handle_t)pBuffer;
    } else {
        EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;

        ret = Exynos_OSAL_GetInfoFromMetaData(pBuffer, &bufferInfo, pExynosPort->eMetaDataType);
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s]: Failed to Exynos_OSAL_GetInfoFromMetaData (err:0x%x)",
                                __FUNCTION__, ret);
            goto EXIT;
        }

        bufferHandle = (buffer_handle_t)bufferInfo.addr[0];
    }

    Exynos_OSAL_MutexLock(phREF->hMutex);

#ifdef USE_WA_ION_BUF_REF
    for (i = 0; i < MAX_BUFFER_REF; i++) {
        /* unmark that component owns it */
        if (phREF->SharedBuffer[i].bufferHandle == bufferHandle) {
            phREF->SharedBuffer[i].bufferHandle = NULL;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] handle(%p)", __FUNCTION__, bufferHandle);
            break;
        }
    }

    for (i = 0; i < VIDEO_BUFFER_MAX_NUM; i++) {
        if (dpbFD[i].fd < 0) {
            break;
        }

        for (j = 0; j < MAX_BUFFER_REF; j++) {
            if ((phREF->SharedBuffer[j].bufferFd == (unsigned long long)dpbFD[i].fd) &&
                (phREF->SharedBuffer[j].bufferHandle == NULL)) {

                phREF->SharedBuffer[j].cnt--;

                if (phREF->SharedBuffer[j].cnt == 0) {
                    if (phREF->SharedBuffer[j].ionHandle != -1)
                        close(phREF->SharedBuffer[j].ionHandle);

                    if (phREF->SharedBuffer[j].ionHandle1 != -1)
                        close(phREF->SharedBuffer[j].ionHandle1);

                    if (phREF->SharedBuffer[j].ionHandle2 != -1)
                        close(phREF->SharedBuffer[j].ionHandle2);

                    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] fd:%d dupfd:%d cnt:%d", __FUNCTION__,
                                        dpbFD[i].fd, phREF->SharedBuffer[j].ionHandle, phREF->SharedBuffer[j].cnt);

                    phREF->SharedBuffer[j].bufferFd    = 0;
                    phREF->SharedBuffer[j].bufferFd1   = 0;
                    phREF->SharedBuffer[j].bufferFd2   = 0;
                    phREF->SharedBuffer[j].ionHandle   = -1;
                    phREF->SharedBuffer[j].ionHandle1  = -1;
                    phREF->SharedBuffer[j].ionHandle2  = -1;
                } else {
                    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] fd:%d dupfd:%d cnt:%d", __FUNCTION__,
                                        dpbFD[i].fd, phREF->SharedBuffer[j].ionHandle, phREF->SharedBuffer[j].cnt);
                }
            }
        }
    }
#else
    for (i = 0; i < VIDEO_BUFFER_MAX_NUM; i++) {
        if (dpbFD[i].fd < 0) {
            break;
        }

        for (j = 0; j < MAX_BUFFER_REF; j++) {
            if (phREF->SharedBuffer[j].bufferFd == (unsigned long long)dpbFD[i].fd) {
                if (phREF->SharedBuffer[j].ionHandle != -1)
                    exynos_ion_free_handle(getIonFd(), phREF->SharedBuffer[j].ionHandle);

                if (phREF->SharedBuffer[j].ionHandle1 != -1)
                    exynos_ion_free_handle(getIonFd(), phREF->SharedBuffer[j].ionHandle1);

                if (phREF->SharedBuffer[j].ionHandle2 != -1)
                    exynos_ion_free_handle(getIonFd(), phREF->SharedBuffer[j].ionHandle2);

                phREF->SharedBuffer[j].cnt--;

                if (phREF->SharedBuffer[j].cnt == 0) {
                    phREF->SharedBuffer[j].bufferFd    = 0;
                    phREF->SharedBuffer[j].bufferFd1   = 0;
                    phREF->SharedBuffer[j].bufferFd2   = 0;
                    phREF->SharedBuffer[j].ionHandle   = -1;
                    phREF->SharedBuffer[j].ionHandle1  = -1;
                    phREF->SharedBuffer[j].ionHandle2  = -1;
                }

                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] fd:%d cnt:%d", __FUNCTION__,
                                    dpbFD[i].fd, phREF->SharedBuffer[j].cnt);
                break;
            }
        }
    }
#endif

    if (i >=  MAX_BUFFER_REF) {
        ret = OMX_ErrorUndefined;
        Exynos_OSAL_MutexUnlock(phREF->hMutex);
        goto EXIT;
    }

    Exynos_OSAL_MutexUnlock(phREF->hMutex);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;

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


    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamPortDefinition:  /* DEC, ENC */
    {
        OMX_PARAM_PORTDEFINITIONTYPE    *pPortDef    = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                          nPortIndex  = pPortDef->nPortIndex;
        EXYNOS_OMX_BASEPORT             *pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if ((pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED) &&
            (pExynosPort->eMetaDataType != METADATA_TYPE_HANDLE)) {
            pPortDef->nBufferSize = MAX_METADATA_BUFFER_SIZE;
        }
    }
        break;
    case OMX_IndexParamGetAndroidNativeBuffer:  /* DEC */
    {
        GetAndroidNativeBufferUsageParams *pANBParams = (GetAndroidNativeBufferUsageParams *)pComponentParameterStructure;
        OMX_U32 portIndex = pANBParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pANBParams, sizeof(GetAndroidNativeBufferUsageParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (portIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        /* NOTE: OMX_IndexParamGetAndroidNativeBuffer returns original 'nUsage' without any
         * modifications since currently not defined what the 'nUsage' is for.
         */
        pANBParams->nUsage |= OMX_GRALLOC_USAGE_DISP;
#ifndef USE_NON_CACHED_GRAPHICBUFFER
        pANBParams->nUsage |= OMX_GRALLOC_USAGE_UVA;
#endif

#ifdef USE_PRIV_USAGE
        pANBParams->nUsage |= OMX_GRALLOC_USAGE_HW_VIDEO_DEC;
#endif

        if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC) {  // secure decoder component
            /* never knows actual image size before parsing a header info
             * could not guarantee DRC(Dynamic Resolution Chnage) case */
            EXYNOS_OMX_BASEPORT *pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            unsigned int HD_SIZE = 1280 * 720 * 3 / 2;  /* 720p */

            if ((pExynosPort->portDefinition.format.video.nFrameWidth *
                 pExynosPort->portDefinition.format.video.nFrameHeight * 3 / 2) > HD_SIZE) {  /* over than 720p */
            }
        }
    }
        break;
    case OMX_IndexParamDescribeColorFormat:
    {
        DescribeColorFormatParams   *pDCFParams     = (DescribeColorFormatParams *)pComponentParameterStructure;
        EXYNOS_OMX_BASEPORT         *pExynosPort    = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
        OMX_U32                      nStride        = pExynosPort->portDefinition.format.video.nStride;
        OMX_U32                      nSliceHeight   = pExynosPort->portDefinition.format.video.nSliceHeight;
        OMX_U32                      yStride        = ALIGN(pExynosPort->portDefinition.format.video.nFrameWidth, 16);
        OMX_U32                      cStride        = ALIGN(pExynosPort->portDefinition.format.video.nFrameWidth / 2, 16);

        ret = Exynos_OMX_Check_SizeVersion(pDCFParams, sizeof(DescribeColorFormatParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pExynosComponent->codecType != HW_VIDEO_DEC_CODEC) {
            if (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] requested on secure codec", pExynosComponent, __FUNCTION__);
            else
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] supported in only decoder ", pExynosComponent, __FUNCTION__);

            goto EXIT;
        }

        MediaImage &image   = pDCFParams->sMediaImage;
        image.mType         = android::MediaImage::MEDIA_IMAGE_TYPE_YUV;
        image.mNumPlanes    = 3;
        image.mBitDepth     = 8;
        image.mWidth        = pExynosPort->portDefinition.format.video.nFrameWidth;
        image.mHeight       = pExynosPort->portDefinition.format.video.nFrameHeight;

        image.mPlane[image.Y].mOffset = 0;
        image.mPlane[image.Y].mColInc = 1;
        image.mPlane[image.Y].mRowInc = nStride;
        image.mPlane[image.Y].mHorizSubsampling = 1;
        image.mPlane[image.Y].mVertSubsampling = 1;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Describe ColorFormat x%x",
                                                    pExynosComponent, __FUNCTION__, pDCFParams->eColorFormat);

        if (pExynosPort->eMetaDataType != METADATA_TYPE_DISABLED) {
            /* in case of graphic buffer */
            switch ((int)pDCFParams->eColorFormat) {
            case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B:
            case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_PRIV:
            case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
            case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN:
            case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_S10B:
            {
                image.mPlane[image.U].mOffset           = 0;
                image.mPlane[image.U].mColInc           = 2;
                image.mPlane[image.U].mRowInc           = nStride;
                image.mPlane[image.U].mHorizSubsampling = 2;
                image.mPlane[image.U].mVertSubsampling  = 2;
                image.mPlane[image.V].mOffset           = 0;
                image.mPlane[image.V].mColInc           = 2;
                image.mPlane[image.V].mRowInc           = nStride;
                image.mPlane[image.V].mHorizSubsampling = 2;
                image.mPlane[image.V].mVertSubsampling  = 2;
            }
                break;
            default:
            {
                image.mType = android::MediaImage::MEDIA_IMAGE_TYPE_UNKNOWN;
                image.mNumPlanes = 0;
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] (0x%x) is not supported Meta mode(0x%x)",
                                            pExynosComponent, __FUNCTION__, pDCFParams->eColorFormat, pExynosPort->eMetaDataType);
            }
                break;
            }
        } else {
            /* in case of byte buffer */
            switch ((int)pDCFParams->eColorFormat) {
            case OMX_COLOR_FormatYUV420SemiPlanar:
            {
                image.mPlane[image.U].mOffset           = nStride * nSliceHeight;
                image.mPlane[image.U].mColInc           = 2;
                image.mPlane[image.U].mRowInc           = nStride;
                image.mPlane[image.U].mHorizSubsampling = 2;
                image.mPlane[image.U].mVertSubsampling  = 2;
                image.mPlane[image.V].mOffset           = image.mPlane[image.U].mOffset + 1;
                image.mPlane[image.V].mColInc           = 2;
                image.mPlane[image.V].mRowInc           = nStride;
                image.mPlane[image.V].mHorizSubsampling = 2;
                image.mPlane[image.V].mVertSubsampling  = 2;
            }
                break;
            case OMX_SEC_COLOR_FormatNV21Linear:
            {
                image.mPlane[image.V].mOffset           = nStride * nSliceHeight;
                image.mPlane[image.V].mColInc           = 2;
                image.mPlane[image.V].mRowInc           = nStride;
                image.mPlane[image.V].mHorizSubsampling = 2;
                image.mPlane[image.V].mVertSubsampling  = 2;
                image.mPlane[image.U].mOffset           = image.mPlane[image.V].mOffset + 1;
                image.mPlane[image.U].mColInc           = 2;
                image.mPlane[image.U].mRowInc           = nStride;
                image.mPlane[image.U].mHorizSubsampling = 2;
                image.mPlane[image.U].mVertSubsampling  = 2;
            }
                break;
            case OMX_SEC_COLOR_FormatYVU420Planar:
            {
                image.mPlane[image.Y].mRowInc           = yStride;
                image.mPlane[image.V].mOffset           = yStride * nSliceHeight;
                image.mPlane[image.V].mColInc           = 1;
                image.mPlane[image.V].mRowInc           = cStride;
                image.mPlane[image.V].mHorizSubsampling = 2;
                image.mPlane[image.V].mVertSubsampling  = 2;
                image.mPlane[image.U].mOffset           = image.mPlane[image.V].mOffset +
                                                            (cStride * nSliceHeight / 2);
                image.mPlane[image.U].mColInc           = 1;
                image.mPlane[image.U].mRowInc           = cStride;
                image.mPlane[image.U].mHorizSubsampling = 2;
                image.mPlane[image.U].mVertSubsampling  = 2;
            }
                break;
            case OMX_COLOR_FormatYUV420Planar:
            {
                image.mPlane[image.U].mOffset           = nStride * nSliceHeight;
                image.mPlane[image.U].mColInc           = 1;
                image.mPlane[image.U].mRowInc           = nStride / 2;
                image.mPlane[image.U].mHorizSubsampling = 2;
                image.mPlane[image.U].mVertSubsampling  = 2;
                image.mPlane[image.V].mOffset           = image.mPlane[image.U].mOffset +
                                                            (nStride * nSliceHeight / 4);
                image.mPlane[image.V].mColInc           = 1;
                image.mPlane[image.V].mRowInc           = nStride / 2;
                image.mPlane[image.V].mHorizSubsampling = 2;
                image.mPlane[image.V].mVertSubsampling  = 2;
            }
                break;
            default:
            {
                image.mType = android::MediaImage::MEDIA_IMAGE_TYPE_UNKNOWN;
                image.mNumPlanes = 0;
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] (0x%x) is not supported",
                                            pExynosComponent, __FUNCTION__, pDCFParams->eColorFormat);
            }
                break;
            }
        }
    }
        break;
     case OMX_IndexParamConsumerUsageBits:  /* ENC */
    {
        OMX_U32 *pUsageBits = (OMX_U32 *)pComponentParameterStructure;

        switch ((int)pExynosComponent->codecType) {
        case HW_VIDEO_ENC_CODEC:
            (*pUsageBits) = OMX_GRALLOC_USAGE_HW_VIDEO_ENC;
            break;
        case HW_VIDEO_ENC_SECURE_CODEC:
            (*pUsageBits) = OMX_GRALLOC_USAGE_HW_VIDEO_ENC | OMX_GRALLOC_USAGE_PROTECTED;
            break;
        default:
            (*pUsageBits) = 0;
            ret = OMX_ErrorUnsupportedIndex;
            break;
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

OMX_ERRORTYPE Exynos_OSAL_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;

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

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexParamEnableAndroidBuffers:
    {
        EnableAndroidNativeBuffersParams *pANBParams  = (EnableAndroidNativeBuffersParams *)pComponentParameterStructure;
        EXYNOS_OMX_BASEPORT              *pExynosPort = NULL;
        OMX_U32                           portIndex   = pANBParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pANBParams, sizeof(EnableAndroidNativeBuffersParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (portIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[portIndex];
        if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][ANB] %s port: metadata(%x), ANB(%d)",
                                        pExynosComponent, __FUNCTION__,
                                        (portIndex == INPUT_PORT_INDEX)? "input":"output",
                                        pExynosPort->eMetaDataType, pANBParams->enable);

        /*
         * adaptive playback(ANB + META): kPortModeDynamicANWBuffer
         * usegraphicbuffer(ANB)        : kPortModePresetANWBuffer
         */
        if ((portIndex == OUTPUT_PORT_INDEX) &&
            ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
             (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC))) {

            if ((pANBParams->enable == OMX_TRUE) &&
                (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
                /* enable */
                pExynosPort->eMetaDataType = METADATA_TYPE_GRAPHIC_HANDLE;

                ret = setBufferProcessTypeForDecoder(pExynosPort);
                if (ret != OMX_ErrorNone)
                    goto EXIT;
            } else if (pANBParams->enable == OMX_FALSE) {
                if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC)
                    pExynosPort->eMetaDataType = METADATA_TYPE_DATA;
                else if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE)
                    pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;

                /* disable */
                resetBufferProcessTypeForDecoder(pExynosPort);
            }
        }

        /* screen recording(ANB + META) : kPortModeDynamicANWBuffer */
        if ((portIndex == INPUT_PORT_INDEX) &&
            ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
             (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC))) {

            if ((pANBParams->enable == OMX_TRUE) &&
                (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED)) {
                pExynosPort->eMetaDataType = METADATA_TYPE_GRAPHIC_HANDLE;
            } else if (pANBParams->enable == OMX_FALSE) {
                if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC)
                    pExynosPort->eMetaDataType = METADATA_TYPE_DATA;
                else if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE)
                    pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;
            }
        }
    }
        break;
    case OMX_IndexParamStoreMetaDataBuffer:
    {
        StoreMetaDataInBuffersParams *pMetaParams = (StoreMetaDataInBuffersParams *)pComponentParameterStructure;
        EXYNOS_OMX_BASEPORT          *pExynosPort = NULL;
        OMX_U32                       portIndex   = pMetaParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pMetaParams, sizeof(StoreMetaDataInBuffersParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (portIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
            if (portIndex == INPUT_PORT_INDEX) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
        }

        pExynosPort = &pExynosComponent->pExynosPort[portIndex];
        if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][META] %s port: metadata mode(%x) -> %s",
                                        pExynosComponent, __FUNCTION__,
                                        (portIndex == INPUT_PORT_INDEX)? "input":"output",
                                        pExynosPort->eMetaDataType,
                                        (pMetaParams->bStoreMetaData == OMX_TRUE)? "enable":"disable");

        if (pMetaParams->bStoreMetaData == OMX_TRUE) {
            if (pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED) {
                /*
                 * scenario : camera recording (camerasource or nativehandlesource)
                 * mode     : kPortModeDynamicNativeHandle
                 * data     : type, native_handle_t
                 */
                pExynosPort->eMetaDataType = METADATA_TYPE_DATA;
            } else if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC_HANDLE) {
                /*
                 * scenario : adaptive playback, screen recording
                 * mode     : kPortModeDynamicANWBuffer
                 * data     : type, buffer_handle_t
                 */
                pExynosPort->eMetaDataType = METADATA_TYPE_GRAPHIC;
            }
        }

        if (pMetaParams->bStoreMetaData == OMX_FALSE) {
            if (pExynosPort->eMetaDataType == METADATA_TYPE_DATA)
                pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;
            else if (pExynosPort->eMetaDataType == METADATA_TYPE_GRAPHIC)
                pExynosPort->eMetaDataType = METADATA_TYPE_GRAPHIC_HANDLE;
        }
    }
        break;
    case OMX_IndexParamAllocateNativeHandle:
    {
        EnableAndroidNativeBuffersParams *pANBParams  = (EnableAndroidNativeBuffersParams *)pComponentParameterStructure;
        EXYNOS_OMX_BASEPORT              *pExynosPort = NULL;
        OMX_U32                           portIndex   = pANBParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pANBParams, sizeof(EnableAndroidNativeBuffersParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (portIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[portIndex];
        if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][NativeHandle] %s port: metadata(%x), NativeHandle(%d)",
                                        pExynosComponent, __FUNCTION__,
                                        (portIndex == INPUT_PORT_INDEX)? "input":"output",
                                        pExynosPort->eMetaDataType, pANBParams->enable);

        /* ideally, hope that StoreMedataBuffer should be called before requesting AllocateNativeHandle.
         * but, frameworks just call AllocateNativeHandle.
         */
        if ((pExynosPort->eMetaDataType == METADATA_TYPE_DISABLED) &&
            (pANBParams->enable == OMX_TRUE)) {
            /*
             * scenario : drm playback, WFD w/ HDCP
             * mode     : kPortModePresetSecureBuffer
             * data     : native_handle_t
             */
            pExynosPort->bufferProcessType = BUFFER_SHARE;
            pExynosPort->eMetaDataType = METADATA_TYPE_HANDLE;
        } else if ((pANBParams->enable == OMX_FALSE) &&
            (pExynosPort->eMetaDataType == METADATA_TYPE_HANDLE)) {
            pExynosPort->bufferProcessType = BUFFER_COPY;
            pExynosPort->eMetaDataType = METADATA_TYPE_DISABLED;
        }
    }
        break;
    case OMX_IndexParamAndroidNatvieBufferConsumerUsage:
    {
        OMX_PARAM_U32TYPE *pConsumerUsageBits = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;
        EXYNOS_OMX_BASEPORT              *pExynosPort = NULL;
        OMX_U32                           portIndex   = pConsumerUsageBits->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pConsumerUsageBits, sizeof(OMX_PARAM_U32TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if (pConsumerUsageBits->nPortIndex != OUTPUT_PORT_INDEX) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid port index", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[portIndex];
        if (pConsumerUsageBits->nU32 & BufferUsage::CPU_READ_OFTEN)
            pExynosPort->bForceUseNonCompFormat = OMX_TRUE;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s][ANB] Consumer usage bits : 0x%x",
                                        pExynosComponent, __FUNCTION__, pConsumerUsageBits->nU32);
        goto EXIT;
    }
        break;

    default:
    {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] unsupported index (%d)",
                                        pExynosComponent, __FUNCTION__, nIndex);
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_GetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure) {
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;

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

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigVideoHdrStaticInfo:
    {
        DescribeHDRStaticInfoParams    *pParams         = (DescribeHDRStaticInfoParams *)pComponentConfigStructure;
        EXYNOS_OMX_VIDEO_HDRSTATICINFO *pHDRStaticInfo  = NULL;
        OMX_U32                         nPortIndex      = pParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pParams, sizeof(DescribeHDRStaticInfoParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
            if (nPortIndex != OUTPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
        } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
                   (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            if (nPortIndex != INPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
        }

        pHDRStaticInfo = &pExynosComponent->pExynosPort[nPortIndex].HDRStaticInfo;

        pParams->sInfo.sType1.mR.x = pHDRStaticInfo->red.x;
        pParams->sInfo.sType1.mR.y = pHDRStaticInfo->red.y;
        pParams->sInfo.sType1.mG.x = pHDRStaticInfo->green.x;
        pParams->sInfo.sType1.mG.y = pHDRStaticInfo->green.y;
        pParams->sInfo.sType1.mB.x = pHDRStaticInfo->blue.x;
        pParams->sInfo.sType1.mB.y = pHDRStaticInfo->blue.y;
        pParams->sInfo.sType1.mW.x = pHDRStaticInfo->white.x;
        pParams->sInfo.sType1.mW.y = pHDRStaticInfo->white.y;

        pParams->sInfo.sType1.mMaxDisplayLuminance = pHDRStaticInfo->nMaxDisplayLuminance / 10000; /* for framework standard : cd/m^2 */
        pParams->sInfo.sType1.mMinDisplayLuminance = pHDRStaticInfo->nMinDisplayLuminance;

        pParams->sInfo.sType1.mMaxContentLightLevel      = pHDRStaticInfo->nMaxContentLight;
        pParams->sInfo.sType1.mMaxFrameAverageLightLevel = pHDRStaticInfo->nMaxPicAverageLight;
    }
        break;
    case OMX_IndexConfigVideoColorAspects:
    {
        DescribeColorAspectsParams    *pParams      = (DescribeColorAspectsParams *)pComponentConfigStructure;
        EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA        = NULL;  /* framework */
        EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA        = NULL;  /* bitstream */
        OMX_U32                        nPortIndex   = pParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pParams, sizeof(DescribeColorAspectsParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        /* get ColorAspects info from DataSpace and pixel format */
        if (pParams->bDataSpaceChanged == OMX_TRUE) {
            /* it will query to encoders.
             * it allows vendors to use extended dataspace and to support change at runtime.
             */
            if ((pExynosComponent->codecType != HW_VIDEO_ENC_CODEC) &&
                (pExynosComponent->codecType != HW_VIDEO_ENC_SECURE_CODEC)) {
                ret = OMX_ErrorUnsupportedSetting;
                goto EXIT;
            }

            ret = getColorAspectsFromDataSpace(pParams);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Updated ColorAspects: format(0x%x), datasapce(0x%x), R(0x%x), P(0x%x), M(0x%x), T(0x%x), ret(0x%x)",
                                                    pExynosComponent, __FUNCTION__,
                                                    pParams->nPixelFormat, pParams->nDataSpace,
                                                    pParams->sAspects.mRange,
                                                    pParams->sAspects.mPrimaries,
                                                    pParams->sAspects.mMatrixCoeffs,
                                                    pParams->sAspects.mTransfer,
                                                    ret);
            goto EXIT;
        }

        /* return supportable dataSpace */
        if (pParams->bRequestingDataSpace == OMX_TRUE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Given ColorAspects: format(0x%x), datasapce(0x%x), R(0x%x), P(0x%x), M(0x%x), T(0x%x), ret(0x%x)",
                                                    pExynosComponent, __FUNCTION__,
                                                    pParams->nPixelFormat, pParams->nDataSpace,
                                                    pParams->sAspects.mRange,
                                                    pParams->sAspects.mPrimaries,
                                                    pParams->sAspects.mMatrixCoeffs,
                                                    pParams->sAspects.mTransfer,
                                                    ret);

            ret = getDataSpaceFromAspects(pParams);

            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] Guidance for DataSpace: datasapce(0x%x), ret(0x%x)", pExynosComponent, __FUNCTION__,
                                                    pParams->nDataSpace, ret);

            if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
                (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
                if ((nPortIndex != OUTPUT_PORT_INDEX) ||
                    (ret != OMX_ErrorNone)) {
                    goto EXIT;
                }

                /* update dataspace only frameworks requests dataspace */
                pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].ColorAspects.nDataSpace = pParams->nDataSpace;
            }
            goto EXIT;
        }

        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
            if (nPortIndex != OUTPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }

            pFWCA = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].ColorAspects;
            pBSCA = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX].ColorAspects;

            if (Exynos_OSAL_Strstr(pExynosComponent->componentName, "VP9") ||
                Exynos_OSAL_Strstr(pExynosComponent->componentName, "vp9")) {
                /* In case of VP9, should rely on information in webm container */
                getColorAspectsPreferFramework(pBSCA, pFWCA, (void *)pParams);
            } else {
                /* generally, rely on information in bitstream */
                getColorAspectsPreferBitstream(pBSCA, pFWCA, (void *)pParams);
            }
        } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
                   (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            if (nPortIndex != INPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }

            pFWCA = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX].ColorAspects;
            pBSCA = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].ColorAspects;

            /* there isn't bitstream */
            getColorAspectsPreferFramework(pBSCA, pFWCA, (void *)pParams);
        }

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] ColorAspects: pixelformat(0x%x), datasapce(0x%x), R(0x%x), P(0x%x), M(0x%x), T(0x%x), ret(0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                pParams->nPixelFormat, pParams->nDataSpace,
                                                pParams->sAspects.mRange,
                                                pParams->sAspects.mPrimaries,
                                                pParams->sAspects.mMatrixCoeffs,
                                                pParams->sAspects.mTransfer,
                                                ret);
    }
        break;
    case OMX_IndexConfigVideoHdr10PlusInfo:
    {
        DescribeHDR10PlusInfoParams *pParams         = (DescribeHDR10PlusInfoParams *)pComponentConfigStructure;
        DescribeHDR10PlusInfoParams *pOutParams      = NULL;
        OMX_U32                      nPortIndex      = pParams->nPortIndex;
        int i;

        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
#if 0
            /* TO DO: Framework should set port index to OUTPUT */
            if (nPortIndex != OUTPUT_PORT_INDEX) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid port index", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
#endif
        } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
                   (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            if (nPortIndex != INPUT_PORT_INDEX) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid port index", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
        }

        if (Exynos_OSAL_GetElemNum(&pExynosComponent->pExynosPort[nPortIndex].HdrDynamicInfoQ) > 0) {
            pOutParams = (DescribeHDR10PlusInfoParams *)Exynos_OSAL_Dequeue(&pExynosComponent->pExynosPort[nPortIndex].HdrDynamicInfoQ);
            if (pOutParams == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Dequeue HDR10+ info", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            Exynos_OSAL_Memcpy(pParams->nValue, pOutParams->nValue, pOutParams->nParamSizeUsed);
            pParams->nParamSizeUsed = pOutParams->nParamSizeUsed;

            for (i = 0; i < MAX_BUFFER_REF; i++) {
                if ((pOutParams == pExynosComponent->HDR10plusList[i].pHDR10PlusInfo) &&
                    (pExynosComponent->HDR10plusList[i].bOccupied == OMX_TRUE)) {
                    pExynosComponent->HDR10plusList[i].bOccupied      = OMX_FALSE;
                    pExynosComponent->HDR10plusList[i].nTag           = -1;
                    pExynosComponent->HDR10plusList[i].pHDR10PlusInfo = NULL;

                    break;
                }
            }

            Exynos_OSAL_Free(pOutParams);
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

OMX_ERRORTYPE Exynos_OSAL_SetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nIndex,
    OMX_IN OMX_PTR pComponentConfigStructure) {
    OMX_ERRORTYPE                    ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent  = NULL;

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

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] index = 0x%x", pExynosComponent, __FUNCTION__, nIndex);
    switch ((int)nIndex) {
    case OMX_IndexConfigVideoHdrStaticInfo:
    {
        DescribeHDRStaticInfoParams    *pParams         = (DescribeHDRStaticInfoParams *)pComponentConfigStructure;
        EXYNOS_OMX_VIDEO_HDRSTATICINFO *pHDRStaticInfo  = NULL;
        OMX_U32                         nPortIndex      = pParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pParams, sizeof(DescribeHDRStaticInfoParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
            if (nPortIndex != OUTPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
        } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
                   (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            if (nPortIndex != INPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
        }

        pHDRStaticInfo = &pExynosComponent->pExynosPort[nPortIndex].HDRStaticInfo;

        pHDRStaticInfo->red.x     = pParams->sInfo.sType1.mR.x;
        pHDRStaticInfo->red.y     = pParams->sInfo.sType1.mR.y;
        pHDRStaticInfo->green.x   = pParams->sInfo.sType1.mG.x;
        pHDRStaticInfo->green.y   = pParams->sInfo.sType1.mG.y;
        pHDRStaticInfo->blue.x    = pParams->sInfo.sType1.mB.x;
        pHDRStaticInfo->blue.y    = pParams->sInfo.sType1.mB.y;
        pHDRStaticInfo->white.x   = pParams->sInfo.sType1.mW.x;
        pHDRStaticInfo->white.y   = pParams->sInfo.sType1.mW.y;

        pHDRStaticInfo->nMaxDisplayLuminance = pParams->sInfo.sType1.mMaxDisplayLuminance * 10000; /* for h.264/265 standard : 0.0001cd/m^2 */
        pHDRStaticInfo->nMinDisplayLuminance = pParams->sInfo.sType1.mMinDisplayLuminance;

        pHDRStaticInfo->nMaxContentLight      = pParams->sInfo.sType1.mMaxContentLightLevel;
        pHDRStaticInfo->nMaxPicAverageLight   = pParams->sInfo.sType1.mMaxFrameAverageLightLevel;
    }
        break;
    case OMX_IndexConfigVideoColorAspects:
    {
        DescribeColorAspectsParams     *pParams     = (DescribeColorAspectsParams *)pComponentConfigStructure;
        EXYNOS_OMX_VIDEO_COLORASPECTS  *pFWCA       = NULL;  /* framework */
        OMX_U32                         nPortIndex  = pParams->nPortIndex;

        ret = Exynos_OMX_Check_SizeVersion(pParams, sizeof(DescribeColorAspectsParams));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Check_SizeVersion", pExynosComponent, __FUNCTION__);
            goto EXIT;
        }

        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
            if (nPortIndex != OUTPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
        } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
                   (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            if (nPortIndex != INPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
        }

        pFWCA = &pExynosComponent->pExynosPort[nPortIndex].ColorAspects;

        pFWCA->nRangeType    = (OMX_U32)pParams->sAspects.mRange;
        pFWCA->nDataSpace    = (OMX_U32)pParams->nDataSpace;
        pFWCA->nPrimaryType  = (OMX_U32)pParams->sAspects.mPrimaries;
        pFWCA->nTransferType = (OMX_U32)pParams->sAspects.mTransfer;
        pFWCA->nCoeffType    = (OMX_U32)pParams->sAspects.mMatrixCoeffs;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%p][%s] ColorAspects: pixelformat(0x%x), datasapce(0x%x), R(0x%x), P(0x%x), M(0x%x), T(0x%x)",
                                                pExynosComponent, __FUNCTION__,
                                                pParams->nPixelFormat, pParams->nDataSpace,
                                                pParams->sAspects.mRange,
                                                pParams->sAspects.mPrimaries,
                                                pParams->sAspects.mMatrixCoeffs,
                                                pParams->sAspects.mTransfer);
    }
        break;
    case OMX_IndexConfigVideoHdr10PlusInfo:
    {
        DescribeHDR10PlusInfoParams *pParams         = (DescribeHDR10PlusInfoParams *)pComponentConfigStructure;
        DescribeHDR10PlusInfoParams *pInParams       = NULL;
        OMX_U32                      nPortIndex      = pParams->nPortIndex;

        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {
#if 0
            /* TO DO: Framework should set port index to OUTPUT */
            if (nPortIndex != OUTPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
#endif
            if (pExynosComponent->pExynosPort->portDefinition.format.video.eCompressionFormat == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Ignore this config for in-band HDR10+ metadata", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorUnsupportedIndex;
                goto EXIT;
            }
        } else if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
                   (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            if (nPortIndex != INPUT_PORT_INDEX) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
            if (pExynosComponent->pExynosPort->portDefinition.format.video.eCompressionFormat == OMX_VIDEO_CodingVP9) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] Ignore this config for out-band HDR10+ metadata", pExynosComponent, __FUNCTION__);
                ret = OMX_ErrorUnsupportedIndex;
                goto EXIT;
            }
        }

        if ((pParams->nParamSize > MAX_HDR10PLUS_SIZE) ||
            (pParams->nParamSize < 1)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] invalid parameter (nParamSize: %d)", pExynosComponent, __FUNCTION__, pParams->nParamSize);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        pInParams = (DescribeHDR10PlusInfoParams *)Exynos_OSAL_Malloc(pParams->nSize);
        if (pInParams == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to malloc", pExynosComponent, __FUNCTION__);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        Exynos_OSAL_Memcpy(pInParams, pParams, pParams->nSize);

        if (Exynos_OSAL_Queue(&pExynosComponent->HDR10plusConfigQ, (void *)pInParams) != 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] Failed to Queue HDR10+ info", pExynosComponent, __FUNCTION__);
            Exynos_OSAL_Free(pInParams);
            ret = OMX_ErrorUndefined;
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

/*
 * meta data contains the following data format.
 * 1) METADATA_TYPE_DATA
 * --------------------------------------------------------------------
 * | type                                  |         payload          |
 * --------------------------------------------------------------------
 * | kMetadataBufferTypeCameraSource       |           FDs            | [ENC] in(Camera)
 * --------------------------------------------------------------------
 * | kMetadataBufferTypeNativeHandleSource |     native_handle_t *    | [ENC] in(Camera)/out(WFD)
 * --------------------------------------------------------------------
 *
 * 2) METADATA_TYPE_HANDLE
 * --------------------------------------------------------------------
 * | native_handle_t *                                                | [DEC] in(Secure)
 * --------------------------------------------------------------------
 *
 * 3) METADATA_TYPE_GRAPHIC (LockMetaData/UnlockMetaData -> GetInfoFromMetaData + lockBuffer/unlockBuffer)
 * --------------------------------------------------------------------
 * | kMetadataBufferTypeGrallocSource      |     buffer_handle_t      | [DEC] out, [ENC] in(Camera:Opaque)
 * --------------------------------------------------------------------
 *
 * 4) METADATA_TYPE_GRAPHIC_HANDLE (LockMetaData/UnlockMetaData -> lockBuffer/unlockBuffer)
 * --------------------------------------------------------------------
 * | buffer_handle_t                                                  | [DEC] out
 * --------------------------------------------------------------------
 */
OMX_ERRORTYPE Exynos_OSAL_GetInfoFromMetaData(
    OMX_IN OMX_PTR                          pBuffer,
    OMX_OUT EXYNOS_OMX_MULTIPLANE_BUFFER   *pBufferInfo,
    OMX_IN EXYNOS_METADATA_TYPE             eMetaDataType)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int i;

    FunctionIn();

    if ((pBuffer == NULL) ||
        (pBufferInfo == NULL) ||
        (eMetaDataType == METADATA_TYPE_DISABLED)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_Memset(pBufferInfo, 0, sizeof(EXYNOS_OMX_MULTIPLANE_BUFFER));

    switch (eMetaDataType) {
    case METADATA_TYPE_DATA:
    {
        MetadataBufferType type;

        /* MetadataBufferType */
        Exynos_OSAL_Memcpy(&type, (MetadataBufferType *)pBuffer, sizeof(MetadataBufferType));

        if (type == kMetadataBufferTypeCameraSource) {
            void *pAddress = NULL;

            /* FD of Y */
            Exynos_OSAL_Memcpy(&pAddress, ((char *)pBuffer) + sizeof(MetadataBufferType), sizeof(void *));
            pBufferInfo->fd[0] = (unsigned long)pAddress;

            /* FD of CbCr */
            Exynos_OSAL_Memcpy(&pAddress, ((char *)pBuffer) + sizeof(MetadataBufferType) + sizeof(void *), sizeof(void *));
            pBufferInfo->fd[1] = (unsigned long)pAddress;

            if ((pBufferInfo->fd[0] == 0) ||
                (pBufferInfo->fd[1] == 0)) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] CameraSource: invalid value(%x, %x)",
                                                    __FUNCTION__, pBufferInfo->fd[0], pBufferInfo->fd[1]);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] METADATA_DATA(CameraSource): %u %u", __FUNCTION__, pBufferInfo->fd[0], pBufferInfo->fd[1]);
        } else if (type == kMetadataBufferTypeNativeHandleSource) {
            VideoNativeHandleMetadata *pMetaData        = (VideoNativeHandleMetadata *)pBuffer;
            native_handle_t           *pNativeHandle    = NULL;

            if (pMetaData->pHandle == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] NativeHandleSource: handle is NULL", __FUNCTION__);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            pNativeHandle = pMetaData->pHandle;
            if (pNativeHandle->version != sizeof(native_handle_t)) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] NativeHandleSource: version is invalid", __FUNCTION__);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            // TODO : check numFds
            for (i = 0; i < pNativeHandle->numFds; i++) {
                pBufferInfo->fd[i] = (unsigned long)(pNativeHandle->data[i]);
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] METADATA_DATA(NativeHandle): %u", __FUNCTION__, pBufferInfo->fd[i]);
            }
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] type(0x%x) is not supported on METADATA_TYPE_DATA",
                                                __FUNCTION__, type);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    case METADATA_TYPE_HANDLE:
    {
        native_handle_t *pNativeHandle = (native_handle_t *)pBuffer;
        int i;

        if (pNativeHandle->version != sizeof(native_handle_t)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] version is invalid on METADATA_TYPE_HANDLE", __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        for (i = 0; i < pNativeHandle->numFds; i++) {
            pBufferInfo->fd[i] = (unsigned long)(pNativeHandle->data[i]);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] METADATA_HANDLE: %u", __FUNCTION__, pBufferInfo->fd[i]);
        }
    }
        break;
    case METADATA_TYPE_GRAPHIC:
    {
        /* just returns buffer_handle_t */
        MetadataBufferType type;
        VideoGrallocMetadata *pMetaData = (VideoGrallocMetadata *)pBuffer;

        /* MetadataBufferType */
        Exynos_OSAL_Memcpy(&type, (MetadataBufferType *)pBuffer, sizeof(MetadataBufferType));

        if (type == kMetadataBufferTypeGrallocSource) {
            if (pMetaData->pHandle == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] GrallocSource: handle is NULL", __FUNCTION__);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }

            /* buffer_handle_t */
            pBufferInfo->addr[0] = (OMX_PTR)(pMetaData->pHandle);
            pBufferInfo->eColorFormat = getBufferFormat(pBufferInfo->addr[0]);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] METADATA_GRAPHIC(GrallocSource): %u, foramt(0x%x)",
                                            __FUNCTION__, pBufferInfo->addr[0], pBufferInfo->eColorFormat);
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] type(0x%x) is not supported on METADATA_TYPE_DATA",
                                                __FUNCTION__, type);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_PTR Exynos_OSAL_AllocMetaDataBuffer(
    OMX_HANDLETYPE          hSharedMemory,
    EXYNOS_METADATA_TYPE    eMetaDataType,
    OMX_U32                 nSizeBytes,
    MEMORY_TYPE             eMemoryType)
{
    native_handle_t *pNativeHandle = NULL;

    OMX_PTR         pTempBuffer = NULL;
    OMX_PTR         pTempVirAdd = NULL;
    unsigned long   nTempFD     = 0;

    FunctionIn();

    if (eMetaDataType == METADATA_TYPE_HANDLE) {  /* native_handle_t */
        pNativeHandle = native_handle_create(FD_NUM, EXT_DATA_NUM);
        if (pNativeHandle == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to native_handle_create", __FUNCTION__);
            pTempBuffer = NULL;
            goto EXIT;
        }

        pTempVirAdd = Exynos_OSAL_SharedMemory_Alloc(hSharedMemory, nSizeBytes, eMemoryType);
        if (pTempVirAdd == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_SharedMemory_Alloc(size:%d, type:0x%x)",
                                                __FUNCTION__, nSizeBytes, eMemoryType);
            native_handle_delete(pNativeHandle);
            pNativeHandle = NULL;
            pTempBuffer = NULL;
            goto EXIT;
        }

        nTempFD = Exynos_OSAL_SharedMemory_VirtToION(hSharedMemory, pTempVirAdd);

        pNativeHandle->data[0] = (int)nTempFD;     /* fd */
        pNativeHandle->data[1] = (int)nSizeBytes;  /* buffer size */
        pNativeHandle->data[2] = 0;                /* stream length. WFD(Converter) uses this for HDCP */

        pTempBuffer = (OMX_PTR)pNativeHandle;
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] not implemented yet", __FUNCTION__);
        pTempBuffer = NULL;
    }

EXIT:
    FunctionOut();

    return pTempBuffer;
}

OMX_ERRORTYPE Exynos_OSAL_FreeMetaDataBuffer(
    OMX_HANDLETYPE          hSharedMemory,
    EXYNOS_METADATA_TYPE    eMetaDataType,
    OMX_PTR                 pTempBuffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    unsigned long nTempFD     = 0;
    OMX_PTR       pTempVirAdd = NULL;

    native_handle_t *pNativeHandle  = NULL;

    FunctionIn();

    if (eMetaDataType == METADATA_TYPE_HANDLE) {  /* native_handle_t */
        pNativeHandle  = (native_handle_t *)pTempBuffer;

        if (pNativeHandle->version != sizeof(native_handle_t)) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] NativeHandleSource: version is invalid", __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        nTempFD     = (unsigned long)pNativeHandle->data[0];
        pTempVirAdd = Exynos_OSAL_SharedMemory_IONToVirt(hSharedMemory, nTempFD);

        Exynos_OSAL_SharedMemory_Free(hSharedMemory, pTempVirAdd);

        native_handle_delete(pNativeHandle);
        pTempBuffer = pNativeHandle = NULL;

        ret = OMX_ErrorNone;
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] not implemented yet", __FUNCTION__);
        ret = OMX_ErrorNotImplemented;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_SetPrependSPSPPSToIDR(
    OMX_PTR pComponentParameterStructure,
    OMX_PTR pbPrependSpsPpsToIdr)
{
    OMX_ERRORTYPE                    ret        = OMX_ErrorNone;
    PrependSPSPPSToIDRFramesParams  *pParams = (PrependSPSPPSToIDRFramesParams *)pComponentParameterStructure;

    ret = Exynos_OMX_Check_SizeVersion(pParams, sizeof(PrependSPSPPSToIDRFramesParams));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OMX_Check_SizeVersion(PrependSPSPPSToIDRFramesParams)", __FUNCTION__);
        goto EXIT;
    }

    (*((OMX_BOOL *)pbPrependSpsPpsToIdr)) = pParams->bEnable;

EXIT:
    return ret;
}

OMX_U32 Exynos_OSAL_GetDisplayExtraBufferCount(void)
{
    char value[PROPERTY_VALUE_MAX] = {0, };

    if (property_get("debug.hwc.otf", value, NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] DisplayExtraBufferCount is 3. The OTF exist", __FUNCTION__);
        return 3;  /* Display System has OTF */
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] DisplayExtraBufferCount is 2. The OTF not exist", __FUNCTION__);
    return 2;  /* min count */
}

OMX_ERRORTYPE Exynos_OSAL_AddVendorExt(
    OMX_HANDLETYPE  hComponent,
    OMX_STRING      cExtName,
    OMX_INDEXTYPE   nIndex)
{
    OMX_ERRORTYPE                            ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE                       *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT                *pExynosComponent   = NULL;
    OMX_PTR                                 *pVendorExts        = NULL;
    OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pVendorExt         = NULL;

    OMX_U32 i;
    int nSlotIndex = -1;

    FunctionIn();

    if ((hComponent == NULL) ||
        (cExtName == NULL)) {
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
    pVendorExts = pExynosComponent->vendorExts;
    for (i = 0; i < MAX_VENDOR_EXT_NUM; i++) {
        if (pVendorExts[i] == NULL) {
            nSlotIndex = (int)i;
            break;
        }
    }

    if (nSlotIndex < 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s][%p] VendorExt(%s) is not added. there is no more empty slot", __FUNCTION__, pExynosComponent, cExtName);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    switch ((int)nIndex) {
    case OMX_IndexConfigVideoQPRange:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((6 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexConfigVideoQPRange;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 6;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"I-minQP");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"I-maxQP");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[2].cKey, (OMX_PTR)"P-minQP");
            pVendorExt->param[2].eValueType = OMX_AndroidVendorValueInt32;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[3].cKey, (OMX_PTR)"P-maxQP");
            pVendorExt->param[3].eValueType = OMX_AndroidVendorValueInt32;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[4].cKey, (OMX_PTR)"B-minQP");
            pVendorExt->param[4].eValueType = OMX_AndroidVendorValueInt32;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[5].cKey, (OMX_PTR)"B-maxQP");
            pVendorExt->param[5].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamNumberRefPframes:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamNumberRefPframes;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"number");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoEnableGPB:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoEnableGPB;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"enable");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexExynosParamBufferCopy:
    {
        int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

        pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
        if (pVendorExt == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(pVendorExt, 0, nSize);

        InitOMXParams(pVendorExt, nSize);
        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

        pVendorExt->nIndex          = (OMX_U32)OMX_IndexExynosParamBufferCopy;
        pVendorExt->eDir            = OMX_DirOutput;
        pVendorExt->nParamCount     = 1;

        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"enable");
        pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

        pVendorExts[nSlotIndex] = pVendorExt;
    }
        break;
    case OMX_IndexParamVideoDropControl:
    {
        int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

        pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
        if (pVendorExt == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(pVendorExt, 0, nSize);

        InitOMXParams(pVendorExt, nSize);
        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

        pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoDropControl;
        pVendorExt->eDir            = OMX_DirInput;
        pVendorExt->nParamCount     = 1;

        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"enable");
        pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

        pVendorExts[nSlotIndex] = pVendorExt;
    }
        break;
    case OMX_IndexParamVideoDisableDFR:
    {
        int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

        pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
        if (pVendorExt == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(pVendorExt, 0, nSize);

        InitOMXParams(pVendorExt, nSize);
        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

        pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoDisableDFR;
        pVendorExt->eDir            = OMX_DirInput;
        pVendorExt->nParamCount     = 1;

        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"disable");
        pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

        pVendorExts[nSlotIndex] = pVendorExt;
    }
        break;
    case OMX_IndexParamVideoCompressedColorFormat:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_DEC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoCompressedColorFormat;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"value");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoChromaQP:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((2 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoChromaQP;
            pVendorExt->eDir            = OMX_DirInput;
            pVendorExt->nParamCount     = 2;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"cr");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"cb");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoDisableHBEncoding:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {
            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoDisableHBEncoding;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"disable");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
#ifdef USE_SKYPE_HD
    case OMX_IndexSkypeParamDriverVersion:
    {
        int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

        pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
        if (pVendorExt == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(pVendorExt, 0, nSize);

        InitOMXParams(pVendorExt, nSize);
        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

        pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamDriverVersion;
        pVendorExt->eDir            = OMX_DirOutput;
        pVendorExt->nParamCount     = 1;

        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"number");
        pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

        pVendorExts[nSlotIndex] = pVendorExt;
    }
        break;
    case OMX_IndexSkypeParamLowLatency:
    {
        int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

        pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
        if (pVendorExt == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(pVendorExt, 0, nSize);

        InitOMXParams(pVendorExt, nSize);
        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

        pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamLowLatency;
        pVendorExt->eDir            = OMX_DirOutput;
        pVendorExt->nParamCount     = 1;

        Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"enable");
        pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

        pVendorExts[nSlotIndex] = pVendorExt;
    }
        break;
    case OMX_IndexSkypeParamEncoderMaxTemporalLayerCount:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((2 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamEncoderMaxTemporalLayerCount;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 2;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"max-p-count");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"max-b-count");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderMaxLTR:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamEncoderMaxLTR;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"max-count");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderLTR:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamEncoderLTR;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"num-ltr-frames");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderPreprocess:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((2 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamEncoderPreprocess;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 2;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"max-downscale-factor");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"rotation");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((2 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoProfileLevelCurrent;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 2;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"profile");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"level");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderSar:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((2 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamEncoderSar;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 2;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"width");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"height");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoAvc:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexParamVideoAvc;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"spacing");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeConfigEncoderLTR:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((2 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeConfigEncoderLTR;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 2;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"mark-frame");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"use-frame");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeConfigQP:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeConfigQP;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"value");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeConfigBasePid:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeConfigBasePid;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"value");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderInputControl:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamEncoderInputControl;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"enable");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeConfigEncoderInputTrigger:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeConfigEncoderInputTrigger;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"timestamp");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt64;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexSkypeParamVideoBitrate:
    {
        if ((pExynosComponent->codecType == HW_VIDEO_ENC_CODEC) ||
            (pExynosComponent->codecType == HW_VIDEO_ENC_SECURE_CODEC)) {

            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) + ((2 - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_PARAMTYPE));

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexSkypeParamVideoBitrate;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 2;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"value");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[1].cKey, (OMX_PTR)"bitrate");
            pVendorExt->param[1].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
#endif  // USE_SKYPE_HD
    case OMX_IndexExynosParamImageConvertMode:
    {
        if (pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) {
            int nSize = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE);

            pVendorExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)Exynos_OSAL_Malloc(nSize);
            if (pVendorExt == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s][%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__, pExynosComponent);
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            Exynos_OSAL_Memset(pVendorExt, 0, nSize);

            InitOMXParams(pVendorExt, nSize);
            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->cName, (OMX_PTR)cExtName);

            pVendorExt->nIndex          = (OMX_U32)OMX_IndexExynosParamImageConvertMode;
            pVendorExt->eDir            = OMX_DirOutput;
            pVendorExt->nParamCount     = 1;

            Exynos_OSAL_Strcpy((OMX_PTR)pVendorExt->param[0].cKey, (OMX_PTR)"value");
            pVendorExt->param[0].eValueType = OMX_AndroidVendorValueInt32;

            pVendorExts[nSlotIndex] = pVendorExt;
        } else {
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
    }
        break;
    default:
        break;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s][%p] vendor extension(name:%s, index:0x%x) is added",
                                            __FUNCTION__, pExynosComponent, cExtName, nIndex);

EXIT:
    FunctionOut();

    return ret;
}

void Exynos_OSAL_DelVendorExts(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                            ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE                       *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT                *pExynosComponent   = NULL;
    OMX_PTR                                 *pVendorExts        = NULL;
    OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pVendorExt         = NULL;

    OMX_U32 i;


    FunctionIn();

    if (hComponent == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pOMXComponent->pComponentPrivate == NULL)
        goto EXIT;

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVendorExts = pExynosComponent->vendorExts;

    for (i = 0; i < MAX_VENDOR_EXT_NUM; i++) {
        if (pVendorExts[i] != NULL) {
            Exynos_OSAL_Free(pVendorExts[i]);
            pVendorExts[i] = NULL;
        }
    }

EXIT:
    FunctionOut();

    return;
}

OMX_ERRORTYPE Exynos_OSAL_GetVendorExt(
    OMX_HANDLETYPE  hComponent,
    OMX_PTR         pConfig)
{
    OMX_ERRORTYPE                            ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE                       *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT                *pExynosComponent   = NULL;
    OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pDstExt            = NULL;
    OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pSrcExt            = NULL;

    OMX_U32 i;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pConfig == NULL)) {
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

    pDstExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)pConfig;

    if ((pDstExt->nIndex >= MAX_VENDOR_EXT_NUM) ||
        (pExynosComponent->vendorExts[pDstExt->nIndex] == NULL)) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    pSrcExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)pExynosComponent->vendorExts[pDstExt->nIndex];

    ret = Exynos_OMX_Check_SizeVersion(pDstExt, pSrcExt->nSize);
    if (ret != OMX_ErrorNone) {
        if ((pDstExt->nSize < pSrcExt->nSize) ||
            (pDstExt->nParamSizeUsed < pSrcExt->nParamCount)) {
            /* in order to inform number of required parameter */
            pDstExt->nParamCount = pSrcExt->nParamCount;
            ret = OMX_ErrorNone;
        }

        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s][%p] index:%d -> vendor extension(%s)",
                                      __FUNCTION__, pExynosComponent, pDstExt->nIndex, pSrcExt->cName);

    switch ((int)pSrcExt->nIndex) {
    case OMX_IndexConfigVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE QpRange;

        Exynos_OSAL_Memset(&QpRange, 0, sizeof(QpRange));
        InitOMXParams(&QpRange, sizeof(QpRange));

        QpRange.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetConfig(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&QpRange);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            for (i = 0; i < pSrcExt->nParamCount; i++) {
                Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
                pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
                pDstExt->param[i].bSet = OMX_TRUE;
            }

            pDstExt->param[0].nInt32 = QpRange.qpRangeI.nMinQP;
            pDstExt->param[1].nInt32 = QpRange.qpRangeI.nMaxQP;
            pDstExt->param[2].nInt32 = QpRange.qpRangeP.nMinQP;
            pDstExt->param[3].nInt32 = QpRange.qpRangeP.nMaxQP;
            pDstExt->param[4].nInt32 = QpRange.qpRangeB.nMinQP;
            pDstExt->param[5].nInt32 = QpRange.qpRangeB.nMaxQP;
        }
    }
        break;
    case OMX_IndexParamNumberRefPframes:
    {
        OMX_PARAM_U32TYPE NumberRefPframes;

        Exynos_OSAL_Memset(&NumberRefPframes, 0, sizeof(NumberRefPframes));
        InitOMXParams(&NumberRefPframes, sizeof(NumberRefPframes));

        NumberRefPframes.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&NumberRefPframes);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet       = OMX_TRUE;
            pDstExt->param[0].nInt32     = NumberRefPframes.nU32;
        }
    }
        break;
    case OMX_IndexParamVideoEnableGPB:
    {
        OMX_CONFIG_BOOLEANTYPE GPBEnable;

        Exynos_OSAL_Memset(&GPBEnable, 0, sizeof(GPBEnable));
        InitOMXParams(&GPBEnable, sizeof(GPBEnable));

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&GPBEnable);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet       = OMX_TRUE;
            pDstExt->param[0].nInt32     = GPBEnable.bEnabled;
        }
    }
        break;
    case OMX_IndexExynosParamBufferCopy:
    {
        OMX_PARAM_U32TYPE bufferCopy;

        Exynos_OSAL_Memset(&bufferCopy, 0, sizeof(bufferCopy));
        InitOMXParams(&bufferCopy, sizeof(bufferCopy));

        bufferCopy.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&bufferCopy);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = bufferCopy.nU32;
        }
    }
        break;
    case OMX_IndexParamVideoDropControl:
    {
        OMX_CONFIG_BOOLEANTYPE dropControl;

        Exynos_OSAL_Memset(&dropControl, 0, sizeof(dropControl));
        InitOMXParams(&dropControl, sizeof(dropControl));

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&dropControl);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = dropControl.bEnabled;
        }
    }
        break;
    case OMX_IndexParamVideoDisableDFR:
    {
        OMX_CONFIG_BOOLEANTYPE disableDFR;

        Exynos_OSAL_Memset(&disableDFR, 0, sizeof(disableDFR));
        InitOMXParams(&disableDFR, sizeof(disableDFR));

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&disableDFR);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = disableDFR.bEnabled;
        }
    }
        break;
    case OMX_IndexParamVideoCompressedColorFormat:
    {
        OMX_PARAM_U32TYPE colorFormat;

        Exynos_OSAL_Memset(&colorFormat, 0, sizeof(colorFormat));
        InitOMXParams(&colorFormat, sizeof(colorFormat));

        colorFormat.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&colorFormat);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = colorFormat.nU32;
        }
    }
        break;
    case OMX_IndexParamVideoChromaQP:
    {
        OMX_VIDEO_PARAM_CHROMA_QP_OFFSET chromaQPOffset;

        Exynos_OSAL_Memset(&chromaQPOffset, 0, sizeof(chromaQPOffset));
        InitOMXParams(&chromaQPOffset, sizeof(chromaQPOffset));

        chromaQPOffset.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&chromaQPOffset);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            for (i = 0; i < pSrcExt->nParamCount; i++) {
                Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
                pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
                pDstExt->param[i].bSet = OMX_TRUE;
            }
            pDstExt->param[0].nInt32        = chromaQPOffset.nCr;
            pDstExt->param[1].nInt32        = chromaQPOffset.nCb;
        }
    }
        break;
    case OMX_IndexParamVideoDisableHBEncoding:
    {
        OMX_CONFIG_BOOLEANTYPE disableHBEncoding;

        Exynos_OSAL_Memset(&disableHBEncoding, 0, sizeof(disableHBEncoding));
        InitOMXParams(&disableHBEncoding, sizeof(disableHBEncoding));

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&disableHBEncoding);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = disableHBEncoding.bEnabled;
        }
    }
        break;
#ifdef USE_SKYPE_HD
    case OMX_IndexSkypeParamDriverVersion:
    {
        OMX_VIDEO_PARAM_DRIVERVER DriverVer;

        Exynos_OSAL_Memset(&DriverVer, 0, sizeof(DriverVer));
        InitOMXParams(&DriverVer, sizeof(DriverVer));

        DriverVer.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&DriverVer);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet       = OMX_TRUE;
            pDstExt->param[0].nInt32     = (OMX_U32)DriverVer.nDriverVersion;
        }
    }
        break;
    case OMX_IndexSkypeParamLowLatency:
    {
        OMX_PARAM_U32TYPE lowLatency;

        Exynos_OSAL_Memset(&lowLatency, 0, sizeof(lowLatency));
        InitOMXParams(&lowLatency, sizeof(lowLatency));

        lowLatency.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&lowLatency);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = lowLatency.nU32;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderMaxTemporalLayerCount:
    {
        OMX_PARAM_ENC_MAX_TEMPORALLAYER_COUNT temporalLayer;

        Exynos_OSAL_Memset(&temporalLayer, 0, sizeof(temporalLayer));
        InitOMXParams(&temporalLayer, sizeof(temporalLayer));

        temporalLayer.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&temporalLayer);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            for (i = 0; i < pSrcExt->nParamCount; i++) {
                Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
                pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
                pDstExt->param[i].bSet = OMX_TRUE;
            }

            pDstExt->param[0].nInt32  = temporalLayer.nMaxCountP;
            pDstExt->param[1].nInt32  = temporalLayer.nMaxCountB;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderMaxLTR:
    {
        OMX_PARAM_U32TYPE LTR;

        Exynos_OSAL_Memset(&LTR, 0, sizeof(LTR));
        InitOMXParams(&LTR, sizeof(LTR));

        LTR.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&LTR);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = LTR.nU32;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderLTR:
    {
        OMX_PARAM_U32TYPE LTR;

        Exynos_OSAL_Memset(&LTR, 0, sizeof(LTR));
        InitOMXParams(&LTR, sizeof(LTR));

        LTR.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&LTR);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = LTR.nU32;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderPreprocess:
    {
        OMX_PARAM_ENC_PREPROCESS preprocess;

        Exynos_OSAL_Memset(&preprocess, 0, sizeof(preprocess));
        InitOMXParams(&preprocess, sizeof(preprocess));

        preprocess.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&preprocess);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            for (i = 0; i < pSrcExt->nParamCount; i++) {
                Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
                pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
                pDstExt->param[i].bSet = OMX_TRUE;
            }

            pDstExt->param[0].nInt32  = preprocess.nResize;
            pDstExt->param[1].nInt32  = preprocess.nRotation;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE profilelevel;

        Exynos_OSAL_Memset(&profilelevel, 0, sizeof(profilelevel));
        InitOMXParams(&profilelevel, sizeof(profilelevel));

        profilelevel.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&profilelevel);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            for (i = 0; i < pSrcExt->nParamCount; i++) {
                Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
                pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
                pDstExt->param[i].bSet = OMX_TRUE;
            }

            pDstExt->param[0].nInt32  = profilelevel.eProfile;
            pDstExt->param[1].nInt32  = profilelevel.eLevel;
        }
    }
        break;
    case OMX_IndexSkypeParamEncoderSar:
    {
        OMX_PARAM_ENC_SAR sar;

        Exynos_OSAL_Memset(&sar, 0, sizeof(sar));
        InitOMXParams(&sar, sizeof(sar));

        sar.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&sar);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            for (i = 0; i < pSrcExt->nParamCount; i++) {
                Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
                pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
                pDstExt->param[i].bSet = OMX_TRUE;
            }

            pDstExt->param[0].nInt32  = sar.nWidth;
            pDstExt->param[1].nInt32  = sar.nHeight;
        }
    }
        break;
    case OMX_IndexParamVideoAvc:
    {
        OMX_VIDEO_PARAM_AVCTYPE avctype;

        Exynos_OSAL_Memset(&avctype, 0, sizeof(avctype));
        InitOMXParams(&avctype, sizeof(avctype));

        avctype.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&avctype);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType    = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet          = OMX_TRUE;
            pDstExt->param[0].nInt32        = avctype.nSliceHeaderSpacing;
        }
    }
        break;
    case OMX_IndexSkypeConfigEncoderLTR:  /* GetConfig() is not supported */
    case OMX_IndexSkypeConfigQP:
    case OMX_IndexSkypeParamEncoderInputControl:
    case OMX_IndexSkypeConfigEncoderInputTrigger:
    {
        Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
        pDstExt->eDir = pSrcExt->eDir;
        pDstExt->nParamCount = pSrcExt->nParamCount;

        for (i = 0; i < pSrcExt->nParamCount; i++) {
            Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
            pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
            pDstExt->param[i].bSet = OMX_FALSE;
        }
    }
        break;
    case OMX_IndexSkypeConfigBasePid:
    {
        OMX_VIDEO_CONFIG_BASELAYERPID BaseLayerPid;

        Exynos_OSAL_Memset(&BaseLayerPid, 0, sizeof(BaseLayerPid));
        InitOMXParams(&BaseLayerPid, sizeof(BaseLayerPid));

        BaseLayerPid.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetConfig(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&BaseLayerPid);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            Exynos_OSAL_Memcpy(pDstExt->param[0].cKey, pSrcExt->param[0].cKey, sizeof(pSrcExt->param[0].cKey));
            pDstExt->param[0].eValueType = pSrcExt->param[0].eValueType;
            pDstExt->param[0].bSet       = OMX_TRUE;
            pDstExt->param[0].nInt32     = BaseLayerPid.nPID;
        }
    }
        break;
    case OMX_IndexSkypeParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE Bitrate;

        Exynos_OSAL_Memset(&Bitrate, 0, sizeof(Bitrate));
        InitOMXParams(&Bitrate, sizeof(Bitrate));

        Bitrate.nPortIndex = pSrcExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pSrcExt->nIndex, (OMX_PTR)&Bitrate);
        if (ret == OMX_ErrorNone) {
            Exynos_OSAL_Memcpy(pDstExt->cName, pSrcExt->cName, sizeof(pDstExt->cName));
            pDstExt->eDir = pSrcExt->eDir;
            pDstExt->nParamCount = pSrcExt->nParamCount;

            for (i = 0; i < pSrcExt->nParamCount; i++) {
                Exynos_OSAL_Memcpy(pDstExt->param[i].cKey, pSrcExt->param[i].cKey, sizeof(pSrcExt->param[i].cKey));
                pDstExt->param[i].eValueType = pSrcExt->param[i].eValueType;
                pDstExt->param[i].bSet = OMX_TRUE;
            }

            pDstExt->param[0].nInt32  = Bitrate.eControlRate;
            pDstExt->param[1].nInt32  = Bitrate.nTargetBitrate;
        }
    }
        break;
#endif  // USE_SKYPE_HD
    default:
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OSAL_SetVendorExt(
    OMX_HANDLETYPE  hComponent,
    OMX_PTR         pConfig)
{
    OMX_ERRORTYPE                            ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE                       *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT                *pExynosComponent   = NULL;
    OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pDstExt            = NULL;
    OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pSrcExt            = NULL;

    OMX_U32 i;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pConfig == NULL)) {
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

    pSrcExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)pConfig;

    for (i = 0; i < MAX_VENDOR_EXT_NUM; i++) {
        pDstExt = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)pExynosComponent->vendorExts[i];
        if (pDstExt == NULL)
            break;

        if (!Exynos_OSAL_Strcmp(pDstExt->cName, pSrcExt->cName))
            break;

        pDstExt = NULL;
    }

    if (pDstExt == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s] vendor extension(name:%s) is not supported",
                                            __FUNCTION__, pExynosComponent, pSrcExt->cName);
        ret = OMX_ErrorUnsupportedSetting;
        goto EXIT;
    }

    ret = Exynos_OMX_Check_SizeVersion(pSrcExt, pDstExt->nSize);
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if ((pSrcExt->eDir != pDstExt->eDir) ||
        (pSrcExt->nParamCount != pDstExt->nParamCount)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s][%p] vendor extension(%s)",
                                        __FUNCTION__, pExynosComponent, pSrcExt->cName);

    switch ((int)pDstExt->nIndex) {
    case OMX_IndexConfigVideoQPRange:
    {
        OMX_VIDEO_QPRANGETYPE QpRange;

        Exynos_OSAL_Memset(&QpRange, 0, sizeof(QpRange));
        InitOMXParams(&QpRange, sizeof(QpRange));

        QpRange.nPortIndex = pDstExt->eDir;

        ret = pOMXComponent->GetConfig(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&QpRange);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < pSrcExt->nParamCount; i++) {
            if (pSrcExt->param[i].bSet == OMX_TRUE) {
                if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"I-minQP")) {
                    QpRange.qpRangeI.nMinQP = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"I-maxQP")) {
                    QpRange.qpRangeI.nMaxQP = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"P-minQP")) {
                    QpRange.qpRangeP.nMinQP = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"P-maxQP")) {
                    QpRange.qpRangeP.nMaxQP = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"B-minQP")) {
                    QpRange.qpRangeB.nMinQP = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"B-maxQP")) {
                    QpRange.qpRangeB.nMaxQP = pSrcExt->param[i].nInt32;
                }
            }
        }

        ret = pOMXComponent->SetConfig(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&QpRange);
    }
        break;
    case OMX_IndexParamNumberRefPframes:
    {
        OMX_PARAM_U32TYPE NumberRefPframes;

        Exynos_OSAL_Memset(&NumberRefPframes, 0, sizeof(NumberRefPframes));
        InitOMXParams(&NumberRefPframes, sizeof(NumberRefPframes));

        NumberRefPframes.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"number"))
                NumberRefPframes.nU32 = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&NumberRefPframes);
    }
        break;
    case OMX_IndexParamVideoEnableGPB:
    {
        OMX_CONFIG_BOOLEANTYPE GPBEnable;

        Exynos_OSAL_Memset(&GPBEnable, 0, sizeof(GPBEnable));
        InitOMXParams(&GPBEnable, sizeof(GPBEnable));

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"enable"))
                GPBEnable.bEnabled = (OMX_BOOL)pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&GPBEnable);
    }
        break;
    case OMX_IndexExynosParamBufferCopy:
    {
        OMX_PARAM_U32TYPE bufferCopy;

        Exynos_OSAL_Memset(&bufferCopy, 0, sizeof(bufferCopy));
        InitOMXParams(&bufferCopy, sizeof(bufferCopy));

        bufferCopy.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"enable"))
                bufferCopy.nU32 = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&bufferCopy);
    }
        break;
    case OMX_IndexParamVideoDropControl:
    {
        OMX_CONFIG_BOOLEANTYPE dropControl;

        Exynos_OSAL_Memset(&dropControl, 0, sizeof(dropControl));
        InitOMXParams(&dropControl, sizeof(dropControl));

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"enable"))
                dropControl.bEnabled = (OMX_BOOL)pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&dropControl);
    }
        break;
    case OMX_IndexParamVideoDisableDFR:
    {
        OMX_CONFIG_BOOLEANTYPE disableDFR;

        Exynos_OSAL_Memset(&disableDFR, 0, sizeof(disableDFR));
        InitOMXParams(&disableDFR, sizeof(disableDFR));

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"disable"))
                disableDFR.bEnabled = (OMX_BOOL)pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&disableDFR);
    }
        break;
    case OMX_IndexParamVideoCompressedColorFormat:
    {
        OMX_PARAM_U32TYPE colorFormat;

        Exynos_OSAL_Memset(&colorFormat, 0, sizeof(colorFormat));
        InitOMXParams(&colorFormat, sizeof(colorFormat));

        colorFormat.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"value"))
                colorFormat.nU32 = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&colorFormat);
    }
        break;
    case OMX_IndexParamVideoChromaQP:
    {
        OMX_VIDEO_PARAM_CHROMA_QP_OFFSET chromaQPOffset;

        Exynos_OSAL_Memset(&chromaQPOffset, 0, sizeof(chromaQPOffset));
        InitOMXParams(&chromaQPOffset, sizeof(chromaQPOffset));

        chromaQPOffset.nPortIndex = pDstExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&chromaQPOffset);
        if (ret!= OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < pSrcExt->nParamCount; i++) {
            if (pSrcExt->param[i].bSet == OMX_TRUE) {
                if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"cr")) {
                    chromaQPOffset.nCr = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"cb")) {
                    chromaQPOffset.nCb = pSrcExt->param[i].nInt32;
                }
            }
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&chromaQPOffset);
    }
        break;
    case OMX_IndexParamVideoDisableHBEncoding:
    {
        OMX_CONFIG_BOOLEANTYPE disableHBEncoding;

        Exynos_OSAL_Memset(&disableHBEncoding, 0, sizeof(disableHBEncoding));
        InitOMXParams(&disableHBEncoding, sizeof(disableHBEncoding));

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"disable"))
                disableHBEncoding.bEnabled = (OMX_BOOL)pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&disableHBEncoding);
    }
        break;
#ifdef USE_SKYPE_HD
    case OMX_IndexSkypeParamLowLatency:
    {
        OMX_PARAM_U32TYPE lowLatency;

        Exynos_OSAL_Memset(&lowLatency, 0, sizeof(lowLatency));
        InitOMXParams(&lowLatency, sizeof(lowLatency));

        lowLatency.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"enable"))
                lowLatency.nU32 = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&lowLatency);
    }
        break;
    case OMX_IndexSkypeParamEncoderLTR:
    {
        OMX_PARAM_U32TYPE LTR;

        Exynos_OSAL_Memset(&LTR, 0, sizeof(LTR));
        InitOMXParams(&LTR, sizeof(LTR));

        LTR.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"num-ltr-frames"))
                LTR.nU32 = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&LTR);
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE profilelevel;

        Exynos_OSAL_Memset(&profilelevel, 0, sizeof(profilelevel));
        InitOMXParams(&profilelevel, sizeof(profilelevel));

        profilelevel.nPortIndex = pDstExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&profilelevel);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < pSrcExt->nParamCount; i++) {
            if (pSrcExt->param[i].bSet == OMX_TRUE) {
                if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"profile")) {
                    profilelevel.eProfile = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"level")) {
                    profilelevel.eLevel = pSrcExt->param[i].nInt32;
                }
            }
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&profilelevel);
    }
        break;
    case OMX_IndexSkypeParamEncoderSar:
    {
        OMX_PARAM_ENC_SAR sar;

        Exynos_OSAL_Memset(&sar, 0, sizeof(sar));
        InitOMXParams(&sar, sizeof(sar));

        sar.nPortIndex = pDstExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&sar);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < pSrcExt->nParamCount; i++) {
            if (pSrcExt->param[i].bSet == OMX_TRUE) {
                if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"width")) {
                    sar.nWidth = pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"height")) {
                    sar.nHeight = pSrcExt->param[i].nInt32;
                }
            }
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&sar);
    }
        break;
    case OMX_IndexParamVideoAvc:
    {
        OMX_VIDEO_PARAM_AVCTYPE avctype;

        Exynos_OSAL_Memset(&avctype, 0, sizeof(avctype));
        InitOMXParams(&avctype, sizeof(avctype));

        avctype.nPortIndex = pDstExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&avctype);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"spacing"))
                avctype.nSliceHeaderSpacing = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&avctype);
    }
        break;
    case OMX_IndexSkypeConfigEncoderLTR:
    {
        for (i = 0; i < pSrcExt->nParamCount; i++) {
            if (pSrcExt->param[i].bSet == OMX_TRUE) {
                if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"mark-frame")) {
                    OMX_VIDEO_CONFIG_MARKLTRFRAME markFrame;

                    Exynos_OSAL_Memset(&markFrame, 0, sizeof(markFrame));
                    InitOMXParams(&markFrame, sizeof(markFrame));

                    markFrame.nPortIndex = pDstExt->eDir;
                    markFrame.nLongTermFrmIdx = pSrcExt->param[i].nInt32;

                    ret = pOMXComponent->SetConfig(hComponent, (OMX_INDEXTYPE)OMX_IndexSkypeConfigMarkLTRFrame, (OMX_PTR)&markFrame);
                    if (ret != OMX_ErrorNone)
                        goto EXIT;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"use-frame")) {
                    OMX_VIDEO_CONFIG_USELTRFRAME useFrame;

                    Exynos_OSAL_Memset(&useFrame, 0, sizeof(useFrame));
                    InitOMXParams(&useFrame, sizeof(useFrame));

                    useFrame.nPortIndex = pDstExt->eDir;
                    useFrame.nUsedLTRFrameBM = (OMX_U16)pSrcExt->param[i].nInt32;

                    ret = pOMXComponent->SetConfig(hComponent, (OMX_INDEXTYPE)OMX_IndexSkypeConfigUseLTRFrame, (OMX_PTR)&useFrame);
                    if (ret != OMX_ErrorNone)
                        goto EXIT;
                }
            }
        }
    }
        break;
    case OMX_IndexSkypeConfigQP:
    {
        OMX_VIDEO_CONFIG_QP ConfigQP;

        Exynos_OSAL_Memset(&ConfigQP, 0, sizeof(ConfigQP));
        InitOMXParams(&ConfigQP, sizeof(ConfigQP));

        ConfigQP.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"value"))
                ConfigQP.nQP = (OMX_U32)pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetConfig(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&ConfigQP);
    }
        break;
    case OMX_IndexSkypeConfigBasePid:
    {
        OMX_VIDEO_CONFIG_BASELAYERPID BaseLayerPid;

        Exynos_OSAL_Memset(&BaseLayerPid, 0, sizeof(BaseLayerPid));
        InitOMXParams(&BaseLayerPid, sizeof(BaseLayerPid));

        BaseLayerPid.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"value"))
                BaseLayerPid.nPID = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetConfig(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&BaseLayerPid);
    }
        break;
    case OMX_IndexSkypeParamEncoderInputControl:
    {
        OMX_PARAM_U32TYPE inputControl;

        Exynos_OSAL_Memset(&inputControl, 0, sizeof(inputControl));
        InitOMXParams(&inputControl, sizeof(inputControl));

        if ((pSrcExt->eDir != pDstExt->eDir) ||
            (pSrcExt->nParamCount != pDstExt->nParamCount)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        inputControl.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"enable"))
                inputControl.nU32 = pSrcExt->param[0].nInt32;
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&inputControl);
    }
        break;
    case OMX_IndexSkypeConfigEncoderInputTrigger:
    {
        OMX_CONFIG_ENC_TRIGGER_TS triggerTS;

        Exynos_OSAL_Memset(&triggerTS, 0, sizeof(triggerTS));
        InitOMXParams(&triggerTS, sizeof(triggerTS));

        triggerTS.nPortIndex = pDstExt->eDir;

        if (pSrcExt->param[0].bSet == OMX_TRUE) {
            if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[0].cKey, (OMX_PTR)"timestamp"))
                triggerTS.nTimestamp = pSrcExt->param[0].nInt64;
        }

        ret = pOMXComponent->SetConfig(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&triggerTS);
    }
        break;
    case OMX_IndexSkypeParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE  Bitrate;

        Exynos_OSAL_Memset(&Bitrate, 0, sizeof(Bitrate));
        InitOMXParams(&Bitrate, sizeof(Bitrate));

        Bitrate.nPortIndex = pDstExt->eDir;

        ret = pOMXComponent->GetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&Bitrate);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < pSrcExt->nParamCount; i++) {
            if (pSrcExt->param[i].bSet == OMX_TRUE) {
                if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"value")) {
                    Bitrate.eControlRate = (OMX_VIDEO_CONTROLRATETYPE)pSrcExt->param[i].nInt32;
                } else if (!Exynos_OSAL_Strcmp((OMX_PTR)pSrcExt->param[i].cKey, (OMX_PTR)"bitrate")) {
                    Bitrate.nTargetBitrate = pSrcExt->param[i].nInt32;
                }
            }
        }

        ret = pOMXComponent->SetParameter(hComponent, (OMX_INDEXTYPE)pDstExt->nIndex, (OMX_PTR)&Bitrate);
    }
        break;
#endif  // USE_SKYPE_HD
    default:
        break;
    }

EXIT:
    FunctionOut();

    return ret;

}

OMX_HANDLETYPE Exynos_OSAL_CreatePerformanceHandle(OMX_BOOL bIsEncoder) {
    PERFORMANCE_HANDLE *pPerfHandle = (PERFORMANCE_HANDLE*)malloc(sizeof(PERFORMANCE_HANDLE));
    if (pPerfHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] failed to allocate PERFORMANCE_HANDLE", __FUNCTION__);
        return NULL;
    }

    pPerfHandle->bIsEncoder = ((bIsEncoder == OMX_TRUE)? true:false);

    pPerfHandle->pEpic = createOperator((pPerfHandle->bIsEncoder)? eVideoEncoding:eVideoDecoding);
    if (pPerfHandle->pEpic == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] Failed to create EpicOperator", __FUNCTION__);
        free(pPerfHandle);
        return NULL;
    }

    if (!pPerfHandle->bIsEncoder) {
        epic::IEpicOperator *pEpic = static_cast<epic::IEpicOperator*>(pPerfHandle->pEpic);
        if (pEpic != NULL)
            pEpic->doAction(eAcquire, NULL);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] EpicOperator is acquired", __FUNCTION__);
    }

    return pPerfHandle;
}

void Exynos_OSAL_Performance(OMX_HANDLETYPE handle, int value, int fps)
{
    PERFORMANCE_HANDLE *pPerfHandle = (PERFORMANCE_HANDLE*)handle;
    if (pPerfHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] PERFORMANCE_HANDLE is invalid", __FUNCTION__);
        return;
    }

    epic::IEpicOperator* pEpic = static_cast<epic::IEpicOperator*>(pPerfHandle->pEpic);
    if (pEpic == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] EpicOperator is invalid", __FUNCTION__);
        return;
    }

    if (pPerfHandle->bIsEncoder) {
        static int timeslice = -1;  /* uninitialized */

        if (timeslice == -1)
            timeslice = property_get_int32("ro.vendor.power.timeslice", 1000/* default: 1000ms */);

        int vectors[] = { 1, 2, 4, 5, 8, 10, 20, 40 };
        int threshold = vectors[0];
        int i = 0;

        for (i = 0; i < (int)(sizeof(vectors)/sizeof(vectors[0])); i++) {
            if ((vectors[i] * 1000/*ms*/ / (fps? fps:1)) > timeslice) {
                /* interval := (vectors[i] * (1000 / fps)ms) > timeslice
                 * it can not be available to inform a hint within timeslice
                 */
                break;
            }

            threshold = vectors[i];
        }

        if ((value >= 0) && (value % threshold) == 0) {
            pEpic->doAction(eAcquire, nullptr);
        }
    }
}

void Exynos_OSAL_ReleasePerformanceHandle(OMX_HANDLETYPE handle) {
    PERFORMANCE_HANDLE *pPerfHandle = (PERFORMANCE_HANDLE*)handle;
    if (pPerfHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] PERFORMANCE_HANDLE is invalid", __FUNCTION__);
        return;
    }

    epic::IEpicOperator* pEpic = static_cast<epic::IEpicOperator*>(pPerfHandle->pEpic);
    if (pEpic == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] EpicOperator is invalid", __FUNCTION__);
        free(pPerfHandle);
        return;
    }

    if (!pPerfHandle->bIsEncoder) {
        pEpic->doAction(eRelease, NULL);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] EpicOperator is released", __FUNCTION__);
    }

    free(pPerfHandle);

    return;
}

#ifdef __cplusplus
}
#endif
