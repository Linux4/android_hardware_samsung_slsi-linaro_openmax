/*
 * Copyright 2019 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OSAL_ImageConverter.c
 * @brief
 * @author      Taehwan Kim (t_h.kim@samsung.com)
 * @author      Byunggwan Kang (bk0917.kang@samsung.com)
 * @author      Seungbeom Kim (sbcrux.kim@samsung.com)
 * @version     1.0.0
 * @history
 *   2019.02.12 : Create
 */
#include <dlfcn.h>

#include <media/hardware/HardwareAPI.h>
#include <media/hardware/MetadataBufferType.h>
#include <media/stagefright/foundation/ColorUtils.h>

#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Def.h"

#include "Exynos_OSAL_Memory.h"
#include "Exynos_OSAL_Library.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Platform.h"

#include "Exynos_OSAL_ImageConverter.h"

#include "VendorVideoAPI.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "Exynos_OSAL_ImageConverter"
//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

using namespace android;

#ifdef __cplusplus
    extern "C" {
#endif

#define CHECK_HDR10(f, r, p, t, c) \
    ((f == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar16) && \
     (r == ColorAspects::RangeLimited) && \
     (p == ColorAspects::PrimariesBT2020) && \
     (t == ColorAspects::TransferST2084) && \
     (c == ColorAspects::MatrixBT2020))

#define LUMINANCE_DIV_FACTOR 10000.0

#define LIB_NAME            "libImageFormatConverter.so"
#define LIB_FN_NAME_INIT    "CL_HDR2SDR_ARM_init"
#define LIB_FN_NAME_DEINIT  "CL_HDR2SDR_ARM_deinit"
#define LIB_FN_NAME_RUN     "CL_HDR2SDR_ARM_convert"

typedef enum _Image_ColorFormat {
    INVALID         = -1,
    TP10_UBWC_QCOM  = 0,
    P010_LINEAR     = 1,
    NV12            = 2,
    RGBA            = 3,
    YV12            = 4,
    NV21            = 5,
} Image_ColorFormat;

typedef enum _ALGO_t {
    NONE    = 0,
    NORMAL  = 1,
    HI_JACK = 2,
} ALGO_t;

typedef struct _MULTIPLANE_t {
	int     handle;
	void   *host_ptr;
	int     size;
} MULTIPLANE_t;

typedef struct _HDR2SDR_config_params_t {
	Image_ColorFormat color_format;
	MULTIPLANE_t buffer[3];
} HDR2SDR_config_params_t;

typedef struct _HDR10PLUS_DYNAMIC_INFO {
    unsigned char country_code;              // 0xB5
    unsigned short provider_code;            // 0x003C
    unsigned short provider_oriented_code;   // 0x0001

    unsigned char application_identifier;
    unsigned char application_version;

    unsigned int display_max_luminance;
    //unsigned char targeted_system_display_actual_peak_luminance_flag = 0;
    //--------------------------------------------------------------------
    /* window = 1, fixed point at this moment */
    unsigned int  maxscl[3];
    unsigned int  avg_maxrgb;
    unsigned char num_maxrgb_percentiles;
    unsigned char maxrgb_percentages[15];
    unsigned int  maxrgb_percentiles[15];
    //unsigned char fraction_bright_pixels = 1;
    //--------------------------------------------------------------------
    //unsigned char mastering_display_actual_peak_luminance_flag = 0;
    //--------------------------------------------------------------------
    /* window = 1, fixed point at this moment */
    unsigned char  tone_mapping_flag;         // 1
    unsigned short knee_point_x;               //[ ~ 4095]
    unsigned short knee_point_y;               //[ ~ 4095]
    unsigned char  num_bezier_curve_anchors;   // 10 [ ~ 15]
    unsigned short bezier_curve_anchors[15];
    //unsigned char color_saturation_mapping_flag = 0;
    //--------------------------------------------------------------------
} HDR10PLUS_DYNAMIC_INFO;

typedef OMX_BOOL (*ConvertInitFunc)(const unsigned int width, const unsigned int height, const ALGO_t algo, void **user_data, const bool usage);
typedef void     (*ConvertDeinitFunc)(void *user_data);
typedef OMX_BOOL (*ConvertRunFunc)(HDR2SDR_config_params_t *In_params, HDR2SDR_config_params_t *Out_params, HDR10PLUS_DYNAMIC_INFO *meta, unsigned int mastering_max_luminance, void *user_data);

typedef struct _EXYNOS_OMX_IMG_CONV_HANDLE {
    void                    *pLibHandle;
    ConvertInitFunc          Init;
    ConvertDeinitFunc        Deinit;
    ConvertRunFunc           Run;
    void                    *pUserData;
    OMX_BOOL                 bHasDynamicInfo;
    HDR10PLUS_DYNAMIC_INFO  *pDynamicInfo;
} EXYNOS_OMX_IMG_CONV_HANDLE;


OMX_HANDLETYPE Exynos_OSAL_ImgConv_Create(
    OMX_U32 nWidth,
    OMX_U32 nHeight,
    OMX_U32 nMode)
{
    EXYNOS_OMX_IMG_CONV_HANDLE *pHandle = NULL;

    pHandle = (EXYNOS_OMX_IMG_CONV_HANDLE *)Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_IMG_CONV_HANDLE));
    if (pHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
        goto EXIT;
    }

    pHandle->pDynamicInfo = (HDR10PLUS_DYNAMIC_INFO *)Exynos_OSAL_Malloc(sizeof(HDR10PLUS_DYNAMIC_INFO));
    if (pHandle->pDynamicInfo == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
        Exynos_OSAL_Free(pHandle);
        pHandle = NULL;
        goto EXIT;
    }

    pHandle->pLibHandle = Exynos_OSAL_dlopen(LIB_NAME, RTLD_NOW|RTLD_GLOBAL);
    if (pHandle->pLibHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "[%s] Failed to Exynos_OSAL_dlopen() : reason(%s)",
                                __FUNCTION__, Exynos_OSAL_dlerror());
        Exynos_OSAL_Free(pHandle->pDynamicInfo);
        Exynos_OSAL_Free(pHandle);
        pHandle = NULL;
        goto EXIT;
    }

    pHandle->Init   = (ConvertInitFunc)Exynos_OSAL_dlsym(pHandle->pLibHandle, (const char *)LIB_FN_NAME_INIT);
    pHandle->Deinit = (ConvertDeinitFunc)Exynos_OSAL_dlsym(pHandle->pLibHandle, (const char *)LIB_FN_NAME_DEINIT);
    pHandle->Run    = (ConvertRunFunc)Exynos_OSAL_dlsym(pHandle->pLibHandle, (const char *)LIB_FN_NAME_RUN);

    if ((pHandle->Init == NULL) ||
        (pHandle->Deinit == NULL) ||
        (pHandle->Run == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_dlsym()", __FUNCTION__);
        Exynos_OSAL_dlclose(pHandle->pLibHandle);
        Exynos_OSAL_Free(pHandle->pDynamicInfo);
        Exynos_OSAL_Free(pHandle);
        pHandle = NULL;
        goto EXIT;
    }

    if (pHandle->Init(nWidth, nHeight, ((nMode == 0)? NORMAL:HI_JACK), &pHandle->pUserData, false) != OMX_TRUE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Init()", __FUNCTION__);
        Exynos_OSAL_dlclose(pHandle->pLibHandle);
        Exynos_OSAL_Free(pHandle->pDynamicInfo);
        Exynos_OSAL_Free(pHandle);
        pHandle = NULL;
        goto EXIT;
    }

    pHandle->bHasDynamicInfo = OMX_FALSE;

EXIT:
    return (OMX_HANDLETYPE)pHandle;
}

void Exynos_OSAL_ImgConv_Terminate(OMX_HANDLETYPE hImgConv)
{
    EXYNOS_OMX_IMG_CONV_HANDLE *pHandle = (EXYNOS_OMX_IMG_CONV_HANDLE *)hImgConv;

    if (pHandle == NULL)
        return;

    if (pHandle->Deinit != NULL)
        pHandle->Deinit(pHandle->pUserData);

    Exynos_OSAL_dlclose(pHandle->pLibHandle);

    if (pHandle->pDynamicInfo != NULL)
        Exynos_OSAL_Free(pHandle->pDynamicInfo);

    Exynos_OSAL_Free(pHandle);
    pHandle = NULL;
}

OMX_ERRORTYPE Exynos_OSAL_ImgConv_Run(
    OMX_HANDLETYPE           hImgConv,
    OMX_COMPONENTTYPE       *pOMXComponent,
    OMX_PTR                  pBuffer,
    OMX_PTR                  pHDRDynamic)
{
    OMX_ERRORTYPE                ret              = OMX_ErrorNone;
    EXYNOS_OMX_IMG_CONV_HANDLE  *pHandle          = (EXYNOS_OMX_IMG_CONV_HANDLE *)hImgConv;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT         *pExynosPort      = NULL;

    /* graphic buffer */
    EXYNOS_OMX_MULTIPLANE_BUFFER bufferInfo;
    EXYNOS_OMX_LOCK_RANGE range;
    OMX_U32               stride = 0;
    unsigned int nAllocLen[MAX_BUFFER_PLANE] = { 0, 0, 0 };
    unsigned int nDataLen[MAX_BUFFER_PLANE]  = { 0, 0, 0 };

    /* HDR information */
    EXYNOS_OMX_VIDEO_COLORASPECTS *pFWCA = NULL;  /* framework */
    EXYNOS_OMX_VIDEO_COLORASPECTS *pBSCA = NULL;  /* bitstream */
    DescribeColorAspectsParams CA;

    /* parmas for image convert */
    HDR2SDR_config_params_t inConfig, outConfig;
    OMX_U16 max_display_luminance_cd_m2 = 0;

    int i;

    if ((pHandle == NULL) ||
        (pOMXComponent == NULL) ||
        (pBuffer == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] invalid parameters", __FUNCTION__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->codecType == HW_VIDEO_DEC_CODEC) {
        pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

        if (pExynosPort->eMetaDataType != METADATA_TYPE_GRAPHIC) {
            /* not supported */
            Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%p][%s] meta type is wrong(0x%x)",
                                                pExynosComponent, __FUNCTION__, pExynosPort->eMetaDataType);
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        range.nWidth        = pExynosPort->portDefinition.format.video.nFrameWidth;
        range.nHeight       = pExynosPort->portDefinition.format.video.nFrameHeight;
        range.eColorFormat  = pExynosPort->portDefinition.format.video.eColorFormat;

        pFWCA = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].ColorAspects;
        pBSCA = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX].ColorAspects;

        if (Exynos_OSAL_Strstr(pExynosComponent->componentName, "VP9") ||
            Exynos_OSAL_Strstr(pExynosComponent->componentName, "vp9")) {
            /* In case of VP9, should rely on information in webm container */
            getColorAspectsPreferFramework(pBSCA, pFWCA, (void *)&CA);
        } else {
            /* generally, rely on information in bitstream */
            getColorAspectsPreferBitstream(pBSCA, pFWCA, (void *)&CA);
        }
    } else {
        /* not supported */
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    ret = Exynos_OSAL_LockMetaData(pBuffer, range, &stride, &bufferInfo, pExynosPort->eMetaDataType);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%p][%s]: Failed to Exynos_OSAL_LockMetaData()",
                                            pExynosComponent, __FUNCTION__);
        goto EXIT;
    }

    if (!CHECK_HDR10(bufferInfo.eColorFormat, CA.sAspects.mRange, CA.sAspects.mPrimaries,
                    CA.sAspects.mTransfer, CA.sAspects.mMatrixCoeffs)) {
        /* it is not HDR10 */
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    Exynos_OSAL_GetPlaneSize(bufferInfo.eColorFormat, pExynosPort->ePlaneType, stride, range.nHeight, nDataLen, nAllocLen);

    Exynos_OSAL_Memset(&inConfig, 1, sizeof(inConfig));
    Exynos_OSAL_Memset(&outConfig, 1, sizeof(outConfig));

    inConfig.color_format          = P010_LINEAR;
    inConfig.buffer[0].handle      = bufferInfo.fd[0];
    inConfig.buffer[0].size        = nAllocLen[0];
    inConfig.buffer[0].host_ptr    = bufferInfo.addr[0];
    inConfig.buffer[1].handle      = bufferInfo.fd[1];
    inConfig.buffer[1].size        = nAllocLen[1];
    inConfig.buffer[1].host_ptr    = bufferInfo.addr[1];

    outConfig.color_format = P010_LINEAR;

    if (pHDRDynamic != NULL) {
        ExynosHdrDynamicInfo *DY = (ExynosHdrDynamicInfo *)pHDRDynamic;

        if (DY->valid != 0) {
            HDR10PLUS_DYNAMIC_INFO info;
            memset(&info, 0, sizeof(info));

            info.country_code           = DY->data.country_code;
            info.provider_code          = DY->data.provider_code;
            info.provider_oriented_code = DY->data.provider_oriented_code;
            info.application_identifier = DY->data.application_identifier;
            info.application_version    = DY->data.application_version;
#ifdef USE_FULL_ST2094_40
            info.display_max_luminance  = DY->data.targeted_system_display_maximum_luminance;
            for (int i = 0; i < 3; i++) {
                info.maxscl[i] = DY->data.maxscl[0][i];
            }

            info.avg_maxrgb = DY->data.average_maxrgb[0];
            info.num_maxrgb_percentiles = DY->data.num_maxrgb_percentiles[0];
            for (int i = 0; i < info.num_maxrgb_percentiles; i++) {
                info.maxrgb_percentages[i] = DY->data.maxrgb_percentages[0][i];
                info.maxrgb_percentiles[i] = DY->data.maxrgb_percentiles[0][i];
            }

            info.tone_mapping_flag          = DY->data.tone_mapping.tone_mapping_flag[0];
            info.knee_point_x               = DY->data.tone_mapping.knee_point_x[0];
            info.knee_point_y               = DY->data.tone_mapping.knee_point_y[0];
            info.num_bezier_curve_anchors   = DY->data.tone_mapping.num_bezier_curve_anchors[0];

            for (int i = 0; i < info.num_bezier_curve_anchors; i++) {
                info.bezier_curve_anchors[i] = DY->data.tone_mapping.bezier_curve_anchors[0][i];
            }
#else // USE_FULL_ST2094_40
            info.display_max_luminance  = DY->data.display_maximum_luminance;
            for (int i = 0; i < 3; i++) {
                info.maxscl[i] = DY->data.maxscl[i];
            }

            info.num_maxrgb_percentiles = DY->data.num_maxrgb_percentiles;
            for (int i = 0; i < info.num_maxrgb_percentiles; i++) {
                info.maxrgb_percentages[i] = DY->data.maxrgb_percentages[i];
                info.maxrgb_percentiles[i] = DY->data.maxrgb_percentiles[i];
            }

            info.tone_mapping_flag          = DY->data.tone_mapping.tone_mapping_flag;
            info.knee_point_x               = DY->data.tone_mapping.knee_point_x;
            info.knee_point_y               = DY->data.tone_mapping.knee_point_y;
            info.num_bezier_curve_anchors   = DY->data.tone_mapping.num_bezier_curve_anchors;

            for (int i = 0; i < info.num_bezier_curve_anchors; i++) {
                info.bezier_curve_anchors[i] = DY->data.tone_mapping.bezier_curve_anchors[i];
            }
#endif
            if (memcmp(pHandle->pDynamicInfo, &info, sizeof(info))) {
                pHandle->bHasDynamicInfo = OMX_TRUE;
                memcpy(pHandle->pDynamicInfo, &info, sizeof(info));
            }
        }
    }

    max_display_luminance_cd_m2 = (int)((pExynosPort->HDRStaticInfo.nMaxDisplayLuminance / LUMINANCE_DIV_FACTOR) + 0.5);

    if (pHandle->Run != NULL) {
        pHandle->Run(&inConfig, &outConfig, ((pHandle->bHasDynamicInfo == OMX_TRUE)? pHandle->pDynamicInfo:NULL),
                        max_display_luminance_cd_m2, pHandle->pUserData);
    } else {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    if ((pBuffer != NULL) && (pExynosPort != NULL))
        Exynos_OSAL_UnlockMetaData(pBuffer, pExynosPort->eMetaDataType);

     return ret;
}

#ifdef __cplusplus
}
#endif
