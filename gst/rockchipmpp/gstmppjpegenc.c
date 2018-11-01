/*
 * Copyright 2018 Rockchip Electronics Co., Ltd
 *     Author: Randy Li <randy.li@rock-chips.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>

#include "gstmppjpegenc.h"

#define JPEG_DEFAULT_QUALITY 85

enum
{
  PROP_0,
  PROP_QUALITY
};

#define GST_CAT_DEFAULT mppvideoenc_debug

#define parent_class gst_mpp_jpeg_enc_parent_class
G_DEFINE_TYPE (GstMppJpegEnc, gst_mpp_jpeg_enc, GST_TYPE_MPP_VIDEO_ENC);

static GstStaticPadTemplate gst_mpp_jpeg_enc_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw,"
        "format = (string) NV12, "
        "width  = (int) [ 96, 8192 ], "
        "height = (int) [ 32, 8192 ], "
        "framerate = (fraction) [0/1, 60/1]"
        ";"
        "video/x-raw,"
        "format = (string) I420, "
        "width  = (int) [ 96, 8192 ], "
        "height = (int) [ 32, 8192 ], "
        "framerate = (fraction) [0/1, 60/1]"
        ";"
        "video/x-raw,"
        "format = (string) YUY2, "
        "width  = (int) [ 96, 8192 ], "
        "height = (int) [ 32, 8192 ], "
        "framerate = (fraction) [0/1, 60/1]"
        ";"
        "video/x-raw,"
        "format = (string) UYVY, "
        "width  = (int) [ 96, 8192 ], "
        "height = (int) [ 32, 8192 ], "
        "framerate = (fraction) [0/1, 60/1]" ";"));

static GstStaticPadTemplate gst_mpp_jpeg_enc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg, "
        "width  = (int) [ 96, 8192 ], " "height = (int) [ 32, 8192 ], "
        /* Up to 90 million pixels per second at the rk3399 */
        "framerate = (fraction) [0/1, 60/1], " "sof-marker = { 0 }")
    );

static gboolean
gst_mpp_jpeg_enc_open (GstVideoEncoder * encoder)
{
  GstMppVideoEnc *mpp_video_enc = GST_MPP_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (mpp_video_enc, "Opening");

  if (mpp_video_enc->mpi->control (mpp_video_enc->mpp_ctx, MPP_ENC_SET_RC_CFG,
          &self->rc_cfg)) {
    GST_DEBUG_OBJECT (self, "Setting rate control for rockchip mpp failed");
    return FALSE;
  }

  if (mpp_video_enc->mpi->control (mpp_video_enc->mpp_ctx,
          MPP_ENC_SET_CODEC_CFG, &self->codec_cfg)) {
    GST_DEBUG_OBJECT (self, "Setting codec info for rockchip mpp failed");
    return FALSE;
  }

  if (mpp_create (&mpp_video_enc->mpp_ctx, &mpp_video_enc->mpi))
    return FALSE;
  if (mpp_init (mpp_video_enc->mpp_ctx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG))
    return FALSE;

  return TRUE;
}

static gboolean
gst_mpp_jpeg_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstMppJpegEnc *self = GST_MPP_JPEG_ENC (encoder);
  GstMppVideoEnc *mpp_video_enc = GST_MPP_VIDEO_ENC (encoder);

  GST_OBJECT_LOCK (self);

  self->rc_cfg.fps_in_num = GST_VIDEO_INFO_FPS_N (&state->info);
  self->rc_cfg.fps_in_denorm = GST_VIDEO_INFO_FPS_D (&state->info);
  self->rc_cfg.fps_out_num = GST_VIDEO_INFO_FPS_N (&state->info);
  self->rc_cfg.fps_out_denorm = GST_VIDEO_INFO_FPS_D (&state->info);
  self->rc_cfg.gop = GST_VIDEO_INFO_FPS_N (&state->info)
      / GST_VIDEO_INFO_FPS_D (&state->info);
  self->rc_cfg.skip_cnt = 0;

  GST_OBJECT_UNLOCK (self);
  return GST_MPP_VIDEO_ENC_CLASS (parent_class)->set_format (encoder, state);
set_format_error:
  GST_OBJECT_UNLOCK (self);
  return FALSE;
}

static GstFlowReturn
gst_mpp_jpeg_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstCaps *outcaps;

  outcaps = gst_caps_new_empty_simple ("image/jpeg");

  return GST_MPP_VIDEO_ENC_CLASS (parent_class)->handle_frame (encoder, frame,
      outcaps);
}

static void
gst_mpp_jpeg_enc_init (GstMppJpegEnc * self)
{
  GST_OBJECT_LOCK (self);
  memset (&self->rc_cfg, 0, sizeof (self->rc_cfg));
  memset (&self->codec_cfg, 0, sizeof (self->codec_cfg));

  self->rc_cfg.change = MPP_ENC_RC_CFG_CHANGE_ALL;
  self->rc_cfg.rc_mode = MPP_ENC_RC_MODE_CBR;
  self->rc_cfg.quality = MPP_ENC_RC_QUALITY_MEDIUM;

  self->rc_cfg.fps_in_flex = 0;
  self->rc_cfg.fps_in_num = 30;
  self->rc_cfg.fps_in_denorm = 1;
  self->rc_cfg.fps_out_flex = 0;
  self->rc_cfg.fps_out_num = 30;
  self->rc_cfg.fps_out_denorm = 1;
  self->rc_cfg.gop = 30;
  self->rc_cfg.skip_cnt = 0;

  self->codec_cfg.coding = MPP_VIDEO_CodingMJPEG;
  self->codec_cfg.jpeg.quant = JPEG_DEFAULT_QUALITY;
  self->codec_cfg.jpeg.change = MPP_ENC_JPEG_CFG_CHANGE_QP;

  GST_OBJECT_UNLOCK (self);
}

static void
gst_mpp_jpeg_enc_class_init (GstMppJpegEncClass * klass)
{
  GstElementClass *element_class;
  GstVideoEncoderClass *video_encoder_class;

  element_class = (GstElementClass *) klass;
  video_encoder_class = (GstVideoEncoderClass *) klass;

  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_int ("quality", "Quality", "Quality of encoding",
          0, 100, JPEG_DEFAULT_QUALITY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  gst_element_class_set_static_metadata (element_class,
      "Rockchip Mpp JPEG Encoder",
      "Codec/Encoder/Video",
      "Encode video streams via Rockchip Mpp",
      "Randy Li <randy.li@rock-chips.com>");

  video_encoder_class->open = GST_DEBUG_FUNCPTR (gst_mpp_jpeg_enc_open);
  video_encoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_mpp_jpeg_enc_set_format);
  video_encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_mpp_jpeg_enc_handle_frame);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_mpp_jpeg_enc_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_mpp_jpeg_enc_sink_template));
}

static void
gst_mpp_jpeg_enc_set_property  (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMppJpegEnc *self = GST_MPP_JPEG_ENC (encoder);
  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_QUALITY:
      self->codec_cfg.jpeg.quant = g_value_get_int (value);
      if (mpp_video_enc->mpi->control (mpp_video_enc->mpp_ctx,
          MPP_ENC_SET_CODEC_CFG, &self->codec_cfg)) {
        GST_DEBUG_OBJECT (self, "Setting codec info for rockchip mpp failed");
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}
