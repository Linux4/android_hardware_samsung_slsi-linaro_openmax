/*
 *
 * Copyright 2016 Samsung Electronics S.LSI Co. LTD
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
 * @file    ExynosVideo_OSAL_Dec.h
 * @brief   ExynosVideo OSAL define related with Decoder
 * @author  SeungBeom Kim (sbcrux.kim@samsung.com)
 *          Taehwan Kim   (t_h.kim@samsung.com)
 * @version    1.0.0
 * @history
 *   2016.01.07 : Create
 */

#ifndef _EXYNOS_VIDEO_OSAL_DEC_H_
#define _EXYNOS_VIDEO_OSAL_DEC_H_

#include "ExynosVideoApi.h"
#include "ExynosVideo_OSAL.h"

/* Normal Node */
#define VIDEO_MFC_DECODER_NAME               "s5p-mfc-dec"
#define VIDEO_HEVC_DECODER_NAME              "exynos-hevc-dec"
/* Secure Node */
#define VIDEO_MFC_SECURE_DECODER_NAME        "s5p-mfc-dec-secure"
#define VIDEO_HEVC_SECURE_DECODER_NAME       "exynos-hevc-dec-secure"

#define FRAME_PACK_SEI_INFO_NUM         4
#define HDR_INFO_NUM                    12

#define OPERATE_BIT(x, mask, shift)     ((x & (mask << shift)) >> shift)

#endif
