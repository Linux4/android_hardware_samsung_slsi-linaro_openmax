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
 * @file       Exynos_OMX_Component_Register.c
 * @brief      Exynos OpenMAX IL Component Register
 * @author     SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version    2.0.0
 * @history
 *    2012.02.20 : Create
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "OMX_Component.h"
#include "Exynos_OSAL_Memory.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Library.h"
#include "Exynos_OMX_Component_Register.h"
#include "Exynos_OMX_Macros.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_COMP_REGS"
#include "Exynos_OSAL_Log.h"

static EXYNOS_OMX_COMPONENT_REGLIST gTableComponents[] = {
#ifndef USE_CUSTOM_COMPONENT_SUPPORT
    /* Video Decoder */
    {
        { "OMX.Exynos.AVC.Decoder",
          {
            {"video_decoder.avc"},
          },
          1,
        },
        "libOMX.Exynos.AVC.Decoder.so"
    },
    {
        { "OMX.Exynos.AVC.Decoder.secure",
          {
            {"video_decoder.avc"},
          },
          1,
        },
        "libOMX.Exynos.AVC.Decoder.so"
    },
    {
        { "OMX.Exynos.HEVC.Decoder",
          {
            {"video_decoder.hevc"},
          },
          1,
        },
        "libOMX.Exynos.HEVC.Decoder.so"
    },
    {
        { "OMX.Exynos.HEVC.Decoder.secure",
          {
            {"video_decoder.hevc"},
          },
          1,
        },
        "libOMX.Exynos.HEVC.Decoder.so"
    },
    {
        { "OMX.Exynos.MPEG2.Decoder",
          {
            {"video_decoder.mpeg2"},
          },
          1,
        },
        "libOMX.Exynos.MPEG2.Decoder.so"
    },
    {
        { "OMX.Exynos.MPEG4.Decoder",
          {
            {"video_decoder.mpeg4"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Decoder.so"
    },
    {
        { "OMX.Exynos.MPEG4.Decoder.secure",
          {
            {"video_decoder.mpeg4"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Decoder.so"
    },
    {
        { "OMX.Exynos.H263.Decoder",
          {
            {"video_decoder.h263"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Decoder.so"
    },
    {
        { "OMX.Exynos.WMV.Decoder",
          {
            {"video_decoder.vc1"},
          },
          1,
        },
        "libOMX.Exynos.WMV.Decoder.so"
    },
    {
        { "OMX.Exynos.VP8.Decoder",
          {
            {"video_decoder.vp8"},
          },
          1,
        },
        "libOMX.Exynos.VP8.Decoder.so"
    },
    {
        { "OMX.Exynos.VP9.Decoder",
          {
            {"video_decoder.vp9"},
          },
          1,
        },
        "libOMX.Exynos.VP9.Decoder.so"
    },
    {
        { "OMX.Exynos.VP9.Decoder.secure",
          {
            {"video_decoder.vp9"},
          },
          1,
        },
          "libOMX.Exynos.VP9.Decoder.so"
    },
#else
    {
        { "OMX.Exynos.avc.dec",
          {
            {"video_decoder.avc"},
          },
          1,
        },
        "libOMX.Exynos.AVC.Decoder.so"
    },
    {
        { "OMX.Exynos.avc.dec.secure",
          {
            {"video_decoder.avc"},
          },
          1,
        },
        "libOMX.Exynos.AVC.Decoder.so"
    },
    {
        { "OMX.Exynos.hevc.dec",
          {
            {"video_decoder.hevc"},
          },
          1,
        },
        "libOMX.Exynos.HEVC.Decoder.so"
    },
    {
        { "OMX.Exynos.hevc.dec.secure",
          {
            {"video_decoder.hevc"},
          },
          1,
        },
        "libOMX.Exynos.HEVC.Decoder.so"
    },
    {
        { "OMX.Exynos.mpeg2.dec",
          {
            {"video_decoder.mpeg2"},
          },
          1,
        },
        "libOMX.Exynos.MPEG2.Decoder.so"
    },
    {
        { "OMX.Exynos.mpeg4.dec",
          {
            {"video_decoder.mpeg4"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Decoder.so"
    },
    {
        { "OMX.Exynos.mpeg4.dec.secure",
          {
            {"video_decoder.mpeg4"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Decoder.so"
    },
    {
        { "OMX.Exynos.h263.dec",
          {
            {"video_decoder.h263"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Decoder.so"
    },
    {
        { "OMX.Exynos.vc1.dec",
          {
            {"video_decoder.vc1"},
          },
          1,
        },
        "libOMX.Exynos.WMV.Decoder.so"
    },
    {
        { "OMX.Exynos.vp8.dec",
          {
            {"video_decoder.vp8"},
          },
          1,
        },
        "libOMX.Exynos.VP8.Decoder.so"
    },
    {
        { "OMX.Exynos.vp9.dec",
          {
            {"video_decoder.vp9"},
          },
          1,
        },
        "libOMX.Exynos.VP9.Decoder.so"
    },
    {
        { "OMX.Exynos.vp9.dec.secure",
          {
            {"video_decoder.vp9"},
          },
          1,
        },
          "libOMX.Exynos.VP9.Decoder.so"
    },
#endif

    /* Video Encoder */
    {
        { "OMX.Exynos.AVC.Encoder",
          {
            {"video_encoder.avc"},
          },
          1,
        },
        "libOMX.Exynos.AVC.Encoder.so"
    },
    {
        { "OMX.Exynos.AVC.Encoder.secure",
          {
            {"video_encoder.avc-wfd"},
          },
          1,
        },
        "libOMX.Exynos.AVC.Encoder.so"
    },
    {
        { "OMX.Exynos.HEVC.Encoder",
          {
            {"video_encoder.hevc"},
          },
          1,
        },
        "libOMX.Exynos.HEVC.Encoder.so"
    },
    {
        { "OMX.Exynos.HEVC.Encoder.secure",
          {
            {"video_encoder.hevc-wfd"},
          },
          1,
        },
        "libOMX.Exynos.HEVC.Encoder.so"
    },
    {
        { "OMX.Exynos.MPEG4.Encoder",
          {
            {"video_encoder.mpeg4"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Encoder.so"
    },
    {
        { "OMX.Exynos.H263.Encoder",
          {
            {"video_encoder.h263"},
          },
          1,
        },
        "libOMX.Exynos.MPEG4.Encoder.so"
    },
    {
        { "OMX.Exynos.VP8.Encoder",
          {
            {"video_encoder.vp8"},
          },
          1,
        },
        "libOMX.Exynos.VP8.Encoder.so"
    },
    {
        { "OMX.Exynos.VP9.Encoder",
          {
            {"video_encoder.vp9"},
          },
          1,
        },
        "libOMX.Exynos.VP9.Encoder.so"
    },
    {
        { "OMX.Exynos.AVC.WFD.Encoder",
            {
                {"video_encoder.avc-wfd"},
            },
            1,
        },
        "libOMX.Exynos.AVC.WFD.Encoder.so"
    },
    {
        { "OMX.Exynos.AVC.WFD.Encoder.secure",
            {
                {"video_encoder.avc-wfd"},
            },
            1,
        },
        "libOMX.Exynos.AVC.WFD.Encoder.so"
    },
    {
        { "OMX.Exynos.HEVC.WFD.Encoder",
            {
                {"video_encoder.hevc-wfd"},
            },
            1,
        },
        "libOMX.Exynos.HEVC.WFD.Encoder.so"
    },
    {
        { "OMX.Exynos.HEVC.WFD.Encoder.secure",
            {
                {"video_encoder.hevc-wfd"},
            },
            1,
        },
        "libOMX.Exynos.HEVC.WFD.Encoder.so"
    },

    /* Audio Decoder */
#ifdef USE_SEIREN_AUDIO_COMPONENT_LOAD
    /* Audio */
    /* MP3 Decoder */
    {
        { "OMX.Exynos.MP3.Decoder",
          {
            {"audio_decoder.mp3"},
          },
          1,
        },
        "libOMX.Exynos.MP3.Decoder.so"
    },

    /* MP1 Decoder */
    {
        { "OMX.Exynos.MP1.Decoder",
          {
            {"audio_decoder.mp1"},
          },
          1,
        },
        "libOMX.Exynos.MP1.Decoder.so"
    },

     /* MP2 Decoder */
    {
        { "OMX.Exynos.MP2.Decoder",
          {
            {"audio_decoder.mp2"},
          },
          1,
        },
        "libOMX.Exynos.MP2.Decoder.so"
    },

     /* AAC Decoder */
    {
        { "OMX.Exynos.AAC.Decoder",
          {
            {"audio_decoder.aac"},
          },
          1,
        },
        "libOMX.Exynos.AAC.Decoder.so"
    },

     /* FLAC Decoder */
    {
        { "OMX.Exynos.FLAC.Decoder",
          {
            {"audio_decoder.flac"},
          },
          1,
        },
        "libOMX.Exynos.FLAC.Decoder.so"
    },
#endif
#ifdef USE_ALP_AUDIO_COMPONENT_LOAD
    /* MP3 Decoder */
    {
        { "OMX.Exynos.MP3.Decoder",
          {
            {"audio_decoder.mp3"},
          },
          1,
        },
        "libOMX.Exynos.MP3.Decoder.so"
    },

    /* WMA Decoder */
    {
        { "OMX.Exynos.WMA.Decoder",
          {
            {"audio_decoder.wma"},
          },
          1,
        },
        "libOMX.Exynos.WMA.Decoder.so"
    },
#endif
};

#ifdef DISABLE_CODEC_COMP
static char *gDisableComponents[] = {
#ifdef DISABLE_HEVC_ENC_CODEC
    "HEVC.Encoder",
#endif
#ifdef DISABLE_VP9_DEC_CODEC
    "VP9.Decoder",
    "vp9.dec",
#endif
#ifdef DISABLE_VP9_ENC_CODEC
    "VP9.Encoder",
#endif
#ifdef DISABLE_WFD_ENC_CODEC
    "WFD.Encoder",
#endif
#ifdef DISABLE_SECURE_CODEC
    ".secure",
#endif
};
#endif

static void registComponent(
    char                         *sLibName,
    EXYNOS_OMX_COMPONENT_REGLIST *pComponentList,
    int                          *pNumComponents)
{
    const char *libPath = NULL;
    int nComponents = 0;
    unsigned int i;

    FunctionIn();

    if ((sLibName == NULL) ||
        (pComponentList == NULL) ||
        (pNumComponents == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        return;
    }

    nComponents = (*pNumComponents);
    libPath = Exynos_OSAL_GetLibPath();

    for (i = 0; i < sizeof(gTableComponents)/sizeof(EXYNOS_OMX_COMPONENT_REGLIST); i++) {
        if (Exynos_OSAL_Strcmp(sLibName, gTableComponents[i].libName) == 0) {
#ifdef DISABLE_CODEC_COMP
            OMX_BOOL skip = OMX_FALSE;
            unsigned int j;

            for (j = 0; j < sizeof(gDisableComponents)/sizeof(gDisableComponents[0]); j++) {
                if (Exynos_OSAL_Strstr((const char *)gTableComponents[i].component.componentName, gDisableComponents[j]) != NULL) {
                    skip = OMX_TRUE;
                    break;
                }
            }

            if (skip == OMX_TRUE)
                continue;
#endif
            Exynos_OSAL_Memcpy(&(pComponentList[nComponents]), &(gTableComponents[i]), sizeof(gTableComponents[i]));
            Exynos_OSAL_Memset(pComponentList[nComponents].libName, 0, MAX_OMX_COMPONENT_LIBNAME_SIZE);
            Exynos_OSAL_Strcpy(pComponentList[nComponents].libName, (OMX_PTR)libPath);
            Exynos_OSAL_Strcat(pComponentList[nComponents].libName, sLibName);
            Exynos_OSAL_Log(EXYNOS_LOG_ESSENTIAL, "%s: %s", pComponentList[nComponents].libName, gTableComponents[i].component.componentName);
            nComponents++;
        }
    }

    (*pNumComponents) = nComponents;

EXIT:
    FunctionOut();

    return;
}

OMX_ERRORTYPE Exynos_OMX_Component_Register(EXYNOS_OMX_COMPONENT_REGLIST **compList, OMX_U32 *compNum)
{
    OMX_ERRORTYPE  ret = OMX_ErrorNone;
    int            componentNum = 0, totalCompNum = 0;
    const char    *libPath = NULL;
    char          *libName = NULL;
    const char    *errorMsg = NULL;
    DIR           *dir = NULL;
    struct dirent *d = NULL;

    int i, j;

    int (*Exynos_OMX_COMPONENT_Library_Register)(ExynosRegisterComponentType **exynosComponents);
    ExynosRegisterComponentType **exynosComponentsTemp = NULL;
    EXYNOS_OMX_COMPONENT_REGLIST *componentList        = NULL;

    FunctionIn();

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "%s lib is loaded", (IS_64BIT_OS? "64bit":"32bit"));
    libPath = Exynos_OSAL_GetLibPath();
    dir = opendir(libPath);
    if (dir == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to opendir(%s)", __FUNCTION__, libPath);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    componentList = (EXYNOS_OMX_COMPONENT_REGLIST *)Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_COMPONENT_REGLIST) * MAX_OMX_COMPONENT_NUM);
    if (componentList == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(componentList, 0, sizeof(EXYNOS_OMX_COMPONENT_REGLIST) * MAX_OMX_COMPONENT_NUM);

    libName = Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_LIBNAME_SIZE);
    if (libName == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    while ((d = readdir(dir)) != NULL) {
        OMX_HANDLETYPE soHandle = NULL;

        if (Exynos_OSAL_CheckLibName(d->d_name) == 0) {
            Exynos_OSAL_Log(EXYNOS_LOG_INFO, "Loading the library: %s", d->d_name);

#ifndef USE_DISABLE_RAPID_COMPONENT_LOAD
            registComponent(d->d_name, componentList, &totalCompNum);
#else
            Exynos_OSAL_Memset(libName, 0, MAX_OMX_COMPONENT_LIBNAME_SIZE);
            Exynos_OSAL_Strcpy(libName, (OMX_PTR)libPath);
            Exynos_OSAL_Strcat(libName, d->d_name);

            if ((soHandle = Exynos_OSAL_dlopen(libName, RTLD_NOW)) != NULL) {
                Exynos_OSAL_dlerror();    /* clear error*/
                if ((Exynos_OMX_COMPONENT_Library_Register = Exynos_OSAL_dlsym(soHandle, "Exynos_OMX_COMPONENT_Library_Register")) != NULL) {
                    componentNum = (*Exynos_OMX_COMPONENT_Library_Register)(NULL);
                    exynosComponentsTemp = (ExynosRegisterComponentType **)Exynos_OSAL_Malloc(sizeof(ExynosRegisterComponentType *) * componentNum);
                    if (exynosComponentsTemp == NULL) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
                        ret = OMX_ErrorInsufficientResources;
                        goto EXIT;
                    }

                    for (i = 0; i < componentNum; i++) {
                        exynosComponentsTemp[i] = Exynos_OSAL_Malloc(sizeof(ExynosRegisterComponentType));
                        if (exynosComponentsTemp[i] == NULL) {
                            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
                            for (j = 0; j < i; j++) {
                                Exynos_OSAL_Free(exynosComponentsTemp[j]);
                                exynosComponentsTemp[j] = NULL;
                            }

                            Exynos_OSAL_Free(exynosComponentsTemp);
                            exynosComponentsTemp = NULL;
                            ret = OMX_ErrorInsufficientResources;
                            goto EXIT;
                        }

                        Exynos_OSAL_Memset(exynosComponentsTemp[i], 0, sizeof(ExynosRegisterComponentType));
                    }

                    (*Exynos_OMX_COMPONENT_Library_Register)(exynosComponentsTemp);

                    for (i = 0; i < componentNum; i++) {
#ifdef DISABLE_CODEC_COMP
                        OMX_BOOL skip = OMX_FALSE;
                        int j;

                        for (j = 0; j < sizeof(gDisableComponents)/sizeof(gDisableComponents[0]); j++) {
                            if (Exynos_OSAL_Strstr(exynosComponentsTemp[i]->componentName, gDisableComponents[j]) != NULL) {
                                skip = OMX_TRUE;
                                break;
                            }
                        }

                        if (skip == OMX_TRUE)
                            continue;
#endif
                        Exynos_OSAL_Strcpy(componentList[totalCompNum].component.componentName, exynosComponentsTemp[i]->componentName);
                        for (j = 0; j < (int)exynosComponentsTemp[i]->totalRoleNum; j++)
                            Exynos_OSAL_Strcpy(componentList[totalCompNum].component.roles[j], exynosComponentsTemp[i]->roles[j]);
                        componentList[totalCompNum].component.totalRoleNum = exynosComponentsTemp[i]->totalRoleNum;

                        Exynos_OSAL_Strcpy(componentList[totalCompNum].libName, libName);

                        totalCompNum++;
                    }

                    for (i = 0; i < componentNum; i++) {
                        Exynos_OSAL_Free(exynosComponentsTemp[i]);
                        exynosComponentsTemp[i] = NULL;
                    }

                    Exynos_OSAL_Free(exynosComponentsTemp);
                    exynosComponentsTemp = NULL;
                } else {
                    if ((errorMsg = Exynos_OSAL_dlerror()) != NULL)
                        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "dlsym failed: %s", errorMsg);
                }

                Exynos_OSAL_dlclose(soHandle);
                soHandle = NULL;
            } else {
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "dlopen failed: %s", Exynos_OSAL_dlerror());
            }
#endif
        } else {
            /* not a component name line. skip */
            continue;
        }
    }

    *compList = componentList;
    *compNum = totalCompNum;

EXIT:
    if ((ret != OMX_ErrorNone) &&
        (componentList != NULL)) {
        Exynos_OSAL_Free(componentList);
        componentList = NULL;
    }

    if (dir != NULL) {
        closedir(dir);
        dir = NULL;
    }

    if (libName != NULL) {
        Exynos_OSAL_Free(libName);
        libName = NULL;
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_Component_Unregister(EXYNOS_OMX_COMPONENT_REGLIST *componentList)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    Exynos_OSAL_Free(componentList);

EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentAPICheck(OMX_COMPONENTTYPE *component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if ((NULL == component->GetComponentVersion)    ||
        (NULL == component->SendCommand)            ||
        (NULL == component->GetParameter)           ||
        (NULL == component->SetParameter)           ||
        (NULL == component->GetConfig)              ||
        (NULL == component->SetConfig)              ||
        (NULL == component->GetExtensionIndex)      ||
        (NULL == component->GetState)               ||
#ifdef TUNNELING_SUPPORT
        (NULL == component->ComponentTunnelRequest) ||
#endif
        (NULL == component->UseBuffer)              ||
        (NULL == component->AllocateBuffer)         ||
        (NULL == component->FreeBuffer)             ||
        (NULL == component->EmptyThisBuffer)        ||
        (NULL == component->FillThisBuffer)         ||
        (NULL == component->SetCallbacks)           ||
        (NULL == component->ComponentDeInit)        ||
#ifdef EGL_IMAGE_SUPPORT
        (NULL == component->UseEGLImage)            ||
#endif
        (NULL == component->ComponentRoleEnum))
        ret = OMX_ErrorInvalidComponent;
    else
        ret = OMX_ErrorNone;

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentLoad(EXYNOS_OMX_COMPONENT *exynos_component)
{
    OMX_ERRORTYPE      ret           = OMX_ErrorNone;
    OMX_HANDLETYPE     libHandle     = NULL;
    OMX_COMPONENTTYPE *pOMXComponent = NULL;

    FunctionIn();

    OMX_ERRORTYPE (*Exynos_OMX_ComponentInit)(OMX_HANDLETYPE hComponent, OMX_STRING componentName);

    libHandle = Exynos_OSAL_dlopen((OMX_STRING)exynos_component->libName, RTLD_NOW);
    if (libHandle == NULL) {
        ret = OMX_ErrorInvalidComponentName;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_dlopen(%s)", __FUNCTION__, exynos_component->libName);
        goto EXIT;
    }

    Exynos_OMX_ComponentInit = Exynos_OSAL_dlsym(libHandle, "Exynos_OMX_ComponentInit");
    if (Exynos_OMX_ComponentInit == NULL) {
        Exynos_OSAL_dlclose(libHandle);
        ret = OMX_ErrorInvalidComponent;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_dlsym(Exynos_OMX_ComponentInit)", __FUNCTION__);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_COMPONENTTYPE));
    if (pOMXComponent == NULL) {
        Exynos_OSAL_dlclose(libHandle);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
        goto EXIT;
    }
    INIT_SET_SIZE_VERSION(pOMXComponent, OMX_COMPONENTTYPE);

    ret = (*Exynos_OMX_ComponentInit)((OMX_HANDLETYPE)pOMXComponent, (OMX_STRING)exynos_component->componentName);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Free(pOMXComponent);
        Exynos_OSAL_dlclose(libHandle);
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OMX_ComponentInit() (ret:0x%x)", __FUNCTION__, ret);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    } else {
        if (Exynos_OMX_ComponentAPICheck(pOMXComponent) != OMX_ErrorNone) {
            if (NULL != pOMXComponent->ComponentDeInit)
                pOMXComponent->ComponentDeInit(pOMXComponent);

            Exynos_OSAL_Free(pOMXComponent);
            Exynos_OSAL_dlclose(libHandle);
            ret = OMX_ErrorInvalidComponent;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OMX_ComponentAPICheck()", __FUNCTION__);
            goto EXIT;
        }

        exynos_component->libHandle     = libHandle;
        exynos_component->pOMXComponent = pOMXComponent;
        ret = OMX_ErrorNone;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentUnload(EXYNOS_OMX_COMPONENT *exynos_component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pOMXComponent = NULL;

    FunctionIn();

    if (!exynos_component) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = exynos_component->pOMXComponent;
    if (pOMXComponent != NULL) {
        pOMXComponent->ComponentDeInit(pOMXComponent);
        Exynos_OSAL_Free(pOMXComponent);
        exynos_component->pOMXComponent = NULL;
    }

    if (exynos_component->libHandle != NULL) {
        Exynos_OSAL_dlclose(exynos_component->libHandle);
        exynos_component->libHandle = NULL;
    }

EXIT:
    FunctionOut();

    return ret;
}

