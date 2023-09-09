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
 * @file        Exynos_OSAL_Library.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/utsname.h>

#include "Exynos_OSAL_Library.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Memory.h"

#undef EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG  "EXYNOS_OSAL_LIB"
#include "Exynos_OSAL_Log.h"

void *Exynos_OSAL_dlopen(const char *filename, int flag)
{
    return dlopen(filename, flag);
}

void *Exynos_OSAL_dlsym(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}

int Exynos_OSAL_dlclose(void *handle)
{
    return dlclose(handle);
}

const char *Exynos_OSAL_dlerror(void)
{
    return dlerror();
}

const char *Exynos_OSAL_GetLibPath(void)
{
#ifdef USE_VENDOR_IMAGE
    const char *ANDROID_LIB_INSTALL_PATH    = "/vendor/lib/omx/";
    const char *ANDROID_LIB64_INSTALL_PATH  = "/vendor/lib64/omx/";
#else
    const char *ANDROID_LIB_INSTALL_PATH    = "/system/lib/omx/";
    const char *ANDROID_LIB64_INSTALL_PATH  = "/system/lib64/omx/";
#endif
    const char *OTHERS_LIB_INSTALL_PATH     = "/usr/lib/";
    const char *OTHERS_LIB64_INSTALL_PATH   = "/usr/lib/";

    struct utsname buf;

    Exynos_OSAL_Memset(&buf, 0, sizeof(buf));

#ifndef USE_ANDROID
    /* try to get system info */
    if (uname(&buf) < 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to uname()", __FUNCTION__);
        return (IS_64BIT_OS)? ANDROID_LIB64_INSTALL_PATH:ANDROID_LIB_INSTALL_PATH;
    }

    if (Exynos_OSAL_Strncmp(buf.sysname, "Linux", Exynos_OSAL_Strlen("Linux")) != 0) {
        /* others */
        return (IS_64BIT_OS)? OTHERS_LIB64_INSTALL_PATH:OTHERS_LIB_INSTALL_PATH;
    }
#endif
    /* default : android */
    return (IS_64BIT_OS)? ANDROID_LIB64_INSTALL_PATH:ANDROID_LIB_INSTALL_PATH;
}

int Exynos_OSAL_CheckLibName(char *pLibName)
{
    if (pLibName == NULL)
        return -1;

    /* it should be started as "libOMX.Exynos." */
    if (Exynos_OSAL_Strncmp(pLibName, "libOMX.Exynos.", Exynos_OSAL_Strlen("libOMX.Exynos.")) == 0) {
        /* it should be delimited as ".so" */
        char *pExtPosit =(((char *)pLibName) + (Exynos_OSAL_Strlen(pLibName) - 3));
        if (Exynos_OSAL_Strncmp(pExtPosit, ".so", Exynos_OSAL_Strlen(".so")) == 0)
            return 0;
    }

    return -1;
}
