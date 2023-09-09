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
 * @file        ExynosVideoDecoder.c
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
#include "ExynosVideoDec.h"
#include "ExynosVideo_OSAL_Dec.h"
#include "OMX_Core.h"

/* #define LOG_NDEBUG 0 */
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ExynosVideoDecoder"

#define MAX_INPUTBUFFER_COUNT 32
#define MAX_OUTPUTBUFFER_COUNT 32

#define HAS_HDR_DYNAMIC_INFO    0x40
#define HAS_HDR_STATIC_INFO     0x3
#define HAS_COLOR_ASPECTS_INFO  0x1C
#define HAS_HDR_INFO (HAS_COLOR_ASPECTS_INFO | HAS_HDR_STATIC_INFO | HAS_HDR_DYNAMIC_INFO)
#define HAS_BLACK_BAR_CROP_INFO 0x20
#define HAS_UNCOMP_DATA         0x100
#define HAS_INTER_RESOLUTION_CHANGE   0x80
#define HAS_ACTUAL_FRAMERATE    0x200

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

/*
 * [Common] __Set_SupportFormat
 */
static void __Set_SupportFormat(ExynosVideoInstInfo *pVideoInstInfo)
{
    int nLastIndex = 0;

    if (pVideoInstInfo == NULL) {
        ALOGE("%s: ExynosVideoInstInfo must be supplied", __FUNCTION__);
        goto EXIT;
    }

    memset(pVideoInstInfo->supportFormat, (int)VIDEO_COLORFORMAT_UNKNOWN,
            sizeof(pVideoInstInfo->supportFormat));

#ifdef USE_HEVC_HWIP
    if (pVideoInstInfo->eCodecType == VIDEO_CODING_HEVC) {
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        goto EXIT;
    }
#endif

    switch (pVideoInstInfo->HwVersion) {
    case MFC_1501:
    case MFC_150:
    case MFC_1410: /* NV12, NV21, I420, YV12, NV12_10B, NV21_10B, SBWC(NV12, NV21, NV12_10B, NV21_10B) */
    case MFC_1400:
    case MFC_140:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
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
    case MFC_130:  /* NV12, NV21, I420, YV12, NV12_10B, NV21_10B */
    case MFC_120:
    case MFC_1220:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M;
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
    case MFC_90:
    case MFC_80:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        break;
    case MFC_92:  /* NV12, NV21 */
    case MFC_78D:
    case MFC_1020:
    case MFC_1021:
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M;
        pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M;
        break;
    case MFC_723:  /* NV12T, [NV12, NV21, I420, YV12] */
    case MFC_72:
    case MFC_77:
#if 0
        if (pVideoInstInfo->supportInfo.dec.bDualDPBSupport == VIDEO_TRUE) {
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV12M;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_NV21M;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_I420M;
            pVideoInstInfo->supportFormat[nLastIndex++] = VIDEO_COLORFORMAT_YV12M;
        }
#endif
    case MFC_78:  /* NV12T */
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
 * [Decoder OPS] Init
 */
static void *MFC_Decoder_Init(ExynosVideoInstInfo *pVideoInfo)
{
    CodecOSALVideoContext *pCtx     = NULL;
    pthread_mutex_t       *pMutex   = NULL;

    int ret = 0;
    int hIonClient = -1;
    int fd = -1;

    if (pVideoInfo == NULL) {
        ALOGE("%s: bad parameter", __FUNCTION__);
        goto EXIT_ALLOC_FAIL;
    }

    pCtx = (CodecOSALVideoContext *)malloc(sizeof(CodecOSALVideoContext));
    if (pCtx == NULL) {
        ALOGE("%s: Failed to allocate decoder context buffer", __FUNCTION__);
        goto EXIT_ALLOC_FAIL;
    }
    memset(pCtx, 0, sizeof(*pCtx));

    pCtx->videoCtx.hDevice = -1;
    pCtx->videoCtx.hIONHandle = -1;
    pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD = -1;
    pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD = -1;

    /* node open */
#ifdef USE_HEVC_HWIP
    if (pVideoInfo->eCodecType == VIDEO_CODING_HEVC) {
        if (pVideoInfo->eSecurityType == VIDEO_SECURE)
            ret = Codec_OSAL_DevOpen(VIDEO_HEVC_SECURE_DECODER_NAME, O_RDWR, pCtx);
        else
            ret = Codec_OSAL_DevOpen(VIDEO_HEVC_DECODER_NAME, O_RDWR, pCtx);
    } else
#endif
    {
        if (pVideoInfo->eSecurityType == VIDEO_SECURE)
            ret = Codec_OSAL_DevOpen(VIDEO_MFC_SECURE_DECODER_NAME, O_RDWR, pCtx);
        else
            ret = Codec_OSAL_DevOpen(VIDEO_MFC_DECODER_NAME, O_RDWR, pCtx);
    }

    if (ret < 0) {
        ALOGE("%s: Failed to open decoder device", __FUNCTION__);
        goto EXIT_OPEN_FAIL;
    }

    memcpy(&pCtx->videoCtx.instInfo, pVideoInfo, sizeof(*pVideoInfo));

    ALOGV("%s: HW version is %x", __FUNCTION__, pCtx->videoCtx.instInfo.HwVersion);

    if (Codec_OSAL_QueryCap(pCtx) != 0) {
        ALOGE("%s: Failed to querycap", __FUNCTION__);
        goto EXIT_QUERYCAP_FAIL;
    }

    pCtx->videoCtx.bStreamonInbuf  = VIDEO_FALSE;
    pCtx->videoCtx.bStreamonOutbuf = VIDEO_FALSE;

    /* mutex for input */
    pMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (pMutex == NULL) {
        ALOGE("%s: Failed to allocate mutex for input buffer", __FUNCTION__);
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
        ALOGE("%s: Failed to allocate mutex for output buffer", __FUNCTION__);
        goto EXIT_QUERYCAP_FAIL;
    }

    if (pthread_mutex_init(pMutex, NULL) != 0) {
        free(pMutex);
        goto EXIT_QUERYCAP_FAIL;
    }
    pCtx->videoCtx.pOutMutex = (void*)pMutex;

    /* for shared memory : referenced DPB handling */
    hIonClient = (long)exynos_ion_open();

    if (hIonClient < 0) {
        ALOGE("%s: Failed to create ion_client", __FUNCTION__);
        goto EXIT_QUERYCAP_FAIL;
    }

    pCtx->videoCtx.hIONHandle = hIonClient;

    /* for DPB ref. count */
    if (pCtx->videoCtx.instInfo.supportInfo.dec.bDrvDPBManageSupport != VIDEO_TRUE) {
        fd = exynos_ion_alloc(pCtx->videoCtx.hIONHandle, sizeof(PrivateDataShareBuffer) * VIDEO_BUFFER_MAX_NUM,
                            EXYNOS_ION_HEAP_SYSTEM_MASK, ION_FLAG_CACHED);
        if (fd < 0) {
            ALOGE("%s: Failed to exynos_ion_alloc() for nPrivateDataShareFD", __FUNCTION__);
            pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD = -1;
            goto EXIT_QUERYCAP_FAIL;
        }

        pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD = fd;

        pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress =
                                        Codec_OSAL_MemoryMap(NULL, (sizeof(PrivateDataShareBuffer) * VIDEO_BUFFER_MAX_NUM),
                                            PROT_READ | PROT_WRITE, MAP_SHARED,
                                            pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD, 0);
        if (pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress == MAP_FAILED) {
            ALOGE("%s: Failed to mmap for pPrivateDataShareAddress", __FUNCTION__);
            goto EXIT_QUERYCAP_FAIL;
        }

        memset(pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress,
                0, sizeof(PrivateDataShareBuffer) * VIDEO_BUFFER_MAX_NUM);
    }

    /* for HDR Dynamic Info */
    if (pCtx->videoCtx.instInfo.supportInfo.dec.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        fd = exynos_ion_alloc(pCtx->videoCtx.hIONHandle, sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM,
                              EXYNOS_ION_HEAP_SYSTEM_MASK, ION_FLAG_CACHED);
        if (fd < 0) {
            ALOGE("[%s] Failed to exynos_ion_alloc() for pHDRInfoShareBufferFD", __FUNCTION__);
            pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD = -1;
            goto EXIT_QUERYCAP_FAIL;
        }

        pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD = fd;

        pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr =
                                        Codec_OSAL_MemoryMap(NULL, (sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM),
                                              PROT_READ | PROT_WRITE, MAP_SHARED,
                                              pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD, 0);
        if (pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr == MAP_FAILED) {
            ALOGE("[%s] Failed to Codec_OSAL_MemoryMap() for pHDRInfoShareBufferAddr", __FUNCTION__);
            goto EXIT_QUERYCAP_FAIL;
        }

        memset(pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr,
                0, sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM);

        if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_SET_HDR_USER_SHARED_HANDLE,
                                    pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD) != 0) {
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

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bDrvDPBManageSupport != VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress,
                    sizeof(PrivateDataShareBuffer) * VIDEO_BUFFER_MAX_NUM);
            pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD >= 0) {
            close(pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD);
            pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD = -1;
        }
    }

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr,
                    sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM);
            pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD);
            pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD = -1;
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
 * [Decoder OPS] Finalize
 */
static ExynosVideoErrorType MFC_Decoder_Finalize(void *pHandle)
{
    CodecOSALVideoContext *pCtx         = (CodecOSALVideoContext *)pHandle;
    ExynosVideoPlane      *pVideoPlane  = NULL;
    pthread_mutex_t       *pMutex       = NULL;
    ExynosVideoErrorType   ret          = VIDEO_ERROR_NONE;
    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bDrvDPBManageSupport != VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress,
                    sizeof(PrivateDataShareBuffer) * VIDEO_BUFFER_MAX_NUM);
            pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD >= 0) {
            close(pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD);
            pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD = -1;
        }
    }

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bHDRDynamicInfoSupport == VIDEO_TRUE) {
        if (pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr != NULL) {
            Codec_OSAL_MemoryUnmap(pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr,
                    sizeof(ExynosVideoHdrDynamic) * VIDEO_BUFFER_MAX_NUM);
            pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr = NULL;
        }

        /* free a ion_buffer */
        if (pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD >= 0) {
            close(pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD);
            pCtx->videoCtx.specificInfo.dec.nHDRInfoShareBufferFD = -1;
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

    if (pCtx->videoCtx.pInbuf != NULL)
        free(pCtx->videoCtx.pInbuf);

    if (pCtx->videoCtx.pOutbuf != NULL)
        free(pCtx->videoCtx.pOutbuf);

    Codec_OSAL_DevClose(pCtx);

    free(pCtx);

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Set Frame Tag
 */
static ExynosVideoErrorType MFC_Decoder_Set_FrameTag(
    void *pHandle,
    int   nFrameTag)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_FRAME_TAG, nFrameTag) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Get Frame Tag
 */
static int MFC_Decoder_Get_FrameTag(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nFrameTag = -1;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        goto EXIT;
    }

    Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_VIDEO_FRAME_TAG, &nFrameTag);

EXIT:
    return nFrameTag;
}

/*
 * [Decoder OPS] Get Buffer Count
 */
static int MFC_Decoder_Get_ActualBufferCount(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int bufferCount = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        goto EXIT;
    }

    Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_NUM_MIN_BUFFERS, &bufferCount);

EXIT:
    return bufferCount;
}

/*
 * [Decoder OPS] Set Display Delay
 */
static ExynosVideoErrorType MFC_Decoder_Set_DisplayDelay(
    void *pHandle,
    int   nDelay)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_DISPLAY_DELAY, nDelay) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Set Immediate Display
 */
static ExynosVideoErrorType MFC_Decoder_Set_ImmediateDisplay(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_IMMEDIATE_DISPLAY, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Set I-Frame Decoding
 */
static ExynosVideoErrorType MFC_Decoder_Set_IFrameDecoding(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

#ifdef USE_HEVC_HWIP
    if ((pCtx->videoCtx.instInfo.eCodecType != VIDEO_CODING_HEVC) &&
        (pCtx->videoCtx.instInfo.HwVersion == (int)MFC_51))
#else
    if (pCtx->videoCtx.instInfo.HwVersion == (int)MFC_51)
#endif
        return MFC_Decoder_Set_DisplayDelay(pHandle, 0);

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_I_FRAME_DECODING, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable Packed PB
 */
static ExynosVideoErrorType MFC_Decoder_Enable_PackedPB(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_PACKED_PB, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable Loop Filter
 */
static ExynosVideoErrorType MFC_Decoder_Enable_LoopFilter(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_DEBLOCK_FILTER, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable Slice Mode
 */
static ExynosVideoErrorType MFC_Decoder_Enable_SliceMode(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_SLICE_MODE, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable SEI Parsing
 */
static ExynosVideoErrorType MFC_Decoder_Enable_SEIParsing(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_SEI_PARSING, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Get Frame Packing information
 */
static ExynosVideoErrorType MFC_Decoder_Get_FramePackingInfo(
    void                    *pHandle,
    ExynosVideoFramePacking *pFramePacking)
{
    CodecOSALVideoContext   *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType     ret  = VIDEO_ERROR_NONE;

    if ((pCtx == NULL) || (pFramePacking == NULL)) {
        ALOGE("%s: Video context info or FramePacking pointer must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_GetControls(pCtx, CODEC_OSAL_CID_DEC_SEI_INFO, (void *)pFramePacking) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable Decoding Timestamp Mode
 */
static ExynosVideoErrorType MFC_Decoder_Enable_DTSMode(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_DTS_MODE, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Set Qos Ratio
 */
static ExynosVideoErrorType MFC_Decoder_Set_QosRatio(
    void *pHandle,
    int   nRatio)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_VIDEO_QOS_RATIO, nRatio) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable Dual DPB Mode
 */
static ExynosVideoErrorType MFC_Decoder_Enable_DualDPBMode(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_DUAL_DPB_MODE, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable Dynamic DPB
 */
static ExynosVideoErrorType MFC_Decoder_Enable_DynamicDPB(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_DYNAMIC_DPB_MODE, 1) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_SET_USER_SHARED_HANDLE,
                                pCtx->videoCtx.specificInfo.dec.nPrivateDataShareFD) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Enable Discard RCV header
 */
static ExynosVideoErrorType MFC_Decoder_Enable_DiscardRcvHeader(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

#if 0  // TODO
    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_DISCARD_RCVHEADER, 1) != 0) {
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
 * [Decoder OPS] Get HDR Info
 */
static ExynosVideoErrorType MFC_Decoder_Get_HDRInfo(void *pHandle, ExynosVideoHdrInfo *pHdrInfo)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if ((pCtx == NULL) ||
        (pHdrInfo == NULL)) {
        ALOGE("%s: Video context info or HDR Info pointer must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    /* get Color Aspects and HDR Static Info
     * in case of HDR Dynamic Info is already updated at dqbuf */
    if (pCtx->videoCtx.outbufGeometry.HdrInfo.eValidType & ~HDR_INFO_DYNAMIC_META) {
        if (Codec_OSAL_GetControls(pCtx, CODEC_OSAL_CID_DEC_HDR_INFO,
                                (void *)&(pCtx->videoCtx.outbufGeometry.HdrInfo)) != 0) {
            ret = VIDEO_ERROR_APIFAIL;
            goto EXIT;
        }
    }

    memcpy(pHdrInfo, &(pCtx->videoCtx.outbufGeometry.HdrInfo), sizeof(ExynosVideoHdrInfo));

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Set Search Black Bar
 */
static ExynosVideoErrorType MFC_Decoder_Set_SearchBlackBar(void *pHandle, ExynosVideoBoolType bUse)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_SEARCH_BLACK_BAR, bUse) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Get Actual Format
 */
static int MFC_Decoder_Get_ActualFormat(void *pHandle)
{
    CodecOSALVideoContext     *pCtx         = (CodecOSALVideoContext *)pHandle;
    ExynosVideoColorFormatType nVideoFormat = VIDEO_COLORFORMAT_UNKNOWN;
    int                        nV4l2Format  = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        goto EXIT;
    }

    if (Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_ACTUAL_FORMAT, &nV4l2Format) != 0) {
        ALOGE("%s: Get extra format is failed", __FUNCTION__);
        goto EXIT;
    }

    nVideoFormat = Codec_OSAL_PixelFormatToColorFormat((unsigned int)nV4l2Format);

EXIT:
    return nVideoFormat;
}

/*
 * [Decoder OPS] Get Actual Framerate
 */
static int MFC_Decoder_Get_ActualFramerate(void *pHandle)
{
    CodecOSALVideoContext     *pCtx         = (CodecOSALVideoContext *)pHandle;
    int                        nFrameRate   = -1;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        goto EXIT;
    }

    if (Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_ACTUAL_FRAMERATE, &nFrameRate) != 0) {
        ALOGW("%s: Get Actual frame rate is failed", __FUNCTION__);
        goto EXIT;
    }

EXIT:
    return nFrameRate;
}

/*
 * [Decoder OPS] Set Operating Rate
 */
static ExynosVideoErrorType MFC_Decoder_Set_OperatingRate(
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

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bOperatingRateSupport != VIDEO_TRUE) {
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
 * [Decoder OPS] Set Priority
 */
static ExynosVideoErrorType MFC_Decoder_Set_Priority(
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

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bPrioritySupport != VIDEO_TRUE) {
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
 * [Decoder Buffer OPS] Enable Cacheable (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Enable_Cacheable_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] Enable Cacheable (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Enable_Cacheable_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] Set Shareable Buffer (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Set_Shareable_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    pCtx->videoCtx.bShareInbuf = VIDEO_TRUE;

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Set Shareable Buffer (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Set_Shareable_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    pCtx->videoCtx.bShareOutbuf = VIDEO_TRUE;

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Get Buffer (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Get_Buffer_Inbuf(
    void               *pHandle,
    int                 nIndex,
    ExynosVideoBuffer **pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        *pBuffer = NULL;
        ret = VIDEO_ERROR_BADPARAM;
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
 * [Decoder Buffer OPS] Get Buffer (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Get_Buffer_Outbuf(
    void               *pHandle,
    int                 nIndex,
    ExynosVideoBuffer **pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        *pBuffer = NULL;
        ret = VIDEO_ERROR_BADPARAM;
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
 * [Decoder Buffer OPS] Set Geometry (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Set_Geometry_Inbuf(
    void                *pHandle,
    ExynosVideoGeometry *pBufferConf)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_Format fmt;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pBufferConf == NULL) {
        ALOGE("%s: Buffer geometry must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type            = CODEC_OSAL_BUF_TYPE_SRC;
    fmt.format          = Codec_OSAL_CodingTypeToCompressdFormat(pBufferConf->eCompressionFormat);
    fmt.planeSize[0]    = pBufferConf->nSizeImage;
    fmt.nPlane          = pBufferConf->nPlaneCnt;

    if (Codec_OSAL_SetFormat(pCtx, &fmt) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memcpy(&pCtx->videoCtx.inbufGeometry, pBufferConf, sizeof(pCtx->videoCtx.inbufGeometry));
    pCtx->videoCtx.nInbufPlanes = pBufferConf->nPlaneCnt;

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Set Geometry (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Set_Geometry_Outbuf(
    void                *pHandle,
    ExynosVideoGeometry *pBufferConf)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_Format fmt;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pBufferConf == NULL) {
        ALOGE("%s: Buffer geometry must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type    = CODEC_OSAL_BUF_TYPE_DST;
    fmt.format  = Codec_OSAL_ColorFormatToPixelFormat(
                        pBufferConf->eColorFormat, pCtx->videoCtx.instInfo.HwVersion);
    fmt.nPlane  = pBufferConf->nPlaneCnt;

    if (Codec_OSAL_SetFormat(pCtx, &fmt) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memcpy(&pCtx->videoCtx.outbufGeometry, pBufferConf, sizeof(pCtx->videoCtx.outbufGeometry));
    pCtx->videoCtx.nOutbufPlanes = pBufferConf->nPlaneCnt;

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Get Geometry (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Get_Geometry_Outbuf(
    void                *pHandle,
    ExynosVideoGeometry *pBufferConf)
{
    CodecOSALVideoContext *pCtx             = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret              = VIDEO_ERROR_NONE;
    ExynosVideoGeometry   *pCodecBufferConf = NULL;

    CodecOSAL_Format fmt;
    CodecOSAL_Crop   crop;
    int i, value;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pBufferConf == NULL) {
        ALOGE("%s: Buffer geometry must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&fmt, 0, sizeof(fmt));
    memset(&crop, 0, sizeof(crop));

    fmt.type = CODEC_OSAL_BUF_TYPE_DST;
    if (Codec_OSAL_GetFormat(pCtx, &fmt) != 0) {
        if (errno == EAGAIN)
            ret = VIDEO_ERROR_HEADERINFO;
        else
            ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    crop.type = CODEC_OSAL_BUF_TYPE_DST;
    if (Codec_OSAL_GetCrop(pCtx, &crop) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pCodecBufferConf = &(pCtx->videoCtx.outbufGeometry);

    pCodecBufferConf->nFrameWidth     = fmt.width;
    pCodecBufferConf->nFrameHeight    = fmt.height;
    pCodecBufferConf->eColorFormat    = Codec_OSAL_PixelFormatToColorFormat(fmt.format);
    pCodecBufferConf->nStride         = fmt.stride;

#ifdef USE_DEINTERLACING_SUPPORT
    if ((fmt.field == CODEC_OSAL_INTER_TYPE_INTER) ||
        (fmt.field == CODEC_OSAL_INTER_TYPE_TB) ||
        (fmt.field == CODEC_OSAL_INTER_TYPE_BT))
        pCodecBufferConf->bInterlaced = VIDEO_TRUE;
    else
#endif
        pCodecBufferConf->bInterlaced = VIDEO_FALSE;

    switch (pCodecBufferConf->eColorFormat) {
        /* if it is 10bit contents, the format obtained from MFC D/D will be one of 10bit formats */
    case VIDEO_COLORFORMAT_NV12_S10B:
        pCodecBufferConf->eFilledDataType = DATA_8BIT_WITH_2BIT;
        pCtx->videoCtx.nOutbufPlanes = 1;
        break;
    case VIDEO_COLORFORMAT_NV12M_S10B:
    case VIDEO_COLORFORMAT_NV21M_S10B:
        pCodecBufferConf->eFilledDataType = DATA_8BIT_WITH_2BIT;
        pCtx->videoCtx.nOutbufPlanes = 2;
        break;
    case VIDEO_COLORFORMAT_NV12M_P010:
    case VIDEO_COLORFORMAT_NV21M_P010:
        pCodecBufferConf->eFilledDataType = DATA_10BIT;
        pCtx->videoCtx.nOutbufPlanes = 2;
        pCodecBufferConf->nStride = (fmt.stride / 2);  /* MFC D/D returns bytes multiplied bpp instead of length on pixel */
        break;
    case VIDEO_COLORFORMAT_NV12M_SBWC:
    case VIDEO_COLORFORMAT_NV21M_SBWC:
        pCodecBufferConf->eFilledDataType = DATA_8BIT_SBWC;
        pCodecBufferConf->nStride = ALIGN(fmt.width, 32);
        pCtx->videoCtx.nOutbufPlanes = 2;
        break;
    case VIDEO_COLORFORMAT_NV12_SBWC:
        pCodecBufferConf->eFilledDataType = DATA_8BIT_SBWC;
        pCodecBufferConf->nStride = ALIGN(fmt.width, 32);
        pCtx->videoCtx.nOutbufPlanes = 1;
        break;
    case VIDEO_COLORFORMAT_NV12M_10B_SBWC:
    case VIDEO_COLORFORMAT_NV21M_10B_SBWC:
        pCodecBufferConf->eFilledDataType = DATA_10BIT_SBWC;
        pCodecBufferConf->nStride = ALIGN(fmt.width, 32);
        pCtx->videoCtx.nOutbufPlanes = 2;
        break;
    case VIDEO_COLORFORMAT_NV12_10B_SBWC:
        pCodecBufferConf->eFilledDataType = DATA_10BIT_SBWC;
        pCodecBufferConf->nStride = ALIGN(fmt.width, 32);
        pCtx->videoCtx.nOutbufPlanes = 1;
        break;
    default:
        /* for backward compatibility : MFC D/D only supports g_ctrl */
        Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_GET_10BIT_INFO, &value);
        if (value == 1) {
            /* supports only S10B format */
            pCodecBufferConf->eFilledDataType = DATA_8BIT_WITH_2BIT;
            pCodecBufferConf->eColorFormat = VIDEO_COLORFORMAT_NV12M_S10B;
            pCtx->videoCtx.nOutbufPlanes = 2;

#ifdef USE_SINGLE_PALNE_SUPPORT
            if (pCtx->videoCtx.instInfo.eSecurityType == VIDEO_SECURE) {
                pCodecBufferConf->eColorFormat = VIDEO_COLORFORMAT_NV12_S10B;
                pCtx->videoCtx.nOutbufPlanes = 1;
            }
#endif
        } else {
            pCodecBufferConf->eFilledDataType = DATA_8BIT;
        }
        break;
    }

    /* Get planes aligned buffer size */
    for (i = 0; i < pCtx->videoCtx.nOutbufPlanes; i++)
        pCodecBufferConf->nAlignPlaneSize[i] = fmt.planeSize[i];

    pCodecBufferConf->cropRect.nTop       = crop.top;
    pCodecBufferConf->cropRect.nLeft      = crop.left;
    pCodecBufferConf->cropRect.nWidth     = crop.width;
    pCodecBufferConf->cropRect.nHeight    = crop.height;

    ALOGV("%s: %s contents", __FUNCTION__, (pCodecBufferConf->eFilledDataType & (DATA_10BIT | DATA_10BIT_SBWC)? "10bit":"8bit"));

    /* HdrInfo in pBufferConf is not used by OMX component */
    memcpy(pBufferConf, pCodecBufferConf, sizeof(ExynosVideoGeometry));

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Get BlackBarCrop (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Get_BlackBarCrop_Outbuf(
        void                *pHandle,
        ExynosVideoRect     *pBufferCrop)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    CodecOSAL_Crop  crop;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pBufferCrop == NULL) {
        ALOGE("%s: Buffer Crop must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&crop, 0, sizeof(crop));

    crop.type = CODEC_OSAL_BUF_TYPE_DST;
    if (Codec_OSAL_GetCrop(pCtx, &crop) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pBufferCrop->nTop      = crop.top;
    pBufferCrop->nLeft     = crop.left;
    pBufferCrop->nWidth    = crop.width;
    pBufferCrop->nHeight   = crop.height;

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Setup (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Setup_Inbuf(
    void         *pHandle,
    unsigned int  nBufferCount)
{
    CodecOSALVideoContext *pCtx         = (CodecOSALVideoContext *)pHandle;
    ExynosVideoPlane      *pVideoPlane  = NULL;
    ExynosVideoErrorType   ret          = VIDEO_ERROR_NONE;

    CodecOSAL_ReqBuf reqBuf;
    CodecOSAL_Buffer buf;

    int i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (nBufferCount == 0) {
        nBufferCount = MAX_INPUTBUFFER_COUNT;
        ALOGV("%s: Change buffer count %d", __FUNCTION__, nBufferCount);
    }

    ALOGV("%s: setting up inbufs(%d) shared = %s\n", __FUNCTION__, nBufferCount,
          pCtx->videoCtx.bShareInbuf ? "true" : "false");

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

    if (reqBuf.count != nBufferCount) {
        ALOGE("%s: asked for %d, got %d\n", __FUNCTION__, nBufferCount, reqBuf.count);
        ret = VIDEO_ERROR_NOMEM;
        goto EXIT;
    }

    pCtx->videoCtx.nInbufs = (int)reqBuf.count;

    pCtx->videoCtx.pInbuf = malloc(sizeof(*pCtx->videoCtx.pInbuf) * pCtx->videoCtx.nInbufs);
    if (pCtx->videoCtx.pInbuf == NULL) {
        ALOGE("Failed to allocate input buffer context");
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
                ret = VIDEO_ERROR_APIFAIL;
                goto EXIT;
            }

            pVideoPlane = &pCtx->videoCtx.pInbuf[i].planes[0];

            pVideoPlane->addr = Codec_OSAL_MemoryMap(NULL,
                    buf.planes[0].bufferSize, PROT_READ | PROT_WRITE,
                    MAP_SHARED, (unsigned long)buf.planes[0].addr, buf.planes[0].offset);
            if (pVideoPlane->addr == MAP_FAILED) {
                ret = VIDEO_ERROR_MAPFAIL;
                goto EXIT;
            }

            pVideoPlane->allocSize  = buf.planes[0].bufferSize;
            pVideoPlane->dataSize   = 0;

            pCtx->videoCtx.pInbuf[i].pGeometry      = &pCtx->videoCtx.inbufGeometry;
            pCtx->videoCtx.pInbuf[i].bQueued        = VIDEO_FALSE;
            pCtx->videoCtx.pInbuf[i].bRegistered    = VIDEO_TRUE;
        }
    }

    return ret;

EXIT:
    if ((pCtx != NULL) &&
        (pCtx->videoCtx.pInbuf != NULL)) {
        if (pCtx->videoCtx.bShareInbuf == VIDEO_FALSE) {
            for (i = 0; i < pCtx->videoCtx.nInbufs; i++) {
                pVideoPlane = &pCtx->videoCtx.pInbuf[i].planes[0];
                if (pVideoPlane->addr == MAP_FAILED) {
                    pVideoPlane->addr = NULL;
                    break;
                }

                Codec_OSAL_MemoryUnmap(pVideoPlane->addr, pVideoPlane->allocSize);
            }
        }

        free(pCtx->videoCtx.pInbuf);
        pCtx->videoCtx.pInbuf  = NULL;
        pCtx->videoCtx.nInbufs = 0;
    }

    return ret;
}

/*
 * [Decoder Buffer OPS] Setup (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Setup_Outbuf(
    void         *pHandle,
    unsigned int  nBufferCount)
{
    CodecOSALVideoContext *pCtx         = (CodecOSALVideoContext *)pHandle;
    ExynosVideoPlane      *pVideoPlane  = NULL;
    ExynosVideoErrorType   ret          = VIDEO_ERROR_NONE;

    CodecOSAL_ReqBuf reqBuf;
    CodecOSAL_Buffer buf;

    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (nBufferCount == 0) {
        nBufferCount = MAX_OUTPUTBUFFER_COUNT;
        ALOGV("%s: Change buffer count %d", __FUNCTION__, nBufferCount);
    }

    ALOGV("%s: setting up outbufs(%d) shared = %s\n", __FUNCTION__, nBufferCount,
          pCtx->videoCtx.bShareOutbuf ? "true" : "false");

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

    if (reqBuf.count != nBufferCount) {
        ALOGE("%s: asked for %d, got %d\n", __FUNCTION__, nBufferCount, reqBuf.count);
        ret = VIDEO_ERROR_NOMEM;
        goto EXIT;
    }

    pCtx->videoCtx.nOutbufs = reqBuf.count;

    pCtx->videoCtx.pOutbuf = malloc(sizeof(*pCtx->videoCtx.pOutbuf) * pCtx->videoCtx.nOutbufs);
    if (pCtx->videoCtx.pOutbuf == NULL) {
        ALOGE("Failed to allocate output buffer context");
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
                ret = VIDEO_ERROR_APIFAIL;
                goto EXIT;
            }

            for (j = 0; j < pCtx->videoCtx.nOutbufPlanes; j++) {
                pVideoPlane = &pCtx->videoCtx.pOutbuf[i].planes[j];

                pVideoPlane->addr = Codec_OSAL_MemoryMap(NULL,
                        buf.planes[j].bufferSize, PROT_READ | PROT_WRITE,
                        MAP_SHARED, (unsigned long)buf.planes[j].addr, buf.planes[j].offset);
                if (pVideoPlane->addr == MAP_FAILED) {
                    ret = VIDEO_ERROR_MAPFAIL;
                    goto EXIT;
                }

                pVideoPlane->allocSize  = buf.planes[j].bufferSize;
                pVideoPlane->dataSize   = 0;
            }

            pCtx->videoCtx.pOutbuf[i].pGeometry     = &pCtx->videoCtx.outbufGeometry;
            pCtx->videoCtx.pOutbuf[i].bQueued       = VIDEO_FALSE;
            pCtx->videoCtx.pOutbuf[i].bSlotUsed     = VIDEO_FALSE;
            pCtx->videoCtx.pOutbuf[i].nIndexUseCnt  = 0;
            pCtx->videoCtx.pOutbuf[i].bRegistered   = VIDEO_TRUE;
        }
    } else {
        for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
            pCtx->videoCtx.pOutbuf[i].pGeometry    = &pCtx->videoCtx.outbufGeometry;
            pCtx->videoCtx.pOutbuf[i].bQueued      = VIDEO_FALSE;
            pCtx->videoCtx.pOutbuf[i].bSlotUsed    = VIDEO_FALSE;
            pCtx->videoCtx.pOutbuf[i].nIndexUseCnt = 0;
            pCtx->videoCtx.pOutbuf[i].bRegistered  = VIDEO_FALSE;
        }
        /* Initialize initial values */
    }

    return ret;

EXIT:
    if ((pCtx != NULL) &&
        (pCtx->videoCtx.pOutbuf != NULL)) {
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
 * [Decoder Buffer OPS] Run (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Run_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] Run (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Run_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] Stop (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Stop_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] Stop (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Stop_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        pCtx->videoCtx.pOutbuf[i].bQueued      = VIDEO_FALSE;
        pCtx->videoCtx.pOutbuf[i].bSlotUsed    = VIDEO_FALSE;
        pCtx->videoCtx.pOutbuf[i].nIndexUseCnt = 0;
    }

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Wait (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Wait_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    struct pollfd poll_events;
    int poll_state;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    poll_events.fd = pCtx->videoCtx.hDevice;
    poll_events.events = POLLOUT | POLLERR;
    poll_events.revents = 0;

    do {
        poll_state = poll((struct pollfd*)&poll_events, 1, VIDEO_DECODER_POLL_TIMEOUT);
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

static ExynosVideoErrorType MFC_Decoder_Register_Inbuf(
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
        ALOGE("%s: params must be supplied", __FUNCTION__);
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

static ExynosVideoErrorType MFC_Decoder_Register_Outbuf(
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
        ALOGE("%s: params must be supplied", __FUNCTION__);
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

            /* this is for saving interlaced type or HDR info */
            if ((pCtx->videoCtx.outbufGeometry.bInterlaced == VIDEO_TRUE) ||
                (pCtx->videoCtx.outbufGeometry.eFilledDataType & DATA_10BIT)) {
                pCtx->videoCtx.pOutbuf[i].planes[2].addr    = pPlanes[2].addr;
                pCtx->videoCtx.pOutbuf[i].planes[2].fd      = pPlanes[2].fd;
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

static ExynosVideoErrorType MFC_Decoder_Clear_RegisteredBuffer_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

static ExynosVideoErrorType MFC_Decoder_Clear_RegisteredBuffer_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i, j;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        for (j = 0; j < pCtx->videoCtx.nOutbufPlanes; j++) {
            pCtx->videoCtx.pOutbuf[i].planes[j].addr = NULL;
            pCtx->videoCtx.pOutbuf[i].planes[j].fd = 0;
        }

        /* this is for saving interlaced type or HDR info */
        if ((pCtx->videoCtx.outbufGeometry.bInterlaced == VIDEO_TRUE) ||
            (pCtx->videoCtx.outbufGeometry.eFilledDataType & DATA_10BIT)) {
            pCtx->videoCtx.pOutbuf[i].planes[2].addr    = NULL;
            pCtx->videoCtx.pOutbuf[i].planes[2].fd      = 0;
        }

        pCtx->videoCtx.pOutbuf[i].bRegistered = VIDEO_FALSE;
    }

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Find (Input)
 */
static int MFC_Decoder_Find_Inbuf(
    void          *pHandle,
    unsigned char *pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] Find (Outnput)
 */
static int MFC_Decoder_Find_Outbuf(
    void          *pHandle,
    unsigned char *pBuffer)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] Enqueue (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Enqueue_Inbuf(
    void          *pHandle,
    void          *pBuffer[],
    unsigned int   nDataSize[],
    int            nPlanes,
    void          *pPrivate)
{
    CodecOSALVideoContext *pCtx     = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret      = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex   = NULL;

    CodecOSAL_Buffer buf;

    int index, i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

    index = MFC_Decoder_Find_Inbuf(pCtx, pBuffer[0]);
    if (index == -1) {
        pthread_mutex_unlock(pMutex);
        ALOGE("%s: Failed to get index", __FUNCTION__);
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
                buf.planes[i].addr = (void *)(unsigned long)pCtx->videoCtx.pInbuf[index].planes[i].fd;

            buf.planes[i].bufferSize    = pCtx->videoCtx.pInbuf[index].planes[i].allocSize;
            buf.planes[i].dataLen       = nDataSize[i];

            ALOGV("%s: shared inbuf(%d) plane(%d) addr = %p len = %d used = %d", __FUNCTION__,
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

        if ((((OMX_BUFFERHEADERTYPE *)pPrivate)->nFlags & OMX_BUFFERFLAG_CODECCONFIG) == OMX_BUFFERFLAG_CODECCONFIG)
            buf.flags |= CSD_FRAME;

        if (buf.flags & (CSD_FRAME | LAST_FRAME))
            ALOGD("%s: DATA with flags(0x%x)", __FUNCTION__, buf.flags);
    }

    signed long long sec  = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp / 1E6);
    signed long long usec = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp) - (sec * 1E6);
    buf.timestamp.tv_sec  = (long)sec;
    buf.timestamp.tv_usec = (long)usec;

    pCtx->videoCtx.pInbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_TRUE;
    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        ALOGE("%s: Failed to enqueue input buffer", __FUNCTION__);
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pInbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pInbuf[buf.index].bQueued  = VIDEO_FALSE;
        pthread_mutex_unlock(pMutex);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Enqueue (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Enqueue_Outbuf(
    void          *pHandle,
    void          *pBuffer[],
    unsigned int   nDataSize[],
    int            nPlanes,
    void          *pPrivate)
{
    CodecOSALVideoContext *pCtx   = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret    = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex = NULL;

    CodecOSAL_Buffer buf;

    int i, index, state = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

    index = MFC_Decoder_Find_Outbuf(pCtx, pBuffer[0]);
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
        for (i = 0; i < buf.nPlane; i++) {
            if (buf.memory == CODEC_OSAL_MEM_TYPE_USERPTR)
                buf.planes[i].addr = pBuffer[i];
            else
                buf.planes[i].addr = (void *)(unsigned long)pCtx->videoCtx.pOutbuf[index].planes[i].fd;

            buf.planes[i].bufferSize    = pCtx->videoCtx.pOutbuf[index].planes[i].allocSize;
            buf.planes[i].dataLen       = nDataSize[i];

            ALOGV("%s: shared outbuf(%d) plane = %d addr = %p len = %d used = %d", __FUNCTION__,
                  index, i,
                  buf.planes[i].addr,
                  buf.planes[i].bufferSize,
                  buf.planes[i].dataLen);
        }
    } else {
        ALOGV("%s: non-shared outbuf(%d)\n", __FUNCTION__, index);
        buf.memory = CODEC_OSAL_MEM_TYPE_MMAP;
    }

    pCtx->videoCtx.pOutbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_TRUE;
    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pOutbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_FALSE;
        Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_CHECK_STATE, &state);
        if (state == 1) {
            /* The case of Resolution is changed */
            ret = VIDEO_ERROR_WRONGBUFFERSIZE;
        } else {
            ALOGE("%s: Failed to enqueue output buffer", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
        }
        pthread_mutex_unlock(pMutex);
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Dequeue (Input)
 */
static ExynosVideoBuffer *MFC_Decoder_Dequeue_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx     = (CodecOSALVideoContext *)pHandle;
    ExynosVideoBuffer     *pInbuf   = NULL;
    pthread_mutex_t       *pMutex   = NULL;

    CodecOSAL_Buffer buf;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

    if (pCtx->videoCtx.bStreamonInbuf == VIDEO_FALSE)
        pInbuf = NULL;

    pthread_mutex_unlock(pMutex);

EXIT:
    return pInbuf;
}

/*
 * [Decoder Buffer OPS] Dequeue (Output)
 */
static ExynosVideoBuffer *MFC_Decoder_Dequeue_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx    = (CodecOSALVideoContext *)pHandle;
    ExynosVideoBuffer     *pOutbuf = NULL;
    pthread_mutex_t       *pMutex  = NULL;

    CodecOSAL_Buffer buf;

    int value = 0, state = 0;
    int ret = 0;
    int i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

    /* HACK: pOutbuf return -1 means DECODING_ONLY for almost cases */
    ret = Codec_OSAL_DequeueBuf(pCtx, &buf);
    if (ret != 0) {
        if (errno == EIO)
            pOutbuf = (ExynosVideoBuffer *)VIDEO_ERROR_DQBUF_EIO;
        else
            pOutbuf = NULL;
        goto EXIT;
    }

    if (buf.frameType & VIDEO_FRAME_CORRUPT) {
#ifdef USE_ORIGINAL_HEADER
        if (pCtx->videoCtx.instInfo.supportInfo.dec.bFrameErrorTypeSupport == VIDEO_TRUE) {
            int corruptType = 0;

            Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_VIDEO_FRAME_ERROR_TYPE, &corruptType);
            switch(corruptType) {
                case 3: /* BROKEN */
                    /* nothing to do */
                    break;
                case 2: /* SYNC POINT */
                case 1: /* CONCEALMENT */
                    buf.frameType = (buf.frameType & ~(VIDEO_FRAME_CORRUPT)) | VIDEO_FRAME_CONCEALMENT;
                    break;
                default:
                    break;
            }
        } else
#endif
        {
            buf.frameType = (buf.frameType & ~(VIDEO_FRAME_CORRUPT)) | VIDEO_FRAME_CONCEALMENT;
        }
    }

    if (pCtx->videoCtx.bStreamonOutbuf == VIDEO_FALSE) {
        pOutbuf = NULL;
        goto EXIT;
    }

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pOutMutex;
    pthread_mutex_lock(pMutex);

    pOutbuf = &pCtx->videoCtx.pOutbuf[buf.index];
    if (pOutbuf->bQueued == VIDEO_FALSE) {
        pOutbuf = NULL;
        ret = VIDEO_ERROR_NOBUFFERS;
        pthread_mutex_unlock(pMutex);
        goto EXIT;
    }

    for (i = 0; i < buf.nPlane; i++)
        pOutbuf->planes[i].dataSize = buf.planes[i].dataLen;

    Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_DISPLAY_STATUS, &value);

    switch (value) {
    case 0:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DECODING_ONLY;
#ifdef USE_HEVC_HWIP
        if ((pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_HEVC) ||
            (pCtx->videoCtx.instInfo.HwVersion != (int)MFC_51)) {
#else
        if (pCtx->videoCtx.instInfo.HwVersion != (int)MFC_51) {
#endif
            Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_CHECK_STATE, &state);
            if (state == 4) /* DPB realloc for S3D SEI */
                pOutbuf->displayStatus = VIDEO_FRAME_STATUS_ENABLED_S3D;
        }
        break;
    case 1:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DISPLAY_DECODING;
        break;
    case 2:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DISPLAY_ONLY;
        break;
    case 3:
        Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_CHECK_STATE, &state);
        if (state == 1) /* Resolution is changed */
            pOutbuf->displayStatus = VIDEO_FRAME_STATUS_CHANGE_RESOL;
        else            /* Decoding is finished */
            pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DECODING_FINISHED;
        break;
    case 4:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_LAST_FRAME;
        break;
    default:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_UNKNOWN;
        break;
    }

    pOutbuf->frameType = buf.frameType;

    if (pCtx->videoCtx.outbufGeometry.bInterlaced == VIDEO_TRUE) {
        if ((buf.field == CODEC_OSAL_INTER_TYPE_TB) ||
            (buf.field == CODEC_OSAL_INTER_TYPE_BT)) {
            pOutbuf->interlacedType = buf.field;
        } else {
            ALOGV("%s: buf.field's value is invald(%d)", __FUNCTION__, buf.field);
            pOutbuf->interlacedType = CODEC_OSAL_INTER_TYPE_NONE;
        }
    }

    if (buf.flags & HAS_HDR_INFO) {
        ExynosVideoHdrInfo  *pHdrInfo = &(pCtx->videoCtx.outbufGeometry.HdrInfo);

        pHdrInfo->eChangedType = buf.flags;

        /* HDR Dynamic Info is obtained from shared buffer instead of ioctl call */
        if ((pCtx->videoCtx.instInfo.supportInfo.dec.bHDRDynamicInfoSupport == VIDEO_TRUE) &&
            (buf.flags & HAS_HDR_DYNAMIC_INFO)) {
            ExynosVideoHdrDynamic *pHdrDynamic = ((ExynosVideoHdrDynamic *)pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr)
                                                    + buf.index;
            if (pHdrDynamic->valid != 0) {
                memcpy((char *)(&(pCtx->videoCtx.outbufGeometry.HdrInfo.sHdrDynamic)),
                       (char *)pHdrDynamic,
                       sizeof(ExynosVideoHdrDynamic));
            //} else {
                //pCtx->videoCtx.outbufGeometry.HdrInfo.sHdrDynamic.valid = 0;
            }
        }

        pOutbuf->frameType |= VIDEO_FRAME_WITH_HDR_INFO;
    }

    if (buf.flags & HAS_BLACK_BAR_CROP_INFO)
        pOutbuf->frameType |= VIDEO_FRAME_WITH_BLACK_BAR;

    if (pCtx->videoCtx.outbufGeometry.eFilledDataType & (DATA_8BIT_SBWC | DATA_10BIT_SBWC)) {
        if (buf.flags & HAS_UNCOMP_DATA) {
            /* SBWC scenario, but data is not compressed by black bar */
            pOutbuf->frameType |= VIDEO_FRAME_NEED_ACTUAL_FORMAT;
        }
    }

    pOutbuf->bQueued = VIDEO_FALSE;

    pthread_mutex_unlock(pMutex);

EXIT:
    return pOutbuf;
}

static ExynosVideoErrorType MFC_Decoder_Clear_Queued_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nInbufs; i++)
        pCtx->videoCtx.pInbuf[i].bQueued = VIDEO_FALSE;

EXIT:
    return ret;
}

static ExynosVideoErrorType MFC_Decoder_Clear_Queued_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;
    int i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++)
        pCtx->videoCtx.pOutbuf[i].bQueued = VIDEO_FALSE;

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] Cleanup Buffer (Input)
 */
static ExynosVideoErrorType MFC_Decoder_Cleanup_Buffer_Inbuf(void *pHandle)
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
 * [Decoder Buffer OPS] Cleanup Buffer (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Cleanup_Buffer_Outbuf(void *pHandle)
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
 * [Decoder Buffer OPS] Apply Registered Buffer (Output)
 */
static ExynosVideoErrorType MFC_Decoder_Apply_RegisteredBuffer_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret  = VIDEO_ERROR_NONE;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (Codec_OSAL_SetControl(pCtx, CODEC_OSAL_CID_DEC_WAIT_DECODING_START, 1) != 0) {
        ALOGW("%s: The requested function is not implemented", __FUNCTION__);
        //ret = VIDEO_ERROR_APIFAIL;
        //goto EXIT;    /* For Backward compatibility */
    }

    ret = MFC_Decoder_Run_Outbuf(pHandle);
    if (VIDEO_ERROR_NONE != ret)
        goto EXIT;

    ret = MFC_Decoder_Stop_Outbuf(pHandle);

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] FindIndex (Input)
 */
static int MFC_Decoder_FindEmpty_Inbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
 * [Decoder Buffer OPS] ExtensionEnqueue (Input)
 */
static ExynosVideoErrorType MFC_Decoder_ExtensionEnqueue_Inbuf(
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
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

    index = MFC_Decoder_FindEmpty_Inbuf(pCtx);
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
    for (i = 0; i < buf.nPlane; i++) {
        if (buf.memory == CODEC_OSAL_MEM_TYPE_USERPTR)
            buf.planes[i].addr = pBuffer[i];
        else
            buf.planes[i].addr = (void *)pFd[i];

        buf.planes[i].bufferSize    = nAllocLen[i];
        buf.planes[i].dataLen       = nDataSize[i];

        /* Temporary storage for Dequeue */
        pCtx->videoCtx.pInbuf[buf.index].planes[i].addr         = pBuffer[i];
        pCtx->videoCtx.pInbuf[buf.index].planes[i].fd           = pFd[i];
        pCtx->videoCtx.pInbuf[buf.index].planes[i].allocSize    = nAllocLen[i];

        ALOGV("%s: shared inbuf(%d) plane=%d addr=%p fd=%lu len=%d used=%d",
              __FUNCTION__, index, i,
              pBuffer[i], pFd[i],
              nAllocLen[i], nDataSize[i]);
    }

    if (nDataSize[0] <= 0) {
        buf.flags = EMPTY_DATA | LAST_FRAME;
        ALOGD("%s: EMPTY DATA", __FUNCTION__);
    } else {
        if ((((OMX_BUFFERHEADERTYPE *)pPrivate)->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)
            buf.flags = LAST_FRAME;

        if ((((OMX_BUFFERHEADERTYPE *)pPrivate)->nFlags & OMX_BUFFERFLAG_CODECCONFIG) == OMX_BUFFERFLAG_CODECCONFIG)
            buf.flags |= CSD_FRAME;

        if (buf.flags & (CSD_FRAME | LAST_FRAME))
            ALOGD("%s: DATA with flags(0x%x)", __FUNCTION__, buf.flags);
    }

    signed long long sec  = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp / 1E6);
    signed long long usec = (((OMX_BUFFERHEADERTYPE *)pPrivate)->nTimeStamp) - (sec * 1E6);
    buf.timestamp.tv_sec  = (long)sec;
    buf.timestamp.tv_usec = (long)usec;

    pCtx->videoCtx.pInbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_TRUE;
    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        ALOGE("%s: Failed to enqueue input buffer", __FUNCTION__);
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pInbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pInbuf[buf.index].bQueued  = VIDEO_FALSE;
        pthread_mutex_unlock(pMutex);
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] ExtensionDequeue (Input)
 */
static ExynosVideoErrorType MFC_Decoder_ExtensionDequeue_Inbuf(
    void              *pHandle,
    ExynosVideoBuffer *pVideoBuffer)
{
    CodecOSALVideoContext *pCtx   = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType   ret    = VIDEO_ERROR_NONE;
    pthread_mutex_t       *pMutex = NULL;

    CodecOSAL_Buffer buf;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    if (pCtx->videoCtx.bStreamonInbuf == VIDEO_FALSE) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = CODEC_OSAL_BUF_TYPE_SRC;
    buf.nPlane = pCtx->videoCtx.nInbufPlanes;
    buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);

    if (Codec_OSAL_DequeueBuf(pCtx, &buf) != 0) {
        ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pInMutex;
    pthread_mutex_lock(pMutex);

    if (pCtx->videoCtx.pInbuf[buf.index].bQueued == VIDEO_TRUE)
        memcpy(pVideoBuffer, &pCtx->videoCtx.pInbuf[buf.index], sizeof(ExynosVideoBuffer));
    else
        ret = VIDEO_ERROR_NOBUFFERS;
    memset(&pCtx->videoCtx.pInbuf[buf.index], 0, sizeof(ExynosVideoBuffer));

    pCtx->videoCtx.pInbuf[buf.index].bQueued = VIDEO_FALSE;
    pthread_mutex_unlock(pMutex);

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] FindIndex (Output)
 */
static int MFC_Decoder_FindEmpty_Outbuf(void *pHandle)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int nIndex = -1, i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
        goto EXIT;
    }

    for (i = 0; i < pCtx->videoCtx.nOutbufs; i++) {
        if ((pCtx->videoCtx.pOutbuf[i].bQueued == VIDEO_FALSE) &&
            (pCtx->videoCtx.pOutbuf[i].bSlotUsed == VIDEO_FALSE)) {
            nIndex = i;
            break;
        }
    }

EXIT:
    return nIndex;
}

/*
 * [Decoder Buffer OPS] BufferIndexFree (Output)
 */
void MFC_Decoder_BufferIndexFree_Outbuf(
    void                   *pHandle,
    PrivateDataShareBuffer *pPDSB,
    int                     nIndex)
{
    CodecOSALVideoContext *pCtx = (CodecOSALVideoContext *)pHandle;
    int i, j;

    ALOGV("De-queue buf.index:%d, fd:%lu", nIndex, pCtx->videoCtx.pOutbuf[nIndex].planes[0].fd);

    if (pCtx->videoCtx.pOutbuf[nIndex].nIndexUseCnt == 0)
        pCtx->videoCtx.pOutbuf[nIndex].bSlotUsed = VIDEO_FALSE;

    for (i = 0; i < VIDEO_BUFFER_MAX_NUM; i++) {
        if (pPDSB->dpbFD[i].fd <= 0)
            break;

        ALOGV("pPDSB->dpbFD[%d].fd:%d", i, pPDSB->dpbFD[i].fd);
        for (j = 0; j < pCtx->videoCtx.nOutbufs; j++) {
            if ((unsigned long)pPDSB->dpbFD[i].fd == pCtx->videoCtx.pOutbuf[j].planes[0].fd) {
                if (pCtx->videoCtx.pOutbuf[j].bQueued == VIDEO_FALSE) {
                    if (pCtx->videoCtx.pOutbuf[j].nIndexUseCnt > 0)
                        pCtx->videoCtx.pOutbuf[j].nIndexUseCnt--;
                } else if(pCtx->videoCtx.pOutbuf[j].bQueued == VIDEO_TRUE) {
                    if (pCtx->videoCtx.pOutbuf[j].nIndexUseCnt > 1) {
                        /* The buffer being used as the reference buffer came again. */
                        pCtx->videoCtx.pOutbuf[j].nIndexUseCnt--;
                    } else {
                        /* Reference DPB buffer is internally reused. */
                    }
                }

                ALOGV("dec Cnt : FD:%d, pCtx->videoCtx.pOutbuf[%d].nIndexUseCnt:%d",
                        pPDSB->dpbFD[i].fd, j, pCtx->videoCtx.pOutbuf[j].nIndexUseCnt);

                if ((pCtx->videoCtx.pOutbuf[j].nIndexUseCnt == 0) &&
                    (pCtx->videoCtx.pOutbuf[j].bQueued == VIDEO_FALSE)) {
                    pCtx->videoCtx.pOutbuf[j].bSlotUsed = VIDEO_FALSE;
                }
            }
        }
    }

    memset((char *)pPDSB, 0, sizeof(PrivateDataShareBuffer));

    return;
}

/*
 * [Decoder Buffer OPS] ExtensionEnqueue (Output)
 */
static ExynosVideoErrorType MFC_Decoder_ExtensionEnqueue_Outbuf(
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

    int i, index, state = 0;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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

    index = MFC_Decoder_Find_Outbuf(pCtx, pBuffer[0]);
    if (index == -1) {
        ALOGV("%s: Failed to find index", __FUNCTION__);
        index = MFC_Decoder_FindEmpty_Outbuf(pCtx);
        if (index == -1) {
            pthread_mutex_unlock(pMutex);
            ALOGE("%s: Failed to get index", __FUNCTION__);
            ret = VIDEO_ERROR_NOBUFFERS;
            goto EXIT;
        }
    }
    buf.index = index;
    ALOGV("%s: index:%d pCtx->videoCtx.pOutbuf[buf.index].bQueued:%d, pFd[0]:%lu",
           __FUNCTION__, index, pCtx->videoCtx.pOutbuf[buf.index].bQueued, pFd[0]);

    buf.memory = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);
    for (i = 0; i < buf.nPlane; i++) {
        if (buf.memory == CODEC_OSAL_MEM_TYPE_USERPTR)
            buf.planes[i].addr = pBuffer[i];
        else
            buf.planes[i].addr = (void *)(unsigned long)pFd[i];

        buf.planes[i].bufferSize    = nAllocLen[i];
        buf.planes[i].dataLen       = nDataSize[i];

        /* Temporary storage for Dequeue */
        pCtx->videoCtx.pOutbuf[buf.index].planes[i].addr        = pBuffer[i];
        pCtx->videoCtx.pOutbuf[buf.index].planes[i].fd          = pFd[i];
        pCtx->videoCtx.pOutbuf[buf.index].planes[i].allocSize   = nAllocLen[i];

        ALOGV("%s: shared outbuf(%d) plane=%d addr=%p fd=%lu len=%d used=%d",
                  __FUNCTION__, index, i,
                  pBuffer[i], pFd[i],
                  nAllocLen[i], nDataSize[i]);
    }

    /* this(private data buffer) is for saving interlaced type or HDR info */
    {
        pCtx->videoCtx.pOutbuf[buf.index].planes[2].addr = pBuffer[2];
        pCtx->videoCtx.pOutbuf[buf.index].planes[2].fd   = pFd[2];
    }

    pCtx->videoCtx.pOutbuf[buf.index].pPrivate = pPrivate;
    pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_TRUE;

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bDrvDPBManageSupport != VIDEO_TRUE) {
        pCtx->videoCtx.pOutbuf[buf.index].bSlotUsed = VIDEO_TRUE;
        pCtx->videoCtx.pOutbuf[buf.index].nIndexUseCnt++;
    }

    pthread_mutex_unlock(pMutex);

    if (Codec_OSAL_EnqueueBuf(pCtx, &buf) != 0) {
        pthread_mutex_lock(pMutex);
        pCtx->videoCtx.pOutbuf[buf.index].nIndexUseCnt--;
        pCtx->videoCtx.pOutbuf[buf.index].pPrivate = NULL;
        pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_FALSE;

        if (pCtx->videoCtx.pOutbuf[buf.index].nIndexUseCnt == 0)
            pCtx->videoCtx.pOutbuf[buf.index].bSlotUsed = VIDEO_FALSE;

        Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_CHECK_STATE, &state);
        if (state == 1) {
            /* The case of Resolution is changed */
            ret = VIDEO_ERROR_WRONGBUFFERSIZE;
        } else {
            ALOGE("%s: Failed to enqueue output buffer", __FUNCTION__);
            ret = VIDEO_ERROR_APIFAIL;
        }

        pthread_mutex_unlock(pMutex);
        goto EXIT;
    }

EXIT:
    return ret;
}

/*
 * [Decoder Buffer OPS] ExtensionDequeue (Output)
 */
static ExynosVideoErrorType MFC_Decoder_ExtensionDequeue_Outbuf(
    void              *pHandle,
    ExynosVideoBuffer *pVideoBuffer)
{
    CodecOSALVideoContext  *pCtx    = (CodecOSALVideoContext *)pHandle;
    ExynosVideoErrorType    ret     = VIDEO_ERROR_NONE;
    pthread_mutex_t        *pMutex  = NULL;
    ExynosVideoBuffer      *pOutbuf = NULL;
    PrivateDataShareBuffer *pPDSB   = NULL;

    CodecOSAL_Buffer buf;

    int value = 0, state = 0;
    int i;

    if (pCtx == NULL) {
        ALOGE("%s: Video context info must be supplied", __FUNCTION__);
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
    buf.memory  = Codec_OSAL_VideoMemoryToSystemMemory(pCtx->videoCtx.instInfo.nMemoryType);

    /* HACK: pOutbuf return -1 means DECODING_ONLY for almost cases */
    if (Codec_OSAL_DequeueBuf(pCtx, &buf) != 0) {
        if (errno == EIO)
            ret = VIDEO_ERROR_DQBUF_EIO;
        else
            ret = VIDEO_ERROR_APIFAIL;
        goto EXIT;
    }

    if (buf.frameType & VIDEO_FRAME_CORRUPT) {
#ifdef USE_ORIGINAL_HEADER
        if (pCtx->videoCtx.instInfo.supportInfo.dec.bFrameErrorTypeSupport == VIDEO_TRUE) {
            int corruptType = 0;

            Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_VIDEO_FRAME_ERROR_TYPE, &corruptType);
            switch(corruptType) {
                case 3: /* BROKEN */
                    /* nothing to do */
                    break;
                case 2: /* SYNC POINT */
                case 1: /* CONCEALMENT */
                    buf.frameType = (buf.frameType & ~(VIDEO_FRAME_CORRUPT)) | VIDEO_FRAME_CONCEALMENT;
                    break;
                default:
                    break;
            }
        } else
#endif
        {
            buf.frameType = (buf.frameType & ~(VIDEO_FRAME_CORRUPT)) | VIDEO_FRAME_CONCEALMENT;
        }
    }

    pMutex = (pthread_mutex_t*)pCtx->videoCtx.pOutMutex;
    pthread_mutex_lock(pMutex);

    pOutbuf = &pCtx->videoCtx.pOutbuf[buf.index];
    if (pOutbuf->bQueued == VIDEO_FALSE) {
        pOutbuf = NULL;
        ret = VIDEO_ERROR_NOBUFFERS;
        pthread_mutex_unlock(pMutex);
        goto EXIT;
    }

    for (i = 0; i < buf.nPlane; i++)
        pOutbuf->planes[i].dataSize = buf.planes[i].dataLen;

    Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_DISPLAY_STATUS, &value);

    switch (value) {
    case 0:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DECODING_ONLY;
#ifdef USE_HEVC_HWIP
        if ((pCtx->videoCtx.instInfo.eCodecType == VIDEO_CODING_HEVC) ||
            (pCtx->videoCtx.instInfo.HwVersion != (int)MFC_51)) {
#else
        if (pCtx->videoCtx.instInfo.HwVersion != (int)MFC_51) {
#endif
            Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_CHECK_STATE, &state);
            if (state == 4) /* DPB realloc for S3D SEI */
                pOutbuf->displayStatus = VIDEO_FRAME_STATUS_ENABLED_S3D;
        }
        break;
    case 1:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DISPLAY_DECODING;
        if (buf.flags & HAS_INTER_RESOLUTION_CHANGE)
            pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DISPLAY_INTER_RESOL_CHANGE;
        break;
    case 2:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DISPLAY_ONLY;
        if (buf.flags & HAS_INTER_RESOLUTION_CHANGE)
            pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DISPLAY_INTER_RESOL_CHANGE;
        break;
    case 3:
        Codec_OSAL_GetControl(pCtx, CODEC_OSAL_CID_DEC_CHECK_STATE, &state);
        if (state == 1) /* Resolution is changed */
            pOutbuf->displayStatus = VIDEO_FRAME_STATUS_CHANGE_RESOL;
        else            /* Decoding is finished */
            pOutbuf->displayStatus = VIDEO_FRAME_STATUS_DECODING_FINISHED;
        break;
    case 4:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_LAST_FRAME;
        break;
    default:
        pOutbuf->displayStatus = VIDEO_FRAME_STATUS_UNKNOWN;
        break;
    }

    pOutbuf->frameType = buf.frameType;

    if (pCtx->videoCtx.outbufGeometry.bInterlaced == VIDEO_TRUE) {
        if ((buf.field == CODEC_OSAL_INTER_TYPE_TB) ||
            (buf.field == CODEC_OSAL_INTER_TYPE_BT)) {
            pOutbuf->interlacedType = buf.field;
        } else {
            ALOGV("%s: buf.field's value is invald(%d)", __FUNCTION__, buf.field);
            pOutbuf->interlacedType = CODEC_OSAL_INTER_TYPE_NONE;
        }
    }

    if (buf.flags & HAS_HDR_INFO) {
        ExynosVideoHdrInfo  *pHdrInfo = &(pCtx->videoCtx.outbufGeometry.HdrInfo);

        pHdrInfo->eValidType   = buf.flags;
        pHdrInfo->eChangedType = HDR_INFO_NO_CHANGES;

        /* HDR Dynamic Info is obtained from shared buffer instead of ioctl call */
        if ((pCtx->videoCtx.instInfo.supportInfo.dec.bHDRDynamicInfoSupport == VIDEO_TRUE) &&
            (buf.flags & HAS_HDR_DYNAMIC_INFO)) {
            ExynosVideoHdrDynamic *pHdrDynamic = ((ExynosVideoHdrDynamic *)pCtx->videoCtx.specificInfo.dec.pHDRInfoShareBufferAddr)
                                                    + buf.index;
            if (memcmp(pHdrDynamic, &pCtx->videoCtx.outbufGeometry.HdrInfo.sHdrDynamic, sizeof(ExynosVideoHdrDynamic))) {
                pHdrInfo->eChangedType |= HDR_INFO_DYNAMIC_META;
            }

            if (pHdrDynamic->valid != 0) {
                memcpy((char *)(&(pCtx->videoCtx.outbufGeometry.HdrInfo.sHdrDynamic)),
                       (char *)pHdrDynamic,
                       sizeof(ExynosVideoHdrDynamic));
            //} else {
                //pCtx->videoCtx.outbufGeometry.HdrInfo.sHdrDynamic.valid = 0;
            }
        }

        pOutbuf->frameType |= VIDEO_FRAME_WITH_HDR_INFO;
    }

    if (buf.flags & HAS_BLACK_BAR_CROP_INFO)
        pOutbuf->frameType |= VIDEO_FRAME_WITH_BLACK_BAR;

    if (pCtx->videoCtx.outbufGeometry.eFilledDataType & (DATA_8BIT_SBWC | DATA_10BIT_SBWC)) {
        if (buf.flags & HAS_UNCOMP_DATA) {
            /* SBWC scenario, but data is not compressed by black bar */
            pOutbuf->frameType |= VIDEO_FRAME_NEED_ACTUAL_FORMAT;
        }
    }

    if (buf.flags & HAS_ACTUAL_FRAMERATE) {
        /* for PowerHint: framerate from app can be invalid */
        pOutbuf->frameType |= VIDEO_FRAME_NEED_ACTUAL_FRAMERATE;
    }

    if (pCtx->videoCtx.instInfo.supportInfo.dec.bDrvDPBManageSupport != VIDEO_TRUE)
        pPDSB = ((PrivateDataShareBuffer *)pCtx->videoCtx.specificInfo.dec.pPrivateDataShareAddress) + buf.index;

    if (pCtx->videoCtx.pOutbuf[buf.index].bQueued == VIDEO_TRUE) {
        memcpy(pVideoBuffer, pOutbuf, sizeof(ExynosVideoBuffer));
        if (pPDSB != NULL)
            memcpy((char *)(&(pVideoBuffer->PDSB)), (char *)pPDSB, sizeof(PrivateDataShareBuffer));
    } else {
        ret = VIDEO_ERROR_NOBUFFERS;
    }

    if (pPDSB != NULL)
        MFC_Decoder_BufferIndexFree_Outbuf(pHandle, pPDSB, buf.index);

    pCtx->videoCtx.pOutbuf[buf.index].bQueued = VIDEO_FALSE;
    pthread_mutex_unlock(pMutex);

EXIT:
    return ret;
}

/*
 * [Decoder OPS] Common
 */
static ExynosVideoDecOps defDecOps = {
    .nSize                      = 0,
    .Init                       = MFC_Decoder_Init,
    .Finalize                   = MFC_Decoder_Finalize,
    .Set_DisplayDelay           = MFC_Decoder_Set_DisplayDelay,
    .Set_IFrameDecoding         = MFC_Decoder_Set_IFrameDecoding,
    .Enable_PackedPB            = MFC_Decoder_Enable_PackedPB,
    .Enable_LoopFilter          = MFC_Decoder_Enable_LoopFilter,
    .Enable_SliceMode           = MFC_Decoder_Enable_SliceMode,
    .Get_ActualBufferCount      = MFC_Decoder_Get_ActualBufferCount,
    .Set_FrameTag               = MFC_Decoder_Set_FrameTag,
    .Get_FrameTag               = MFC_Decoder_Get_FrameTag,
    .Enable_SEIParsing          = MFC_Decoder_Enable_SEIParsing,
    .Get_FramePackingInfo       = MFC_Decoder_Get_FramePackingInfo,
    .Set_ImmediateDisplay       = MFC_Decoder_Set_ImmediateDisplay,
    .Enable_DTSMode             = MFC_Decoder_Enable_DTSMode,
    .Set_QosRatio               = MFC_Decoder_Set_QosRatio,
    .Enable_DiscardRcvHeader    = MFC_Decoder_Enable_DiscardRcvHeader,
    .Enable_DualDPBMode         = MFC_Decoder_Enable_DualDPBMode,
    .Enable_DynamicDPB          = MFC_Decoder_Enable_DynamicDPB,
    .Get_HDRInfo                = MFC_Decoder_Get_HDRInfo,
    .Set_SearchBlackBar         = MFC_Decoder_Set_SearchBlackBar,
    .Get_ActualFormat           = MFC_Decoder_Get_ActualFormat,
    .Get_ActualFramerate        = MFC_Decoder_Get_ActualFramerate,
    .Set_OperatingRate          = MFC_Decoder_Set_OperatingRate,
    .Set_Priority               = MFC_Decoder_Set_Priority,
};

/*
 * [Decoder Buffer OPS] Input
 */
static ExynosVideoDecBufferOps defInbufOps = {
    .nSize                  = 0,
    .Enable_Cacheable       = MFC_Decoder_Enable_Cacheable_Inbuf,
    .Set_Shareable          = MFC_Decoder_Set_Shareable_Inbuf,
    .Get_Buffer             = NULL,
    .Set_Geometry           = MFC_Decoder_Set_Geometry_Inbuf,
    .Get_Geometry           = NULL,
    .Get_BlackBarCrop       = NULL,
    .Setup                  = MFC_Decoder_Setup_Inbuf,
    .Run                    = MFC_Decoder_Run_Inbuf,
    .Stop                   = MFC_Decoder_Stop_Inbuf,
    .Enqueue                = MFC_Decoder_Enqueue_Inbuf,
    .Enqueue_All            = NULL,
    .Dequeue                = MFC_Decoder_Dequeue_Inbuf,
    .Register               = MFC_Decoder_Register_Inbuf,
    .Clear_RegisteredBuffer = MFC_Decoder_Clear_RegisteredBuffer_Inbuf,
    .Clear_Queue            = MFC_Decoder_Clear_Queued_Inbuf,
    .Cleanup_Buffer         = MFC_Decoder_Cleanup_Buffer_Inbuf,
    .Apply_RegisteredBuffer = NULL,
    .ExtensionEnqueue       = MFC_Decoder_ExtensionEnqueue_Inbuf,
    .ExtensionDequeue       = MFC_Decoder_ExtensionDequeue_Inbuf,
};

/*
 * [Decoder Buffer OPS] Output
 */
static ExynosVideoDecBufferOps defOutbufOps = {
    .nSize                  = 0,
    .Enable_Cacheable       = MFC_Decoder_Enable_Cacheable_Outbuf,
    .Set_Shareable          = MFC_Decoder_Set_Shareable_Outbuf,
    .Get_Buffer             = MFC_Decoder_Get_Buffer_Outbuf,
    .Set_Geometry           = MFC_Decoder_Set_Geometry_Outbuf,
    .Get_Geometry           = MFC_Decoder_Get_Geometry_Outbuf,
    .Get_BlackBarCrop       = MFC_Decoder_Get_BlackBarCrop_Outbuf,
    .Setup                  = MFC_Decoder_Setup_Outbuf,
    .Run                    = MFC_Decoder_Run_Outbuf,
    .Stop                   = MFC_Decoder_Stop_Outbuf,
    .Enqueue                = MFC_Decoder_Enqueue_Outbuf,
    .Enqueue_All            = NULL,
    .Dequeue                = MFC_Decoder_Dequeue_Outbuf,
    .Register               = MFC_Decoder_Register_Outbuf,
    .Clear_RegisteredBuffer = MFC_Decoder_Clear_RegisteredBuffer_Outbuf,
    .Clear_Queue            = MFC_Decoder_Clear_Queued_Outbuf,
    .Cleanup_Buffer         = MFC_Decoder_Cleanup_Buffer_Outbuf,
    .Apply_RegisteredBuffer = MFC_Decoder_Apply_RegisteredBuffer_Outbuf,
    .ExtensionEnqueue       = MFC_Decoder_ExtensionEnqueue_Outbuf,
    .ExtensionDequeue       = MFC_Decoder_ExtensionDequeue_Outbuf,
};

ExynosVideoErrorType MFC_Exynos_Video_GetInstInfo_Decoder(
    ExynosVideoInstInfo *pVideoInstInfo)
{
    CodecOSALVideoContext   videoCtx;
    ExynosVideoErrorType    ret = VIDEO_ERROR_NONE;

    int codecRet = 0;
    int mode = 0, version = 0;

    if (pVideoInstInfo == NULL) {
        ALOGE("%s: bad parameter", __FUNCTION__);
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    memset(&videoCtx, 0, sizeof(videoCtx));

#ifdef USE_HEVC_HWIP
    if (pVideoInstInfo->eCodecType == VIDEO_CODING_HEVC)
        codecRet = Codec_OSAL_DevOpen(VIDEO_HEVC_DECODER_NAME, O_RDWR, &videoCtx);
    else
#endif
        codecRet = Codec_OSAL_DevOpen(VIDEO_MFC_DECODER_NAME, O_RDWR, &videoCtx);

    if (codecRet < 0) {
        ALOGE("%s: Failed to open decoder device", __FUNCTION__);
        ret = VIDEO_ERROR_OPENFAIL;
        goto EXIT;
    }

    if (Codec_OSAL_GetControl(&videoCtx, CODEC_OSAL_CID_VIDEO_GET_VERSION_INFO, &version) != 0) {
        ALOGW("%s: HW version information is not available", __FUNCTION__);
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
        pVideoInstInfo->supportInfo.dec.bSkypeSupport = VIDEO_FALSE;
        goto EXIT;
    }

    pVideoInstInfo->supportInfo.dec.bPrioritySupport        = (mode & (0x1 << 23))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->bVideoBufFlagCtrl                       = (mode & (0x1 << 16))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.dec.bOperatingRateSupport   = (mode & (0x1 << 8))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.dec.bFrameErrorTypeSupport  = (mode & (0x1 << 7))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.dec.bDrvDPBManageSupport    = (mode & (0x1 << 5))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.dec.bHDRDynamicInfoSupport  = (mode & (0x1 << 4))? VIDEO_TRUE:VIDEO_FALSE;
    pVideoInstInfo->supportInfo.dec.bSkypeSupport           = (mode & (0x1 << 3))? VIDEO_TRUE:VIDEO_FALSE;

    pVideoInstInfo->SwVersion = 0;
    if (pVideoInstInfo->supportInfo.dec.bSkypeSupport == VIDEO_TRUE) {
        int swVersion = 0;

        if (Codec_OSAL_GetControl(&videoCtx, CODEC_OSAL_CID_VIDEO_GET_DRIVER_VERSION, &swVersion) != 0) {
            ALOGE("%s: g_ctrl is failed(CODEC_OSAL_CID_VIDEO_GET_DRIVER_VERSION)", __FUNCTION__);
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

ExynosVideoErrorType MFC_Exynos_Video_Register_Decoder(
    ExynosVideoDecOps       *pDecOps,
    ExynosVideoDecBufferOps *pInbufOps,
    ExynosVideoDecBufferOps *pOutbufOps)
{
    ExynosVideoErrorType ret = VIDEO_ERROR_NONE;

    if ((pDecOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        ret = VIDEO_ERROR_BADPARAM;
        goto EXIT;
    }

    defDecOps.nSize = sizeof(defDecOps);
    defInbufOps.nSize = sizeof(defInbufOps);
    defOutbufOps.nSize = sizeof(defOutbufOps);

    memcpy((char *)pDecOps + sizeof(pDecOps->nSize), (char *)&defDecOps + sizeof(defDecOps.nSize),
            pDecOps->nSize - sizeof(pDecOps->nSize));

    memcpy((char *)pInbufOps + sizeof(pInbufOps->nSize), (char *)&defInbufOps + sizeof(defInbufOps.nSize),
            pInbufOps->nSize - sizeof(pInbufOps->nSize));

    memcpy((char *)pOutbufOps + sizeof(pOutbufOps->nSize), (char *)&defOutbufOps + sizeof(defOutbufOps.nSize),
            pOutbufOps->nSize - sizeof(pOutbufOps->nSize));

EXIT:
    return ret;
}
