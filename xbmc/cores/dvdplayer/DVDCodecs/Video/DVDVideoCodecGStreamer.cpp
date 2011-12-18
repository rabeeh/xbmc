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

typedef unsigned char byte;
#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#endif
#undef byte
#include "DVDVideoCodecGStreamer.h"
#include "DVDStreamInfo.h"
#include "DVDClock.h"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

bool CDVDVideoCodecGStreamer::gstinitialized = false;

CDVDVideoCodecGStreamer::CDVDVideoCodecGStreamer()
{
  if (gstinitialized == false)
 {
    gst_init (NULL, NULL);
    gstinitialized = true;
  }

  m_initialized = false;
  m_pictureBuffer = NULL;
  m_pictureBufferTbr = NULL;
  m_pictureBufferTbr1 = NULL;

  m_decoder = NULL;
  m_needData = true;
  m_dropState = false;
  m_AppSrc = NULL;
  m_AppSrcCaps = NULL;
  m_ptsinvalid = true;

  m_timebase = 1000.0;
}

CDVDVideoCodecGStreamer::~CDVDVideoCodecGStreamer()
{
  Dispose();
}

bool CDVDVideoCodecGStreamer::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  Dispose();

  m_ptsinvalid = hints.ptsinvalid;

  m_AppSrcCaps = CreateVideoCaps(hints, options);

  if (m_AppSrcCaps)
  {
    m_decoder = new CGstDecoder(this);
    m_AppSrc = m_decoder->Open(m_AppSrcCaps);
  }

  return (m_AppSrc != NULL);
}

void CDVDVideoCodecGStreamer::Dispose()
{

  if (0)//m_AppSrc)
  {
    GstFlowReturn ret;
    g_signal_emit_by_name(m_AppSrc, "end-of-stream", &ret);

    if (ret != GST_FLOW_OK)
      printf("GStreamer: OnDispose. Flow error %i\n", ret);
    GstStateChangeReturn state;
    state = gst_element_get_state(m_AppSrc, NULL, NULL, GST_CLOCK_TIME_NONE);

    gst_object_unref(m_AppSrc);
    m_AppSrc = NULL;
  }
  m_AppSrc = NULL;
//  if (m_decoder)
//    m_decoder->DisposePipeline();
  while (m_pictureQueue.size())
  {
    gst_buffer_unref(m_pictureQueue.front());
    m_pictureQueue.pop();
  }

  if (m_pictureBuffer)
  {
	printf ("Disposing buffer at 0x%x\n",m_pictureBuffer->data);
    gst_buffer_unref(m_pictureBuffer);
    m_pictureBuffer = NULL;
  }
  if (m_pictureBufferTbr)
  {
	printf ("Disposing buffer at 0x%x\n",m_pictureBufferTbr->data);
    gst_buffer_unref(m_pictureBufferTbr);
    m_pictureBufferTbr = NULL;
  }
  if (m_pictureBufferTbr1)
  {
	printf ("Disposing buffer at 0x%x\n",m_pictureBufferTbr1->data);
    gst_buffer_unref(m_pictureBufferTbr1);
    m_pictureBufferTbr1 = NULL;
  }
  if (m_AppSrcCaps)
  {
    gst_caps_unref(m_AppSrcCaps);
    m_AppSrcCaps = NULL;
  }

  if (m_decoder)
  {
    printf ("Stopping thread...");
    m_decoder->StopThread();
    printf ("Done\n");
    delete m_decoder;
    m_decoder = NULL;

    m_initialized = false;
  }
   while (m_pictureQueue.size())
  {
    gst_buffer_unref(m_pictureQueue.front());
    m_pictureQueue.pop();
  }

 printf ("Everything should be disposed now\n");
}

int CDVDVideoCodecGStreamer::Decode(BYTE* pData, int iSize, double dts, double pts)
{
  CSingleLock lock(m_monitorLock);
  int result = 0;
  int queueSize = m_pictureQueue.size();
  GstBuffer *buffer = NULL;
  if (pData)
  {
    buffer = gst_buffer_new_and_alloc(iSize);
    if (buffer)
    {
      memcpy(GST_BUFFER_DATA(buffer), pData, iSize);

      GST_BUFFER_TIMESTAMP(buffer) = pts * 1000.0;

      GstFlowReturn ret;
      g_signal_emit_by_name(m_AppSrc, "push-buffer", buffer, &ret);

      if (ret != GST_FLOW_OK)
        printf("GStreamer: OnDecode. Flow error %i\n", ret);

      gst_buffer_unref(buffer);
   } else printf ("WARNING - Couldn't allocate GST buffer\n");
  }

#if 0 
	// Rabeeh - invsetigate the following
  if (m_pictureBufferTbr1)
 {
//	printf ("Disposing buffer 0x%x in ::Decode\n",m_pictureBuffer->data);
	// This code runs all the time, but seems in the wrong place !!!
    gst_buffer_unref(m_pictureBufferTbr1);
    m_pictureBufferTbr1 = NULL;
  }
#endif
  if ((queueSize < 34)) {// && (m_needData == true)) {
	result |= VC_BUFFER;
  }
  if (queueSize > 0 ) {
	result |= VC_PICTURE;
  }
  { static int counter=0;
	counter++;
	if (!(counter%20)) printf ("Queue size is %d\n",queueSize);
    return result;
  }
}

void CDVDVideoCodecGStreamer::Reset()
{
	GstPad *pad;
	CSingleLock lock(m_monitorLock);
	GstStateChangeReturn state;
	int disposed = 0;
	printf ("Enter reset\n");
#if 0
	pad = gst_element_get_pad (m_AppSrc, "src");
	if (pad) {
		gst_pad_push_event (pad, gst_event_new_flush_start());
		state = gst_element_get_state(m_AppSrc, NULL, NULL, GST_CLOCK_TIME_NONE);
		gst_pad_push_event (pad, gst_event_new_flush_stop());
		state = gst_element_get_state(m_AppSrc, NULL, NULL, GST_CLOCK_TIME_NONE);
	}
#endif
	while (m_pictureQueue.size())
	{
		gst_buffer_unref(m_pictureQueue.front());
		m_pictureQueue.pop();
		disposed++;
	}
	if (m_pictureBuffer)
	{
		gst_buffer_unref(m_pictureBuffer);
		m_pictureBuffer = NULL;
		disposed ++;
	}
	if (m_pictureBufferTbr)
	{
		gst_buffer_unref(m_pictureBufferTbr);
		m_pictureBufferTbr = NULL;
		disposed ++;
	}
	if (m_pictureBufferTbr1)
	{
		gst_buffer_unref(m_pictureBufferTbr1);
		m_pictureBufferTbr1 = NULL;
		disposed ++;
	}
	m_decoder->Seek(m_AppSrc);
	if (disposed) printf ("In %s - Disposed %d frames\n",__FUNCTION__,disposed);
 
}

bool CDVDVideoCodecGStreamer::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
	static int counter1=0, counter2=0,counter3=0;
  CSingleLock lock(m_monitorLock);
	counter1++;
  if (m_pictureQueue.size())
  {
	counter2++;
	// Rabeeh - invsetigate the following
    if (m_pictureBufferTbr1)
    {
       gst_buffer_unref(m_pictureBufferTbr1);
    }
    m_pictureBufferTbr1 = m_pictureBufferTbr;
    m_pictureBufferTbr = m_pictureBuffer;
    m_pictureBuffer = m_pictureQueue.front();
    m_pictureQueue.pop();
  }
  else
	{
		counter3++;
	    return false;
	}
  GstCaps *caps = gst_buffer_get_caps(m_pictureBuffer);
  if (caps == NULL)
  {
    printf("GStreamer: No caps on decoded buffer\n");
    return false;
  }

  GstStructure *structure = gst_caps_get_structure (caps, 0);
  int width = 0, height = 0;
  if (structure == NULL ||
      !gst_structure_get_int (structure, "width", (int *) &width) ||
      !gst_structure_get_int (structure, "height", (int *) &height))
  {
    printf("GStreamer: invalid caps on decoded buffer\n");
    return false;
  }

  pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth  = width;
  pDvdVideoPicture->iDisplayHeight = pDvdVideoPicture->iHeight = height;

//  pDvdVideoPicture->format = DVDVideoPicture::FMT_YUV420P;
  pDvdVideoPicture->format = DVDVideoPicture::FMT_UYVY;

#define ALIGN(x, n) (((x) + (n) - 1) & (~((n) - 1)))
//	printf ("Sending up picture buffer at 0x%x\n",m_pictureBuffer->data);
  pDvdVideoPicture->data[0] = m_pictureBuffer->data;
  pDvdVideoPicture->iLineSize[0] = ALIGN (width, 4);
  pDvdVideoPicture->data[1] = pDvdVideoPicture->data[0] + pDvdVideoPicture->iLineSize[0] * ALIGN (height, 2);
  pDvdVideoPicture->iLineSize[1] = ALIGN (width, 8) / 2;
  pDvdVideoPicture->data[2] = (BYTE *)m_pictureBuffer;
//pDvdVideoPicture->data[1] + pDvdVideoPicture->iLineSize[1] * ALIGN (height, 2) / 2;
  pDvdVideoPicture->iLineSize[2] = pDvdVideoPicture->iLineSize[1];
  static int counter = 0;
#undef ALIGN

  pDvdVideoPicture->dts = DVD_NOPTS_VALUE;
  if (GST_BUFFER_TIMESTAMP_IS_VALID(m_pictureBuffer))
    pDvdVideoPicture->pts = ((double)GST_BUFFER_TIMESTAMP(m_pictureBuffer) / 1000.0);
  else pDvdVideoPicture->pts = DVD_NOPTS_VALUE;
  if (GST_BUFFER_DURATION_IS_VALID(m_pictureBuffer))
    pDvdVideoPicture->iDuration = (double)GST_BUFFER_DURATION(m_pictureBuffer) / 1000.0;
  if (0){//!(counter1%20)) {
	printf ("Counters %d %d %d (pts/dts/duration for this frame %f / %f / %f)\n",counter1,counter2,counter3, pDvdVideoPicture->pts,pDvdVideoPicture->dts,(double)GST_BUFFER_DURATION(m_pictureBuffer) / 1000.0);
	printf ("Total ready pictures %d\n",m_pictureQueue.size());
	}
  return true;
}

void CDVDVideoCodecGStreamer::SetDropState(bool bDrop)
{
	CSingleLock lock(m_monitorLock);
	if (bDrop != m_dropState) printf ("%s Notice - Called set drop state %d\n",__FUNCTION__,bDrop);
	m_dropState = bDrop;
	if (m_dropState)
	{
		int disposed=0;
		if /*while*/ (m_pictureQueue.size() > 1) // Keep last frame
		{
			gst_buffer_unref(m_pictureQueue.front());
			m_pictureQueue.pop();
			disposed++;
		}
#if 0
		if (m_pictureBuffer)
		{
			gst_buffer_unref(m_pictureBuffer);
			m_pictureBuffer = NULL;
		}
		if (m_pictureBufferTbr)
		{
			gst_buffer_unref(m_pictureBufferTbr);
			m_pictureBufferTbr = NULL;
		}
		if (m_pictureBufferTbr1)
		{
			gst_buffer_unref(m_pictureBufferTbr1);
			m_pictureBufferTbr1 = NULL;
		}
#endif
		if (disposed) printf ("Disposed %d frames\n",disposed);
	}
}

const char *CDVDVideoCodecGStreamer::GetName()
{
  return "GStreamer";
}

void CDVDVideoCodecGStreamer::OnDecodedBuffer(GstBuffer *buffer)
{
  if (buffer)
  {
//    if (m_dropState) gst_buffer_unref(buffer);
    CSingleLock lock(m_monitorLock);
    m_pictureQueue.push(buffer);
  }
  else
    printf("GStreamer: Received null buffer?\n");
}

void CDVDVideoCodecGStreamer::OnNeedData()
{
//	if (m_needData == false) printf ("Setting m_needData to true\n");
  m_needData = true;
}

void CDVDVideoCodecGStreamer::OnEnoughData()
{
//	if (m_needData == true) printf ("Setting m_needData to false\n");
  m_needData = false;
}

GstCaps *CDVDVideoCodecGStreamer::CreateVideoCaps(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  GstCaps *caps = NULL;
  GstCaps *caps_extra = NULL;
  printf ("Codec ID = %d\n",hints.codec);
  switch (hints.codec)
  {
    case CODEC_ID_H264:
      caps = gst_caps_new_simple ("video/x-h264", NULL);
      gst_caps_set_simple(caps, 
                          "width", G_TYPE_INT, hints.width,
                          "height", G_TYPE_INT, hints.height,
                          "framerate", GST_TYPE_FRACTION,
                            (hints.vfr ? 0 : hints.fpsrate),
                            (hints.vfr ? 1 : hints.fpsscale),
                          NULL);
      break;
    case CODEC_ID_WMV3:
     caps = gst_caps_new_simple ("video/x-wmv", NULL);
      gst_caps_set_simple(caps, 
                          "wmversion", G_TYPE_INT, 3,
                          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'M', 'V', '3'),
                          NULL);
      break;
     case CODEC_ID_VC1:
     caps = gst_caps_new_simple ("video/x-wmv", NULL);
      gst_caps_set_simple(caps, 
                          "wmversion", G_TYPE_INT, 3,
                          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'V', 'C', '1'),
                          NULL);
      break;
#if 0
    case CODEC_ID_MPEG4:
      caps = gst_caps_new_simple ("video/mpeg", NULL);
      gst_caps_set_simple(caps,
                          "mpegversion", G_TYPE_INT, 4,
//                           "systemstream", G_TYPE_BOOLEAN, FALSE,
                          NULL);
      break;
#endif
    case CODEC_ID_MPEG2VIDEO:
      caps = gst_caps_new_simple ("video/mpeg", NULL);
      gst_caps_set_simple(caps,
                          "mpegversion", G_TYPE_INT, 2,
//                           "systemstream", G_TYPE_BOOLEAN, FALSE,
                          NULL);
      break;
    default:
      printf("GStreamer: codec: unknown = %i\n", hints.codec);
      return NULL;
  }

  if (caps)
  {
      gst_caps_set_simple(caps, 
                          "width", G_TYPE_INT, hints.width,
                          "height", G_TYPE_INT, hints.height,
                          "framerate", GST_TYPE_FRACTION,
                            (hints.vfr ? 0 : hints.fpsrate),
                            (hints.vfr ? 1 : hints.fpsscale),
                          NULL);

    if (hints.extradata && hints.extrasize > 0)
    {
      GstBuffer *data = NULL;
      data = gst_buffer_new_and_alloc(hints.extrasize);
      memcpy(GST_BUFFER_DATA(data), hints.extradata, hints.extrasize);

      gst_caps_set_simple(caps, "codec_data", GST_TYPE_BUFFER, data, NULL);
      gst_buffer_unref(data);
    }
  }

  return caps;
}
