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

#ifndef _EXYNOS_VIDEO_ENC_H_
#define _EXYNOS_VIDEO_ENC_H_

#include "ExynosVideoApi.h"
#include "ExynosVideo_OSAL.h"

#define VIDEO_ENCODER_DEFAULT_INBUF_PLANES  2
#define VIDEO_ENCODER_DEFAULT_OUTBUF_PLANES 1
#define VIDEO_ENCODER_POLL_TIMEOUT      25

#ifndef FRAME_RATE_CHANGE_THRESH_HOLD
#define FRAME_RATE_CHANGE_THRESH_HOLD 1
#endif
#define FRAME_RATE_CHANGE_LIMIT       480
#define ROUND_UP(x) ((((int)((x) * 10) % 10) > 4)? (x + 1):(x))
#define CHECK_FRAMERATE_VALIDITY(x, y)                   \
    (((x) > 0) &&                                        \
     ((((x) - (y)) >= FRAME_RATE_CHANGE_THRESH_HOLD)  || \
      (((y) - (x)) >= FRAME_RATE_CHANGE_THRESH_HOLD)) && \
     ((x) <= FRAME_RATE_CHANGE_LIMIT))

static const int vp8_qp_trans[] =
{
    0,   1,  2,  3,  4,  5,  7,  8,
    9,  10, 12, 13, 15, 17, 18, 19,
    20,  21, 23, 24, 25, 26, 27, 28,
    29,  30, 31, 33, 35, 37, 39, 41,
    43,  45, 47, 49, 51, 53, 55, 57,
    59,  61, 64, 67, 70, 73, 76, 79,
    82,  85, 88, 91, 94, 97, 100, 103,
    106, 109, 112, 115, 118, 121, 124, 127,
};

static const int vp9_qp_trans[] = {
  0,    4,   8,  12,  16,  20,  24,  28,
  32,   36,  40,  44,  48,  52,  56,  60,
  64,   68,  72,  76,  80,  84,  88,  92,
  96,  100, 104, 108, 112, 116, 120, 124,
  128, 132, 136, 140, 144, 148, 152, 156,
  160, 164, 168, 172, 176, 180, 184, 188,
  192, 196, 200, 204, 208, 212, 216, 220,
  224, 228, 232, 236, 240, 244, 249, 255,
};

#define GET_VALUE(value, min, max) ((value < min)? min:(value > max)? max:value)
#define GET_H264_QP_VALUE(value) GET_VALUE(value, 0, 51)
#define GET_MPEG4_QP_VALUE(value) GET_VALUE(value, 1, 31)
#define GET_H263_QP_VALUE(value) GET_VALUE(value, 1, 31)
#define GET_VP8_QP_VALUE(value) (vp8_qp_trans[GET_VALUE(value, 0, ((int)(sizeof(vp8_qp_trans)/sizeof(int)) - 1))])
inline int GET_HEVC_QP_VALUE(int value, int version) {
    if (version == MFC_120)
        return GET_VALUE(value, -12, 51);
    else
        return GET_VALUE(value, 0, 51);
}
#define GET_VP9_QP_VALUE(value) (vp9_qp_trans[GET_VALUE(value, 1, ((int)(sizeof(vp9_qp_trans)/sizeof(int)) - 1))])

ExynosVideoErrorType MFC_Exynos_Video_GetInstInfo_Encoder(
    ExynosVideoInstInfo *pVideoInstInfo);

int MFC_Exynos_Video_Register_Encoder(
    ExynosVideoEncOps       *pEncOps,
    ExynosVideoEncBufferOps *pInbufOps,
    ExynosVideoEncBufferOps *pOutbufOps);

#endif /* _EXYNOS_VIDEO_ENC_H_ */
