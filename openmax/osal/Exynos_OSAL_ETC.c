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
 * @file        Exynos_OSAL_ETC.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#ifdef USE_ANDROID
#include <system/graphics.h>
#else
#include "graphics.h"
#endif

#include "Exynos_OSAL_Memory.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OMX_Macros.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VIDEO_OSAL"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


#ifdef PERFORMANCE_DEBUG
#include <time.h>
#include "Exynos_OSAL_Mutex.h"

#define INPUT_PORT_INDEX    0
#define OUTPUT_PORT_INDEX   1
#endif

#include "ExynosVideoApi.h"
#include "exynos_format.h"
#include "csc.h"

#define CUSTOM_UHD_MFC_OUTPUT_BUFFER_SIZE     (3840 * 2160 * 3 / 2 / 2)
#define CUSTOM_8K_MFC_OUTPUT_BUFFER_SIZE      (7680 * 4320 * 3 / 2 / 4)

static struct timeval perfStart[PERF_ID_MAX+1], perfStop[PERF_ID_MAX+1];
static unsigned long perfTime[PERF_ID_MAX+1], totalPerfTime[PERF_ID_MAX+1];
static unsigned int perfFrameCount[PERF_ID_MAX+1], perfOver30ms[PERF_ID_MAX+1];

size_t Exynos_OSAL_Strcpy(OMX_PTR dest, OMX_PTR src)
{
#ifdef USE_ANDROID
    size_t len = (size_t)(strlen((const char *)src) + 1);

    return strlcpy(dest, src, len);
#else
    return strlen(strncpy(dest, src, strlen((const char *)src)));
#endif
}

size_t Exynos_OSAL_Strncpy(OMX_PTR dest, OMX_PTR src, size_t num)
{
#ifdef USE_ANDROID
    return strlcpy(dest, src, (size_t)(num + 1));
#else
    return strlen(strncpy(dest, src, num));
#endif
}

OMX_S32 Exynos_OSAL_Strcmp(OMX_PTR str1, OMX_PTR str2)
{
    return strcmp(str1, str2);
}

OMX_S32 Exynos_OSAL_Strncmp(OMX_PTR str1, OMX_PTR str2, size_t num)
{
    return strncmp(str1, str2, num);
}

const char* Exynos_OSAL_Strstr(const char *str1, const char *str2)
{
    return strstr(str1, str2);
}

size_t Exynos_OSAL_Strcat(OMX_PTR dest, OMX_PTR src)
{
#ifdef USE_ANDROID
    return strlcat(dest, src, (size_t)(strlen((const char *)dest) + strlen((const char *)src) + 1));
#else
    return strlen(strncat(dest, src, strlen((const char *)src)));
#endif
}

size_t Exynos_OSAL_Strncat(OMX_PTR dest, OMX_PTR src, size_t num)
{
    size_t len = num;

#ifdef USE_ANDROID
    /* caution : num should be a size of dest buffer */
    return strlcat(dest, src, (size_t)(strlen((const char *)dest) + strlen((const char *)src) + 1));
#else
    return strlen(strncat(dest, src, len));
#endif
}

size_t Exynos_OSAL_Strlen(const char *str)
{
    return strlen(str);
}

static OMX_S32 Exynos_OSAL_MeasureTime(struct timeval *start, struct timeval *stop)
{
    signed long sec, usec, time;

    sec = stop->tv_sec - start->tv_sec;
    if (stop->tv_usec >= start->tv_usec) {
        usec = (signed long)stop->tv_usec - (signed long)start->tv_usec;
    } else {
        usec = (signed long)stop->tv_usec + 1000000 - (signed long)start->tv_usec;
        sec--;
    }

    time = sec * 1000000 + (usec);

    return time;
}

void Exynos_OSAL_PerfInit(PERF_ID_TYPE id)
{
    Exynos_OSAL_Memset(&perfStart[id], 0, sizeof(perfStart[id]));
    Exynos_OSAL_Memset(&perfStop[id], 0, sizeof(perfStop[id]));
    perfTime[id] = 0;
    totalPerfTime[id] = 0;
    perfFrameCount[id] = 0;
    perfOver30ms[id] = 0;
}

void Exynos_OSAL_PerfStart(PERF_ID_TYPE id)
{
    gettimeofday(&perfStart[id], NULL);
}

void Exynos_OSAL_PerfStop(PERF_ID_TYPE id)
{
    gettimeofday(&perfStop[id], NULL);

    perfTime[id] = Exynos_OSAL_MeasureTime(&perfStart[id], &perfStop[id]);
    totalPerfTime[id] += perfTime[id];
    perfFrameCount[id]++;

    if (perfTime[id] > 30000)
        perfOver30ms[id]++;
}

OMX_U32 Exynos_OSAL_PerfFrame(PERF_ID_TYPE id)
{
    return perfTime[id];
}

OMX_U32 Exynos_OSAL_PerfTotal(PERF_ID_TYPE id)
{
    return totalPerfTime[id];
}

OMX_U32 Exynos_OSAL_PerfFrameCount(PERF_ID_TYPE id)
{
    return perfFrameCount[id];
}

int Exynos_OSAL_PerfOver30ms(PERF_ID_TYPE id)
{
    return perfOver30ms[id];
}

void Exynos_OSAL_PerfPrint(OMX_STRING prefix, PERF_ID_TYPE id)
{
    OMX_U32 perfTotal;
    int frameCount;

    frameCount = Exynos_OSAL_PerfFrameCount(id);
    perfTotal = Exynos_OSAL_PerfTotal(id);

    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%s][%s] Frame Count: %d", __FUNCTION__, prefix, frameCount);
    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%s][%s] Avg Time: %.2f ms, Over 30ms: %d",
                prefix, (float)perfTotal / (float)(frameCount * 1000),
                Exynos_OSAL_PerfOver30ms(id));
}

unsigned int Exynos_OSAL_GetPlaneCount(
    OMX_COLOR_FORMATTYPE eOMXFormat,
    PLANE_TYPE           ePlaneType)
{
    unsigned int nPlaneCnt = 0;

    switch ((int)eOMXFormat) {
    case OMX_COLOR_FormatYCbYCr:
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_SEC_COLOR_FormatYVU420Planar:
        nPlaneCnt = 3;
        break;
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar:
    case OMX_COLOR_FormatYUV420Planar16:
    case OMX_SEC_COLOR_FormatNV21Linear:
    case OMX_SEC_COLOR_FormatS10bitYVU420SemiPlanar:
    case OMX_SEC_COLOR_FormatNV12Tiled:
    case OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC:
    case OMX_SEC_COLOR_FormatYVU420SemiPlanarSBWC:
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC:
    case OMX_SEC_COLOR_Format10bitYVU420SemiPlanarSBWC:
    case OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L50:
    case OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L75:
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L40:
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L60:
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L80:
        nPlaneCnt = 2;
        break;
    case OMX_COLOR_Format32bitARGB8888:
    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_COLOR_Format32BitRGBA8888:
        nPlaneCnt = 1;
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unsupported color format(0x%x).", __FUNCTION__, eOMXFormat);
        nPlaneCnt = 0;
        break;
    }

    if ((ePlaneType & PLANE_SINGLE) &&
        (nPlaneCnt > 0)) {
        nPlaneCnt = 1;
    }

    return nPlaneCnt;
}

void Exynos_OSAL_GetPlaneSize(
    OMX_COLOR_FORMATTYPE    eColorFormat,
    PLANE_TYPE              ePlaneType,
    OMX_U32                 nWidth,
    OMX_U32                 nHeight,
    unsigned int            nDataLen[MAX_BUFFER_PLANE],
    unsigned int            nAllocLen[MAX_BUFFER_PLANE])
{
    switch ((int)eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = (nWidth * nHeight) * 3 / 2;
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = (ALIGN(nWidth, 16) * ALIGN(nHeight, 16) + 256) +
                           (ALIGN((ALIGN(nWidth >> 1, 16) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN) + 256), 16) * 2) +
                            ((nWidth % 128)? MSCL_EXT_SIZE:0); /* Scaler Restriction */
        } else {
            nDataLen[0] = nWidth * nHeight;
            nDataLen[1] = nDataLen[0] >> 2;
            nDataLen[2] = nDataLen[1];

            nAllocLen[0] = ALIGN(ALIGN(nWidth, 16) * ALIGN(nHeight, 16), 256) + 256 + ((nWidth % 128)? MSCL_EXT_SIZE:0); /* Scaler Restriction */
            nAllocLen[1] = ALIGN(ALIGN(nWidth >> 1, 16) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN), 256) + 256 +
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0); /* Scaler Restriction */
            nAllocLen[2] = nAllocLen[1];
        }
        break;
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear:
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = (nWidth * nHeight) * 3 / 2;
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = (ALIGN(nWidth, 16) * ALIGN(nHeight, 16) + 256) +
                           (ALIGN((ALIGN(nWidth, 16) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN) + 256), 16)) +
                            ((nWidth % 128)? MSCL_EXT_SIZE:0); /* Scaler Restriction */
        } else {
            nDataLen[0] = nWidth * nHeight;
            nDataLen[1] = nDataLen[0] >> 1;
            nDataLen[2] = 0;

            nAllocLen[0] = ALIGN(ALIGN(nWidth, 16) * ALIGN(nHeight, 16), 256) + 256 + ((nWidth % 128)? MSCL_EXT_SIZE:0); /* Scaler Restriction */
            nAllocLen[1] = ALIGN(ALIGN(nWidth, 16) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN), 256) + 256 +
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0); /* Scaler Restriction */
        }
        break;
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar:
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatS10bitYVU420SemiPlanar:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = (nWidth * nHeight) * 3 / 2;
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = ((ALIGN(nWidth, S10B_FORMAT_8B_ALIGNMENT) * ALIGN(nHeight, 16) + 256) +  /* Y 8bit */
                            (ALIGN(nWidth / 4, 16) * ALIGN(nHeight, 16) + 64)) +                   /* Y 2bit */
                           ((ALIGN((ALIGN(nWidth, S10B_FORMAT_8B_ALIGNMENT) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN) + 256), 16)) +  /* CbCr 8bit */
                            (ALIGN(nWidth / 4, 16) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN) + 64)) +                                 /* CbCr 2bit */
                            ((nWidth % 128)? MSCL_EXT_SIZE:0); /* Scaler Restriction */
        } else {
            nDataLen[0] = nWidth * nHeight;
            nDataLen[1] = nWidth * (nHeight / 2);
            nDataLen[2] = 0;

            nAllocLen[0] = ALIGN((ALIGN(nWidth, S10B_FORMAT_8B_ALIGNMENT) * ALIGN(nHeight, 16) + 256) +  /* Y 8bit */
                                 (ALIGN(nWidth / 4, 16) * ALIGN(nHeight, 16) + 256), 256) +              /* Y 2bit */
                                ((nWidth % 128)? MSCL_EXT_SIZE:0);                                       /* Scaler Restriction */
            nAllocLen[1] = ALIGN((ALIGN(nWidth, S10B_FORMAT_8B_ALIGNMENT) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN) + 256) +  /* CbCr 8bit */
                                 (ALIGN(nWidth / 4, 16) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN) + 256), 256) +              /* CbCr 2bit */
                                ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0);                                                              /* Scaler Restriction */
        }
        break;
    case OMX_COLOR_FormatYUV420Planar16:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = ((nWidth * 2) * nHeight) * 3 / 2;
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = ((ALIGN(nWidth, 16) * 2) * ALIGN(nHeight, 16) + 256) +                                           /* Y 10bit */
                           (ALIGN(((ALIGN(nWidth, 16) * 2) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN) + 256), 16)) + /* CbCr 10bit */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);                                                               /* Scaler Restriction */
        } else {
            nDataLen[0] = (nWidth * 2) * nHeight;
            nDataLen[1] = nDataLen[0] >> 1;
            nDataLen[2] = 0;

            nAllocLen[0] = ALIGN((ALIGN(nWidth, 16) * 2) * ALIGN(nHeight, 16), 256) + 256 +
                           ((nWidth % 128)? MSCL_EXT_SIZE:0); /* Scaler Restriction */
            nAllocLen[1] = ALIGN((ALIGN(nWidth, 16) * 2) * ALIGN((ALIGN(nHeight, 16) >> 1), CHROMA_VALIGN), 256) + 256 +
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0); /* Scaler Restriction */
        }
        break;
    case OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC:
    case OMX_SEC_COLOR_FormatYVU420SemiPlanarSBWC:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = 0; /* NOT SUPPROT */
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = 0;
        } else {
            nAllocLen[0] = (SBWC_8B_Y_SIZE(nWidth, nHeight) +           /* SBWC Y */
                            SBWC_8B_Y_HEADER_SIZE(nWidth, nHeight)) +   /* SBWC Y HEADER */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);           /* Scaler Restriction */
            nAllocLen[1] = (SBWC_8B_CBCR_SIZE(nWidth, nHeight) +        /* SBWC CBCR */
                            SBWC_8B_CBCR_HEADER_SIZE(nWidth, nHeight)) +/* SBWC CBCR HEADER */
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0);     /* Scaler Restriction */

            nDataLen[0] = nAllocLen[0];
            nDataLen[1] = nAllocLen[1];
            nDataLen[2] = 0;
        }
        break;
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC:
    case OMX_SEC_COLOR_Format10bitYVU420SemiPlanarSBWC:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = 0; /* NOT SUPPORT */
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = 0;
        } else {
            nAllocLen[0] = (SBWC_10B_Y_SIZE(nWidth, nHeight) +           /* SBWC Y */
                            SBWC_10B_Y_HEADER_SIZE(nWidth, nHeight)) +   /* SBWC Y HEADER */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);            /* Scaler Restriction */
            nAllocLen[1] = (SBWC_10B_CBCR_SIZE(nWidth, nHeight) +        /* SBWC CBCR */
                            SBWC_10B_CBCR_HEADER_SIZE(nWidth, nHeight)) +/* SBWC CBCR HEADER */
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0);      /* Scaler Restriction */

            nDataLen[0] = nAllocLen[0];
            nDataLen[1] = nAllocLen[1];
            nDataLen[2] = 0;
        }
        break;
    case OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L50:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = 0; /* NOT SUPPROT */
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = 0;
        } else {
            nAllocLen[0] = SBWCL_8B_Y_SIZE(nWidth, nHeight, 50) +   /* SBWC_L50 Y */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);            /* Scaler Restriction */
            nAllocLen[1] = SBWCL_8B_CBCR_SIZE(nWidth, nHeight, 50) +/* SBWC_L50 CBCR */
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0);      /* Scaler Restriction */

            nDataLen[0] = nAllocLen[0];
            nDataLen[1] = nAllocLen[1];
            nDataLen[2] = 0;
        }
        break;
    case OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L75:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = 0; /* NOT SUPPROT */
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = 0;
        } else {
            nAllocLen[0] = SBWCL_8B_Y_SIZE(nWidth, nHeight, 75) +   /* SBWC_L75 Y */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);       /* Scaler Restriction */
            nAllocLen[1] = SBWCL_8B_CBCR_SIZE(nWidth, nHeight, 75) +/* SBWC_L75 CBCR */
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0); /* Scaler Restriction */

            nDataLen[0] = nAllocLen[0];
            nDataLen[1] = nAllocLen[1];
            nDataLen[2] = 0;
        }
        break;
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L40:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = 0; /* NOT SUPPROT */
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = 0;
        } else {
            nAllocLen[0] = SBWCL_10B_Y_SIZE(nWidth, nHeight, 40) +   /* SBWC_L40 Y */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);        /* Scaler Restriction */
            nAllocLen[1] = SBWCL_10B_CBCR_SIZE(nWidth, nHeight, 40) +/* SBWC_L40 CBCR */
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0); /* Scaler Restriction */

            nDataLen[0] = nAllocLen[0];
            nDataLen[1] = nAllocLen[1];
            nDataLen[2] = 0;
        }

        break;
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L60:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = 0; /* NOT SUPPROT */
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = 0;
        } else {
            nAllocLen[0] = SBWCL_10B_Y_SIZE(nWidth, nHeight, 60) +   /* SBWC_L60 Y */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);        /* Scaler Restriction */
            nAllocLen[1] = SBWCL_10B_CBCR_SIZE(nWidth, nHeight, 60) +/* SBWC_L60 CBCR */
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0);  /* Scaler Restriction */

            nDataLen[0] = nAllocLen[0];
            nDataLen[1] = nAllocLen[1];
            nDataLen[2] = 0;
        }
        break;
    case OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L80:
        if (ePlaneType == PLANE_SINGLE) {
            nDataLen[0] = 0; /* NOT SUPPROT */
            nDataLen[1] = 0;
            nDataLen[2] = 0;

            nAllocLen[0] = 0;
        } else {
            nAllocLen[0] = SBWCL_10B_Y_SIZE(nWidth, nHeight, 80) +   /* SBWC_L80 Y */
                           ((nWidth % 128)? MSCL_EXT_SIZE:0);        /* Scaler Restriction */
            nAllocLen[1] = SBWCL_10B_CBCR_SIZE(nWidth, nHeight, 80) +/* SBWC_L80 CBCR */
                           ((nWidth % 128)? (MSCL_EXT_SIZE / 2):0);  /* Scaler Restriction */

            nDataLen[0] = nAllocLen[0];
            nDataLen[1] = nAllocLen[1];
            nDataLen[2] = 0;
        }
        break;
    case OMX_COLOR_Format32bitARGB8888:
    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_COLOR_Format32BitRGBA8888:
        nDataLen[0] = nWidth * nHeight * 4;
        nDataLen[1] = 0;
        nDataLen[2] = 0;

        nAllocLen[0] = ALIGN(ALIGN(nWidth, 16) * ALIGN(nHeight, 16) * 4, 256) + 256 +
                       ((nWidth % 128)? MSCL_EXT_SIZE:0); /* Scaler Restriction */
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unsupported color format(0x%x).", __FUNCTION__, eColorFormat);
        break;
    }
}

OMX_U32 Exynos_OSAL_GetOutBufferSize(OMX_U32 nWidth, OMX_U32 nHeight, OMX_U32 nDefaultBufferSize) {
    OMX_U32 nBufferSize = 0;

    FunctionIn();

    nBufferSize = nDefaultBufferSize;

    if (((nWidth * nHeight) > 0) && ((nWidth * nHeight) < (3840 * 2160))) {
        if (nBufferSize > CUSTOM_UHD_MFC_OUTPUT_BUFFER_SIZE)
            nBufferSize = CUSTOM_UHD_MFC_OUTPUT_BUFFER_SIZE;
        else
            nBufferSize = (ALIGN(nWidth, 16) * ALIGN(nHeight, 16) * 3) / 2;
    } else if (((nWidth * nHeight) >= (3840 * 2160)) && ((nWidth * nHeight) < (7680 * 4320))) {
        if (nBufferSize > CUSTOM_8K_MFC_OUTPUT_BUFFER_SIZE)
            nBufferSize = CUSTOM_8K_MFC_OUTPUT_BUFFER_SIZE;
        else
            nBufferSize = (ALIGN(nWidth, 16) * ALIGN(nHeight, 16) * 3) / 2 / 2;
    } else if (((nWidth * nHeight) >= (7680 * 4320))) {
        nBufferSize = (ALIGN(nWidth, 16) * ALIGN(nHeight, 16) * 3) / 2 / 4;
    }

EXIT:
    FunctionOut();

    return nBufferSize;
}

static struct {
    ExynosVideoColorFormatType  eVideoFormat[2];  /* multi-FD, single-FD(H/W) */
    OMX_COLOR_FORMATTYPE        eOMXFormat;
} VIDEO_COLORFORMAT_MAP[] = {
{{VIDEO_COLORFORMAT_NV12M, VIDEO_COLORFORMAT_NV12}, OMX_COLOR_FormatYUV420SemiPlanar},
{{VIDEO_COLORFORMAT_NV12M_S10B, VIDEO_COLORFORMAT_NV12_S10B}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar},
{{VIDEO_COLORFORMAT_NV12M_P010, 0 /* not supported */ }, (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16},
{{VIDEO_COLORFORMAT_NV12M_TILED, VIDEO_COLORFORMAT_NV12_TILED}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled},
{{VIDEO_COLORFORMAT_NV21M, 0 /* not supported */}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear},
{{VIDEO_COLORFORMAT_NV21M_S10B, 0 /* not supported */}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatS10bitYVU420SemiPlanar},
{{VIDEO_COLORFORMAT_I420M, VIDEO_COLORFORMAT_I420}, OMX_COLOR_FormatYUV420Planar},
{{VIDEO_COLORFORMAT_YV12M, 0 /* not supported */}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar},
{{VIDEO_COLORFORMAT_ARGB8888, VIDEO_COLORFORMAT_ARGB8888}, OMX_COLOR_Format32bitBGRA8888},
{{VIDEO_COLORFORMAT_BGRA8888, VIDEO_COLORFORMAT_BGRA8888}, OMX_COLOR_Format32bitARGB8888},
{{VIDEO_COLORFORMAT_RGBA8888, VIDEO_COLORFORMAT_RGBA8888}, (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888},
{{VIDEO_COLORFORMAT_NV12M_SBWC, VIDEO_COLORFORMAT_NV12_SBWC}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC},
{{VIDEO_COLORFORMAT_NV12M_10B_SBWC, VIDEO_COLORFORMAT_NV12_10B_SBWC}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC},
{{VIDEO_COLORFORMAT_NV21M_SBWC, 0 /* not supported */}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420SemiPlanarSBWC},
{{VIDEO_COLORFORMAT_NV21M_10B_SBWC, 0 /* not supported */}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYVU420SemiPlanarSBWC},
{{VIDEO_COLORFORMAT_NV12M_SBWC_L50, VIDEO_COLORFORMAT_NV12_SBWC_L50}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L50},
{{VIDEO_COLORFORMAT_NV12M_SBWC_L75, VIDEO_COLORFORMAT_NV12_SBWC_L75}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L75},
{{VIDEO_COLORFORMAT_NV12M_10B_SBWC_L40, VIDEO_COLORFORMAT_NV12_10B_SBWC_L40}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L40},
{{VIDEO_COLORFORMAT_NV12M_10B_SBWC_L60, VIDEO_COLORFORMAT_NV12_10B_SBWC_L60}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L60},
{{VIDEO_COLORFORMAT_NV12M_10B_SBWC_L80, VIDEO_COLORFORMAT_NV12_10B_SBWC_L80}, (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L80},
};

int Exynos_OSAL_OMX2VideoFormat(
    OMX_COLOR_FORMATTYPE    eColorFormat,
    PLANE_TYPE              ePlaneType)
{
    ExynosVideoColorFormatType nVideoFormat = VIDEO_COLORFORMAT_UNKNOWN;
    int nVideoFormats = (int)(sizeof(VIDEO_COLORFORMAT_MAP)/sizeof(VIDEO_COLORFORMAT_MAP[0]));
    int i;

    for (i = 0; i < nVideoFormats; i++) {
        if (VIDEO_COLORFORMAT_MAP[i].eOMXFormat == eColorFormat) {
            nVideoFormat = VIDEO_COLORFORMAT_MAP[i].eVideoFormat[(ePlaneType & 0x10)? 1:0];
            break;
        }
    }

    if (nVideoFormat == VIDEO_COLORFORMAT_UNKNOWN) {
        Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%s] color format(0x%x)/plane type(0x%x) is not supported",
                                        __FUNCTION__, eColorFormat, ePlaneType);
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] color format(0x%x)/plane type(0x%x) -> video format(0x%x)",
                                        __FUNCTION__, eColorFormat, ePlaneType, nVideoFormat);

EXIT:

    return (int)nVideoFormat;
}

OMX_COLOR_FORMATTYPE Exynos_OSAL_Video2OMXFormat(
    int nVideoFormat)
{
    OMX_COLOR_FORMATTYPE eOMXFormat = OMX_COLOR_FormatUnused;
    int nVideoFormats = (int)(sizeof(VIDEO_COLORFORMAT_MAP)/sizeof(VIDEO_COLORFORMAT_MAP[0]));
    int i;

    for (i = 0; i < nVideoFormats; i++) {
        if (((int)VIDEO_COLORFORMAT_MAP[i].eVideoFormat[0] == nVideoFormat) ||
            ((int)VIDEO_COLORFORMAT_MAP[i].eVideoFormat[1] == nVideoFormat)) {
            eOMXFormat = VIDEO_COLORFORMAT_MAP[i].eOMXFormat;
            break;
        }
    }

    if (eOMXFormat == OMX_COLOR_FormatUnused) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] video format(0x%x) is not supported", __FUNCTION__, nVideoFormat);
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] video format(0x%x) -> color format(0x%x)", __FUNCTION__, nVideoFormat, eOMXFormat);

EXIT:


    return eOMXFormat;
}

static struct {
    unsigned int         nHALFormat[PLANE_MAX_NUM];  /* multi-FD, single-FD(H/W), sigle-FD(general) */
    OMX_COLOR_FORMATTYPE eOMXFormat;
} HAL_COLORFORMAT_MAP[] = {
/* NV12 format */
#ifdef USE_PRIV_FORMAT
{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_PRIV, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP},
  OMX_COLOR_FormatYUV420SemiPlanar},
#endif
{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP},
  OMX_COLOR_FormatYUV420SemiPlanar},

{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_S10B, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatS10bitYUV420SemiPlanar},

{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M, 0 /* not implemented */, HAL_PIXEL_FORMAT_YCBCR_P010},
 (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16},

/* NV12T format */
{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_TILED, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled},

/* NV21 format */
{{HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M, 0 /* not implemented */, HAL_PIXEL_FORMAT_YCrCb_420_SP},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear},

{{0 /* not implemented */, 0 /* not implemented */, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatS10bitYVU420SemiPlanar},

/* I420P format */
{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_PN, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P},
 OMX_COLOR_FormatYUV420Planar},

/* YV12 format */
{{HAL_PIXEL_FORMAT_EXYNOS_YV12_M, 0 /* not implemented */, HAL_PIXEL_FORMAT_YV12},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar},

/* RGB format */
{{HAL_PIXEL_FORMAT_RGBA_8888, HAL_PIXEL_FORMAT_RGBA_8888, HAL_PIXEL_FORMAT_RGBA_8888},
 (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32BitRGBA8888},

{{HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888, HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888, HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888},
 OMX_COLOR_Format32bitBGRA8888},

{{HAL_PIXEL_FORMAT_BGRA_8888, HAL_PIXEL_FORMAT_BGRA_8888, HAL_PIXEL_FORMAT_BGRA_8888},
 OMX_COLOR_Format32bitARGB8888},

/* SBWC format */
{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC},

{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC},

{{HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_SBWC, 0 /* not implemented */, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420SemiPlanarSBWC},

{{HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_10B_SBWC, 0 /* not implemented */, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYVU420SemiPlanarSBWC},

/* SBWC Lossy format */
{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L50, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L50, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L50},

{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L75, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L75, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYUV420SemiPlanarSBWC_L75},

{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L40, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L40, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L40},

{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L60, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L60, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L60},

{{HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L80, HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L80, 0 /* not implemented */},
 (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format10bitYUV420SemiPlanarSBWC_L80},
};

OMX_COLOR_FORMATTYPE Exynos_OSAL_HAL2OMXColorFormat(
    unsigned int nHALFormat)
{
    OMX_COLOR_FORMATTYPE eOMXFormat = OMX_COLOR_FormatUnused;
    int nHALFormats = (int)(sizeof(HAL_COLORFORMAT_MAP)/sizeof(HAL_COLORFORMAT_MAP[0]));
    int i;

    for (i = 0; i < nHALFormats; i++) {
        if ((HAL_COLORFORMAT_MAP[i].nHALFormat[0] == nHALFormat) ||
            (HAL_COLORFORMAT_MAP[i].nHALFormat[1] == nHALFormat)
#if 0
            || (HAL_COLORFORMAT_MAP[i].nHALFormat[2] == nHALFormat)  /* userspace format : not used in vendor scenario */
#endif
            ) {
            eOMXFormat = HAL_COLORFORMAT_MAP[i].eOMXFormat;
            break;
        }
    }

    if (eOMXFormat == OMX_COLOR_FormatUnused) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] HAL format(0x%x) is not supported", __FUNCTION__, nHALFormat);
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] HAL format(0x%x) -> color format(0x%x)", __FUNCTION__, nHALFormat, eOMXFormat);

EXIT:

    return eOMXFormat;
}

unsigned int Exynos_OSAL_OMX2HALPixelFormat(
    OMX_COLOR_FORMATTYPE eOMXFormat,
    PLANE_TYPE           ePlaneType)
{
    unsigned int nHALFormat = 0;
    int nHALFormats = (int)(sizeof(HAL_COLORFORMAT_MAP)/sizeof(HAL_COLORFORMAT_MAP[0]));
    int i;

    for (i = 0; i < nHALFormats; i++) {
        if (HAL_COLORFORMAT_MAP[i].eOMXFormat == eOMXFormat) {
            nHALFormat = HAL_COLORFORMAT_MAP[i].nHALFormat[ePlaneType & 0xF];
            break;
        }
    }

    if (nHALFormat == 0)
        goto EXIT;

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] color format(0x%x)/plane type(0x%x) -> HAL format(0x%x)",
                                        __FUNCTION__, eOMXFormat, ePlaneType, nHALFormat);

EXIT:

    return nHALFormat;
}

int Exynos_OSAL_DataSpaceToColorSpace(int nDataSpace, int nFormat)
{
    int nColorSpace = CSC_EQ_COLORSPACE_SMPTE170M;
    OMX_BOOL isRGB  = OMX_FALSE;

    if ((nFormat == HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888) ||
        (nFormat == HAL_PIXEL_FORMAT_RGBA_8888) ||
        (nFormat == HAL_PIXEL_FORMAT_BGRA_8888)) {
        /* it depends on
         * libstagefright/colorconversion/ColorConverter.cpp
         * only BT.601 is supported.
         */

        isRGB = OMX_TRUE;
    }

#ifdef USE_ANDROID
    switch (nDataSpace) {
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_HLG | HAL_DATASPACE_STANDARD_BT2020):
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT2020):
    case (HAL_DATASPACE_RANGE_LIMITED | HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_STANDARD_BT2020):
    case HAL_DATASPACE_BT2020:
    case HAL_DATASPACE_BT2020_PQ:
        nColorSpace = CSC_EQ_COLORSPACE_BT2020;
#ifndef USE_BT2020_SUPPORT
        if (isRGB) {
            nColorSpace = CSC_EQ_COLORSPACE_REC709;  /* GPU(BT.709) : default about BT.2020 */
#ifndef USE_BT709_SUPPORT
            nColorSpace = CSC_EQ_COLORSPACE_SMPTE170M;  /* GPU(BT.601 only) : default */
#endif
        }
#endif
        break;
    case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT709):
    case HAL_DATASPACE_BT709:
    case HAL_DATASPACE_V0_BT709:
        nColorSpace = CSC_EQ_COLORSPACE_REC709;  /* BT.709 */
#ifndef USE_BT709_SUPPORT
        if (isRGB) {
            nColorSpace = CSC_EQ_COLORSPACE_SMPTE170M;  /* GPU(BT.601 only) : default */
        }
#endif
        break;
    case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_625):
    case HAL_DATASPACE_BT601_625:
    case HAL_DATASPACE_V0_BT601_625:
    case (HAL_DATASPACE_RANGE_FULL | HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_STANDARD_BT601_525):
    case HAL_DATASPACE_BT601_525:
    case HAL_DATASPACE_V0_BT601_525:
    default:
        nColorSpace = CSC_EQ_COLORSPACE_SMPTE170M;  /* BT.601 */
        break;
    };
#endif

    return nColorSpace;
}

void Exynos_OSAL_GetRGBColorTypeForBitStream(
    EXYNOS_OMX_VIDEO_COLORASPECTS *pColorAspects,
    int                            nDataSpace,
    int                            nFormat)
{
    int colorspace = CSC_EQ_COLORSPACE_SMPTE170M;

    /* for enabling VUI encoding, set range value if it is not set */
    if (pColorAspects->nRangeType == RANGE_UNSPECIFIED) {
        pColorAspects->nRangeType = RANGE_LIMITED;  /* preferred value */
    }

    /* decides transfer vlaue depended on other modules  */
    colorspace = Exynos_OSAL_DataSpaceToColorSpace(nDataSpace, nFormat);

    switch(colorspace) {
    case CSC_EQ_COLORSPACE_SMPTE170M:  /* uses BT.601 forcefully */
        pColorAspects->nPrimaryType     = PRIMARIES_BT601_6_625;
        pColorAspects->nCoeffType       = MATRIX_COEFF_SMPTE170M;
        pColorAspects->nTransferType    = TRANSFER_SMPTE_170M;
        break;
    case CSC_EQ_COLORSPACE_REC709:  /* uses BT.709 furcefully */
    default:  /* best effort. uses BT.709 */
        pColorAspects->nPrimaryType     = PRIMARIES_BT709_5;
        pColorAspects->nCoeffType       = MATRIX_COEFF_REC709;
        pColorAspects->nTransferType    = TRANSFER_BT709;
        break;
    };

    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%s] colorspace: %d", __FUNCTION__, colorspace);

    return ;
}


#ifdef PERFORMANCE_DEBUG

typedef enum _DEBUG_LEVEL
{
    PERF_LOG_OFF,
    PERF_LOG_ON,
} EXYNOS_DEBUG_LEVEL;

static int gPerfLevel = PERF_LOG_OFF;

void Exynos_OSAL_Get_Perf_Property()
{
    char perfProp[PROPERTY_VALUE_MAX] = { 0, };

    if (property_get("debug.omx.perf", perfProp, NULL) > 0) {
        if(!(Exynos_OSAL_Strncmp(perfProp, "1", 1)))
            gPerfLevel = PERF_LOG_ON;
        else
            gPerfLevel = PERF_LOG_OFF;
    }
}

typedef struct _BUFFER_INFO
{
    OMX_BUFFERHEADERTYPE *pBufferHeader;
    OMX_TICKS             timestamp;            /* given from framework */
    struct timeval        currentTime;          /* system time */
    OMX_TICKS             previousTimeStamp;
    struct timeval        previousTime;
    OMX_S32               nCountInOMX;


    struct timeval        queueTime;            /* system time : VL42 qbuf */
    struct timeval        dequeueTime;          /* system time : V4L2 dqbuf */
    OMX_S32               nCountInV4L2;
} BUFFER_INFO;

typedef struct _EXYNOS_OMX_PERF_INFO
{
    OMX_HANDLETYPE mutex;

    OMX_S32        nCountInOMX;
    OMX_S32        nCountInV4L2;

    OMX_S32        nCurSlotIndex;
    OMX_S32        nNextSlotIndex;
    BUFFER_INFO    sBufferTime[MAX_TIMESTAMP];

    OMX_TICKS      latestOutTimestamp;
} EXYNOS_OMX_PERF_INFO;

OMX_ERRORTYPE Exynos_OSAL_CountCreate(OMX_HANDLETYPE *hPerfInfo)
{
    OMX_ERRORTYPE           ret         = OMX_ErrorNone;
    EXYNOS_OMX_PERF_INFO   *pPerfInfo   = NULL;

    if (hPerfInfo == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /* it only works when perperty is enabled */
    if (gPerfLevel == PERF_LOG_OFF) {
        (*hPerfInfo) = NULL;
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    pPerfInfo = (EXYNOS_OMX_PERF_INFO *)Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_PERF_INFO));
    if (pPerfInfo == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to malloc", __FUNCTION__);
        (*hPerfInfo) = NULL;
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    Exynos_OSAL_Memset((OMX_PTR)pPerfInfo, 0, sizeof(EXYNOS_OMX_PERF_INFO));

    ret = Exynos_OSAL_MutexCreate(&(pPerfInfo->mutex));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to MutexCreate", __FUNCTION__);
        Exynos_OSAL_Free(pPerfInfo);
        (*hPerfInfo) = NULL;
        goto EXIT;
    }

    (*hPerfInfo) = pPerfInfo;

EXIT:

    return ret;
}

void Exynos_OSAL_CountTerminate(OMX_HANDLETYPE *hPerfInfo)
{
    EXYNOS_OMX_PERF_INFO *pPerfInfo = (EXYNOS_OMX_PERF_INFO *)(*hPerfInfo);

    if (pPerfInfo == NULL)
        return;

    Exynos_OSAL_MutexTerminate(pPerfInfo->mutex);
    Exynos_OSAL_Free(pPerfInfo);
    (*pPerfInfo) = NULL;

    return;
}

OMX_S32 Exynos_OSAL_CountIncrease(
    OMX_HANDLETYPE          *hPerfInfo,
    OMX_BUFFERHEADERTYPE    *pBufferHeader,
    int                      nPortIndex)
{
    OMX_S32                  nCountInOMX   = 0;
    EXYNOS_OMX_PERF_INFO    *pPerfInfo     = (EXYNOS_OMX_PERF_INFO *)hPerfInfo;

    struct timeval   currentTime;
    struct tm       *ptm  = NULL;
    char time_string[40] = { 0, };

    int nSlotIndex = 0;
    int i;

    if (pPerfInfo == NULL)
        return 0;

    if (pBufferHeader == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] bufferHeader is NULL", __FUNCTION__);
        return pPerfInfo->nCountInOMX;
    }

    Exynos_OSAL_MutexLock(pPerfInfo->mutex);

    pPerfInfo->nCountInOMX++;  /* queued to OMX's port */
    nCountInOMX = pPerfInfo->nCountInOMX;

    nSlotIndex = pPerfInfo->nNextSlotIndex;

    /* if could not find an empty slot at the past */
    if (nSlotIndex < 0) {
        /* re-try to find an empty slot */
        nSlotIndex = (pPerfInfo->nCurSlotIndex + 1) % MAX_TIMESTAMP;
        for (i = 0; i < MAX_TIMESTAMP; i++) {
            if (pPerfInfo->sBufferTime[nSlotIndex].pBufferHeader == NULL)
                break;

            nSlotIndex = (nSlotIndex + 1) % MAX_TIMESTAMP;
        }

        if (i >= MAX_TIMESTAMP) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Can not find a empty slot", __FUNCTION__);
        } else {
            pPerfInfo->sBufferTime[nSlotIndex].previousTimeStamp = pPerfInfo->sBufferTime[pPerfInfo->nCurSlotIndex].timestamp;
            Exynos_OSAL_Memcpy(&pPerfInfo->sBufferTime[nSlotIndex].previousTime,
                               &pPerfInfo->sBufferTime[pPerfInfo->nCurSlotIndex].timestamp, sizeof(struct timeval));
            pPerfInfo->nNextSlotIndex = nSlotIndex;
        }
    }

    /* save buffer header and timestamp */
    pPerfInfo->sBufferTime[nSlotIndex].pBufferHeader    = pBufferHeader;
    pPerfInfo->sBufferTime[nSlotIndex].timestamp        = pBufferHeader->nTimeStamp;

    /* get currnet system time and save this for performance measurement */
    gettimeofday(&(currentTime), NULL);
    Exynos_OSAL_Memcpy(&pPerfInfo->sBufferTime[nSlotIndex].currentTime, &currentTime, sizeof(currentTime));
    pPerfInfo->sBufferTime[nSlotIndex].nCountInOMX = pPerfInfo->nCountInOMX;
    pPerfInfo->nCurSlotIndex = nSlotIndex;

    /* find an empty slot for next */
    nSlotIndex = (pPerfInfo->nCurSlotIndex + 1) % MAX_TIMESTAMP;
    for (i = 0; i < MAX_TIMESTAMP; i++) {
        if (pPerfInfo->sBufferTime[nSlotIndex].pBufferHeader == NULL)
            break;

        nSlotIndex = (nSlotIndex + 1) % MAX_TIMESTAMP;
    }

    if (i >= MAX_TIMESTAMP) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Can not find a empty slot", __FUNCTION__);
    } else {
        /* save cur info to next slot that will have future info */
        pPerfInfo->sBufferTime[nSlotIndex].previousTimeStamp = pPerfInfo->sBufferTime[pPerfInfo->nCurSlotIndex].timestamp;
        Exynos_OSAL_Memcpy(&pPerfInfo->sBufferTime[nSlotIndex].previousTime,
                           &pPerfInfo->sBufferTime[pPerfInfo->nCurSlotIndex].timestamp, sizeof(struct timeval));
        pPerfInfo->nNextSlotIndex = nSlotIndex;
    }

    ptm = localtime(&currentTime.tv_sec);
    strftime(time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] %s time = %s.%03ld, header:0x%x, OMX Count:%d", __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "ETB":"FTB",
                                            time_string, (currentTime.tv_usec / 1000), pBufferHeader, nCountInOMX);

    Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);

    return nCountInOMX;
}

OMX_S32 Exynos_OSAL_CountDecrease(
    OMX_HANDLETYPE          *hPerfInfo,
    OMX_BUFFERHEADERTYPE    *pBufferHeader,
    int                      nPortIndex)
{
    OMX_S32                  nCountInOMX   = 0;
    EXYNOS_OMX_PERF_INFO    *pPerfInfo     = (EXYNOS_OMX_PERF_INFO *)hPerfInfo;

    struct timeval   currentTime;
    struct tm       *ptm  = NULL;
    char time_string[40] = { 0, };

    int nSlotIndex = 0;
    int i;

    if (pPerfInfo == NULL)
        return 0;

    if (pBufferHeader == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] bufferHeader is NULL", __FUNCTION__);
        return pPerfInfo->nCountInOMX;
    }

    Exynos_OSAL_MutexLock(pPerfInfo->mutex);

    /* find a buffer */
    for (i = 0; i < MAX_TIMESTAMP; i++) {
        if (pPerfInfo->sBufferTime[i].pBufferHeader == pBufferHeader) {
            /* clear info */
            pPerfInfo->sBufferTime[i].pBufferHeader = NULL;

            if (nPortIndex == OUTPUT_PORT_INDEX)
                pPerfInfo->latestOutTimestamp = pPerfInfo->sBufferTime[i].timestamp;

            nSlotIndex = i;
            break;
        }
    }

    if (i >= MAX_TIMESTAMP) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Can not find %p in slot", __FUNCTION__, pBufferHeader);
        Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);
        return pPerfInfo->nCountInOMX;
    }

    pPerfInfo->nCountInOMX--;  /* dequeued to OMX's port */
    nCountInOMX = pPerfInfo->nCountInOMX;

    gettimeofday(&(currentTime), NULL);
    ptm = localtime(&currentTime.tv_sec);
    strftime(time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] %s time = %s.%03ld, header:0x%x, OMX Count:%d", __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "EBD":"FBD",
                                            time_string, (currentTime.tv_usec / 1000), pBufferHeader, nCountInOMX);

    Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);

    return nCountInOMX;
}

OMX_S32 Exynos_OSAL_V4L2CountIncrease(
    OMX_HANDLETYPE          *hPerfInfo,
    OMX_BUFFERHEADERTYPE    *pBufferHeader,
    int                      nPortIndex)
{
    OMX_S32                  nCountInV4L2  = 0;
    EXYNOS_OMX_PERF_INFO    *pPerfInfo     = (EXYNOS_OMX_PERF_INFO *)hPerfInfo;

    struct timeval   currentTime;
    struct tm       *ptm  = NULL;
    char time_string[40] = { 0, };

    int nSlotIndex = 0;
    int i;

    if (pPerfInfo == NULL)
        return 0;

    if (pBufferHeader == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] bufferHeader is NULL", __FUNCTION__);
        return pPerfInfo->nCountInV4L2;
    }

    Exynos_OSAL_MutexLock(pPerfInfo->mutex);

    for (i = 0; i < MAX_TIMESTAMP; i++) {
        if (pPerfInfo->sBufferTime[i].pBufferHeader == pBufferHeader) {
            nSlotIndex = i;
            break;
        }
    }

    if (i >= MAX_TIMESTAMP) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Can not find %p in slot", __FUNCTION__, pBufferHeader);
        Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);
        return pPerfInfo->nCountInOMX;
    }

    pPerfInfo->nCountInV4L2++;  /* queued to MFC */

    gettimeofday(&(currentTime), NULL);
    Exynos_OSAL_Memcpy(&pPerfInfo->sBufferTime[nSlotIndex].queueTime, &currentTime, sizeof(currentTime));
    pPerfInfo->sBufferTime[nSlotIndex].nCountInV4L2 = pPerfInfo->nCountInV4L2;
    nCountInV4L2 = pPerfInfo->V4L2QCount;


    ptm = localtime(&pPerfInfo->sBufferTime[nSlotIndex].queueTime.tv_sec);
    strftime(time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] V4L2 %s time = %s.%03ld, header:0x%x, V4L2 Count:%d", __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "SRC Q":"DST Q",
                                            time_string, (currentTime.tv_usec / 1000), pBufferHeader, nCountInV4L2);

    Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);

    return nCountInV4L2;
}

OMX_S32 Exynos_OSAL_V4L2CountDecrease(
    OMX_HANDLETYPE          *hPerfInfo,
    OMX_BUFFERHEADERTYPE    *pBufferHeader,
    int                      nPortIndex)
{
    OMX_S32                  nCountInV4L2  = 0;
    EXYNOS_OMX_PERF_INFO    *pPerfInfo     = (EXYNOS_OMX_PERF_INFO *)hPerfInfo;

    struct timeval   currentTime;
    struct tm       *ptm  = NULL;
    char time_string[40] = { 0, };

    int nSlotIndex = 0;
    int i;

    if (pPerfInfo == NULL)
        return 0;

    if (pBufferHeader == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] bufferHeader is NULL", __FUNCTION__);
        return pPerfInfo->nCountInV4L2;
    }

    Exynos_OSAL_MutexLock(pPerfInfo->mutex);

    for (i = 0; i < MAX_TIMESTAMP; i++) {
        if (pPerfInfo->sBufferTime[i].pBufferHeader == pBufferHeader) {
            nSlotIndex = i;
            break;
        }
    }

    if (i >= MAX_TIMESTAMP) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Can not find %p in slot", __FUNCTION__, pBufferHeader);
        Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);
        return pPerfInfo->nCountInOMX;
    }

    pPerfInfo->nCountInV4L2--;  /* dequeued to MFC */

    gettimeofday(&(currentTime), NULL);
    Exynos_OSAL_Memcpy(&pPerfInfo->sBufferTime[nSlotIndex].queueTime, &currentTime, sizeof(currentTime));
    pPerfInfo->sBufferTime[nSlotIndex].nCountInV4L2 = pPerfInfo->nCountInV4L2;
    nCountInV4L2 = pPerfInfo->V4L2QCount;

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        pPerfInfo->sBufferTime[nSlotIndex].timestamp            = pBufferHeader->nTimeStamp;
        pPerfInfo->sBufferTime[nSlotIndex].previousTimeStamp    = pPerfInfo->latestOutTimestamp;
    }

    ptm = localtime(&pPerfInfo->sBufferTime[index].dequeueTime.tv_sec);
    strftime(time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
    Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] V4L2 %s time = %s.%03ld, header:0x%x, V4L2 Count:%d", __FUNCTION__,
                                            (nPortIndex == INPUT_PORT_INDEX)? "SRC DQ":"DST DQ",
                                            time_string, (currentTime.tv_usec / 1000), pBufferHeader, nCountInV4L2);

    Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);

    return nCountInV4L2;
}

void Exynos_OSAL_CountReset(OMX_HANDLETYPE *hPerfInfo)
{
    EXYNOS_OMX_PERF_INFO *pPerfInfo = (EXYNOS_OMX_PERF_INFO *)hPerfInfo;

    if (pPerfInfo == NULL)
        return;

    Exynos_OSAL_MutexLock(pPerfInfo->mutex);

    pPerfInfo->nCountInOMX = 0;
    pPerfInfo->nCountInV4L2 = 0;

    pPerfInfo->nCurSlotIndex = 0;
    pPerfInfo->nNextSlotIndex = 1;
    Exynos_OSAL_Memset(&pPerfInfo->sBufferTime, 0, sizeof(sBufferTime));

    pPerfInfo->latestOutTimestamp = 0;

    Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);

    return;
}

#if 0
OMX_ERRORTYPE Exynos_OSAL_GetCountInfoUseOMXBuffer(OMX_HANDLETYPE *hPerfInfo, OMX_BUFFERHEADERTYPE *OMXBufferHeader, BUFFER_TIME *pBufferInfo)
{
    OMX_ERRORTYPE     ret         = OMX_ErrorNone;
    EXYNOS_OMX_PERF_INFO *countHandle = (EXYNOS_OMX_PERF_INFO *)hCountHandle;
    OMX_U32 i = 0;

    if ((countHandle == NULL) ||
        (OMXBufferHeader == NULL) ||
        (pBufferInfo == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Error : OMX_ErrorBadParameter");
        return OMX_ErrorBadParameter;
    }

    if (OMX_ErrorNone == Exynos_OSAL_MutexLock(countHandle->mutex)) {
        for (i = 0; i < MAX_TIMESTAMP; i++) {
            if (countHandle->sBufferTime[i].pBufferHeader == OMXBufferHeader) {
                Exynos_OSAL_Memcpy(pBufferInfo, &(countHandle->sBufferTime[i]), sizeof(BUFFER_TIME));
                break;
            }
        }
        Exynos_OSAL_MutexUnlock(countHandle->mutex);
        if (i >= MAX_TIMESTAMP) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "can not find a bufferHeader !!");
            ret = OMX_ErrorNoMore;
        }
    }
    return ret;
}
#endif

static OMX_ERRORTYPE Exynos_OSAL_GetPerfInfoUseTimestamp(
    OMX_HANDLETYPE  *hPerfInfo,
    OMX_TICKS        timestamp,
    BUFFER_INFO     *pBufferInfo)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    EXYNOS_OMX_PERF_INFO    *pPerfInfo  = (EXYNOS_OMX_PERF_INFO *)hCountHandle;
    OMX_U32 i = 0;


    if ((pPerfInfo == NULL) ||
        (pBufferInfo == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        ret =  OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_MutexLock(pPerfInfo->mutex);

    for (i = 0; i < MAX_TIMESTAMP; i++) {
        if (pPerfInfo->sBufferTime[i].timestamp == timestamp) {
            Exynos_OSAL_Memcpy(pBufferInfo, &(pPerfInfo->sBufferTime[i]), sizeof(BUFFER_INFO));
            break;
        }
    }

    if (i >= MAX_TIMESTAMP) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Can not find %p in slot", __FUNCTION__, pBufferHeader);
        ret = OMX_ErrorNoMore;
    }

    Exynos_OSAL_MutexUnlock(pPerfInfo->mutex);

EXIT:
    return ret;
}

void Exynos_OSAL_PrintCountInfo(
    OMX_HANDLETYPE          *hSrcPerfInfo,
    OMX_HANDLETYPE          *hDstPerfInfo,
    OMX_BUFFERHEADERTYPE    *pBufferHeader)
{
    OMX_ERRORTYPE         ret          = OMX_ErrorNone;
    EXYNOS_OMX_PERF_INFO *pSrcPerfInfo = (EXYNOS_OMX_PERF_INFO *)hSrcPerfInfo;
    EXYNOS_OMX_PERF_INFO *pDstPerfInfo = (EXYNOS_OMX_PERF_INFO *)hDstPerfInfo;

    BUFFER_INFO srcBufferInfo, dstBufferInfo;

    struct timeval   currentTime;
    struct tm       *ptm  = NULL;
    char time_string[40] = { 0, };
    long milliseconds;

    OMX_S32 srcOMXtoV4L2 = 0;
    OMX_S32 srcV4L2toOMX = 0;
    OMX_S32 dstOMXtoV4L2 = 0;
    OMX_S32 dstV4L2toOMX = 0;

    OMX_S32 ETBFTB = 0;
    OMX_S32 ETBFBD = 0;
    OMX_S32 FTBFBD = 0;

    OMX_S32 srcQdstQ = 0;
    OMX_S32 srcQdstDQ = 0;
    OMX_S32 dstQdstDQ = 0;

    Exynos_OSAL_Memset(&srcBufferInfo, 0, sizeof(srcBufferInfo));
    Exynos_OSAL_Memset(&dstBufferInfo, 0, sizeof(dstBufferInfo));

    ret = Exynos_OSAL_GetPerfInfoUseTimestamp(hSrcPerfInfo, pBufferHeader->nTimeStamp, &srcBufferInfo);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Failed to GetPerfInfoUseTimestamp about src", __FUNCTION__);
        return;
    }

    ret = Exynos_OSAL_GetPerfInfoUseTimestamp(hDstPerfInfo, pBufferHeader->nTimeStamp, &dstBufferInfo);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Failed to GetPerfInfoUseTimestamp about dst", __FUNCTION__);
        return;
    }

    /* internal */
    {
        gettimeofday(&currentTime, NULL);

        srcOMXtoV4L2 = Exynos_OSAL_MeasureTime(&(srcBufferInfo.currentTime), &(srcBufferInfo.queueTime)) / 1000;
        //srcV4L2toOMX = Exynos_OSAL_MeasureTime(&(srcBufferInfo.queueTime), &(srcBufferInfo.?????????????????)) / 1000;

        dstOMXtoV4L2 = Exynos_OSAL_MeasureTime(&(dstBufferInfo.currentTime), &(dstBufferInfo.queueTime)) / 1000;
        dstV4L2toOMX = Exynos_OSAL_MeasureTime(&(dstBufferInfo.dequeueTime), &currentTime) / 1000;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] INTERNAL: srcOMXtoV4L2:%d, srcV4L2toOMX:%d, dstOMXtoV4L2:%d, dstV4L2toOMX:%d",
                                            __FUNCTION__, srcOMXtoV4L2, srcV4L2toOMX, dstOMXtoV4L2, dstV4L2toOMX);
    }

    /* vl42 */
    {
        srcQdstQ = Exynos_OSAL_MeasureTime(&(srcBufferInfo.queueTime), &(dstBufferInfo.queueTime)) / 1000;
        srcQdstDQ = Exynos_OSAL_MeasureTime(&(srcBufferInfo.queueTime), &(dstBufferInfo.dequeueTime)) / 1000;
        dstQdstDQ = Exynos_OSAL_MeasureTime(&(dstBufferInfo.queueTime), &(dstBufferInfo.dequeueTime)) / 1000;

        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] V4L2: srcQdstQ:%d, srcQdstDQ:%d, dstQdstDQ:%d, src-V4L2QBufferCount:%d, dst-V4L2QBufferCount:%d",
                                            __FUNCTION__, srcQdstQ, srcQdstDQ, dstQdstDQ, srcBufferInfo.nCountInV4L2, dstBufferInfo.nCountInV4L2);
    }

    /* buffer rotation */
    {
        ETBFTB = Exynos_OSAL_MeasureTime(&(srcBufferInfo.currentTime), &(dstBufferInfo.currentTime)) / 1000;
        ETBFBD = Exynos_OSAL_MeasureTime(&(srcBufferInfo.currentTime), &currentTime) / 1000;
        FTBFBD = Exynos_OSAL_MeasureTime(&(dstBufferInfo.currentTime), &currentTime) / 1000;
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] BUFFER: ETBFTB:%d, ETBFBD:%d, FTBFBD:%d, src-OMXQBufferCount:%d, dst-OMXQBufferCount:%d",
                                            __FUNCTION__, ETBFTB, ETBFBD, FTBFBD, srcBufferInfo.nCountInOMX, dstBufferInfo.nCountInOMX);

        if (ETBFTB > 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] BUFFER: OUTPUT is slow!! real decode time:%d",
                                                __FUNCTION__, FTBFBD / dstBufferInfo.nCountInOMX);
        } else if (ETBFTB < 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] BUFFER: INPUT is slow!! real decode time:%d",
                                                __FUNCTION__, ETBFBD / srcBufferInfo.nCountInOMX);
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "[%s] BUFFER: real decode time:%d or %d",
                                                __FUNCTION__, ETBFBD / srcBufferInfo.nCountInOMX, FTBFBD / dstBufferInfo.nCountInOMX);
        }
    }

    /* buffer interval */
    {
        OMX_S32 srcBufferInterval;
        OMX_S32 dstBufferInterval;

        srcBufferInterval = Exynos_OSAL_MeasureTime(&(srcBufferInfo.previousIncomingTime), &(srcBufferInfo.currentIncomingTime)) / 1000;
        dstBufferInterval = Exynos_OSAL_MeasureTime(&(dstBufferInfo.previousIncomingTime), &(dstBufferInfo.currentIncomingTime)) / 1000;

        if ((srcBufferInterval / (srcBufferInfo.OMXQBufferCount - 1)) > ((dstBufferInfo.timestamp - dstBufferInfo.previousTimeStamp) / 1000))
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "PERFORM:: SLOW: Warning!! src buffer slow. srcBuffer Interval:%d, correct interval:%d, OMXQBufferCount:%d",
                            srcBufferInterval,
                            (srcBufferInfo.OMXQBufferCount == 1) ? srcBufferInterval : (srcBufferInterval / (srcBufferInfo.OMXQBufferCount - 1)),
                            srcBufferInfo.OMXQBufferCount);
        if ((dstBufferInterval / (dstBufferInfo.OMXQBufferCount - 1)) > ((dstBufferInfo.timestamp - dstBufferInfo.previousTimeStamp) / 1000))
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "PERFORM:: SLOW: Warning!! dst buffer slow. dstBuffer Interval:%d, correct interval:%d, OMXQBufferCount:%d",
                            dstBufferInterval,
                            (dstBufferInfo.OMXQBufferCount == 1) ? dstBufferInterval : (dstBufferInterval / (dstBufferInfo.OMXQBufferCount - 1)),
                            dstBufferInfo.OMXQBufferCount);
    }

    {
        OMX_S32 srcTimestampInterval;
        OMX_S32 dstTimestampInterval;

        srcTimestampInterval = ((OMX_S32)(srcBufferInfo.timestamp - srcBufferInfo.previousTimeStamp)) / 1000;
        dstTimestampInterval = ((OMX_S32)(dstBufferInfo.timestamp - dstBufferInfo.previousTimeStamp)) / 1000;

        if ((srcTimestampInterval > 0) && (dstTimestampInterval > 0))
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "PERFORM:: TYPE: Normal timestamp contents");
        else if ((srcTimestampInterval < 0) && (dstTimestampInterval > 0))
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "PERFORM:: TYPE: PTS timestamp contents");
        else if ((srcTimestampInterval > 0) && (dstTimestampInterval < 0))
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "PERFORM:: TYPE: DTS timestamp contents");
        else
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "PERFORM:: TYPE: Timestamp is strange!!");
    }

/*
    ptm = localtime (&srcBufferInfo.currentIncomingTime.tv_sec);
    strftime (time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
    milliseconds = srcBufferInfo.currentIncomingTime.tv_usec / 1000;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "ETB time = %s.%03ld\n", time_string, milliseconds);

    ptm = localtime (&dstBufferInfo.currentIncomingTime.tv_sec);
    strftime (time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
    milliseconds = dstBufferInfo.currentIncomingTime.tv_usec / 1000;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "FTB time = %s.%03ld\n", time_string, milliseconds);

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "return time = %d ms",
                            Exynos_OSAL_MeasureTime(&(srcBufferInfo.currentIncomingTime), &(dstBufferInfo.currentIncomingTime)) / 1000);
*/

    return;
}
#endif
