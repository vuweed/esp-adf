/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_video_enc_default.h"

esp_vc_err_t esp_video_enc_register_default(void)
{
    esp_vc_err_t ret = ESP_VC_ERR_OK;
#ifdef CONFIG_VIDEO_ENCODER_HW_MJPEG_SUPPORT
    ret |= esp_video_enc_register_mjpeg();
#endif

#ifdef CONFIG_VIDEO_ENCODER_SW_MJPEG_SUPPORT
    ret |= esp_video_enc_register_sw_mjpeg();
#endif

#ifdef CONFIG_VIDEO_ENCODER_HW_H264_SUPPORT
    ret |= esp_video_enc_register_h264();
#endif

#ifdef CONFIG_VIDEO_ENCODER_SW_H264_SUPPORT
    ret |= esp_video_enc_register_sw_h264();
#endif
    return ret;
}

void esp_video_enc_unregister_default(void)
{
#ifdef CONFIG_VIDEO_ENCODER_HW_MJPEG_SUPPORT
    esp_video_enc_unregister_mjpeg();
#endif

#ifdef CONFIG_VIDEO_ENCODER_SW_MJPEG_SUPPORT
    esp_video_enc_unregister_sw_mjpeg();
#endif

#ifdef CONFIG_VIDEO_ENCODER_HW_H264_SUPPORT
    esp_video_enc_unregister_h264();
#endif

#ifdef CONFIG_VIDEO_ENCODER_SW_H264_SUPPORT
    esp_video_enc_unregister_sw_h264();
#endif
}
