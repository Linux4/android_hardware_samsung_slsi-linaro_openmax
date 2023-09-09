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
 * @file    Exynos_OMX_Macros.h
 * @brief   Macros
 * @author  SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version    2.0.0
 * @history
 *   2012.02.20 : Create
 */

#ifndef EXYNOS_OMX_MACROS
#define EXYNOS_OMX_MACROS

#include "Exynos_OMX_Def.h"
#include "Exynos_OSAL_Memory.h"


/*
 * MACROS
 */
#define ALIGN(x, a)       (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_TO_16B(x)   ((((x) + (1 <<  4) - 1) >>  4) <<  4)
#define ALIGN_TO_32B(x)   ((((x) + (1 <<  5) - 1) >>  5) <<  5)
#define ALIGN_TO_128B(x)  ((((x) + (1 <<  7) - 1) >>  7) <<  7)
#define ALIGN_TO_8KB(x)   ((((x) + (1 << 13) - 1) >> 13) << 13)

/* This is for single FD support.
 * data is not filled like as continuous padding area for satisfying H/W constraints.
 */
/* 8bit format */
#define GET_Y_SIZE(w, h) (ALIGN(w, 16) * ALIGN(h, 16) + 256)
#define GET_UV_SIZE(w, h) (ALIGN(((ALIGN(w, 16) * ALIGN(h, 16) / 2) + 256), 16))
#define GET_CB_SIZE(w, h) (ALIGN(((ALIGN(w / 2, 16) * ALIGN(h, 16) / 2) + 256), 16))
#define GET_CR_SIZE(w, h) (ALIGN(((ALIGN(w / 2, 16) * ALIGN(h, 16) / 2) + 256), 16))

#define GET_UV_OFFSET(w, h) GET_Y_SIZE(w, h)
#define GET_CB_OFFSET(w, h) GET_Y_SIZE(w, h)
#define GET_CR_OFFSET(w, h) (GET_Y_SIZE(w, h) + GET_CB_SIZE(w, h))

/* 10bit format */

#ifndef S10B_FORMAT_8B_ALIGNMENT
#define S10B_FORMAT_8B_ALIGNMENT 16
#endif
#define GET_8B_Y_SIZE(w, h) (ALIGN(w, S10B_FORMAT_8B_ALIGNMENT) * ALIGN(h, 16) + 256)
#define GET_8B_UV_SIZE(w, h) (ALIGN(((ALIGN(w, S10B_FORMAT_8B_ALIGNMENT) * ALIGN(h, 16) / 2) + 256), 16))
#define GET_8B_CB_SIZE(w, h) (ALIGN(((ALIGN(w / 2, S10B_FORMAT_8B_ALIGNMENT) * ALIGN(h, 16) / 2) + 256), 16))
#define GET_8B_CR_SIZE(w, h) (ALIGN(((ALIGN(w / 2, S10B_FORMAT_8B_ALIGNMENT) * ALIGN(h, 16) / 2) + 256), 16))
#define GET_2B_SIZE(w, h) (ALIGN(w / 4, 16) * ALIGN(h, 16) + 64)
#define GET_10B_Y_SIZE(w, h) (GET_8B_Y_SIZE(w, h) + GET_2B_SIZE(w, h))
#define GET_10B_UV_SIZE(w, h) (GET_8B_UV_SIZE(w, h) + GET_2B_SIZE(w, h / 2))
#define GET_10B_CB_SIZE(w, h) (GET_8B_CB_SIZE(w, h) + GET_2B_SIZE(w, h / 2))
#define GET_10B_CR_SIZE(w, h) (GET_8B_CR_SIZE(w, h) + GET_2B_SIZE(w, h / 2))

#define GET_10B_UV_OFFSET(w, h) GET_10B_Y_SIZE(w, h)
#define GET_10B_CB_OFFSET(w, h) GET_10B_Y_SIZE(w, h)
#define GET_10B_CR_OFFSET(w, h) (GET_10B_Y_SIZE(w, h) + GET_10B_CB_SIZE(w, h))

#define SBWC_8B_STRIDE(w)     (128 * (((w) + 31) / 32))
#define SBWC_10B_STRIDE(w)    (160 * (((w) + 31) / 32))
#define SBWC_HEADER_STRIDE(w) ((((((w) + 63) / 64) + 15) / 16) * 16)

#define SBWC_8B_Y_SIZE(w, h)            ((SBWC_8B_STRIDE(w) * ((ALIGN((h), 16) + 3) / 4)) + 64)
#define SBWC_8B_Y_HEADER_SIZE(w, h)     ALIGN(((SBWC_HEADER_STRIDE(w) * ((ALIGN((h), 16) + 3) / 4)) + 256), 32)
#define SBWC_8B_CBCR_SIZE(w, h)         ((SBWC_8B_STRIDE(w) * (((ALIGN((h), 16) / 2) + 3) / 4)) + 64)
#define SBWC_8B_CBCR_HEADER_SIZE(w, h)  ((SBWC_HEADER_STRIDE(w) * (((ALIGN((h), 16) / 2) + 3) / 4)) + 128)

#define SBWC_10B_Y_SIZE(w, h)           ((SBWC_10B_STRIDE(w) * ((ALIGN((h), 16) + 3) / 4)) + 64)
#define SBWC_10B_Y_HEADER_SIZE(w, h)    ALIGN((((ALIGN((w), 32) * ALIGN((h), 16) * 2) + 256) - SBWC_10B_Y_SIZE(w, h)), 32)
#define SBWC_10B_CBCR_SIZE(w, h)        ((SBWC_10B_STRIDE(w) * (((ALIGN((h), 16) / 2) + 3) / 4)) + 64)
#define SBWC_10B_CBCR_HEADER_SIZE(w, h) (((ALIGN((w), 32) * ALIGN((h), 16)) + 256) - SBWC_10B_CBCR_SIZE(w, h))

#define SBWCL_8B_STRIDE(w, r)           (((128 * (r)) / 100) * (((w) + 31) / 32))
#define SBWCL_10B_STRIDE(w, r)          (((160 * (r)) / 100) * (((w) + 31) / 32))
#define SBWCL_8B_Y_SIZE(w, h, r)        ((SBWCL_8B_STRIDE(w, r) * ((ALIGN((h), 16) + 3) / 4)) + 64)
#define SBWCL_8B_CBCR_SIZE(w, h, r)     ((SBWCL_8B_STRIDE(w, r) * (((ALIGN((h), 16) / 2) + 3) / 4)) + 64)
#define SBWCL_10B_Y_SIZE(w, h, r)       ((SBWCL_10B_STRIDE(w, r) * ((ALIGN((h), 16) + 3) / 4)) + 64)
#define SBWCL_10B_CBCR_SIZE(w, h, r)    ((SBWCL_10B_STRIDE(w, r) * (((ALIGN((h), 16) / 2) + 3) / 4)) + 64)

#define InitOMXParams(params, size) \
    do { \
        (params)->nSize = size; \
        (params)->nVersion.s.nVersionMajor = 1; \
        (params)->nVersion.s.nVersionMinor = 0; \
        (params)->nVersion.s.nRevision     = 0; \
        (params)->nVersion.s.nStep         = 0; \
    } while(0)

#define INIT_SET_SIZE_VERSION(_struct_, _structType_)               \
    do {                                                            \
        Exynos_OSAL_Memset((_struct_), 0, sizeof(_structType_));       \
        (_struct_)->nSize = sizeof(_structType_);                   \
        (_struct_)->nVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER; \
        (_struct_)->nVersion.s.nVersionMinor = VERSIONMINOR_NUMBER; \
        (_struct_)->nVersion.s.nRevision = REVISION_NUMBER;         \
        (_struct_)->nVersion.s.nStep = STEP_NUMBER;                 \
    } while (0)

#define INIT_ARRAY_TO_VAL(array, value, size)   \
    do {                                        \
        int i;                                  \
        for (i = 0; i < size; i++) {            \
            array[i] = value;                   \
        }                                       \
    } while(0)

/*
 * Port Specific
 */
#define EXYNOS_TUNNEL_ESTABLISHED 0x0001
#define EXYNOS_TUNNEL_IS_SUPPLIER 0x0002

#define CHECK_PORT_BEING_FLUSHED(port)                 (((port)->portState == EXYNOS_OMX_PortStateFlushing) || ((port)->portState == EXYNOS_OMX_PortStateFlushingForDisable))
#define CHECK_PORT_BEING_DISABLED(port)                ((port)->portState == EXYNOS_OMX_PortStateDisabling)
#define CHECK_PORT_BEING_ENABLED(port)                 ((port)->portState == EXYNOS_OMX_PortStateEnabling)
#define CHECK_PORT_ENABLED(port)                       ((port)->portDefinition.bEnabled == OMX_TRUE)
#define CHECK_PORT_POPULATED(port)                     ((port)->portDefinition.bPopulated == OMX_TRUE)
#define CHECK_PORT_TUNNELED(port)                      ((port)->tunnelFlags & EXYNOS_TUNNEL_ESTABLISHED)
#define CHECK_PORT_BUFFER_SUPPLIER(port)               ((port)->tunnelFlags & EXYNOS_TUNNEL_IS_SUPPLIER)

#endif
