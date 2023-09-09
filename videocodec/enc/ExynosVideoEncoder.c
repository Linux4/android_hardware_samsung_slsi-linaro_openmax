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
 * @file        ExynosVideoEncoder.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Taehwan Kim   (t_h.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.01.15: Initial Version
 *   2016.01.28: Update Version to support OSAL
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include <sys/poll.h>

#include "ExynosVideoApi.h"
#include "ExynosVideoEnc.h"
#include "ExynosVideo_OSAL_Enc.h"
#include "OMX_Core.h"

/* #define LOG_NDEBUG 0 */
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ExynosVideoEncoder"

#define MAX_INPUTBUFFER_COUNT 32
#define MAX_OUTPUTBUFFER_COUNT 32

static struct {
    int eColorFormat;
    int nFormatFlag;
} SBWC_LOSSY_TABLE[] = {
    /* 8Bit multi plane format */
    {VIDEO_COLORFORMAT_NV12M_SBWC_L50    , 0},
    {VIDEO_COLORFORMAT_NV12M_SBWC_L75    , 2},
    /* 10Bit multi plane format */
    {VIDEO_COLORFORMAT_NV12M_10B_SBWC_L60, 1},
    {VIDEO_COLORFORMAT_NV12M_10B_SBWC_L80, 3},
    /* 8Bit single plane format */
    {VIDEO_COLORFORMAT_NV12_SBWC_L50    , 0},
    {VIDEO_COLORFORMAT_NV12_10B_SBWC_L60, 2},
    /* 10Bit single plane format */
    {VIDEO_COLORFORMAT_NV12_10B_SBWC_L60, 1},
    {VIDEO_COLORFORMAT_NV12_10B_SBWC_L80, 3},
};

static struct {
    int eSBWCFormat;
    int eNormalFormat;
} SBWC_NONCOMP_TABLE[] = {
    {VIDEO_COLORFORMAT_NV12_SBWC,      VIDEO_COLORFORMAT_NV12},
    {VIDEO_COLORFORMAT_NV12M_SBWC,     VIDEO_COLORFORMAT_NV12M},
    {VIDEO_COLORFORMAT_NV12_10B_SBWC,  VIDEO_COLORFORMAT_NV12_S10B},
    {VIDEO_COLORFORMAT_NV12M_10B_SBWC, VIDEO_COLORFORMAT_NV12M_P010},
    {VIDEO_COLORFORMAT_NV21M_SBWC,     VIDEO_COLORFORMAT_NV21M},
    {VIDEO_COLORFORMAT_NV21M_10B_SBWC, VIDEO_COLORFORMAT_NV21M_P010},
};

/*
 * [Common] __Set_SupportFormat
 */
static void __Set_SupportFormat(ExynosVideoInstInfo *pVideoInstInfo)
{
    int nLastIndex = 0;

    if (pVideoInstInfo == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    memset(pVideoInstInfo->supportFormat, (int)VIDEO_COLORFORMAT_UNKNOWN, sizeof(pVideoInstInfo->supportFormat));

    pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12;
    pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M;
    pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M;

    switch ((int)pVideoInstInfo->HwVersion) {
    case MFC_1501:
    case MFC_150:
    case MFC_1410: /* NV12, NV21, I420, YV12, BGRA, RGBA, ARGB, NV12_S10B, NV21_S10B, SBWC(NV12, NV21, NV12_10B, NV21_10B), SBWC Lossy */
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_BGRA8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_RGBA8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_ARGB8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_SBWC;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_SBWC;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_SBWC;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_SBWC_L50;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_SBWC_L75;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_SBWC_L50;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_SBWC_L75;

        if ((pVideoInstInfo->eCodecType == VIDEO_CODING_HEVC) ||
                (pVideoInstInfo->eCodecType == VIDEO_CODING_VP9)) {
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_P010;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_P010;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_10B_SBWC;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_10B_SBWC;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_10B_SBWC;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_10B_SBWC_L40;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_10B_SBWC_L60;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_10B_SBWC_L80;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_10B_SBWC_L40;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_10B_SBWC_L60;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_10B_SBWC_L80;
        }
        break;
    case MFC_140: /* NV12, NV21, I420, YV12, BGRA, RGBA, ARGB, NV12_S10B, NV21_S10B, SBWC(NV12, NV21, NV12_10B, NV21_10B) */
    case MFC_1400:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_BGRA8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_RGBA8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_ARGB8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_SBWC;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_SBWC;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_SBWC;

        if ((pVideoInstInfo->eCodecType == VIDEO_CODING_HEVC) ||
                (pVideoInstInfo->eCodecType == VIDEO_CODING_VP9)) {
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_P010;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_P010;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_10B_SBWC;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_10B_SBWC;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_10B_SBWC;
        }
        break;
    case MFC_130: /* NV12, NV21, I420, YV12, NV12_S10B, NV21_S10B */
    case MFC_120:
    case MFC_1220:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;

        if ((pVideoInstInfo->eCodecType == VIDEO_CODING_HEVC) ||
            (pVideoInstInfo->eCodecType == VIDEO_CODING_VP9)) {
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_P010;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_S10B;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M_P010;
        }
        break;
    case MFC_110:  /* NV12, NV21, I420, YV12 */
    case MFC_1120:
    case MFC_101:
    case MFC_100:
    case MFC_1010:
    case MFC_1011:
    case MFC_1020:
    case MFC_1021:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        break;
    case MFC_90:  /* NV12, NV21, BGRA, RGBA, I420, YV12, ARGB */
    case MFC_80:
#ifdef USE_HEVC_HWIP
    case HEVC_10:
#endif
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_BGRA8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_RGBA8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_ARGB8888;
        break;
    case MFC_723:  /* NV12, NV21, BGRA, RGBA, I420, YV12, ARGB, NV12T */
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_BGRA8888;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_RGBA8888;
    case MFC_72:  /* NV12, NV21, I420, YV12, ARGB, NV12T */
    case MFC_77:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_ARGB8888;
    case MFC_78:  /* NV12, NV21, NV12T */
    case MFC_65:
    case MFC_61:
    case MFC_51:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12_TILED;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M_TILED;
        break;
    default:
        break;
    }

EXIT:
    return;
}

/*
 * [Encoder OPS] Init
 */
static void *MFC_Encoder_Init(ExynosVideoInstInfo *pVideoInfo)
{
    CodecOSALVideoContext *pCtx     = NULL;
    pthread_mutex_t       *pMutex   = NULL;

    int ret = 0;
    int hIonClient = -1;
    int fd = -1;

    if (pVideoInfo == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT_ALLOC_FAIL;
    }

    pCtx = (CodecOSALVideoContext *)malloc(sizeof(*pCtx));
    if (pCtx == NULL) {
        ALOGE("%s: Failed to allocate encoder context buffer", __FUNCTION__);
        goto EXIT_ALLOC_FAIL;
    }

    memset(pCtx, 0, sizeof(*pCtx));

    pCtx->videoCtx.hDevice = -1;
    pCtx->videoCtx.hIONHandle = -1;
    pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD = -1;
    pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD = -1;
    pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD = -1;

    /* node open */
#ifdef USE_HEVC_HWIP
    if (pVideoInfo->eCodecType == VIDEO_CODING_HEVC) {
        if (pVideoInfo->eSecurityType == VIDEO_SECURE)
            ret = Codec_OSAL_DevOpen(VIDEO_HEVC_SECURE_ENCODER_NAME, O_RDWR, pCtx);
        else
            ret = Codec_OSAL_DevOpen(VIDEO_HEVC_ENCODER_NAME, O_RDWR, pCtx);
    } else
#endif
    {
        if (pVideoInfo->bOTFMode == VIDEO_TRUE) {
            if (pVideoInfo->eSecurityType == VIDEO_SECURE)
                ret = Codec_OSAL_DevOpen(VIDEO_MFC_OTF_SECURE_ENCODER_NAME, O_RDWR, pCtx);
            else
                ret = Codec_OSAL_DevOpen(VIDEO_MFC_OTF_ENCODER_NAME, O_RDWR, pCtx);
        } else {
            if (pVideoInfo->eSecurityType == VIDEO_SECURE)
                ret = Codec_OSAL_DevOpen(VIDEO_MFC_SECURE_ENCODER_NAME, O_RDWR, pCtx);
            else
                ret = Codec_OSAL_DevOpen(VIDEO_MFC_ENCODER_NAME, O_RDWR, pCtx);
        }
    }

    if (ret < 0) {
        ALOGE("%s: Failed to open encoder device", __FUNCTION__);
        goto EXIT_OPEN_FAIL;
    }

    memcpy(&pCtx->videoCtx.instInfo, pVideoInfo, sizeof(pCtx->videoCtx.instInfo));

    ALOGV("%s: MFC version is %x", __FUNCTION__, pCtx->videoCtx.instInfo.HwVersion);

    if (Codec_OSAL_QueryCap(pCtx) != 0) {
        ALOGE("%s: Failed to QueryCap", __FUNCTION__);
        goto EXIT_QUERYCAP_FAIL;
    }

    pCtx->videoCtx.bStreamonInbuf  = VIDEO_FALSE;
    pCtx->videoCtx.bStreamonOutbuf = VIDEO_FALSE;

    /* mutex for input */
    pMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (pMutex == NULL) {
        ALOGE("%s: Failed to allocate mutex about input buffer", __FUNCTION__);
        goto EXIT_QUERYCAP_FAIL;
    }
    if (pthread_mutex_init(pMutex, NULL) != 0) {
        free(pMutex);
        goto EXIT_QUERYCAP_FAIL;
    }
    pCtx->videoCtx.pInMutex = (void*)pMutex;

    /* mutex for output */
    pMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (pMutex == NULL) {
        ALOGE("%s: Failed to allocate mutex about output buffer", __FUNCTION__);
        goto EXIT_QUERYCAP_FAIL;
    }
    if (pthread_mutex_init(pMutex, NULL) != 0) {
        free(pMutex);
        goto EXIT_QUERYCAP_FAIL;
    }
    pCtx->videoCtx.pOutMutex = (void*)pMutex;

    /* for shared memory : temporal svc info, ROI info */
    hIonClient = exynos_ion_open();

    if (hIonClient < 0) {
        ALOGE("%s: Failed to create ion_client", __FUNCTION__);
        pCtx->videoCtx.hIONHandle = -1;
        goto EXIT_QUERYCAP_FAIL;
    }

    pCtx->videoCtx.hIONHandle = hIonClient;

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_TRUE) {
        fd = exynos_ion_alloc(pCtx->videoCtx.hIONHandle, sizeof(TemporalLayerShareBuffer),
                              EXYNOS_ION_HEAP_SYSTEM_MASK, ION_FLAG_CACHED);
        if (fd < 0) {
            ALOGE("%s: Failed to exynos_ion_alloc() for nTemporalLayerShareBufferFD", __FUNCTION__);
            pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD = -1;
            goto EXIT_QUERYCAP_FAIL;
        }
        pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD = fd;
        pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr = Codec_OSAL_MemoryMap(NULL, sizeof(TemporalLayerShareBuffer),
                                                   PROT_READ | PROT_WRITE, MAP_SHARED,  pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD, 0);
        if (pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr == MAP_FAILED) {
            ALOGE("%s: Failed to mmap for nTemporalLayerShareBufferFD", __FUNCTION__);
            goto EXIT_QUERYCAP_FAIL;
        }

        memset(pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr, 0, sizeof(TemporalLayerShareBuffer));
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bRoiInfoSupport == VIDEO_TRUE) {
        fd = exynos_ion_alloc(pCtx->videoCtx.hIONHandle, sizeof(RoiInfoShareBuffer),
                              EXYNOS_ION_HEAP_SYSTEM_MASK, ION_FLAG_CACHED);
        if (fd < 0) {
            ALOGE("%s: Failed to exynos_ion_alloc() for nRoiShareBufferFD", __FUNCTION__);
            pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD = -1;
            goto EXIT_QUERYCAP_FAIL;
        }
        pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD = fd;
        pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr = Codec_OSAL_MemoryMap(NULL, sizeof(RoiInfoShareBuffer),
                                                   PROT_READ | PROT_WRITE, MAP_SHARED,  pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD, 0);
        if (pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr == MAP_FAILED) {
            ALOGE("%s: Failed to mmap for nRoiShareBufferFD", __FUNCTION__);
            goto EXIT_QUERYCAP_FAIL;
        }

        memset(pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr, 0, sizeof(RoiInfoShareBuffer));
    }

    /* for HDR Dynamic Info */
    if (pCtx->videoCtx.instInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        fd = exynos_ion_alloc(pCtx->videoCtx.hIONHandle, sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM,
                              EXYNOS_ION_HEAP_SYSTEM_MASK, ION_FLAG_CACHED);
        if (fd < 0) {
            ALOGE("%s: Failed to exynos_ion_alloc() for nHDRInfoShareBufferFD", __FUNCTION__);
            pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD = -1;
            goto EXIT_QUERYCAP_FAIL;
        }

        pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD = fd;

        pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr =
                                        Codec_OSAL_MemoryMap(NULL, (sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM),
                                              PROT_READ | PROT_WRITE, MAP_SHARED,
                                              pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD, 0);
        if (pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr == MAP_FAILED) {
            ALOGE("%s: Failed to mmap for pHDRInfoShareBufferAddr", __FUNCTION__);
            goto EXIT_QUERYCAP_FAIL;
        }

        memset(pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr,
                0, sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM);

        if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_SET_HDR_USER_SHARED_HANDLE,
                                    pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD) != 0) {
            ALOGE("[%s] Failed to Codec_OSAL_SetControl(CODEC_OSAL_CID_VIDEO_SET_HDR_USER_SHARED_HANDLE)", __FUNCTION__);
            goto EXIT_QUERYCAP_FAIL;
        }
    }

    if (pCtx->videoCtx.instInfo.bVideoBufFlagCtrl == VIDEO_TRUE) {
        pCtx->videoCtx.bVideoBufFlagCtrl = VIDEO_TRUE;
    }

    return (void *)pCtx;

EXIT_QUERYCAP_FAIL:
    if (pCtx->videoCtx.pInMutex != NULL) {
        pthread_mutex_destroy(pCtx->videoCtx.pInMutex);
        free(pCtx->videoCtx.pInMutex);
    }

    if (pCtx->videoCtx.pOutMutex != NULL) {
        pthread_mutex_destroy(pCtx->videoCtx.pOutMutex);
        free(pCtx->videoCtx.pOutMutex);
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr, sizeof(TemporalLayerShareBuffer));
            pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD);
            pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD = -1;
        }
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bRoiInfoSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr, sizeof(RoiInfoShareBuffer));
            pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD);
            pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD = -1;
        }
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr,
                    sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM);
            pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD);
            pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD = -1;
        }
    }

    /* free a ion_client */
    if (pCtx->videoCtx.hIONHandle >= 0) {
        exynos_ion_close(pCtx->videoCtx.hIONHandle);
        pCtx->videoCtx.hIONHandle = -1;
    }

    Codec_OSAL_DevClose(pCtx);

EXIT_OPEN_FAIL:
    free(pCtx);

EXIT_ALLOC_FAIL:
    return NULL;
}

/*
 * [Encoder OPS] Finalize
 */
static ExynosVideoErrorType MFC_Encoder_Finalize(void *pHandle)
{
    CodecOSALVideoContext *pCtx         = (CodecOSALVideoContext *)pHandle;
    ExynosVideoPlane      *pVideoPlane  = NULL;
    pthread_mutex_t       *pMutex       = NULL;
    ExynosVideoErrorType   ret          = VIDEO_ERROR_NONE;
    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bTemporalSvcSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr, sizeof(TemporalLayerShareBuffer));
            pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD);
            pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD = -1;
        }
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bRoiInfoSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr, sizeof(RoiInfoShareBuffer));
            pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD);
            pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD = -1;
        }
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr,
                    sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM);
            pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD);
            pCtx->videoCtx.specificInfo.enc.nHDRInfoShareBufferFD = -1;
        }
    }

    /* free a ion_client */
    if (pCtx->videoCtx.hIONHandle >= 0) {
        exynos_ion_close(pCtx->videoCtx.hIONHandle);
        pCtx->videoCtx.hIONHandle = -1;
    }

    if (pCtx->videoCtx.pOutMutex != NULL) {
        pMutex = (pthread_mutex_t*)pCtx->videoCtx.pOutMutex;
        pthread_mutex_destroy(pMutex);
        free(pMutex);
        pCtx->videoCtx.pOutMutex = NULL;
    }

    if (pCtx->videoCtx.pInMutex != NULL) {
        pMutex = (pthread_mutex_t*)pCtx->videoCtx.pInMutex;
        pthread_mutex_destroy(pMutex);
        free(pMutex);
        pCtx->videoCtx.pInMutex = NULL;
    }

    if (pCtx->videoCtx.bShareInbuf == VIDEO_FALSE) {
        for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
            for (j = 0; j < pCtx->videoCtx.nInbufPlanes; j++) {
                pVideoPlane = &pCtx->videoCtx.pInbuf[i].planes[j];
                if (pVideoPlane->addr != NULL) {
                    Codec_OSAL_MemoryUnmap(pVideoPlane->addr, pVideoPlane->allocSize);
                    pVideoPlane->addr = NULL;
                    pVideoPlane->allocSize = 0;
                    pVideoPlane->dataSize = 0;
                }

                pCtx->videoCtx.pInbuf[i].pGeometry = NULL;
                pCtx->videoCtx.pInbuf[i].bQueued = VIDEO_FALSE;
                pCtx->videoCtx.pInbuf[i].bRegistered = VIDEO_FALSE;
            }
        }
    }

    if (pCtx->videoCtx.bShareOutbuf == VIDEO_FALSE) {
        for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
            for (j = 0; j < pCtx->videoCtx.nOutbufPlanes; j++) {
                pVideoPlane = &pCtx->videoCtx.pOutbuf[i].planes[j];
                if (pVideoPlane->addr != NULL) {
                    Codec_OSAL_MemoryUnmap(pVideoPlane->addr, pVideoPlane->allocSize);
                    pVideoPlane->addr = NULL;
                    pVideoPlane->allocSize = 0;
                    pVideoPlane->dataSize = 0;
                }

                pCtx->videoCtx.pOutbuf[i].pGeometry = NULL;
                pCtx->videoCtx.pOutbuf[i].bQueued = VIDEO_FALSE;
                pCtx->videoCtx.pOutbuf[i].bRegistered = VIDEO_FALSE;
            }
        }
    }

    if (pCtx->videoCtx.pInbuf != NULL) {
        free(pCtx->videoCtx.pInbuf);
        pCtx->videoCtx.pInbuf = NULL;
    }

    if (pCtx->videoCtx.pOutbuf != NULL) {
        free(pCtx->videoCtx.pOutbuf);
        pCtx->videoCtx.pOutbuf = NULL;
    }

    Codec_OSAL_DevClose(pCtx);

    free(pCtx);

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Extended Control
 */
static ExynosVideoErrorType MFC_Encoder_Set_EncParam (
    void                *pHandle,
    ExynosVideoEncParam *pEncParam)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if ((pCtx == NULL) ||
        (pEncParam == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControls(pCtx, CODEC_OSAL_CID_ENC_SET_PARAMS, (void *)pEncParam) != 0) {
        ALOGE("%s: Failed to SetControls", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Frame Tag
 */
static ExynosVideoErrorType MFC_Encoder_Set_FrameTag(
    void   *pHandle,
    int     nFrameTag)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_FRAME_TAG, nFrameTag) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nFrameTag);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Get Frame Tag
 */
static int MFC_Encoder_Get_FrameTag(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;

    int nFrameTag = -1;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_VIDEO_FRAME_TAG, &nFrameTag) != 0) {
        ALOGE("%s: Failed to GetControl(val : 0x%x)", __FUNCTION__, nFrameTag);
        goto EXIT;
    }

EXIT:
    return nFrameTag;
}

/*
 * [Encoder OPS] Set Frame Type
 */
static ExynosVideoErrorType MFC_Encoder_Set_FrameType(
    void                    *pHandle,
    ExynosVideoFrameType     eFrameType)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_FRAME_TYPE, eFrameType) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, eFrameType);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Frame Rate
 */
static ExynosVideoErrorType MFC_Encoder_Set_FrameRate(
    void   *pHandle,
    int     nFramerate)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_FRAME_RATE, nFramerate) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nFramerate);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Bit Rate
 */
static ExynosVideoErrorType MFC_Encoder_Set_BitRate(
    void   *pHandle,
    int     nBitrate)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_BIT_RATE, nBitrate) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nBitrate);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Quantization Min/Max
 */
static ExynosVideoErrorType MFC_Encoder_Set_QuantizationRange(
    void                *pHandle,
    ExynosVideoQPRange   qpRange)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (qpRange.QpMin_I > qpRange.QpMax_I) {
        ALOGE("%s: QP(I) range(%d, %d) is wrong", __FUNCTION__, qpRange.QpMin_I, qpRange.QpMax_I);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControls(pCtx, CODEC_OSAL_CID_ENC_QP_RANGE, &qpRange) != 0) {
        ALOGE("%s: Failed to SetControl", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Frame Skip
 */
static ExynosVideoErrorType MFC_Encoder_Set_FrameSkip(
    void   *pHandle,
    int     nFrameSkip)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_FRAME_SKIP_MODE, nFrameSkip) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nFrameSkip);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set IDR Period
 */
static ExynosVideoErrorType MFC_Encoder_Set_IDRPeriod(
    void   *pHandle,
    int     nIDRPeriod)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_IDR_PERIOD, (nIDRPeriod < 0)? 0:nIDRPeriod) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nIDRPeriod);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Slice Mode
 */
static ExynosVideoErrorType MFC_Encoder_Set_SliceMode(
    void *pHandle,
    int   nSliceMode,
    int   nSliceArgument)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int value;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    value = (((nSliceMode << 16) & 0xffff0000) | (nSliceArgument & 0xffff));

#if 0  // TODO
    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_SLICE_MODE, value) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, value);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }
#else
    ALOGW("%s: not implemented yet", __FUNCTION__);
#endif

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Enable Prepend SPS and PPS to every IDR Frames
 */
static ExynosVideoErrorType MFC_Encoder_Enable_PrependSpsPpsToIdr(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    switch ((int)pCtx->videoCtx.instInfo.eCodecType) {
    case VIDEO_CODING_AVC:
        if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_H264_PREPEND_SPSPPS_TO_IDR, 1) != 0) {
            ALOGE("%s: Failed to SetControl", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }
        break;
    case VIDEO_CODING_HEVC:
        if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_HEVC_PREPEND_SPSPPS_TO_IDR, 1) != 0) {
            ALOGE("%s: Failed to SetControl", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }
        break;
    default:
        ALOGE("%s: codec(%x) can't support PrependSpsPpsToIdr", __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Qos Ratio
 */
static ExynosVideoErrorType MFC_Encoder_Set_QosRatio(
    void *pHandle,
    int   nRatio)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_QOS_RATIO, nRatio) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nRatio);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Layer Change
 */
static ExynosVideoErrorType MFC_Encoder_Set_LayerChange(
    void                        *pHandle,
    TemporalLayerShareBuffer     TemporalSVC)
{
    CodecOSALVideoContext      *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType        ret   = VIDEO_ERROR_NONE;
    TemporalLayerShareBuffer   *pTLSB = NULL;

    unsigned int CID = 0;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    pTLSB = (TemporalLayerShareBuffer *)pCtx->videoCtx.specificInfo.enc.pTemporalLayerShareBufferAddr;

    switch ((int)pCtx->videoCtx.instInfo.eCodecType) {
    case VIDEO_CODING_AVC:
        CID = CODEC_OSAL_CID_ENC_H264_TEMPORAL_SVC_LAYER_CH;
        break;
    case VIDEO_CODING_HEVC:
        CID = CODEC_OSAL_CID_ENC_HEVC_TEMPORAL_SVC_LAYER_CH;
        break;
    case VIDEO_CODING_VP8:
        CID = CODEC_OSAL_CID_ENC_VP8_TEMPORAL_SVC_LAYER_CH;
        break;
    case VIDEO_CODING_VP9:
        CID = CODEC_OSAL_CID_ENC_VP9_TEMPORAL_SVC_LAYER_CH;
        break;
    default:
        ALOGE("%s: this codec type is not supported(%x), F/W ver(%x)",
                __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType, pCtx->videoCtx.instInfo.HwVersion);
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
        break;
    }

    memcpy(pTLSB, &TemporalSVC, sizeof(TemporalLayerShareBuffer));
    if (Codec_OSAL_SetControl(pCtx, CID,
                                pCtx->videoCtx.specificInfo.enc.nTemporalLayerShareBufferFD) != 0) {
        ALOGE("%s: Failed to SetControl", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Dynamic QP Control
 */
static ExynosVideoErrorType MFC_Encoder_Set_DynamicQpControl(
    void    *pHandle,
    int      nQp)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_SET_CONFIG_QP, nQp) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nQp);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Mark LTR-frame
 */
static ExynosVideoErrorType MFC_Encoder_Set_MarkLTRFrame(
    void    *pHandle,
    int      nLongTermFrmIdx)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_H264_LTR_MARK_INDEX, nLongTermFrmIdx) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nLongTermFrmIdx);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Used LTR-frame
 */
static ExynosVideoErrorType MFC_Encoder_Set_UsedLTRFrame(
    void    *pHandle,
    int      nUsedLTRFrameNum)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_H264_LTR_USE_INDEX, nUsedLTRFrameNum) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nUsedLTRFrameNum);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Base PID
 */
static ExynosVideoErrorType MFC_Encoder_Set_BasePID(
    void    *pHandle,
    int      nPID)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_H264_LTR_BASE_PID, nPID) != 0) {
        ALOGE("%s: Failed to SetControl(val : 0x%x)", __FUNCTION__, nPID);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Roi Information
 */
static ExynosVideoErrorType MFC_Encoder_Set_RoiInfo(
    void                   *pHandle,
    RoiInfoShareBuffer     *pRoiInfo)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;
    RoiInfoShareBuffer      *pRISB = NULL;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    pRISB = (RoiInfoShareBuffer *)pCtx->videoCtx.specificInfo.enc.pRoiShareBufferAddr;
    if (pCtx->videoCtx.instInfo.supportInfo.enc.bRoiInfoSupport == VIDEO_FALSE) {
        ALOGE("%s: ROI Info setting is not supported :: codec type(%x), F/W ver(%x)",
                    __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType, pCtx->videoCtx.instInfo.HwVersion);
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
    }

    memcpy(pRISB, pRoiInfo, sizeof(RoiInfoShareBuffer));
    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_ROI_INFO,
                                pCtx->videoCtx.specificInfo.enc.nRoiShareBufferFD) != 0) {
        ALOGE("%s: Failed to SetControl", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Enable Weighted Prediction Encoding
 */
static ExynosVideoErrorType MFC_Encoder_Enable_WeightedPrediction(void *pHandle)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_WP_ENABLE, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Y-SUM Data
 */
static ExynosVideoErrorType MFC_Encoder_Set_YSumData(
    void           *pHandle,
    unsigned int    nHighData,
    unsigned int    nLowData)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    unsigned long long nYSumData = 0;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    nYSumData = (((unsigned long long)nHighData) << 32) | nLowData;  /* 64bit data */

    /* MFC D/D just want a LowData */
    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_YSUM_DATA, (int)nLowData) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set I-frame ratio (It is applied only CBR mode)
 */
static ExynosVideoErrorType MFC_Encoder_Set_IFrameRatio(
    void           *pHandle,
    int             nRatio)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    switch ((int)pCtx->videoCtx.instInfo.eCodecType) {
    case VIDEO_CODING_AVC:
    case VIDEO_CODING_HEVC:
        if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_I_FRAME_RATIO, nRatio) != 0) {
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }

        ret = VIDEO_ERROR_NONE;
        break;
    default:
        ALOGE("%s: codec(%x) can't support Set_IFrameRatio", __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Color Aspects
 */
ExynosVideoErrorType MFC_Encoder_Set_ColorAspects(
    void                    *pHandle,
    ExynosVideoColorAspects *pColorAspects)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if ((pCtx == NULL) ||
        (pColorAspects == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bColorAspectsSupport == VIDEO_FALSE) {
        ALOGE("%s: can't support Set_ColorAspects. // codec type(%x), F/W ver(%x)",
                    __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType, pCtx->videoCtx.instInfo.HwVersion);
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
    }

    if (Codec_OSAL_SetControls(pCtx, CODEC_OSAL_CID_ENC_COLOR_ASPECTS, (void *)pColorAspects) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memcpy(&(pCtx->videoCtx.inbufGeometry.HdrInfo.sColorAspects), pColorAspects, sizeof(ExynosVideoColorAspects));
    ret = VIDEO_ERROR_NONE;

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Enable Adaptive Layer Bitrate mode
 */
static ExynosVideoErrorType MFC_Encoder_Enable_AdaptiveLayerBitrate(
        void    *pHandle,
        int      bEnable)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    switch ((int)pCtx->videoCtx.instInfo.eCodecType) {
    case VIDEO_CODING_AVC:
    case VIDEO_CODING_HEVC:
    case VIDEO_CODING_VP8:
    case VIDEO_CODING_VP9:
        if (pCtx->videoCtx.instInfo.supportInfo.enc.bAdaptiveLayerBitrateSupport == VIDEO_TRUE) {
            if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_ENABLE_ADAPTIVE_LAYER_BITRATE, bEnable) != 0) {
                ret = VIDEO_ERROR_APIFAIL;
                goto EXIT;
            }
        }

        ret = VIDEO_ERROR_NONE;
        break;
    default:
        ALOGE("%s: codec(%x) can't support Enable_AdaptiveLayerBitrate", __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set HDR Static Info
 */
ExynosVideoErrorType MFC_Encoder_Set_HDRStaticInfo(
    void                  *pHandle,
    ExynosVideoHdrStatic  *pHDRStaticInfo)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if ((pCtx == NULL) ||
        (pHDRStaticInfo == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bHDRStaticInfoSupport == VIDEO_FALSE) {
        ALOGE("%s: can't support Set_HDRStaticInfo. // codec type(%x), F/W ver(%x)",
                    __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType, pCtx->videoCtx.instInfo.HwVersion);
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
    }

    switch ((int)pCtx->videoCtx.instInfo.eCodecType) {
    case VIDEO_CODING_HEVC:
        if (Codec_OSAL_SetControls(pCtx, CODEC_OSAL_CID_ENC_HDR_STATIC_INFO, (void *)pHDRStaticInfo) != 0) {
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }

        memcpy(&(pCtx->videoCtx.inbufGeometry.HdrInfo.sHdrStatic), pHDRStaticInfo, sizeof(ExynosVideoHdrStatic));
        ret = VIDEO_ERROR_NONE;
        break;
    default:
        ALOGE("%s: codec(%x) can't support Set_HDRStaticInfo", __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set HDR Dynamic Info
 */
ExynosVideoErrorType MFC_Encoder_Set_HDRDynamicInfo(
    void                  *pHandle,
    ExynosVideoHdrDynamic *pHDRDynamicInfo)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if ((pCtx == NULL) ||
        (pHDRDynamicInfo == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_FALSE) {
        ALOGE("%s: can't support Set_HDRStaticInfo. // codec type(%x), F/W ver(%x)",
                    __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType, pCtx->videoCtx.instInfo.HwVersion);
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
    }

    memcpy(&(pCtx->videoCtx.inbufGeometry.HdrInfo.sHdrDynamic), pHDRDynamicInfo, sizeof(ExynosVideoHdrDynamic));

EXIT:
    return ret;
}


/*
 * [Encoder OPS] Set Header Mode
 */
ExynosVideoErrorType MFC_Encoder_Set_HeaderMode(
    void                  *pHandle,
    ExynosVideoBoolType    bSeparated)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_HEADER_MODE,
            ((bSeparated == VIDEO_TRUE)? CODEC_OSAL_HEADER_MODE_SEPARATE:CODEC_OSAL_HEADER_WITH_1ST_IDR)) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Drop Control
 */
ExynosVideoErrorType MFC_Encoder_Set_DropControl(
        void                  *pHandle,
        ExynosVideoBoolType    bEnable)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bDropControlSupport == VIDEO_FALSE) {
        ALOGE("%s: can't support Set_DropControl. // codec type(%x), F/W ver(%x)",
                __FUNCTION__, pCtx->videoCtx.instInfo.eCodecType, pCtx->videoCtx.instInfo.HwVersion);
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_ENABLE_DROP_CTRL, (bEnable == VIDEO_TRUE)? 1:0) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Disable DFR
 */
ExynosVideoErrorType MFC_Encoder_Disable_DynamicFrameRate(
        void                  *pHandle,
        ExynosVideoBoolType    bEnable)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    pCtx->videoCtx.instInfo.bDisableDFR = bEnable;

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set ActualFormat
 */
ExynosVideoErrorType MFC_Encoder_Set_ActualFormat(
        void                  *pHandle,
        int                    nFormat)
{
    CodecOSALVideoContext   *pCtx  = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret   = VIDEO_ERROR_NONE;

    int i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < (int)(sizeof(SBWC_NONCOMP_TABLE)/sizeof(SBWC_NONCOMP_TABLE[0])); i++) {
        if (SBWC_NONCOMP_TABLE[i].eSBWCFormat == pCtx->videoCtx.inbufGeometry.eColorFormat) {
            if (SBWC_NONCOMP_TABLE[i].eNormalFormat == nFormat) {
                pCtx->videoCtx.specificInfo.enc.actualFormat = (ExynosVideoColorFormatType)nFormat;
                ALOGV("%s: actual format is 0x%x", __FUNCTION__, pCtx->videoCtx.specificInfo.enc.actualFormat);
                goto EXIT;
            }
        }
    }

    /* start with non SBWC, discard nFormat */
    //ret = VIDEO_ERROR_BADPARAM;

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Operating Rate
 */
static ExynosVideoErrorType MFC_Encoder_Set_OperatingRate(
    void        *pHandle,
    unsigned int framerate)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bOperatingRateSupport != VIDEO_TRUE) {
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ACTUAL_FRAMERATE, framerate) != 0) {
        ALOGE("%s: Failed to set operating rate", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Set Priority
 */
static ExynosVideoErrorType MFC_Encoder_Set_Priority(
    void        *pHandle,
    unsigned int priority)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.enc.bPrioritySupport != VIDEO_TRUE) {
        ret = VIDEO_ERROR_NOSUPPORT;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_PRIOTIY, priority) != 0) {
        ALOGE("%s: Failed to set priority", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Enable Cacheable (Input)
 */
static ExynosVideoErrorType MFC_Encoder_Enable_Cacheable_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_CACHEABLE_BUFFER, 2) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Enable Cacheable (Output)
 */
static ExynosVideoErrorType MFC_Encoder_Enable_Cacheable_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_CACHEABLE_BUFFER, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Set Shareable Buffer (Input)
 */
static ExynosVideoErrorType MFC_Encoder_Set_Shareable_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    pCtx->videoCtx.bShareInbuf = VIDEO_TRUE;

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Set Shareable Buffer (Output)
 */
static ExynosVideoErrorType MFC_Encoder_Set_Shareable_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    pCtx->videoCtx.bShareOutbuf = VIDEO_TRUE;

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Get Buffer (Input)
 */
static ExynosVideoErrorType MFC_Encoder_Get_Buffer_Inbuf(
    void               *pHandle,
    int                 nIndex,
    ExynosVideoBuffer **pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        *pBuffer = NULL;
        ret = VIDEO_ERROR_NOBUFFERS;
        goto EXIT;
    }

    if ((nIndex < 0) ||
        (pCtx->videoCtx.nInbufs <= nIndex)) {
        *pBuffer = NULL;
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    *pBuffer = (ExynosVideoBuffer *)&pCtx->videoCtx.pInbuf[nIndex];

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Get Buffer (Output)
 */
static ExynosVideoErrorType MFC_Encoder_Get_Buffer_Outbuf(
    void               *pHandle,
    int                 nIndex,
    ExynosVideoBuffer **pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        *pBuffer = NULL;
        ret = VIDEO_ERROR_NOBUFFERS;
        goto EXIT;
    }

    if ((nIndex < 0) ||
        (pCtx->videoCtx.nOutbufs <= nIndex)) {
        *pBuffer = NULL;
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    *pBuffer = (ExynosVideoBuffer *)&pCtx->videoCtx.pOutbuf[nIndex];

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Set Geometry (Src)
 */
static ExynosVideoErrorType MFC_Encoder_Set_Geometry_Inbuf(
    void                *pHandle,
    ExynosVideoGeometry *pBufferConf)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_Format fmt;
    int i;

    if ((pCtx == NULL) ||
        (pBufferConf == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&fmt, 0, sizeof(fmt));

    fmt.type   = CODEC_OSAL_BUF_TYPE_SRC;
    fmt.format = Codec_OSAL_ColorFormatToPixelFormat(pBufferConf->eColorFormat, pCtx->videoCtx.instInfo.HwVersion);
    fmt.width  = pBufferConf->nFrameWidth;
    fmt.height = pBufferConf->nFrameHeight;
    fmt.stride = pBufferConf->nStride;
    fmt.nPlane = pBufferConf->nPlaneCnt;

    for (i = 0; i < (int)(sizeof(SBWC_LOSSY_TABLE)/sizeof(SBWC_LOSSY_TABLE[0])); i++) {
        if (pBufferConf->eColorFormat == SBWC_LOSSY_TABLE[i].eColorFormat)
            fmt.field |= (1 << SBWC_LOSSY_TABLE[i].nFormatFlag);
    }

    if (Codec_OSAL_SetFormat(pCtx, &fmt) != 0) {
        ALOGE("%s: Failed to SetFormat", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memcpy(&pCtx->videoCtx.inbufGeometry, pBufferConf, sizeof(pCtx->videoCtx.inbufGeometry));
    pCtx->videoCtx.nInbufPlanes = pBufferConf->nPlaneCnt;

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Get Geometry (Src)
 */
static ExynosVideoErrorType MFC_Encoder_Get_Geometry_Inbuf(
    void                *pHandle,
    ExynosVideoGeometry *pBufferConf)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_Format fmt;

    if ((pCtx == NULL) ||
        (pBufferConf == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memcpy(pBufferConf, &pCtx->videoCtx.inbufGeometry, sizeof(pCtx->videoCtx.inbufGeometry));

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = CODEC_OSAL_BUF_TYPE_SRC;

    if (Codec_OSAL_GetFormat(pCtx, &fmt) != 0) {
        ALOGE("%s: Failed to GetFormat", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pBufferConf->nFrameWidth    = fmt.width;
    pBufferConf->nFrameHeight   = fmt.height;
    pBufferConf->nSizeImage     = fmt.planeSize[0];

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Set Geometry (Dst)
 */
static ExynosVideoErrorType MFC_Encoder_Set_Geometry_Outbuf(
    void                *pHandle,
    ExynosVideoGeometry *pBufferConf)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_Format fmt;

    if ((pCtx == NULL) ||
        (pBufferConf == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type            = CODEC_OSAL_BUF_TYPE_DST;
    fmt.format          = Codec_OSAL_CodingTypeToCompressdFormat(pBufferConf->eCompressionFormat);
    fmt.planeSize[0]    = pBufferConf->nSizeImage;
    fmt.nPlane          = pBufferConf->nPlaneCnt;

    if (Codec_OSAL_SetFormat(pCtx, &fmt) != 0) {
        ALOGE("%s: Failed to SetFormat", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memcpy(&pCtx->videoCtx.outbufGeometry, pBufferConf, sizeof(pCtx->videoCtx.outbufGeometry));
    pCtx->videoCtx.nOutbufPlanes = pBufferConf->nPlaneCnt;

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Get Geometry (Dst)
 */
static ExynosVideoErrorType MFC_Encoder_Get_Geometry_Outbuf(
    void                *pHandle,
    ExynosVideoGeometry *pBufferConf)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_Format fmt;

    if ((pCtx == NULL) ||
        (pBufferConf == NULL)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = CODEC_OSAL_BUF_TYPE_DST;

    if (Codec_OSAL_GetFormat(pCtx, &fmt) != 0) {
        ALOGE("%s: Failed to GetFormat", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pBufferConf->nSizeImage = fmt.planeSize[0];

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Setup (Src)
 */
static ExynosVideoErrorType MFC_Encoder_Setup_Inbuf(
    void           *pHandle,
    unsigned int    nBufferCount)
{
    CodecOSALVideoContext *pCtx         = (CodecOSALVideoContext *)pHandle;
    ExynosVideoPlane      *pVideoPlane  = NULL;
    ExynosVideoErrorType   ret          = VIDEO_ERROR_NONE;

    CodecOSAL_ReqBuf reqBuf;
    CodecOSAL_Buffer buf;

    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (nBufferCount == 0) {
        nBufferCount = MAX_INPUTBUFFER_COUNT;
        ALOGV("%s: Change buffer count %d", __FUNCTION__, nBufferCount);
    }

    memset(&reqBuf, 0, sizeof(reqBuf));

    reqBuf.type = CODEC_OSAL_BUF_TYPE_SRC;
    reqBuf.count = nBufferCount;
    if (pCtx->videoCtx.bShareInbuf == VIDEO_TRUE)
        reqBuf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    else
        reqBuf.memory = CODEC_OSAL_MEM_TYPE_MMAP;

    if (Codec_OSAL_RequestBuf(pCtx, &reqBuf) != 0) {
        ALOGE("Failed to RequestBuf");
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pCtx->videoCtx.nInbufs = (int)reqBuf.count;

    pCtx->videoCtx.pInbuf = malloc(sizeof(*pCtx->videoCtx.pInbuf) * pCtx->videoCtx.nInbufs);
    if (pCtx->videoCtx.pInbuf == NULL) {
        ALOGE("%s: Failed to allocate input buffer context", __FUNCTION__);
        ret = VIDEO_ERROR_NOMEM;
        goto EXIT;
    }
    memset(pCtx->videoCtx.pInbuf, 0, sizeof(*pCtx->videoCtx.pInbuf) * pCtx->videoCtx.nInbufs);

    memset(&buf, 0, sizeof(buf));

    if (pCtx->videoCtx.bShareInbuf == VIDEO_FALSE) {
        buf.type    = CODEC_OSAL_BUF_TYPE_SRC;
        buf.memory  = CODEC_OSAL_MEM_TYPE_MMAP;
        buf.nPlane  = pCtx->videoCtx.nInbufPlanes;

        for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
            buf.index = i;

            if (Codec_OSAL_QueryBuf(pCtx, &buf) != 0) {
                ALOGE("%s: Failed to QueryBuf", __FUNCTION__);
                ret = VIDEO_ERROR_APIFAIL;
                goto EXIT;
            }

            for (j = 0; j < pCtx->videoCtx.nInbufPlanes; j++) {
                pVideoPlane = &pCtx->videoCtx.pInbuf[i].planes[j];

                pVideoPlane->addr = Codec_OSAL_MemoryMap(NULL,
                        buf.planes[j].bufferSize, PROT_READ | PROT_WRITE,
                        MAP_SHARED, (unsigned long)buf.planes[j].addr, buf.planes[j].offset);
                if (pVideoPlane->addr == MAP_FAILED) {
                    ALOGE("%s: Failed to map", __FUNCTION__);
                    ret = VIDEO_ERROR_MAPFAIL;
                    goto EXIT;
                }

                pVideoPlane->allocSize = buf.planes[j].bufferSize;
                pVideoPlane->dataSize = 0;
            }

            pCtx->videoCtx.pInbuf[i].pGeometry = &pCtx->videoCtx.inbufGeometry;
            pCtx->videoCtx.pInbuf[i].bQueued = VIDEO_FALSE;
            pCtx->videoCtx.pInbuf[i].bRegistered = VIDEO_TRUE;

        }
    } else {
        for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
            pCtx->videoCtx.pInbuf[i].pGeometry = &pCtx->videoCtx.inbufGeometry;
            pCtx->videoCtx.pInbuf[i].bQueued = VIDEO_FALSE;
            pCtx->videoCtx.pInbuf[i].bRegistered = VIDEO_FALSE;
        }
    }

    return ret;

EXIT:
    if ((pCtx != NULL) && (pCtx->videoCtx.pInbuf != NULL)) {
        if (pCtx->videoCtx.bShareInbuf == VIDEO_FALSE) {
            for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
                for (j = 0; j < pCtx->videoCtx.nInbufPlanes; j++) {
                    pVideoPlane = &pCtx->videoCtx.pInbuf[i].planes[j];

                    if (pVideoPlane->addr == MAP_FAILED) {
                        pVideoPlane->addr = NULL;
                        break;
                    }

                    Codec_OSAL_MemoryUnmap(pVideoPlane->addr, pVideoPlane->allocSize);
                }
            }
        }

        free(pCtx->videoCtx.pInbuf);
        pCtx->videoCtx.pInbuf = NULL;
    }

    return ret;
}

/*
 * [Encoder Buffer OPS] Setup (Dst)
 */
static ExynosVideoErrorType MFC_Encoder_Setup_Outbuf(
    void           *pHandle,
    unsigned int    nBufferCount)
{
    CodecOSALVideoContext *pCtx         = (CodecOSALVideoContext *)pHandle;
    ExynosVideoPlane      *pVideoPlane  = NULL;
    ExynosVideoErrorType   ret          = VIDEO_ERROR_NONE;

    CodecOSAL_ReqBuf reqBuf;
    CodecOSAL_Buffer buf;

    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (nBufferCount == 0) {
        nBufferCount = MAX_OUTPUTBUFFER_COUNT;
        ALOGV("%s: Change buffer count %d", __FUNCTION__, nBufferCount);
    }

    memset(&reqBuf, 0, sizeof(reqBuf));

    reqBuf.type = CODEC_OSAL_BUF_TYPE_DST;
    reqBuf.count = nBufferCount;
    if (pCtx->videoCtx.bShareOutbuf == VIDEO_TRUE)
        reqBuf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    else
        reqBuf.memory = CODEC_OSAL_MEM_TYPE_MMAP;

    if (Codec_OSAL_RequestBuf(pCtx, &reqBuf) != 0) {
        ALOGE("%s: Failed to RequestBuf", __FUNCTION__);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pCtx->videoCtx.nOutbufs = reqBuf.count;

    pCtx->videoCtx.pOutbuf = malloc(sizeof(*pCtx->videoCtx.pOutbuf) * pCtx->videoCtx.nOutbufs);
    if (pCtx->videoCtx.pOutbuf == NULL) {
        ALOGE("%s: Failed to allocate output buffer context", __FUNCTION__);
        ret = VIDEO_ERROR_NOMEM;
        goto EXIT;
    }
    memset(pCtx->videoCtx.pOutbuf, 0, sizeof(*pCtx->videoCtx.pOutbuf) * pCtx->videoCtx.nOutbufs);

    memset(&buf, 0, sizeof(buf));

    if (pCtx->videoCtx.bShareOutbuf == VIDEO_FALSE) {
        buf.type    = CODEC_OSAL_BUF_TYPE_DST;
        buf.memory  = CODEC_OSAL_MEM_TYPE_MMAP;
        buf.nPlane  = pCtx->videoCtx.nOutbufPlanes;

        for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
            buf.index = i;

            if (Codec_OSAL_QueryBuf(pCtx, &buf) != 0) {
                ALOGE("%s: Failed to QueryBuf", __FUNCTION__);
                ret = VIDEO_ERROR_APIFAIL;
                goto EXIT;
            }

            for (j = 0; j < pCtx->videoCtx.nOutbufPlanes; j++) {
                pVideoPlane = &pCtx->videoCtx.pOutbuf[i].planes[j];

                pVideoPlane->addr = Codec_OSAL_MemoryMap(NULL,
                        buf.planes[j].bufferSize, PROT_READ | PROT_WRITE,
                        MAP_SHARED, (unsigned long)buf.planes[j].addr, buf.planes[j].offset);
                if (pVideoPlane->addr == MAP_FAILED) {
                    ALOGE("%s: Failed to map", __FUNCTION__);
                    ret = VIDEO_ERROR_MAPFAIL;
                    goto EXIT;
                }

                pVideoPlane->allocSize = buf.planes[j].bufferSize;
                pVideoPlane->dataSize = 0;
            }

            pCtx->videoCtx.pOutbuf[i].pGeometry = &pCtx->videoCtx.outbufGeometry;
            pCtx->videoCtx.pOutbuf[i].bQueued = VIDEO_FALSE;
            pCtx->videoCtx.pOutbuf[i].bRegistered = VIDEO_TRUE;
        }
    } else {
        for (i = 0; i < pCtx->videoCtx.nOutbufs; i++ ) {
            pCtx->videoCtx.pOutbuf[i].pGeometry = &pCtx->videoCtx.outbufGeometry;
            pCtx->videoCtx.pOutbuf[i].bQueued = VIDEO_FALSE;
            pCtx->videoCtx.pOutbuf[i].bRegistered = VIDEO_FALSE;
        }
    }

    return ret;

EXIT:
    if ((pCtx != NULL) && (pCtx->videoCtx.pOutbuf != NULL)) {
        if (pCtx->videoCtx.bShareOutbuf == VIDEO_FALSE) {
            for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
                for (j = 0; j < pCtx->videoCtx.nOutbufPlanes; j++) {
                    pVideoPlane = &pCtx->videoCtx.pOutbuf[i].planes[j];

                    if (pVideoPlane->addr == MAP_FAILED) {
                        pVideoPlane->addr = NULL;
                        break;
                    }

                    Codec_OSAL_MemoryUnmap(pVideoPlane->addr, pVideoPlane->allocSize);
                }
            }
        }

        free(pCtx->videoCtx.pOutbuf);
        pCtx->videoCtx.pOutbuf = NULL;
    }

    return ret;
}

/*
 * [Encoder Buffer OPS] Run (src)
 */
static ExynosVideoErrorType MFC_Encoder_Run_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonInbuf == VIDEO_FALSE) {
        if (Codec_OSAL_SetStreamOn(pCtx, CODEC_OSAL_BUF_TYPE_SRC) != 0) {
            ALOGE("%s: Failed to streamon for input buffer", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }

        pCtx->videoCtx.bStreamonInbuf = VIDEO_TRUE;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Run (Dst)
 */
static ExynosVideoErrorType MFC_Encoder_Run_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonOutbuf == VIDEO_FALSE) {
        if (Codec_OSAL_SetStreamOn(pCtx, CODEC_OSAL_BUF_TYPE_DST) != 0) {
            ALOGE("%s: Failed to streamon for output buffer", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }

        pCtx->videoCtx.bStreamonOutbuf = VIDEO_TRUE;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Stop (Src)
 */
static ExynosVideoErrorType MFC_Encoder_Stop_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i = 0;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonInbuf == VIDEO_TRUE) {
        if (Codec_OSAL_SetStreamOff(pCtx, CODEC_OSAL_BUF_TYPE_SRC) != 0) {
            ALOGE("%s: Failed to streamoff for input buffer", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }

        pCtx->videoCtx.bStreamonInbuf = VIDEO_FALSE;
    }

    for (i = 0; i <  pCtx->videoCtx.nInbufs; i++)
        pCtx->videoCtx.pInbuf[i].bQueued = VIDEO_FALSE;

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Stop (Dst)
 */
static ExynosVideoErrorType MFC_Encoder_Stop_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i = 0;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonOutbuf == VIDEO_TRUE) {
        if (Codec_OSAL_SetStreamOff(pCtx, CODEC_OSAL_BUF_TYPE_DST) != 0) {
            ALOGE("%s: Failed to streamoff for output buffer", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }

        pCtx->videoCtx.bStreamonOutbuf = VIDEO_FALSE;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++)
        pCtx->videoCtx.pOutbuf[i].bQueued = VIDEO_FALSE;

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Wait (Src)
 */
static ExynosVideoErrorType MFC_Encoder_Wait_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    struct pollfd poll_events;
    int poll_state;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    poll_events.fd = pCtx->videoCtx.hDevice;
    poll_events.events = POLLOUT | POLLERR;
    poll_events.revents = 0;

    do {
        poll_state = poll((struct pollfd*)&poll_events, 1, VIDEO_ENCODER_POLL_TIMEOUT);
        if (poll_state > 0) {
            if (poll_events.revents & POLLOUT) {
                break;
            } else {
                ALOGE("%s: Poll return error", __FUNCTION__);
                ret = VIDEO_ERROR_POLL;
                break;
            }
        } else if (poll_state < 0) {
            ALOGE("%s: Poll state error", __FUNCTION__);
            ret = VIDEO_ERROR_POLL;
            break;
        }
    } while (poll_state == 0);

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Wait (Dst)
 */
static ExynosVideoErrorType MFC_Encoder_Wait_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    struct pollfd poll_events;
    int poll_state;
    int bframe_count = 0; // FIXME

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    poll_events.fd = pCtx->videoCtx.hDevice;
    poll_events.events = POLLIN | POLLERR;
    poll_events.revents = 0;

    do {
        poll_state = poll((struct pollfd*)&poll_events, 1, VIDEO_ENCODER_POLL_TIMEOUT);
        if (poll_state > 0) {
            if (poll_events.revents & POLLIN) {
                break;
            } else {
                ALOGE("%s: Poll return error", __FUNCTION__);
                ret = VIDEO_ERROR_POLL;
                break;
            }
        } else if (poll_state < 0) {
            ALOGE("%s: Poll state error", __FUNCTION__);
            ret = VIDEO_ERROR_POLL;
            break;
        } else {
            bframe_count++; // FIXME
        }
    } while (poll_state == 0 && bframe_count < 5); // FIXME

EXIT:
    return ret;
}

static ExynosVideoErrorType MFC_Encoder_Register_Inbuf(
    void             *pHandle,
    ExynosVideoPlane *pPlanes,
    int               nPlanes)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i, j;

    if ((pCtx == NULL) ||
        (pPlanes == NULL) ||
        (nPlanes != pCtx->videoCtx.nInbufPlanes)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
        if (pCtx->videoCtx.pInbuf[i].bRegistered == VIDEO_FALSE) {
            for (j = 0; j < nPlanes; j++) {
                pCtx->videoCtx.pInbuf[i].planes[j].addr        = pPlanes[j].addr;
                pCtx->videoCtx.pInbuf[i].planes[j].allocSize   = pPlanes[j].allocSize;
                pCtx->videoCtx.pInbuf[i].planes[j].fd          = pPlanes[j].fd;

                ALOGV("%s: registered buf[%d][%d]: addr = %p, alloc_sz = %u, fd = %lu",
                        __FUNCTION__, i, j, pPlanes[j].addr, pPlanes[j].allocSize, pPlanes[j].fd);
            }

            pCtx->videoCtx.pInbuf[i].bRegistered = VIDEO_TRUE;
            break;
        }
    }

    if (i == pCtx->videoCtx.nInbufs) {
        ALOGE("%s: can not find non-registered input buffer", __FUNCTION__);
        ret = VIDEO_ERROR_NOBUFFERS;
    }

EXIT:
    return ret;
}

static ExynosVideoErrorType MFC_Encoder_Register_Outbuf(
    void             *pHandle,
    ExynosVideoPlane *pPlanes,
    int               nPlanes)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i, j;

    if ((pCtx == NULL) ||
        (pPlanes == NULL) ||
        (nPlanes != pCtx->videoCtx.nOutbufPlanes)) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        if (pCtx->videoCtx.pOutbuf[i].bRegistered == VIDEO_FALSE) {
            for (j = 0; j < nPlanes; j++) {
                pCtx->videoCtx.pOutbuf[i].planes[j].addr       = pPlanes[j].addr;
                pCtx->videoCtx.pOutbuf[i].planes[j].allocSize  = pPlanes[j].allocSize;
                pCtx->videoCtx.pOutbuf[i].planes[j].fd         = pPlanes[j].fd;

                ALOGV("%s: registered buf[%d][%d]: addr = %p, alloc_sz = %d, fd = %lu",
                      __FUNCTION__, i, j, pPlanes[j].addr, pPlanes[j].allocSize, pPlanes[j].fd);
            }

            pCtx->videoCtx.pOutbuf[i].bRegistered = VIDEO_TRUE;
            break;
        }
    }

    if (i == pCtx->videoCtx.nOutbufs) {
        ALOGE("%s: can not find non-registered output buffer", __FUNCTION__);
        ret = VIDEO_ERROR_NOBUFFERS;
    }

EXIT:
    return ret;
}

static ExynosVideoErrorType MFC_Encoder_Clear_RegisteredBuffer_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
        for (j = 0; j < pCtx->videoCtx.nInbufPlanes; j++) {
            pCtx->videoCtx.pInbuf[i].planes[j].addr = NULL;
            pCtx->videoCtx.pInbuf[i].planes[j].fd   = 0;
        }

        pCtx->videoCtx.pInbuf[i].bRegistered = VIDEO_FALSE;
    }

EXIT:
    return ret;
}

static ExynosVideoErrorType MFC_Encoder_Clear_RegisteredBuffer_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        for (j = 0; j < pCtx->videoCtx.nOutbufPlanes; j++) {
            pCtx->videoCtx.pOutbuf[i].planes[j].addr = NULL;
            pCtx->videoCtx.pOutbuf[i].planes[j].fd   = 0;
        }

        pCtx->videoCtx.pOutbuf[i].bRegistered = VIDEO_FALSE;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Find (Input)
 */
static int MFC_Encoder_Find_Inbuf(
    void          *pHandle,
    unsigned char *pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
        if (pCtx->videoCtx.pInbuf[i].bQueued == VIDEO_FALSE) {
            if ((pBuffer == NULL) ||
                (pCtx->videoCtx.pInbuf[i].planes[0].addr == pBuffer)) {
                nIndex = i;
                break;
            }
        }
    }

EXIT:
    return nIndex;
}

/*
 * [Encoder Buffer OPS] Find (Output)
 */
static int MFC_Encoder_Find_Outbuf(
    void          *pHandle,
    unsigned char *pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        if (pCtx->videoCtx.pOutbuf[i].bQueued == VIDEO_FALSE) {
            if ((pBuffer == NULL) ||
                (pCtx->videoCtx.pOutbuf[i].planes[0].addr == pBuffer)) {
                nIndex = i;
                break;
            }
        }
    }

EXIT:
    return nIndex;
}

/*
 * [Encoder Buffer OPS] Enqueue (Input)
 */
static ExynosVideoErrorType MFC_Encoder_Enqueue_Inbuf(
    void          *pHandle,
    void          *pBuffer[],
    unsigned int   nDataSize[],
    int            nPlanes,
    void          *pPrivate)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex = NULL;

    CodecOSAL_Buffer buf;

    int index, i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.nInbufPlanes < nPlanes) {
        ALOGE("%s: Number of max planes : %d, nPlanes : %d", __FUNCTION__,
                                    pCtx->videoCtx.nInbufPlanes, nPlanes);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type    = CODEC_OSAL_BUF_TYPE_SRC;
    buf.nPlane  = pCtx->videoCtx.nInbufPlanes;

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pInMutex;
    pthread_mutex_lock(pMutex);

    index = MFC_Encoder_Find_Inbuf(pCtx, pBuffer[0]);
    if (index == -1) {
        pthread_mutex_unlock(pMutex);
        ALOGW("%s: Matching Buffer index not found", __FUNCTION__);
        ret = VIDEO_ERROR_NOBUFFERS;
        goto EXIT;
    }
    buf.index = index;
    ALOGV("%s: index:%d pCtx->videoCtx.pInbuf[buf.index].bQueued:%d, pBuffer[0]:%p",
           __FUNCTION__, index, pCtx->videoCtx.pInbuf[buf.index].bQueued, pBuffer[0]);

    if (pCtx->videoCtx.bShareInbuf == VIDEO_TRUE) {
        buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
        for (i = 0; i < buf.nPlane; i++) {
            if (buf.memory == CODEC_OSAL_MEM_TYPE_USERPTR)
                buf.planes[i].addr = pBuffer[i];
            else
                buf.planes[i].addr = (void *)(unsigned long)pCtx->videoCtx.pInbuf[buf.index].planes[i].fd;

            buf.planes[i].bufferSize    = pCtx->videoCtx.pInbuf[index].planes[i].allocSize;
            buf.planes[i].dataLen       = pCtx->videoCtx.pInbuf[index].planes[i].allocSize; /* for exact cache flush size */

            ALOGV("%s: shared inbuf(%d) plane=%d addr=%p len=%d used=%d\n", __FUNCTION__,
                  index, i,
                  buf.planes[i].addr,
                  buf.planes[i].bufferSize,
                  buf.planes[i].dataLen);
        }
    } else {
        buf.memory = CODEC_OSAL_MEM_TYPE_MMAP;
        for (i = 0; i < nPlanes; i++)
            buf.planes[i].dataLen = nDataSize[i];
    }

    if (nDataSize[0] <= 0) {
        buf.flags = EMPTY_DATA | LAST_FRAME;
        ALOGD("%s: EMPTY DATA", __FUNCTION__);
    } else {
        if ((((OMX_BUFFERHEADERTYPE *)pPrivate)->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)
            buf.flags = LAST_FRAME;

        if (buf.flags & LAST_FRAME)
            ALOGD("%s: DATA with flags(0x%x)", __FUNCTION__, buf.flags);
    }

    signed long long sec = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp / 1E6);
    signed long long usec = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp) - (sec * 1E6);
    buf.timestamp.tv_sec = (long)sec;
    buf.timestamp.tv_usec = (long)usec;

    pCtx->videoCtx.pInbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_TRUE;

    if ((pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_VP8) ||
        (pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_VP9) ||
        (pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_AVC) ||
        (pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_HEVC)) {
        if (pCtx->videoCtx.instInfo.bDisableDFR != VIDEO_TRUE) {
            int     oldFrameRate   = 0;
            int     curFrameRate   = 0;
            int64_t curDuration    = 0;

            curDuration = ((int64_t)((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp - pCtx->videoCtx.specificInfo.enc.oldTimeStamp);
            if ((curDuration > 0) && (pCtx->videoCtx.specificInfo.enc.oldDuration > 0)) {
                oldFrameRate = ROUND_UP(1E6 / pCtx->videoCtx.specificInfo.enc.oldDuration);
                curFrameRate = ROUND_UP(1E6 / curDuration);

                if (CHECK_FRAMERATE_VALIDITY(curFrameRate, oldFrameRate)) {
                    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_FRAME_RATE, curFrameRate) != 0) {
                        ALOGE("%s: Failed to SetControl", __FUNCTION__);
                        pthread_mutex_unlock(pMutex);
                        ret = VIDEO_ERROR_APIFAIL;
                        goto EXIT;
                    }
                    pCtx->videoCtx.specificInfo.enc.oldFrameRate = curFrameRate;
                } else {
                    ALOGV("%s: current ts: %lld, old ts: %lld, curFrameRate: %d, oldFrameRate: %d", __FUNCTION__,
                            (long long)((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp,
                            (long long)pCtx->videoCtx.specificInfo.enc.oldTimeStamp,
                            curFrameRate, oldFrameRate);
                }
            }

            if (curDuration > 0)
                pCtx->videoCtx.specificInfo.enc.oldDuration = curDuration;
            pCtx->videoCtx.specificInfo.enc.oldTimeStamp = (int64_t)((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp;
        }
    }

    /* save HDR Dynamic Info to shared buffer instead of ioctl call */
    if (pCtx->videoCtx.instInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        ExynosVideoHdrDynamic *pHdrDynamic = ((ExynosVideoHdrDynamic *)pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr) + buf.index;
        memcpy(pHdrDynamic, &(pCtx->videoCtx.inbufGeometry.HdrInfo.sHdrDynamic), sizeof(ExynosVideoHdrDynamic));
    }

    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        ALOGE("%s: Failed to enqueue input buffer", __FUNCTION__);
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pInbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_FALSE;
        pthread_mutex_unlock(pMutex);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Enqueue (Output)
 */
static ExynosVideoErrorType MFC_Encoder_Enqueue_Outbuf(
    void          *pHandle,
    void          *pBuffer[],
    unsigned int   nDataSize[],
    int            nPlanes,
    void          *pPrivate)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex = NULL;

    CodecOSAL_Buffer buf;

    int i, index;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.nOutbufPlanes < nPlanes) {
        ALOGE("%s: Number of max planes : %d, nPlanes : %d", __FUNCTION__,
                                    pCtx->videoCtx.nOutbufPlanes, nPlanes);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type    = CODEC_OSAL_BUF_TYPE_DST;
    buf.nPlane  = pCtx->videoCtx.nOutbufPlanes;

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pOutMutex;
    pthread_mutex_lock(pMutex);

    index = MFC_Encoder_Find_Outbuf(pCtx, pBuffer[0]);
    if (index == -1) {
        pthread_mutex_unlock(pMutex);
        ALOGE("%s: Failed to get index", __FUNCTION__);
        ret = VIDEO_ERROR_NOBUFFERS;
        goto EXIT;
    }
    buf.index = index;
    ALOGV("%s: index:%d pCtx->videoCtx.pOutbuf[buf.index].bQueued:%d, pBuffer[0]:%p",
           __FUNCTION__, index, pCtx->videoCtx.pOutbuf[buf.index].bQueued, pBuffer[0]);

    if (pCtx->videoCtx.bShareOutbuf == VIDEO_TRUE) {
        buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
        for (i = 0; i < nPlanes; i++) {
            if (buf.memory == CODEC_OSAL_MEM_TYPE_USERPTR)
                buf.planes[i].addr = pBuffer[i];
            else
                buf.planes[i].addr = (void *)(unsigned long)pCtx->videoCtx.pOutbuf[index].planes[i].fd;

            buf.planes[i].bufferSize    = pCtx->videoCtx.pOutbuf[index].planes[i].allocSize;
            buf.planes[i].dataLen       = nDataSize[i];

            ALOGV("%s: shared outbuf(%d) plane=%d addr=%p len=%d used=%d\n", __FUNCTION__,
                  index, i,
                  buf.planes[i].addr,
                  buf.planes[i].bufferSize,
                  buf.planes[i].dataLen);
        }
    } else {
        buf.memory = CODEC_OSAL_MEM_TYPE_MMAP;
    }

    pCtx->videoCtx.pOutbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_TRUE;
    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        ALOGE("%s: Failed to enqueue output buffer", __FUNCTION__);
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pOutbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_FALSE;
        pthread_mutex_unlock(pMutex);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Enqueue All (Output)
 */
static ExynosVideoErrorType MFC_Encoder_Enqueue_All_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    void           *pBuffer[VIDEO_BUFFER_MAX_PLANES]  = {NULL, };
    unsigned int    nDataSize[VIDEO_BUFFER_MAX_PLANES] = {0, };

    int i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        ret = MFC_Encoder_Enqueue_Outbuf(pCtx, pBuffer, nDataSize, 1, NULL);
        if (ret != VIDEO_ERROR_NONE)
            goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Dequeue (Input)
 */
static ExynosVideoBuffer *MFC_Encoder_Dequeue_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx     = (CodecOSALVideoContext *)pHandle;
    ExynosVideoBuffer     *pInbuf   = NULL;
    pthread_mutex_t       *pMutex   = NULL;

    CodecOSAL_Buffer buf;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonInbuf == VIDEO_FALSE) {
        pInbuf = NULL;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));

    buf.type    = CODEC_OSAL_BUF_TYPE_SRC;
    buf.nPlane  = pCtx->videoCtx.nInbufPlanes;
    if (pCtx->videoCtx.bShareInbuf == VIDEO_TRUE)
        buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    else
        buf.memory = CODEC_OSAL_MEM_TYPE_MMAP;

    if (Codec_OSAL_DequeueBuf(pCtx, &buf) != 0) {
        pInbuf = NULL;
        goto EXIT;
    }

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pInMutex;
    pthread_mutex_lock(pMutex);

    pInbuf = &pCtx->videoCtx.pInbuf[buf.index];
    if (pInbuf->bQueued == VIDEO_FALSE) {
        pInbuf = NULL;
        pthread_mutex_unlock(pMutex);
        goto EXIT;
    }

    pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_FALSE;
    pthread_mutex_unlock(pMutex);

EXIT:
    return pInbuf;
}

/*
 * [Encoder Buffer OPS] Dequeue (Output)
 */
static ExynosVideoBuffer *MFC_Encoder_Dequeue_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx    = (CodecOSALVideoContext *)pHandle;
    ExynosVideoBuffer     *pOutbuf = NULL;
    pthread_mutex_t       *pMutex  = NULL;

    CodecOSAL_Buffer buf;

    int ret = 0;
    int i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonOutbuf == VIDEO_FALSE) {
        pOutbuf = NULL;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type    = CODEC_OSAL_BUF_TYPE_DST;
    buf.nPlane  = pCtx->videoCtx.nOutbufPlanes;
    if (pCtx->videoCtx.bShareOutbuf == VIDEO_TRUE)
        buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    else
        buf.memory = CODEC_OSAL_MEM_TYPE_MMAP;

    /* returning DQBUF_EIO means MFC H/W status is invalid */
    ret = Codec_OSAL_DequeueBuf(pCtx, &buf);
    if (ret != 0) {
        if (errno == EIO)
            pOutbuf = (ExynosVideoBuffer *)VIDEO_ERROR_DQBUF_EIO;
        else
            pOutbuf = NULL;
        goto EXIT;
    }

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pOutMutex;
    pthread_mutex_lock(pMutex);

    pOutbuf = &pCtx->videoCtx.pOutbuf[buf.index];
    if (pOutbuf->bQueued == VIDEO_FALSE) {
        pOutbuf = NULL;
        pthread_mutex_unlock(pMutex);
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufPlanes; i++)
        pOutbuf->planes[i].dataSize = buf.planes[i].dataLen;

    pOutbuf->frameType  = buf.frameType;

    {
        int64_t sec  = (int64_t)(buf.timestamp.tv_sec * 1E6);
        int64_t usec = (int64_t)buf.timestamp.tv_usec;
        pOutbuf->timestamp = sec + usec;
    }

    pOutbuf->bQueued    = VIDEO_FALSE;
    pthread_mutex_unlock(pMutex);

EXIT:
    return pOutbuf;
}

static ExynosVideoErrorType MFC_Encoder_Clear_Queued_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i = 0;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nInbufs; i++)
        pCtx->videoCtx.pInbuf[i].bQueued = VIDEO_FALSE;

EXIT:
    return ret;
}

static ExynosVideoErrorType MFC_Encoder_Clear_Queued_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i = 0;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++)
        pCtx->videoCtx.pOutbuf[i].bQueued = VIDEO_FALSE;

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Cleanup Buffer (Input)
 */
static ExynosVideoErrorType MFC_Encoder_Cleanup_Buffer_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_ReqBuf reqBuf;
    int nBufferCount = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    nBufferCount = 0; /* for clean-up */

    memset(&reqBuf, 0, sizeof(reqBuf));

    reqBuf.type = CODEC_OSAL_BUF_TYPE_SRC;
    reqBuf.count = nBufferCount;
    if (pCtx->videoCtx.bShareInbuf == VIDEO_TRUE)
        reqBuf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    else
        reqBuf.memory = CODEC_OSAL_MEM_TYPE_MMAP;

    if (Codec_OSAL_RequestBuf(pCtx, &reqBuf) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pCtx->videoCtx.nInbufs = (int)reqBuf.count;

    if (pCtx->videoCtx.pInbuf != NULL) {
        free(pCtx->videoCtx.pInbuf);
        pCtx->videoCtx.pInbuf = NULL;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] Cleanup Buffer (Output)
 */
static ExynosVideoErrorType MFC_Encoder_Cleanup_Buffer_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_ReqBuf reqBuf;
    int nBufferCount = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    nBufferCount = 0; /* for clean-up */

    memset(&reqBuf, 0, sizeof(reqBuf));

    reqBuf.type = CODEC_OSAL_BUF_TYPE_DST;
    reqBuf.count = nBufferCount;
    if (pCtx->videoCtx.bShareOutbuf == VIDEO_TRUE)
        reqBuf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    else
        reqBuf.memory = CODEC_OSAL_MEM_TYPE_MMAP;

    if (Codec_OSAL_RequestBuf(pCtx, &reqBuf) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pCtx->videoCtx.nOutbufs = (int)reqBuf.count;

    if (pCtx->videoCtx.pOutbuf != NULL) {
        free(pCtx->videoCtx.pOutbuf);
        pCtx->videoCtx.pOutbuf = NULL;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] FindIndex (Input)
 */
static int MFC_Encoder_FindEmpty_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
        if (pCtx->videoCtx.pInbuf[i].bQueued == VIDEO_FALSE) {
            nIndex = i;
            break;
        }
    }

EXIT:
    return nIndex;
}

/*
 * [Encoder Buffer OPS] FindIndex (Output)
 */
static int MFC_Encoder_FindEmpty_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        if (pCtx->videoCtx.pOutbuf[i].bQueued == VIDEO_FALSE) {
            nIndex = i;
            break;
        }
    }

EXIT:
    return nIndex;
}

/*
 * [Encoder Buffer OPS] ExtensionEnqueue (Input)
 */
static ExynosVideoErrorType MFC_Encoder_ExtensionEnqueue_Inbuf(
    void          *pHandle,
    void          *pBuffer[],
    unsigned long  pFd[],
    unsigned int   nAllocLen[],
    unsigned int   nDataSize[],
    int            nPlanes,
    void          *pPrivate)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex = NULL;

    CodecOSAL_Buffer buf;

    int index, i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.nInbufPlanes < nPlanes) {
        ALOGE("%s: Number of max planes : %d, nPlanes : %d", __FUNCTION__,
                                    pCtx->videoCtx.nInbufPlanes, nPlanes);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type    = CODEC_OSAL_BUF_TYPE_SRC;
    buf.nPlane  = pCtx->videoCtx.nInbufPlanes;

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pInMutex;
    pthread_mutex_lock(pMutex);

    index = MFC_Encoder_FindEmpty_Inbuf(pCtx);
    if (index == -1) {
        pthread_mutex_unlock(pMutex);
        ALOGE("%s: Failed to get index", __FUNCTION__);
        ret = VIDEO_ERROR_NOBUFFERS;
        goto EXIT;
    }
    buf.index = index;
    ALOGV("%s: index:%d pCtx->videoCtx.pInbuf[buf.index].bQueued:%d, pFd[0]:%lu",
           __FUNCTION__, index, pCtx->videoCtx.pInbuf[buf.index].bQueued, pFd[0]);

    buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    for (i = 0; i < nPlanes; i++) {
        if (buf.memory == CODEC_OSAL_MEM_TYPE_USERPTR)
            buf.planes[i].addr = pBuffer[i];
        else
            buf.planes[i].addr = (void *)pFd[i];

        buf.planes[i].bufferSize    = nAllocLen[i];
        buf.planes[i].dataLen       = nAllocLen[i]; /* for exact cache flush size */

        /* Temporary storage for Dequeue */
        pCtx->videoCtx.pInbuf[buf.index].planes[i].addr         = pBuffer[i];
        pCtx->videoCtx.pInbuf[buf.index].planes[i].fd           = pFd[i];
        pCtx->videoCtx.pInbuf[buf.index].planes[i].allocSize    = nAllocLen[i];

        ALOGV("%s: shared inbuf(%d) plane = %d addr = %p len = %d used = %d\n", __FUNCTION__,
              index, i,
              buf.planes[i].addr,
              buf.planes[i].bufferSize,
              buf.planes[i].dataLen);
    }

    if (nDataSize[0] <= 0) {
        buf.flags = EMPTY_DATA | LAST_FRAME;
        ALOGD("%s: EMPTY DATA", __FUNCTION__);
    } else {
        if ((((OMX_BUFFERHEADERTYPE *)pPrivate)->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)
            buf.flags = LAST_FRAME;

        if (buf.flags & LAST_FRAME)
            ALOGD("%s: DATA with flags(0x%x)", __FUNCTION__, buf.flags);

        if ((pCtx->videoCtx.specificInfo.enc.actualFormat >= VIDEO_COLORFORMAT_NV12) &&
            (pCtx->videoCtx.specificInfo.enc.actualFormat <= VIDEO_COLORFORMAT_RGBA8888)) {
            buf.flags |= UNCOMP_FORMAT;
            pCtx->videoCtx.specificInfo.enc.actualFormat = VIDEO_COLORFORMAT_UNKNOWN;
        }
    }

    signed long long sec = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp / 1E6);
    signed long long usec = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp) - (sec * 1E6);
    buf.timestamp.tv_sec = (long)sec;
    buf.timestamp.tv_usec = (long)usec;

    pCtx->videoCtx.pInbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_TRUE;

    if ((pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_VP8) ||
        (pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_VP9) ||
        (pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_AVC) ||
        (pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_HEVC)) {
        if (pCtx->videoCtx.instInfo.bDisableDFR != VIDEO_TRUE) {
            int     oldFrameRate   = 0;
            int     curFrameRate   = 0;
            int64_t curDuration    = 0;


            curDuration = ((int64_t)((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp - pCtx->videoCtx.specificInfo.enc.oldTimeStamp);
            if ((curDuration > 0) && (pCtx->videoCtx.specificInfo.enc.oldDuration > 0)) {
                oldFrameRate = ROUND_UP(1E6 / pCtx->videoCtx.specificInfo.enc.oldDuration);
                curFrameRate = ROUND_UP(1E6 / curDuration);

                if (CHECK_FRAMERATE_VALIDITY(curFrameRate, oldFrameRate)) {
                    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_ENC_FRAME_RATE, curFrameRate) != 0) {
                        ALOGE("%s: Failed to SetControl(oldts: %lld, curts: %lld, oldfps: %d, curfps: %d)", __FUNCTION__,
                                pCtx->videoCtx.specificInfo.enc.oldTimeStamp,
                                ((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp,
                                oldFrameRate,
                                curFrameRate);

                        pthread_mutex_unlock(pMutex);
                        ret = VIDEO_ERROR_APIFAIL;
                        goto EXIT;
                    }
                    pCtx->videoCtx.specificInfo.enc.oldFrameRate = curFrameRate;
                } else {
                    ALOGV("%s: current ts: %lld, old ts: %lld, curFrameRate: %d, oldFrameRate: %d", __FUNCTION__,
                            (long long)((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp,
                            (long long)pCtx->videoCtx.specificInfo.enc.oldTimeStamp,
                            curFrameRate, oldFrameRate);
                }
            }

            if (curDuration > 0)
                pCtx->videoCtx.specificInfo.enc.oldDuration = curDuration;
            pCtx->videoCtx.specificInfo.enc.oldTimeStamp = (int64_t)((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp;
        }
    }

    /* save HDR Dynamic Info to shared buffer instead of ioctl call */
    if (pCtx->videoCtx.instInfo.supportInfo.enc.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        ExynosVideoHdrDynamic *pHdrDynamic = ((ExynosVideoHdrDynamic *)pCtx->videoCtx.specificInfo.enc.pHDRInfoShareBufferAddr) + buf.index;
        memcpy(pHdrDynamic, &(pCtx->videoCtx.inbufGeometry.HdrInfo.sHdrDynamic), sizeof(ExynosVideoHdrDynamic));
    }

    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        ALOGE("%s: Failed to enqueue input buffer", __FUNCTION__);
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pInbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_FALSE;
        pthread_mutex_unlock(pMutex);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] ExtensionDequeue (Input)
 */
static ExynosVideoErrorType MFC_Encoder_ExtensionDequeue_Inbuf(
    void              *pHandle,
    ExynosVideoBuffer *pVideoBuffer)
{
    CodecOSALVideoContext *pCtx     = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret      = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex   = NULL;

    CodecOSAL_Buffer buf;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonInbuf == VIDEO_FALSE) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type    = CODEC_OSAL_BUF_TYPE_SRC;
    buf.nPlane  = pCtx->videoCtx.nInbufPlanes;
    buf.memory  = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);

    if (Codec_OSAL_DequeueBuf(pCtx, &buf) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pInMutex;

    if (buf.index >= 0) {
        pthread_mutex_lock(pMutex);

        if (pCtx->videoCtx.pInbuf[buf.index].bQueued == VIDEO_TRUE)
            memcpy(pVideoBuffer, &pCtx->videoCtx.pInbuf[buf.index], sizeof(ExynosVideoBuffer));
        else
            ret = VIDEO_ERROR_NOBUFFERS;
        memset(&pCtx->videoCtx.pInbuf[buf.index], 0, sizeof(ExynosVideoBuffer));

        pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_FALSE;

        pthread_mutex_unlock(pMutex);
    } else {
        ALOGE("[%s] tag is (%d)", __FUNCTION__, buf.index);
        ret = VIDEO_ERROR_NOBUFFERS;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] ExtensionEnqueue (Output)
 */
static ExynosVideoErrorType MFC_Encoder_ExtensionEnqueue_Outbuf(
    void          *pHandle,
    void          *pBuffer[],
    unsigned long  pFd[],
    unsigned int   nAllocLen[],
    unsigned int   nDataSize[],
    int            nPlanes,
    void          *pPrivate)
{
    CodecOSALVideoContext *pCtx   = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret    = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex = NULL;

    CodecOSAL_Buffer buf;

    int index, i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.nOutbufPlanes < nPlanes) {
        ALOGE("%s: Number of max planes : %d, nPlanes : %d", __FUNCTION__,
                                    pCtx->videoCtx.nOutbufPlanes, nPlanes);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type    = CODEC_OSAL_BUF_TYPE_DST;
    buf.nPlane  = pCtx->videoCtx.nOutbufPlanes;

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pOutMutex;
    pthread_mutex_lock(pMutex);

    index = MFC_Encoder_FindEmpty_Outbuf(pCtx);
    if (index == -1) {
        pthread_mutex_unlock(pMutex);
        ALOGE("%s: Failed to get index", __FUNCTION__);
        ret = VIDEO_ERROR_NOBUFFERS;
        goto EXIT;
    }
    buf.index = index;
    ALOGV("%s: index:%d pCtx->videoCtx.pOutbuf[buf.index].bQueued:%d, pFd[0]:%lu",
           __FUNCTION__, index, pCtx->videoCtx.pOutbuf[buf.index].bQueued, pFd[0]);

    buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    for (i = 0; i < nPlanes; i++) {
        if (buf.memory == CODEC_OSAL_MEM_TYPE_USERPTR)
            buf.planes[i].addr = pBuffer[i];
        else
            buf.planes[i].addr = (void *)pFd[i];

        buf.planes[i].bufferSize    = nAllocLen[i];
        buf.planes[i].dataLen       = nDataSize[i];

        /* Temporary storage for Dequeue */
        pCtx->videoCtx.pOutbuf[buf.index].planes[i].addr        = pBuffer[i];
        pCtx->videoCtx.pOutbuf[buf.index].planes[i].fd          = pFd[i];
        pCtx->videoCtx.pOutbuf[buf.index].planes[i].allocSize   = nAllocLen[i];

        ALOGV("%s: shared outbuf(%d) plane = %d addr = %p len = %d used = %d\n", __FUNCTION__,
              index, i,
              buf.planes[i].addr,
              buf.planes[i].bufferSize,
              buf.planes[i].dataLen);
    }

    pCtx->videoCtx.pOutbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_TRUE;
    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        ALOGE("%s: Failed to enqueue output buffer", __FUNCTION__);
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pOutbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_FALSE;
        pthread_mutex_unlock(pMutex);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Encoder Buffer OPS] ExtensionDequeue (Output)
 */
static ExynosVideoErrorType MFC_Encoder_ExtensionDequeue_Outbuf(
    void              *pHandle,
    ExynosVideoBuffer *pVideoBuffer)
{
    CodecOSALVideoContext *pCtx    = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret     = VIDEO_ERROR_NONE;
    ExynosVideoBuffer     *pOutbuf = NULL;
    pthread_mutex_t       *pMutex  = NULL;

    CodecOSAL_Buffer buf;

    int i;

    if (pCtx == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonOutbuf == VIDEO_FALSE) {
        pOutbuf = NULL;
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type    = CODEC_OSAL_BUF_TYPE_DST;
    buf.nPlane  = pCtx->videoCtx.nOutbufPlanes;
    if (pCtx->videoCtx.bShareOutbuf == VIDEO_TRUE)
        buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    else
        buf.memory = CODEC_OSAL_MEM_TYPE_MMAP;

    /* returning DQBUF_EIO means MFC H/W status is invalid */
    if (Codec_OSAL_DequeueBuf(pCtx, &buf) != 0) {
        if (errno == EIO)
            ret = VIDEO_ERROR_DQBUF_EIO;
        else
            ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pOutMutex;
    pthread_mutex_lock(pMutex);

    pOutbuf = &pCtx->videoCtx.pOutbuf[buf.index];
    for (i = 0; i < pCtx->videoCtx.nOutbufPlanes; i++)
        pOutbuf->planes[i].dataSize = buf.planes[i].dataLen;

    pOutbuf->frameType = buf.frameType;

    {
        int64_t sec  = (int64_t)(buf.timestamp.tv_sec * 1E6);
        int64_t usec = (int64_t)buf.timestamp.tv_usec;
        pOutbuf->timestamp = sec + usec;
    }

    if (pCtx->videoCtx.pOutbuf[buf.index].bQueued == VIDEO_TRUE)
        memcpy(pVideoBuffer, pOutbuf, sizeof(ExynosVideoBuffer));
    else
        ret = VIDEO_ERROR_NOBUFFERS;
    memset(pOutbuf, 0, sizeof(ExynosVideoBuffer));

    pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_FALSE;
    pthread_mutex_unlock(pMutex);

EXIT:
    return ret;
}

/*
 * [Encoder OPS] Common
 */
static ExynosVideoEncOps defEncOps = {
    .nSize                       = 0,
    .Init                        = MFC_Encoder_Init,
    .Finalize                    = MFC_Encoder_Finalize,
    .Set_EncParam                = MFC_Encoder_Set_EncParam,
    .Set_FrameType               = MFC_Encoder_Set_FrameType,
    .Set_FrameRate               = MFC_Encoder_Set_FrameRate,
    .Set_BitRate                 = MFC_Encoder_Set_BitRate,
    .Set_QpRange                 = MFC_Encoder_Set_QuantizationRange,
    .Set_FrameSkip               = MFC_Encoder_Set_FrameSkip,
    .Set_IDRPeriod               = MFC_Encoder_Set_IDRPeriod,
    .Set_SliceMode               = MFC_Encoder_Set_SliceMode,
    .Set_FrameTag                = MFC_Encoder_Set_FrameTag,
    .Get_FrameTag                = MFC_Encoder_Get_FrameTag,
    .Enable_PrependSpsPpsToIdr   = MFC_Encoder_Enable_PrependSpsPpsToIdr,
    .Set_QosRatio                = MFC_Encoder_Set_QosRatio,
    .Set_LayerChange             = MFC_Encoder_Set_LayerChange,
    .Set_DynamicQpControl        = MFC_Encoder_Set_DynamicQpControl,
    .Set_MarkLTRFrame            = MFC_Encoder_Set_MarkLTRFrame,
    .Set_UsedLTRFrame            = MFC_Encoder_Set_UsedLTRFrame,
    .Set_BasePID                 = MFC_Encoder_Set_BasePID,
    .Set_RoiInfo                 = MFC_Encoder_Set_RoiInfo,
    .Enable_WeightedPrediction   = MFC_Encoder_Enable_WeightedPrediction,
    .Set_YSumData                = MFC_Encoder_Set_YSumData,
    .Set_IFrameRatio             = MFC_Encoder_Set_IFrameRatio,
    .Set_ColorAspects            = MFC_Encoder_Set_ColorAspects,
    .Enable_AdaptiveLayerBitrate = MFC_Encoder_Enable_AdaptiveLayerBitrate,
    .Set_HDRStaticInfo           = MFC_Encoder_Set_HDRStaticInfo,
    .Set_HDRDynamicInfo          = MFC_Encoder_Set_HDRDynamicInfo,
    .Set_HeaderMode              = MFC_Encoder_Set_HeaderMode,
    .Set_DropControl             = MFC_Encoder_Set_DropControl,
    .Disable_DynamicFrameRate    = MFC_Encoder_Disable_DynamicFrameRate,
    .Set_ActualFormat            = MFC_Encoder_Set_ActualFormat,
    .Set_OperatingRate           = MFC_Encoder_Set_OperatingRate,
    .Set_Priority                = MFC_Encoder_Set_Priority,
};

/*
 * [Encoder Buffer OPS] Input
 */
static ExynosVideoEncBufferOps defInbufOps = {
    .nSize                  = 0,
    .Enable_Cacheable       = MFC_Encoder_Enable_Cacheable_Inbuf,
    .Set_Shareable          = MFC_Encoder_Set_Shareable_Inbuf,
    .Get_Buffer             = NULL,
    .Set_Geometry           = MFC_Encoder_Set_Geometry_Inbuf,
    .Get_Geometry           = MFC_Encoder_Get_Geometry_Inbuf,
    .Setup                  = MFC_Encoder_Setup_Inbuf,
    .Run                    = MFC_Encoder_Run_Inbuf,
    .Stop                   = MFC_Encoder_Stop_Inbuf,
    .Enqueue                = MFC_Encoder_Enqueue_Inbuf,
    .Enqueue_All            = NULL,
    .Dequeue                = MFC_Encoder_Dequeue_Inbuf,
    .Register               = MFC_Encoder_Register_Inbuf,
    .Clear_RegisteredBuffer = MFC_Encoder_Clear_RegisteredBuffer_Inbuf,
    .Clear_Queue            = MFC_Encoder_Clear_Queued_Inbuf,
    .Cleanup_Buffer         = MFC_Encoder_Cleanup_Buffer_Inbuf,
    .ExtensionEnqueue       = MFC_Encoder_ExtensionEnqueue_Inbuf,
    .ExtensionDequeue       = MFC_Encoder_ExtensionDequeue_Inbuf,
};

/*
 * [Encoder Buffer OPS] Output
 */
static ExynosVideoEncBufferOps defOutbufOps = {
    .nSize                  = 0,
    .Enable_Cacheable       = MFC_Encoder_Enable_Cacheable_Outbuf,
    .Set_Shareable          = MFC_Encoder_Set_Shareable_Outbuf,
    .Get_Buffer             = MFC_Encoder_Get_Buffer_Outbuf,
    .Set_Geometry           = MFC_Encoder_Set_Geometry_Outbuf,
    .Get_Geometry           = MFC_Encoder_Get_Geometry_Outbuf,
    .Setup                  = MFC_Encoder_Setup_Outbuf,
    .Run                    = MFC_Encoder_Run_Outbuf,
    .Stop                   = MFC_Encoder_Stop_Outbuf,
    .Enqueue                = MFC_Encoder_Enqueue_Outbuf,
    .Enqueue_All            = NULL,
    .Dequeue                = MFC_Encoder_Dequeue_Outbuf,
    .Register               = MFC_Encoder_Register_Outbuf,
    .Clear_RegisteredBuffer = MFC_Encoder_Clear_RegisteredBuffer_Outbuf,
    .Clear_Queue            = MFC_Encoder_Clear_Queued_Outbuf,
    .Cleanup_Buffer         = MFC_Encoder_Cleanup_Buffer_Outbuf,
    .ExtensionEnqueue       = MFC_Encoder_ExtensionEnqueue_Outbuf,
    .ExtensionDequeue       = MFC_Encoder_ExtensionDequeue_Outbuf,
};

ExynosVideoErrorType MFC_Exynos_Video_GetInstInfo_Encoder(
    ExynosVideoInstInfo *pVideoInstInfo)
{
    CodecOSALVideoContext   videoCtx;
    ExynosVideoErrorType    ret = VIDEO_ERROR_NONE;

    int codecRet = -1;
    int mode = 0, version = 0;

    if (pVideoInstInfo == NULL) {
        ALOGE("%s: invalid parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&videoCtx, 0, sizeof(videoCtx));

#ifdef USE_HEVC_HWIP
    if (pVideoInstInfo->eCodecType == VIDEO_CODING_HEVC)
        codecRet = Codec_OSAL_DevOpen(VIDEO_HEVC_ENCODER_NAME, O_RDWR, &videoCtx);
    else
#endif
        if (pVideoInstInfo->bOTFMode == VIDEO_TRUE)
            codecRet = Codec_OSAL_DevOpen(VIDEO_MFC_OTF_ENCODER_NAME, O_RDWR, &videoCtx);
        else
            codecRet = Codec_OSAL_DevOpen(VIDEO_MFC_ENCODER_NAME, O_RDWR, &videoCtx);

    if (codecRet < 0) {
        ALOGE("%s: Failed to open encoder device", __FUNCTION__);
        ret = VIDEO_ERROR_OPENFAIL;
        goto EXIT;
    }

    if (Codec_OSAL_GetControl(&videoCtx, CODEC_OSAL_CID_VIDEO_GET_VERSION_INFO, &version) != 0) {
        ALOGW("%s: MFC version information is not available", __FUNCTION__);
#ifdef USE_HEVC_HWIP
        if (pVideoInstInfo->eCodecType == VIDEO_CODING_HEVC)
            pVideoInstInfo->HwVersion = (int)HEVC_10;
        else
#endif
            pVideoInstInfo->HwVersion = (int)MFC_65;
    } else {
        pVideoInstInfo->HwVersion = version;
    }

    if (Codec_OSAL_GetControl(&videoCtx, CODEC_OSAL_CID_VIDEO_GET_EXT_INFO, &mode) != 0) {
        memset(&(pVideoInstInfo->supportInfo.enc), 0, sizeof(pVideoInstInfo->supportInfo.enc));
        goto EXIT;
    }

    pVideoInstInfo->supportInfo.enc.bPrioritySupport             = (mode & (0x1 << 23))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bOperatingRateSupport        = (mode & (0x1 << 18))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->bVideoBufFlagCtrl                            = (mode & (0x1 << 16))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bChromaQpSupport             = (mode & (0x1 << 15))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bDropControlSupport          = (mode & (0x1 << 14))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bHDRDynamicInfoSupport       = (mode & (0x1 << 12))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bHDRStaticInfoSupport        = (mode & (0x1 << 11))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bAdaptiveLayerBitrateSupport = (mode & (0x1 << 10))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bColorAspectsSupport         = (mode & (0x1 << 9))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bIFrameRatioSupport          = (mode & (0x1 << 8))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bPVCSupport                  = (mode & (0x1 << 7))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bFixedSliceSupport           = (mode & (0x1 << 6))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bQpRangePBSupport            = (mode & (0x1 << 5))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bRoiInfoSupport              = (mode & (0x1 << 4))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bSkypeSupport                = (mode & (0x1 << 3))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.enc.bTemporalSvcSupport          = (mode & (0x1 << 2))? VIDEO_TRUE:VIDEO_FALSE;
    if (mode & (0x1 << 1)) {
        if (Codec_OSAL_GetControl(&videoCtx, CODEC_OSAL_CID_ENC_EXT_BUFFER_SIZE, &(pVideoInstInfo->supportInfo.enc.nSpareSize)) != 0) {
            ALOGE("%s: Failed to GetControl(CODEC_OSAL_CID_ENC_EXT_BUFFER_SIZE)", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }
    }

    pVideoInstInfo->SwVersion = 0;
    if (pVideoInstInfo->supportInfo.enc.bSkypeSupport == VIDEO_TRUE) {
        int swVersion = 0;

        if (Codec_OSAL_GetControl(&videoCtx, CODEC_OSAL_CID_VIDEO_GET_DRIVER_VERSION, &swVersion) != 0) {
            ALOGE("%s: Failed to GetControl(CODEC_OSAL_CID_VIDEO_GET_DRIVER_VERSION)", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }

        pVideoInstInfo->SwVersion = (unsigned int)swVersion;
    }

    __Set_SupportFormat(pVideoInstInfo);

EXIT:
    Codec_OSAL_DevClose(&videoCtx);

    return ret;
}

ExynosVideoErrorType MFC_Exynos_Video_Register_Encoder(
    ExynosVideoEncOps       *pEncOps,
    ExynosVideoEncBufferOps *pInbufOps,
    ExynosVideoEncBufferOps *pOutbufOps)
{
    ExynosVideoErrorType ret = VIDEO_ERROR_NONE;

    if ((pEncOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    defEncOps.nSize = sizeof(defEncOps);
    defInbufOps.nSize = sizeof(defInbufOps);
    defOutbufOps.nSize = sizeof(defOutbufOps);

    memcpy((char *)pEncOps + sizeof(pEncOps->nSize), (char *)&defEncOps + sizeof(defEncOps.nSize),
            pEncOps->nSize - sizeof(pEncOps->nSize));

    memcpy((char *)pInbufOps + sizeof(pInbufOps->nSize), (char *)&defInbufOps + sizeof(defInbufOps.nSize),
            pInbufOps->nSize - sizeof(pInbufOps->nSize));

    memcpy((char *)pOutbufOps + sizeof(pOutbufOps->nSize), (char *)&defOutbufOps + sizeof(defOutbufOps.nSize),
            pOutbufOps->nSize - sizeof(pOutbufOps->nSize));

EXIT:
    return ret;
}
