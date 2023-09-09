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
 * @file    ExynosVideo_OSAL_Enc.h
 * @brief   ExynosVideo OSAL define related with Encoder
 * @author  SeungBeom Kim (sbcrux.kim@samsung.com)
 *          Taehwan Kim   (t_h.kim@samsung.com)
 * @version    1.0.0
 * @history
 *   2016.01.07 : Create
 */

#ifndef _EXYNOS_VIDEO_OSAL_ENC_H_
#define _EXYNOS_VIDEO_OSAL_ENC_H_

#include "ExynosVideoApi.h"
#include "ExynosVideo_OSAL.h"

/* Normal Node */
#define VIDEO_MFC_ENCODER_NAME               "s5p-mfc-enc"
#define VIDEO_HEVC_ENCODER_NAME              "exynos-hevc-enc"
/* Secure Node */
#define VIDEO_MFC_SECURE_ENCODER_NAME        "s5p-mfc-enc-secure"
#define VIDEO_HEVC_SECURE_ENCODER_NAME       "exynos-hevc-enc-secure"
/* OTF Node */
#define VIDEO_MFC_OTF_ENCODER_NAME           "s5p-mfc-enc-otf"
#define VIDEO_MFC_OTF_SECURE_ENCODER_NAME    "s5p-mfc-enc-otf-secure"

#define MAX_CTRL_NUM          112 /* +1(drop ctrl) */
#define H264_CTRL_NUM         92  /* +7(svc), +4(skype), +1(roi), +4(qp range) +1(pvc) +1(drop ctrl) + 2(chroma_qp) */
#define MPEG4_CTRL_NUM        26  /* +4(qp range) */
#define H263_CTRL_NUM         18  /* +2(qp range) */
#define VP8_CTRL_NUM          30  /* +3(svc), +2(qp range) */
#define HEVC_CTRL_NUM         53  /* +7(svc), +1(roi), +4(qp range) + 2(chroma_qp) */
#define VP9_CTRL_NUM          29  /* +3(svc), +4(qp range) */

#endif
