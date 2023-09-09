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
 * @file        Exynos_OSAL_SharedMemory.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Jinsung Yang (jsgood.yang@samsung.com)
 *              Taehwan Kim (t_h.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#ifdef USE_ANDROID
#include <log/log.h>
#include <cutils/atomic.h>
#endif
#include <fcntl.h>
#include <sys/mman.h>

#include <hardware/exynos/ion.h>

#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_Memory.h"
#include "Exynos_OSAL_SharedMemory.h"

#include "Exynos_OSAL_ETC.h"

//#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


static int mem_cnt = 0;
static int map_cnt = 0;

struct EXYNOS_SHAREDMEM_LIST;
typedef struct _EXYNOS_SHAREDMEM_LIST
{
    unsigned long                  IONBuffer;
    OMX_PTR                        mapAddr;
    OMX_U32                        allocSize;
    OMX_BOOL                       owner;
    struct _EXYNOS_SHAREDMEM_LIST *pNextMemory;
} EXYNOS_SHAREDMEM_LIST;

typedef struct _EXYNOS_SHARED_MEMORY
{
    unsigned long          hIONHandle;
    EXYNOS_SHAREDMEM_LIST *pAllocMemory;
    OMX_HANDLETYPE         hSMMutex;
} EXYNOS_SHARED_MEMORY;


OMX_HANDLETYPE Exynos_OSAL_SharedMemory_Open()
{
    EXYNOS_SHARED_MEMORY *pHandle   = NULL;
    long                  IONClient = -1;

    pHandle = (EXYNOS_SHARED_MEMORY *)Exynos_OSAL_Malloc(sizeof(EXYNOS_SHARED_MEMORY));
    if (pHandle == NULL)
        goto EXIT;
    Exynos_OSAL_Memset(pHandle, 0, sizeof(EXYNOS_SHARED_MEMORY));

    IONClient = (long)exynos_ion_open();
    if (IONClient < 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "ion_open is failed: %d", IONClient);
        Exynos_OSAL_Free((void *)pHandle);
        pHandle = NULL;
        goto EXIT;
    }

    pHandle->hIONHandle = (unsigned long)IONClient;

    if (OMX_ErrorNone != Exynos_OSAL_MutexCreate(&pHandle->hSMMutex)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_MutexCreate", __FUNCTION__);
        /* free a ion_client */
        exynos_ion_close(pHandle->hIONHandle);
        pHandle->hIONHandle = 0;

        Exynos_OSAL_Free((void *)pHandle);
        pHandle = NULL;
    }

EXIT:
    return (OMX_HANDLETYPE)pHandle;
}

void Exynos_OSAL_SharedMemory_Close(OMX_HANDLETYPE handle)
{
    EXYNOS_SHARED_MEMORY  *pHandle = (EXYNOS_SHARED_MEMORY *)handle;
    EXYNOS_SHAREDMEM_LIST *pSMList = NULL;
    EXYNOS_SHAREDMEM_LIST *pCurrentElement = NULL;
    EXYNOS_SHAREDMEM_LIST *pDeleteElement = NULL;

    if (pHandle == NULL)
        goto EXIT;

    Exynos_OSAL_MutexLock(pHandle->hSMMutex);
    pCurrentElement = pSMList = pHandle->pAllocMemory;

    while (pCurrentElement != NULL) {
        pDeleteElement = pCurrentElement;
        pCurrentElement = pCurrentElement->pNextMemory;

        /* if mmap was not called, mapAddr is same as IONBuffer */
        if (pDeleteElement->mapAddr != (void *)pDeleteElement->IONBuffer) {
            if (Exynos_OSAL_Munmap(pDeleteElement->mapAddr, pDeleteElement->allocSize))
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Munmap", __FUNCTION__);
        }

        pDeleteElement->mapAddr = NULL;
        pDeleteElement->allocSize = 0;

        if (pDeleteElement->owner) {
            /* free a ion_buffer */
            close(pDeleteElement->IONBuffer);
            mem_cnt--;
        }
        pDeleteElement->IONBuffer = 0;

        Exynos_OSAL_Free(pDeleteElement);

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] count: %d", __FUNCTION__, mem_cnt);
    }

    pHandle->pAllocMemory = pSMList = NULL;
    Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);

    Exynos_OSAL_MutexTerminate(pHandle->hSMMutex);
    pHandle->hSMMutex = NULL;

    /* free a ion_client */
    exynos_ion_close(pHandle->hIONHandle);
    pHandle->hIONHandle = 0;

    Exynos_OSAL_Free(pHandle);

EXIT:
    return;
}

OMX_PTR Exynos_OSAL_SharedMemory_Alloc(OMX_HANDLETYPE handle, OMX_U32 size, MEMORY_TYPE memoryType)
{
    EXYNOS_SHARED_MEMORY  *pHandle         = (EXYNOS_SHARED_MEMORY *)handle;
    EXYNOS_SHAREDMEM_LIST *pSMList         = NULL;
    EXYNOS_SHAREDMEM_LIST *pElement        = NULL;
    EXYNOS_SHAREDMEM_LIST *pCurrentElement = NULL;
    long                   IONBuffer       = -1;
    OMX_PTR                pBuffer         = NULL;
    unsigned int mask;
    unsigned int flag;

    if (pHandle == NULL)
        goto EXIT;

    pElement = (EXYNOS_SHAREDMEM_LIST *)Exynos_OSAL_Malloc(sizeof(EXYNOS_SHAREDMEM_LIST));
    if (pElement == NULL)
        goto EXIT;
    Exynos_OSAL_Memset(pElement, 0, sizeof(EXYNOS_SHAREDMEM_LIST));
    pElement->owner = OMX_TRUE;

    /* priority is like as EXT > SECURE > CONTIG > CACHED > NORMAL */
    switch ((int)memoryType) {
    case (EXT_MEMORY | SECURE_MEMORY | CONTIG_MEMORY | CACHED_MEMORY):  /* EXTRA */
    case (EXT_MEMORY | SECURE_MEMORY | CONTIG_MEMORY):
    case (EXT_MEMORY | SECURE_MEMORY | CACHED_MEMORY):
    case (EXT_MEMORY | SECURE_MEMORY):
        mask = EXYNOS_ION_HEAP_VIDEO_STREAM_MASK;
        flag = ION_FLAG_PROTECTED;
        break;
    case (EXT_MEMORY | CONTIG_MEMORY | CACHED_MEMORY):
    case (EXT_MEMORY | CONTIG_MEMORY):
    case (EXT_MEMORY | CACHED_MEMORY):
    case EXT_MEMORY:
        mask = EXYNOS_ION_HEAP_VIDEO_STREAM_MASK;
        flag = 0;
        break;
    case (SECURE_MEMORY | CONTIG_MEMORY | CACHED_MEMORY):  /* SECURE */
    case (SECURE_MEMORY | CONTIG_MEMORY):
    case (SECURE_MEMORY | CACHED_MEMORY):
    case SECURE_MEMORY:
        mask = EXYNOS_ION_HEAP_VIDEO_STREAM_MASK;
        flag = ION_FLAG_PROTECTED;
        break;
    case (CONTIG_MEMORY | CACHED_MEMORY):  /* CONTIG */
    case CONTIG_MEMORY:
        mask = EXYNOS_ION_HEAP_VIDEO_STREAM_MASK;
        flag = 0;
        break;
    case CACHED_MEMORY:  /* CACHED */
        mask = EXYNOS_ION_HEAP_SYSTEM_MASK;
        flag = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
        break;
    default:  /* NORMAL */
	// NOTE: cached?
        mask = EXYNOS_ION_HEAP_SYSTEM_MASK;
        flag = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
        break;
    }

    if (flag & ION_FLAG_CACHED)  /* use improved cache oprs */
        flag |= ION_FLAG_CACHED_NEEDS_SYNC;

    if ((IONBuffer = exynos_ion_alloc(pHandle->hIONHandle, size, mask, flag)) < 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "[%s] Failed to exynos_ion_alloc(mask:%x, flag:%x)", __FUNCTION__, mask, flag);
        if (memoryType == CONTIG_MEMORY) {
            /* retry at normal area */
            flag = 0;
            if ((IONBuffer = exynos_ion_alloc(pHandle->hIONHandle, size, mask, flag)) < 0) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] retry: Failed to exynos_ion_alloc(mask:%x, flag:%x)", __FUNCTION__, mask, flag);
                Exynos_OSAL_Free((OMX_PTR)pElement);
                goto EXIT;
            }
        }
    }

    if (flag & ION_FLAG_PROTECTED) {
        /* in case of DRM, do not call mmap. so set a fd instead of vaddr */
        pBuffer = (OMX_PTR)IONBuffer;
    } else {
        pBuffer = Exynos_OSAL_Mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, IONBuffer, 0);
        if (pBuffer == MAP_FAILED) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Mmap(size:%d)", __FUNCTION__, size);
            /* free a ion_buffer */
            close(IONBuffer); // NOTE: argument to exynos_ion_close/ion_close() should be allocated by ion_open/exynos_ion_open
            Exynos_OSAL_Free((OMX_PTR)pElement);
            pBuffer = NULL;
            goto EXIT;
        }
    }

    pElement->IONBuffer   = (unsigned long)IONBuffer;
    pElement->mapAddr     = pBuffer;
    pElement->allocSize   = size;
    pElement->pNextMemory = NULL;

    Exynos_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        pHandle->pAllocMemory = pSMList = pElement;
    } else {
        pCurrentElement = pSMList;
        while (pCurrentElement->pNextMemory != NULL) {
            pCurrentElement = pCurrentElement->pNextMemory;
        }
        pCurrentElement->pNextMemory = pElement;
    }
    Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);

    mem_cnt++;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] count: %d", __FUNCTION__, mem_cnt);

EXIT:
    return pBuffer;
}

void Exynos_OSAL_SharedMemory_Free(OMX_HANDLETYPE handle, OMX_PTR pBuffer)
{
    EXYNOS_SHARED_MEMORY  *pHandle         = (EXYNOS_SHARED_MEMORY *)handle;
    EXYNOS_SHAREDMEM_LIST *pSMList         = NULL;
    EXYNOS_SHAREDMEM_LIST *pCurrentElement = NULL;
    EXYNOS_SHAREDMEM_LIST *pDeleteElement  = NULL;

    if (pHandle == NULL)
        goto EXIT;

    Exynos_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->mapAddr == pBuffer) {
        pDeleteElement = pSMList;
        pHandle->pAllocMemory = pSMList = pSMList->pNextMemory;
    } else {
        while ((pCurrentElement != NULL) && (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr != pBuffer))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr == pBuffer)) {
            pDeleteElement = pCurrentElement->pNextMemory;
            pCurrentElement->pNextMemory = pDeleteElement->pNextMemory;
        } else {
            Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] can't find a buffer(%p) in list", __FUNCTION__, pBuffer);
            goto EXIT;
        }
    }
    Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);

    if (pDeleteElement->mapAddr != (void *)pDeleteElement->IONBuffer) {
        if (Exynos_OSAL_Munmap(pDeleteElement->mapAddr, pDeleteElement->allocSize)) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Munmap", __FUNCTION__);
            goto EXIT;
        }
    }

    pDeleteElement->mapAddr = NULL;
    pDeleteElement->allocSize = 0;

    if (pDeleteElement->owner) {
        /* free a ion_buffer */
        close(pDeleteElement->IONBuffer);
        mem_cnt--;
    }
    pDeleteElement->IONBuffer = 0;

    Exynos_OSAL_Free(pDeleteElement);

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] count: %d", __FUNCTION__, mem_cnt);

EXIT:
    return;
}

OMX_PTR Exynos_OSAL_SharedMemory_Map(OMX_HANDLETYPE handle, OMX_U32 size, unsigned long ionfd)
{
    EXYNOS_SHARED_MEMORY  *pHandle = (EXYNOS_SHARED_MEMORY *)handle;
    EXYNOS_SHAREDMEM_LIST *pSMList = NULL;
    EXYNOS_SHAREDMEM_LIST *pElement = NULL;
    EXYNOS_SHAREDMEM_LIST *pCurrentElement = NULL;
    OMX_S32 IONBuffer = 0;
    OMX_PTR pBuffer = NULL;

    if (pHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] invalid parameter", __FUNCTION__);
        goto EXIT;
    }

    pElement = (EXYNOS_SHAREDMEM_LIST *)Exynos_OSAL_Malloc(sizeof(EXYNOS_SHAREDMEM_LIST));
    if (pElement == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Malloc()", __FUNCTION__);
        goto EXIT;
    }

    Exynos_OSAL_Memset(pElement, 0, sizeof(EXYNOS_SHAREDMEM_LIST));

    IONBuffer = (OMX_S32)ionfd;

    if (IONBuffer < 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] FD(%u) is wrong", __FUNCTION__, IONBuffer);
        Exynos_OSAL_Free((void*)pElement);
        goto EXIT;
    }

    pBuffer = Exynos_OSAL_Mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, IONBuffer, 0);
    if (pBuffer == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Mmap(size:%d)", __FUNCTION__, size);
        /* free a ion_buffer */
        close(IONBuffer);
        Exynos_OSAL_Free((void*)pElement);
        goto EXIT;
    }

    pElement->IONBuffer = IONBuffer;
    pElement->mapAddr = pBuffer;
    pElement->allocSize = size;
    pElement->pNextMemory = NULL;

    Exynos_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        pHandle->pAllocMemory = pSMList = pElement;
    } else {
        pCurrentElement = pSMList;
        while (pCurrentElement->pNextMemory != NULL) {
            pCurrentElement = pCurrentElement->pNextMemory;
        }
        pCurrentElement->pNextMemory = pElement;
    }
    Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);

    map_cnt++;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] count: %d", __FUNCTION__, map_cnt);

EXIT:
    return pBuffer;
}

void Exynos_OSAL_SharedMemory_Unmap(OMX_HANDLETYPE handle, unsigned long ionfd)
{
    EXYNOS_SHARED_MEMORY  *pHandle = (EXYNOS_SHARED_MEMORY *)handle;
    EXYNOS_SHAREDMEM_LIST *pSMList = NULL;
    EXYNOS_SHAREDMEM_LIST *pCurrentElement = NULL;
    EXYNOS_SHAREDMEM_LIST *pDeleteElement = NULL;

    if (pHandle == NULL)
        goto EXIT;

    Exynos_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->IONBuffer == ionfd) {
        pDeleteElement = pSMList;
        pHandle->pAllocMemory = pSMList = pSMList->pNextMemory;
    } else {
        while ((pCurrentElement != NULL) && (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->IONBuffer != ionfd))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->IONBuffer == ionfd)) {
            pDeleteElement = pCurrentElement->pNextMemory;
            pCurrentElement->pNextMemory = pDeleteElement->pNextMemory;
        } else {
            Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] can't find a buffer(%u) in list", __FUNCTION__, ionfd);
            goto EXIT;
        }
    }
    Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);

    if (Exynos_OSAL_Munmap(pDeleteElement->mapAddr, pDeleteElement->allocSize)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] Failed to Exynos_OSAL_Munmap", __FUNCTION__);
        goto EXIT;
    }
    pDeleteElement->mapAddr = NULL;
    pDeleteElement->allocSize = 0;
    pDeleteElement->IONBuffer = 0;

    Exynos_OSAL_Free(pDeleteElement);

    map_cnt--;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] count: %d", __FUNCTION__, map_cnt);

EXIT:
    return;
}

unsigned long Exynos_OSAL_SharedMemory_VirtToION(OMX_HANDLETYPE handle, OMX_PTR pBuffer)
{
    EXYNOS_SHARED_MEMORY  *pHandle         = (EXYNOS_SHARED_MEMORY *)handle;
    EXYNOS_SHAREDMEM_LIST *pSMList         = NULL;
    EXYNOS_SHAREDMEM_LIST *pCurrentElement = NULL;
    EXYNOS_SHAREDMEM_LIST *pFindElement    = NULL;

    unsigned long ion_addr = 0;

    if (pHandle == NULL || pBuffer == NULL)
        goto EXIT;

    Exynos_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->mapAddr == pBuffer) {
        pFindElement = pSMList;
    } else {
        while ((pCurrentElement != NULL) && (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr != pBuffer))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr == pBuffer)) {
            pFindElement = pCurrentElement->pNextMemory;
        } else {
            Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] can't find a buffer(%p) in list", __FUNCTION__, pBuffer);
            goto EXIT;
        }
    }
    Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);

    ion_addr = pFindElement->IONBuffer;

EXIT:
    return ion_addr;
}

OMX_PTR Exynos_OSAL_SharedMemory_IONToVirt(OMX_HANDLETYPE handle, unsigned long ionfd)
{
    EXYNOS_SHARED_MEMORY  *pHandle         = (EXYNOS_SHARED_MEMORY *)handle;
    EXYNOS_SHAREDMEM_LIST *pSMList         = NULL;
    EXYNOS_SHAREDMEM_LIST *pCurrentElement = NULL;
    EXYNOS_SHAREDMEM_LIST *pFindElement    = NULL;

    OMX_PTR pBuffer = NULL;

    if ((pHandle == NULL) || ((long)ionfd < 0))
        goto EXIT;

    Exynos_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->IONBuffer == ionfd) {
        pFindElement = pSMList;
    } else {
        while ((pCurrentElement != NULL) && (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->IONBuffer != ionfd))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((EXYNOS_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->IONBuffer == ionfd)) {
            pFindElement = pCurrentElement->pNextMemory;
        } else {
            Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "[%s] can't find a buffer(%u) in list", __FUNCTION__, ionfd);
            goto EXIT;
        }
    }
    Exynos_OSAL_MutexUnlock(pHandle->hSMMutex);

    pBuffer = pFindElement->mapAddr;

EXIT:
    return pBuffer;
}
