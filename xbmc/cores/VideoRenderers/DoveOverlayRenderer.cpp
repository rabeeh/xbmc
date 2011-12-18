/*
 *      Copyright (C) 2011 Solid-Run
 *      http://www.solid-run.com
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

#include "system.h"
#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#endif

#undef COLOR_KEY_BLACK
#define COLOR_KEY_ALPHA
#ifdef HAS_DOVE_OVERLAY
#include "DoveOverlayRenderer.h"
#include "dovefb.h"
#include "bmm_drv.h"
#include "utils/log.h"
#include <stdlib.h>
#include <malloc.h>
#include "utils/fastmemcpy.h"
#include "settings/GUISettings.h"
#include "guilib/GraphicContext.h"


CDoveOverlayRenderer::CDoveOverlayRenderer()
{
  m_yuvBuffers[0].plane[0] = NULL;
  m_yuvBuffers[0].plane[1] = NULL;
  m_yuvBuffers[0].plane[2] = NULL;

  m_yuvBuffers[1].plane[0] = NULL;
  m_yuvBuffers[1].plane[1] = NULL;
  m_yuvBuffers[1].plane[2] = NULL;

  m_overlayfd = -1;
  m_bmm = -1;

  m_framebuffers[0].buf = NULL;
  m_framebuffers[1].buf = NULL;
  UnInit();
}

CDoveOverlayRenderer::~CDoveOverlayRenderer()
{
  UnInit();
}

bool CDoveOverlayRenderer::Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned int flags, unsigned int format)
{
  int i;
	// When no i420 flag added in fourcc, flags were 0x1011 (4113 decimal)
  printf("Configure with [%i, %i] and [%i, %i] and fps %f and flags 0x%x format 0x%x\n", width, height, d_width, d_height, fps, flags,format);

  if (CONF_FLAGS_FORMAT_MASK(flags) == CONF_FLAGS_FORMAT_NV12)
  {
    printf("Bad format\n");
    return false;
  }

//  if (width != m_sourceWidth || height != m_sourceHeight)
  {
    m_sourceWidth = width;
    m_sourceHeight = height;

    // Set output to image size but pad it to be a multiple of 16
    m_overlayWidth = (m_sourceWidth+15)&~15;

    // Open the framebuffer
    m_overlayfd = open("/dev/fb1", O_RDWR);
    if (m_overlayfd == -1)
    {
      CLog::Log(LOGERROR, "DoveOverlay: Failed to open framebuffer");
      return false;
    }
    m_bmm = open("/dev/bmm", O_RDWR);
    if (m_bmm == -1)
    {
      CLog::Log(LOGERROR, "DoveOverlay: Failed to open bmm");
      return false;
    }



	/*
	 * First allocate memory for two framebuffers and map them.
	 */
//	unsigned int frameSize = m_overlayWidth * m_sourceHeight * 2;
//	unsigned int memSize = frameSize*2;
//	ioctl_arg_t bmm_cmd;
//	bmm_cmd.input = memSize;
//	bmm_cmd.arg = 0x0; 
//	if ((ioctl(m_bmm, BMM_MALLOC, &bmm_cmd) != 0))
//	{
//	      CLog::Log(LOGERROR, "DoveOverlay: Failed to alloc memory from bmm");
//		printf ("BMM memory allocate FAILED\n");
//		return false;
//	}
//	m_offset = bmm_cmd.output;
//	buf1_phys = (unsigned char *)m_offset;
//	buf2_phys = (unsigned char *)m_offset + frameSize;
	for (i=0;i<3;i++)
		gst_buf[i] = NULL;
//	printf ("Allocated BMM memory - physical address 0x%x\n",m_offset);
//	uint8_t *fbmem = (uint8_t *)mmap(NULL, memSize, PROT_READ|PROT_WRITE, MAP_SHARED, m_bmm, m_offset);
//	printf ("fbmem mapped to 0x%x (size 0x%x)\n",(unsigned int)fbmem,memSize);
//	if (fbmem == MAP_FAILED)
//	{
//		CLog::Log(LOGERROR, "DoveOverlay: Failed to map the framebuffer");
//		return false;
//	}


	/*
	 * Second setup the screen overlays
	 */
#if 0
	for (unsigned int i = 0; i < memSize / 4; i++)
		((uint32_t*)fbmem)[i] = 0x80008000;
#endif
	m_framebuffers[0].x = 0;
	m_framebuffers[0].y = 0;
	m_framebuffers[0].buf = 0;//fbmem;
	m_framebuffers[1].x = 0;
	m_framebuffers[1].y = m_sourceHeight;
	m_framebuffers[1].buf = 0;//fbmem + frameSize;
	memset (&m_overlaySurface, 0, sizeof(m_overlaySurface));
	if (CONF_FLAGS_FORMAT_MASK(flags) == CONF_FLAGS_FORMAT_UYVY)
	{
		m_overlaySurface.videoMode = DOVEFB_VMODE_YUV422PACKED_SWAPYUorV;//DOVEFB_VMODE_YUV422PACKED;
		m_overlaySurface.viewPortInfo.ycPitch = m_sourceWidth*2;
		m_overlaySurface.viewPortInfo.uvPitch = 0;
	} else if (CONF_FLAGS_FORMAT_MASK(flags) == CONF_FLAGS_FORMAT_YV12)
	{
		m_overlaySurface.videoMode = DOVEFB_VMODE_YUV420PLANAR;
		m_overlaySurface.viewPortInfo.ycPitch = m_sourceWidth;
		m_overlaySurface.viewPortInfo.uvPitch = m_sourceWidth/2;
	} else
	{
		printf ("Unkown format 0x%x\n",CONF_FLAGS_FORMAT_MASK(flags));
		return false;
	}
	m_overlaySurface.viewPortInfo.srcWidth = m_sourceWidth;
	m_overlaySurface.viewPortInfo.srcHeight = m_sourceHeight;
	m_overlaySurface.viewPortInfo.zoomXSize = m_sourceWidth;
	m_overlaySurface.viewPortInfo.zoomYSize = m_sourceHeight;
	printf ("Setting ycPitch to %d, uvPitch to %d\n",m_overlaySurface.viewPortInfo.ycPitch ,m_overlaySurface.viewPortInfo.uvPitch);
	
	
	m_overlaySurface.viewPortOffset.xOffset = 0;
	m_overlaySurface.viewPortOffset.yOffset = 0;

	m_overlaySurface.videoBufferAddr.startAddr = (unsigned char *)m_offset;
	m_overlaySurface.videoBufferAddr.length = 0;//frameSize;
	m_overlaySurface.videoBufferAddr.inputData = 0;
	m_overlaySurface.videoBufferAddr.frameID = 0;

	int srcMode = SHM_NORMAL;
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_SET_SRC_MODE, &srcMode) == -1)
	{
		CLog::Log(LOGERROR, "DoveOverlay: Failed to enable video overlay");
		return false;
	}
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_SET_VIDEO_MODE, &m_overlaySurface.videoMode) == -1)
	{
		CLog::Log(LOGERROR, "DoveOverlay: Failed to setup video mode");
		return false;
	}
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_SET_VIEWPORT_INFO, &m_overlaySurface.viewPortInfo) != 0)
	{
		CLog::Log(LOGERROR, "DoveOverlay: Failed to setup video port");
		return false;
	}
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_SET_VID_OFFSET, &m_overlaySurface.viewPortOffset) != 0)
	{
		CLog::Log(LOGERROR, "DoveOverlay: Failed to setup video port offset");
		return false;
	}
	int interpolation = 3; // bi-linear interpolation
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_SET_INTERPOLATION_MODE, &interpolation) != 0)
	{
		CLog::Log(LOGERROR, "DoveOverlay: Failed to setup video interpolation mode");
		return false;
	}

	struct _sColorKeyNAlpha alpha;
	memset (&alpha, 0, sizeof(alpha));
	alpha.mode = DOVEFB_ENABLE_RGB_COLORKEY_MODE;
	alpha.alphapath = DOVEFB_GRA_PATH_ALPHA;
	alpha.config = 0xff;//c0;
#ifdef COLOR_KEY_ALPHA
	alpha.Y_ColorAlpha = 0x02020200;
	alpha.U_ColorAlpha = 0x05050500;
	alpha.V_ColorAlpha = 0x07070700;
#endif
#ifdef COLOR_KEY_BLACK
	alpha.Y_ColorAlpha = 0x0;
	alpha.U_ColorAlpha = 0x0;
	alpha.V_ColorAlpha = 0x0;
#endif
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_SET_COLORKEYnALPHA, &alpha) == -1)
	{
		CLog::Log(LOGERROR, "DoveOverlay: Failed to configure alpha");
		return false;
	}

	for (unsigned int i = 0; i < 2; i++)
	{
		FreeYV12Image(i);
		CreateYV12Image(i, m_sourceWidth, m_sourceHeight);
	}
	m_currentBuffer = 0;
	printf("Proper format, continuing\n");
  }

  m_iFlags = flags;
  m_bConfigured = true;
  return m_bConfigured;
}

int CDoveOverlayRenderer::GetImage(YV12Image *image, int source, bool readonly)
{
//	printf ("GetImage called (source = %d)\n",source);
  if (!m_bConfigured)
    return -1;
  /* take next available buffer */
//  printf("GetImage before if %i\n", source);
  if( source == AUTOSOURCE || source > 1 || source < 0)
//    source = NextYV12Image();
	source = m_currentBuffer;

//  printf("GetImage %i\n", source);

  YV12Image &im = m_yuvBuffers[source];

  for (int p=0;p<MAX_PLANES;p++)
  {
    image->plane[p]  = im.plane[p];
    image->stride[p] = im.stride[p];
  }

  image->width    = im.width;
  image->height   = im.height;
  image->flags    = im.flags;
  image->cshift_x = im.cshift_x;
  image->cshift_y = im.cshift_y;

 // printf("image [%i, %i]\n", image->width, image->height);
//	printf ("In DoveOverlay GetImage --> image->plane[0] = 0x%x\n",image->plane[0]);

  return source;
}

void CDoveOverlayRenderer::ReleaseImage(int source, bool preserve)
{
  if (!m_bConfigured)
    return;

}

void CDoveOverlayRenderer::FlipPage(int source)
{
	unsigned char fbid,now;
	if (!m_bConfigured)
		return;
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_GET_FBID, &fbid) != 0)
	{
		printf ("Error getting FBID\n");
	}
	now = m_overlaySurface.videoBufferAddr.frameID;
//	if (fbid == 255) fbid=0;
//	else fbid++;
//	if (fbid != now) {
//		printf ("Skipping frame presentation (fbid=%d, now = %d)\n",fbid,now);
//		return;
//	}
//	printf ("fbid = %d, frameID = %d\n",fbid, m_overlaySurface.videoBufferAddr.frameID);

	if (gst_buf[0] != NULL) {
		m_overlaySurface.videoBufferAddr.startAddr = gst_buf[0];
	} else {
		if (m_currentBuffer) m_overlaySurface.videoBufferAddr.startAddr = buf2_phys;
		else  m_overlaySurface.videoBufferAddr.startAddr = buf1_phys;
	}
//	if (m_overlaySurface.videoBufferAddr.frameID == 0) {
//		printf ("Flipping FIRST frame\n");
//	}
//	printf ("Flipping page %d (frameID = %d) address 0x%x...",m_currentBuffer, m_overlaySurface.videoBufferAddr.frameID, m_overlaySurface.videoBufferAddr.startAddr);
	{ static int hack=0;
		if (hack>=0)
	{
		if (ioctl(m_overlayfd, DOVEFB_IOCTL_XBMC_PRESENT, &gst_buf) != 0)
	{
		printf ("Error flipping\n");
	}}
	else
	{
//		hack++;
//		if (ioctl(m_overlayfd, DOVEFB_IOCTL_FLIP_VID_BUFFER, &m_overlaySurface) != 0)
	{
		printf ("Error flipping\n");
	}}
	}
	m_overlaySurface.videoBufferAddr.frameID ++;
	m_currentBuffer = NextYV12Image();
	if (enabled == 0) {
		enabled = 1;
		if (ioctl(m_overlayfd, DOVEFB_IOCTL_SWITCH_VID_OVLY, &enabled) == -1)
		{
			CLog::Log(LOGERROR, "DoveOverlay: Failed to enable video overlay");
		}
	}
	if (ioctl(m_overlayfd, DOVEFB_IOCTL_WAIT_VSYNC, 0) != 0)
	{
		printf ("Error waiting for vsync\n");
	}

//	printf ("Done\n");
}

void CDoveOverlayRenderer::Reset()
{
	printf ("Was asked to Reset\n");
}

void CDoveOverlayRenderer::Update(bool bPauseDrawing)
{
	printf ("Was asked to Update bPauseDrawing = %d\n",bPauseDrawing);
}

void CDoveOverlayRenderer::AddProcessor(YV12Image *image, DVDVideoPicture *pic)
{
#if 0
  image->plane[0]  = pic->data[0];
  image->plane[1]  = pic->data[1];
  image->plane[2]  = pic->data[2];
  image->stride[0] = pic->iLineSize[0];
  image->stride[1] = pic->iLineSize[1];
  image->stride[2] = pic->iLineSize[2];
#endif
  DrawSlice(pic->data, pic->iLineSize, pic->iWidth, pic->iHeight, 0, 0);
}

void CDoveOverlayRenderer::RenderUpdate(bool clear, DWORD flags, DWORD alpha)
{
//	printf ("flags is 0x%x\n",flags);
  if (!m_bConfigured)
    return;
#ifdef COLOR_KEY_ALPHA
	static int counter = 0;
	counter++;
//	if ((counter%20)) return ; // Rabeeh hack
	GLfloat value;
	GLint scissorBox[8];
	glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
	m_zoomWidth = scissorBox[2] - scissorBox[0];
	m_zoomHeight = scissorBox[3] - scissorBox[1];
	if ((m_overlaySurface.viewPortInfo.zoomXSize != m_zoomWidth) ||
	    (m_overlaySurface.viewPortInfo.zoomYSize != m_zoomHeight)) {
		// Updating zooming required
		m_overlaySurface.viewPortInfo.zoomXSize = m_zoomWidth;
		m_overlaySurface.viewPortInfo.zoomYSize = m_zoomHeight;
		if (ioctl(m_overlayfd, DOVEFB_IOCTL_SET_VIEWPORT_INFO, &m_overlaySurface.viewPortInfo) != 0)
		{
			printf ("Error changing viewport\n");
		}
	}
//	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
//	if (!clear)
//		glScissor(m_destRect.x1, g_graphicsContext.GetHeight() - m_destRect.y2, m_destRect.Width(), m_destRect.Height());
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // RGB e2 1f 70
	glClear(GL_COLOR_BUFFER_BIT);
//	glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
#endif
}

bool CDoveOverlayRenderer::RenderCapture(CRenderCapture* capture)
{

  printf ("Rabeeh - fixme In RenderCapture. returning true\n");
  return true;
}


unsigned int CDoveOverlayRenderer::DrawSlice(unsigned char *src[], int stride[], int w, int h, int x, int y)
{
	static unsigned int counter1=0, counter2=0;
	ioctl_arg_t bmm_cmd;
	int i;

	counter1++;
//	if (!(counter1%20)) printf ("Frames %d\n",counter1);
	memset(&bmm_cmd, 0, sizeof(ioctl_arg_t));
	bmm_cmd.input = (unsigned int)src[0];
	bmm_cmd.arg = 0x0; 
	if ((ioctl(m_bmm, BMM_GET_PHYS_ADDR, &bmm_cmd) != 0))
	{
		CLog::Log(LOGERROR, "DoveOverlay: Failed to get physical address");
		return false;
	}
	
	if (bmm_cmd.output) { // Has buffer that can be directly displayed
		/* Typically UYVY, need to see if gst_buf[1/2] needs update too */
		gst_buf[0] = (unsigned char *) bmm_cmd.output;
	} else
       {
		unsigned int memSize = w*h*2;
		ioctl_arg_t bmm_cmd;
		bmm_cmd.input = memSize;
		bmm_cmd.arg = 0x0; 
		if ((ioctl(m_bmm, BMM_MALLOC, &bmm_cmd) != 0))
		{
			CLog::Log(LOGERROR, "DoveOverlay: Failed to alloc memory from bmm");
			printf ("BMM memory allocate FAILED\n");
			return false;
		}
		m_offset = bmm_cmd.output;
		uint8_t *virt = (uint8_t *)mmap(NULL, memSize, PROT_READ|PROT_WRITE, MAP_SHARED, m_bmm, m_offset);
		if (virt == MAP_FAILED)
		{
			CLog::Log(LOGERROR, "DoveOverlay: Failed to map the framebuffer");
			return false;
		}

		unsigned int dst = (unsigned int)virt;
		gst_buf[0] = (unsigned char *) bmm_cmd.output;
		gst_buf[1] = (unsigned char *) (bmm_cmd.output + h*w);
		gst_buf[2] = (unsigned char *) (bmm_cmd.output + h*w*3/2);
#if 1
		/* Notice that src[0..3] are not contiguos in memory */
#if 0
		printf ("src[0] =0x%x, src[1] = 0x%x, src[2] = 0x%x\n",src[0],src[1],src[2]);
		printf ("stride[0] =0x%x, stride[1] = 0x%x, stride[2] = 0x%x\n",stride[0],stride[1],stride[2]);
#endif
		for (i = 0 ; i < h ; i++) {
			fast_memcpy ((void*)dst, (void*)((unsigned int)src[0]+stride[0]*i),stride[0]);
			dst += w;
		}
		for (i = 0 ; i < h ; i++) {
			fast_memcpy ((void*)dst, (void*)((unsigned int)src[1]+stride[1]*i),stride[1]);
			dst += w/2;
		}
		for (i = 0 ; i < h ; i++) {
			fast_memcpy ((void*)dst, (void*)((unsigned int)src[2]+stride[2]*i),stride[2]);
			dst += w/2;
		}

#else
		unsigned int dst_addr = (unsigned int) virt;
		unsigned int size = stride[0]*h;
		printf ("stride = %d %d %d\n",stride[0],stride[1],stride[2]);
		printf ("Y dst addr = 0x%x\n",dst_addr);
		memcpy ((void *) dst_addr,src[0],size);
		dst_addr += h * stride[0];
		printf ("U dst addr = 0x%x\n",dst_addr);
		size = (stride[1]*h);
		memcpy ((void *) dst_addr,src[1],size);
		dst_addr += (h * stride[1]);///2;
		printf ("V dst addr = 0x%x\n",dst_addr);
		size = (stride[2]*h);
		memcpy ((void *) dst_addr,src[1]+stride[1]*h/2,size);
#endif
		munmap (virt, memSize);
		bmm_cmd.input = bmm_cmd.output;
		bmm_cmd.arg = 0x0; 
		if ((ioctl(m_bmm, BMM_FREE, &bmm_cmd) != 0))
		{
			CLog::Log(LOGERROR, "DoveOverlay: Failed to alloc memory from bmm");
			printf ("BMM memory allocate FAILED\n");
			return false;
		}

	}
  return 0;
}

unsigned int CDoveOverlayRenderer::PreInit()
{
  UnInit();

  return true;
}

void CDoveOverlayRenderer::UnInit()
{
  CLog::Log(LOGINFO, "CDoveOverlayRenderer::UnInit");
  printf("UnInit\n");
  m_bConfigured = false;
  m_iFlags = 0;
  m_currentBuffer = 0;
  for (unsigned int i = 0; i < 2; i++)
    FreeYV12Image(i);

  enabled = 0;
  if (ioctl(m_overlayfd, DOVEFB_IOCTL_SWITCH_VID_OVLY, &enabled) == -1)
  {
      CLog::Log(LOGERROR, "DoveOverlay: Failed to disable video overlay");
  }
	// Rabeeh - TODO add framebuffer memory release code
  if (m_overlayfd > 0)
  {
    close(m_overlayfd);
    m_overlayfd = -1;
  }
  m_sourceWidth = 0;
  m_sourceHeight = 0;
  m_zoomWidth = 0;
  m_zoomHeight = 0;
  m_offsetX = 0;
  m_offsetY = 0;
}

void CDoveOverlayRenderer::CreateThumbnail(CBaseTexture* texture, unsigned int width, unsigned int height)
{
	printf ("Was asked to create thumbnail (width = %d, height = %d)\n",width,height);
}

bool CDoveOverlayRenderer::Supports(EDEINTERLACEMODE mode)
{
#if 0
  if (mode == VS_DEINTERLACEMODE_OFF)
    return true;

  if(m_renderMethod & RENDER_OMXEGL)
    return false;

  if(m_renderMethod & RENDER_CVREF)
    return false;

  if(mode == VS_DEINTERLACEMODE_AUTO
  || mode == VS_DEINTERLACEMODE_FORCE)
    return true;
#endif
	printf ("Called %s\n",__FUNCTION__);
  return false;
}

bool CDoveOverlayRenderer::Supports(ERENDERFEATURE feature)
{
	printf ("Called %s\n",__FUNCTION__);
  return false;
}

bool CDoveOverlayRenderer::SupportsMultiPassRendering()
{
  return false;
}

bool CDoveOverlayRenderer::Supports(EINTERLACEMETHOD method)
{
	printf ("Called %s\n",__FUNCTION__);
  return false;
}

bool CDoveOverlayRenderer::Supports(ESCALINGMETHOD method)
{
	printf ("Called %s\n",__FUNCTION__);
  if(method == VS_SCALINGMETHOD_NEAREST
  || method == VS_SCALINGMETHOD_LINEAR)
    return true;

  return false;
}

EINTERLACEMETHOD CDoveOverlayRenderer::AutoInterlaceMethod()
{
#if 0
  if(m_renderMethod & RENDER_OMXEGL)
    return VS_INTERLACEMETHOD_NONE;

  if(m_renderMethod & RENDER_CVREF)
    return VS_INTERLACEMETHOD_NONE;

#if defined(__i386__) || defined(__x86_64__)
  return VS_INTERLACEMETHOD_DEINTERLACE_HALF;
#else
  return VS_INTERLACEMETHOD_SW_BLEND;
#endif
#endif
//  printf ("Called %s\n",__FUNCTION__);
  return VS_INTERLACEMETHOD_NONE;
}

unsigned int CDoveOverlayRenderer::NextYV12Image()
{
//	printf ("Called to get NextYV12Image (m_currentBuffer = %d)\n",m_currentBuffer);
  return 1 - m_currentBuffer;
}

bool CDoveOverlayRenderer::CreateYV12Image(unsigned int index, unsigned int width, unsigned int height)
{
  YV12Image &im = m_yuvBuffers[index];

  im.width  = width;
  im.height = height;
  im.cshift_x = 1;
  im.cshift_y = 1;

#if 0
  im.stride[0] = im.width;
  im.stride[1] = im.width >> im.cshift_x;
  im.stride[2] = im.width >> im.cshift_x;
#else
unsigned paddedWidth = (im.width + 15) & ~15;
//	printf("w %i | padded %i\n", width, paddedWidth);
	im.stride[0] = paddedWidth;
	im.stride[1] = paddedWidth >> im.cshift_x;
	im.stride[2] = paddedWidth >> im.cshift_x;
#endif
  im.planesize[0] = im.stride[0] * im.height;
  im.planesize[1] = im.stride[1] * ( im.height >> im.cshift_y );
  im.planesize[2] = im.stride[2] * ( im.height >> im.cshift_y );
#if 0
  for (int i = 0; i < 3; i++) {
//    im.plane[i] = new BYTE[im.planesize[i]];
	im.plane[i] = (BYTE *)memalign(16, im.planesize[i]);
//	 printf ("CreateYV12Image shows 0x%x (index = %d, width = %d, height = %d) --> size = 0x%x\n",im.plane[i], index, width, height,im.planesize[i]);
	}
#endif
  return true;
}

bool CDoveOverlayRenderer::FreeYV12Image(unsigned int index)
{
  YV12Image &im = m_yuvBuffers[index];
  for (int i = 0; i < 3; i++)
  {
    delete[] im.plane[i];
    im.plane[i] = NULL;
  }

  memset(&im , 0, sizeof(YV12Image));

  return true;
}

#endif
