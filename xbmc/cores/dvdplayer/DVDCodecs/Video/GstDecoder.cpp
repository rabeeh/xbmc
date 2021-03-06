/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

/*
 * Rabeeh - TODO -
 * 1. Add better reset implementation instead of flushing by seeking
 * 2. Add delay for pipeline gstreamer creation and wait for paused state
 *    The reason is that there is always hickups when video starts to play
 *    since XBMC starts sending frames for decoding, but gstreamer pipe
 *    still not ready.
 * 3. Fix proper pipe closing. There seems to be missing 30 frames at the
 *    end of every playback. The reason is that gstreamer is done on decoding
 *    but video renderer is still showing those decoded frames.
 * 4. Change ffmpegcolorspace in gstreamer to output either YUV420, UYVU and
 *    other supported dove-overlay video layer color formats (I420 is YUV420
 *    but with swapped color space etc...)
 */

#include "GstDecoder.h"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

CGstDecoder::CGstDecoder(IGstDecoderCallback *callback) : m_callback(callback)
{
  m_loop      = NULL;
  m_pipeline  = NULL;
}

CGstDecoder::~CGstDecoder()
{
  if (m_pipeline)
  {
    gst_object_unref(m_pipeline);
    m_pipeline = NULL;
  }
}

GstElement *CGstDecoder::Open(GstCaps *sourceCapabilities)
{
  m_loop = g_main_loop_new (NULL, FALSE);
  if (m_loop == NULL)
    return false;

  gchar *capsString = gst_caps_to_string(sourceCapabilities);

  printf("GStreamer: The capabilities from source are %s\n", capsString);

  gchar *pipelineString = g_strdup_printf("appsrc caps=\"%s\" name=\"AppSrc\" stream-type=\"seekable\" ! decodebin2 ! ffmpegcolorspace ! appsink caps=\"video/x-raw-yuv,format=(fourcc)UYVY\" name=\"AppSink\" sync=\"false\" async=\"true\"", capsString);
  printf("GStreamer: Entire pipeline is %s\n", pipelineString);

  m_pipeline = gst_parse_launch(pipelineString, NULL);
  g_free(capsString);
  g_free(pipelineString);

  if (m_pipeline == NULL)
    return NULL;

  GstBus *bus = gst_element_get_bus(m_pipeline);
  gst_bus_add_watch (bus, (GstBusFunc)BusCallback, this);
  gst_object_unref (bus);

  GstElement *AppSrc = gst_bin_get_by_name(GST_BIN(m_pipeline), "AppSrc");

  if (AppSrc)
  {
    g_signal_connect(AppSrc, "need-data", G_CALLBACK (OnNeedData), this);
    g_signal_connect(AppSrc, "enough-data", G_CALLBACK (OnEnoughData), this);
    g_signal_connect(AppSrc, "seek-data", G_CALLBACK (OnSeekData), this);

  }
  else
    printf("GStreamer: Failure to hook up to AppSrc\n");

  GstElement *AppSink = gst_bin_get_by_name(GST_BIN(m_pipeline), "AppSink");
  if (AppSink)
  {
    g_object_set(G_OBJECT(AppSink), "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(AppSink, "new-buffer", G_CALLBACK(CGstDecoder::OnDecodedBuffer), this);
    gst_object_unref(AppSink);
  }
  else printf("GStreamer: Failure to hook up to AppSink\n");

  Create();
  m_wait_timeout = 20; // CrystalHD uses 1;
  return AppSrc;
}

bool CGstDecoder::Seek(GstElement *element)
{
  if (!gst_element_seek (m_pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                         GST_SEEK_TYPE_NONE, 0,
                         GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
    g_print ("Seek failed!\n");
	}
}

void CGstDecoder::DisposePipeline(void)
{
  if (m_pipeline) {
    printf ("DisposePipeline\n");
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_element_get_state(m_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
    printf ("OK\n");
  }
}

void CGstDecoder::StopThread(bool bWait)
{
//  printf ("Called StopThread (bWait = %d)\n",bWait);
//  gst_element_set_state(m_pipeline, GST_STATE_NULL);
//  gst_element_get_state(m_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
//  printf ("Called StopThread (bWait = %d)\n",bWait);
  g_main_loop_quit(m_loop);
  printf ("Called StopThread (bWait = %d)\n",bWait);
//  Sleep(1000);
  CThread::StopThread(bWait);
}

void CGstDecoder::Process()
{
	printf ("Putting STATE to PLAYING\n");
  gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
  gst_element_get_state(m_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
  g_main_loop_run(m_loop);
  gst_element_set_state(m_pipeline, GST_STATE_NULL);
  gst_object_unref (m_pipeline);
  m_pipeline = NULL;
//  gst_element_get_state(m_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
}

void CGstDecoder::OnDecodedBuffer(GstElement *appsink, void *data)
{
  CGstDecoder *decoder = (CGstDecoder *)data;

  GstBuffer *buffer = gst_app_sink_pull_buffer(GST_APP_SINK(appsink));
  if (buffer)
  {
    if (decoder->m_callback)
      decoder->m_callback->OnDecodedBuffer(buffer);
    else
      gst_buffer_unref(buffer);
  }
  else
    printf("GStreamer: OnDecodedBuffer - Null Buffer\n");
}

void CGstDecoder::OnNeedData(GstElement *appsrc, guint size, void *data)
{
  CGstDecoder *decoder = (CGstDecoder *)data;

  if (decoder->m_callback)
    decoder->m_callback->OnNeedData();
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
gboolean CGstDecoder::OnSeekData (GstElement *appsrc, guint64 position, void *data)
{
	static int counter=0;
	counter ++;
//  printf ("Requested to seek to %lld\n",position);
//	if (counter == 2) while(1){};
  return TRUE; 
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
void CGstDecoder::OnEnoughData (GstElement *appsrc, void *data)
{
  CGstDecoder *decoder = (CGstDecoder *)data;

  if (decoder->m_callback)
    decoder->m_callback->OnEnoughData();
}

gboolean CGstDecoder::BusCallback(GstBus *bus, GstMessage *msg, gpointer data)
{
  CGstDecoder *decoder = (CGstDecoder *)data;
  gchar  *str;

  switch (GST_MESSAGE_TYPE(msg))
  {
    case GST_MESSAGE_EOS:
      g_print ("GStreamer: End of stream\n");
      g_main_loop_quit(decoder->m_loop);
      break;

    case GST_MESSAGE_ERROR:
      GError *error;

      gst_message_parse_error (msg, &error, &str);
      g_free (str);

      g_printerr ("GStreamer: Error - %s %s\n", str, error->message);
      g_error_free (error);

      g_main_loop_quit(decoder->m_loop);
      break;

    case GST_MESSAGE_WARNING:
      GError *warning;

      gst_message_parse_error (msg, &warning, &str);
      g_free (str);

      g_printerr ("GStreamer: Warning - %s %s\n", str, warning->message);
      g_error_free (warning);
      break;

    case GST_MESSAGE_INFO:
      GError *info;

      gst_message_parse_error (msg, &info, &str);
      g_free (str);

      g_printerr ("GStreamer: Info - %s %s\n", str, info->message);
      g_error_free (info);
      break;

    case GST_MESSAGE_TAG:
      printf("GStreamer: Message TAG\n");
      break;
    case GST_MESSAGE_BUFFERING:
      printf("GStreamer: Message BUFFERING\n");
      break;
    case GST_MESSAGE_STATE_CHANGED:
      printf("GStreamer: Message STATE_CHANGED\n");
      GstState old_state, new_state;
      gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
      printf("GStreamer: Element %s changed state from %s to %s.\n",
          GST_OBJECT_NAME (msg->src),
          gst_element_state_get_name (old_state),
          gst_element_state_get_name (new_state));
      break;
    case GST_MESSAGE_STATE_DIRTY:
      printf("GStreamer: Message STATE_DIRTY\n");
      break;
    case GST_MESSAGE_STEP_DONE:
      printf("GStreamer: Message STEP_DONE\n");
      break;
    case GST_MESSAGE_CLOCK_PROVIDE:
      printf("GStreamer: Message CLOCK_PROVIDE\n");
      break;
    case GST_MESSAGE_CLOCK_LOST:
      printf("GStreamer: Message CLOCK_LOST\n");
      break;
    case GST_MESSAGE_NEW_CLOCK:
      printf("GStreamer: Message NEW_CLOCK\n");
      break;
    case GST_MESSAGE_STRUCTURE_CHANGE:
      printf("GStreamer: Message STRUCTURE_CHANGE\n");
      break;
    case GST_MESSAGE_STREAM_STATUS:
      printf("GStreamer: Message STREAM_STATUS\n");
      break;
    case GST_MESSAGE_APPLICATION:
      printf("GStreamer: Message APPLICATION\n");
      break;
    case GST_MESSAGE_ELEMENT:
      printf("GStreamer: Message ELEMENT\n");
      break;
    case GST_MESSAGE_SEGMENT_START:
      printf("GStreamer: Message SEGMENT_START\n");
      break;
    case GST_MESSAGE_SEGMENT_DONE:
      printf("GStreamer: Message SEGMENT_DONE\n");
      break;
    case GST_MESSAGE_DURATION:
      printf("GStreamer: Message DURATION\n");
      break;
    case GST_MESSAGE_LATENCY:
      printf("GStreamer: Message LATENCY\n");
      break;
    case GST_MESSAGE_ASYNC_START:
      printf("GStreamer: Message ASYNC_START\n");
      break;
    case GST_MESSAGE_ASYNC_DONE:
      printf("GStreamer: Message ASYNC_DONE\n");
      break;
    case GST_MESSAGE_REQUEST_STATE:
      printf("GStreamer: Message REQUEST_STATE\n");
      break;
    case GST_MESSAGE_STEP_START:
      printf("GStreamer: Message STEP_START\n");
      break;
#if 0 /* Breaks build for now */
    case GST_MESSAGE_QOS:
      printf("GStreamer: Message QOS\n");
      break;
#endif
    default:
      printf("GStreamer: Unknown message %i\n", GST_MESSAGE_TYPE(msg));
      break;
  }

  return TRUE;
}
