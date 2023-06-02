#include "ZeDMD.h"

ZeDMD::ZeDMD()
{
   m_width = 0;
   m_height = 0;

   memset(&m_palette, 0, sizeof(m_palette));

   m_pFrameBuffer = NULL;
   m_pScaledFrameBuffer = NULL;
   m_pCommandBuffer = NULL;
   m_pPlanes = NULL;
}

ZeDMD::~ZeDMD()
{
   m_zedmdComm.Disconnect();

   if (m_pFrameBuffer)
      delete m_pFrameBuffer;

   if (m_pScaledFrameBuffer)
      delete m_pScaledFrameBuffer;

   if (m_pCommandBuffer)
      delete m_pCommandBuffer;

   if (m_pPlanes)
      delete m_pPlanes;
}

void ZeDMD::IgnoreDevice(const char *ignore_device)
{
   m_zedmdComm.IgnoreDevice(ignore_device);
}

void ZeDMD::SetFrameSize(uint8_t width, uint8_t height)
{
   m_width = width;
   m_height = height;
   int frameWidth = m_zedmdComm.GetWidth();
   int frameHeight = m_zedmdComm.GetHeight();
   uint8_t size[4];

   if ((m_downscaling && (width > frameWidth || height > frameHeight)) || (m_upscaling && (width < frameWidth || height < frameHeight)))
   {
      size[0] = (uint8_t)(frameWidth & 0xFF);
      size[1] = (uint8_t)((frameWidth >> 8) & 0xFF);
      size[2] = (uint8_t)(frameHeight & 0xFF);
      size[3] = (uint8_t)((frameHeight >> 8) & 0xFF);
   }
   else
   {
      size[0] = (uint8_t)(width & 0xFF);
      size[1] = (uint8_t)((width >> 8) & 0xFF);
      size[2] = (uint8_t)(height & 0xFF);
      size[3] = (uint8_t)((height >> 8) & 0xFF);
   }

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::FrameSize, size, 4);
}

void ZeDMD::EnableDebug()
{
   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::EnableDebug);
}

void ZeDMD::DisableDebug()
{
   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::DisableDebug);
}

void ZeDMD::EnablePreDownscaling()
{
   m_downscaling = true;
}

void ZeDMD::DisablePreDownscaling()
{
   m_downscaling = false;
}

void ZeDMD::EnablePreUpscaling()
{
   m_upscaling = true;
}

void ZeDMD::DisablePreUpscaling()
{
   m_upscaling = false;
}

void ZeDMD::EnableUpscaling()
{
   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::EnableUpscaling);
}

void ZeDMD::DisableUpscaling()
{
   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::DisableUpscaling);
}

bool ZeDMD::Open()
{
   m_available = m_zedmdComm.Connect();

   if (m_available)
   {

      m_pFrameBuffer = (uint8_t *)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
      m_pScaledFrameBuffer = (uint8_t *)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
      m_pCommandBuffer = (uint8_t *)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
      m_pPlanes = (uint8_t *)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);

      if (m_debug)
      {
         m_zedmdComm.QueueCommand(ZEDMD_COMMAND::EnableDebug);
         // PLOGI.printf("ZeDMD debug enabled");
      }

      if (m_rgbOrder != -1)
      {
         m_zedmdComm.QueueCommand(ZEDMD_COMMAND::RGBOrder, m_rgbOrder);
         // PLOGI.printf("ZeDMD RGB order override: rgbOrder=%d", rgbOrder);
      }

      if (m_brightness != -1)
      {
         m_zedmdComm.QueueCommand(ZEDMD_COMMAND::Brightness, m_brightness);
         // PLOGI.printf("ZeDMD brightness override: brightness=%d", brightness);
      }

      m_zedmdComm.Run();
   }

   return m_available;
}

bool ZeDMD::Open(int width, int height)
{
   if (Open())
   {
      SetFrameSize(width, height);
   }

   return m_available;
}

void ZeDMD::SetPalette(uint8_t *pPalette)
{
   memcpy(&m_palette, pPalette, sizeof(m_palette));
}

void ZeDMD::SetDefaultPalette(int bitDepth)
{
   switch (bitDepth)
   {
   case 2:
      SetPalette(m_DmdDefaultPalette2Bit);
      break;

   default:
      SetPalette(m_DmdDefaultPalette4Bit);
   }
}

uint8_t *ZeDMD::GetDefaultPalette(int bitDepth)
{
   switch (bitDepth)
   {
   case 2:
      return m_DmdDefaultPalette2Bit;
      break;

   default:
      return m_DmdDefaultPalette4Bit;
   }
}

void ZeDMD::RenderGray2(uint8_t *pFrame)
{
   if (!m_available || !UpdateFrameBuffer8(pFrame))
      return;

   int bufferSize = Scale(m_pScaledFrameBuffer, m_pFrameBuffer, 1) / 8 * 2;
   Split(m_pPlanes, m_zedmdComm.GetWidth(), m_zedmdComm.GetHeight(), 2, m_pScaledFrameBuffer);

   memcpy(m_pCommandBuffer, &m_palette, 12);
   memcpy(m_pCommandBuffer + 12, m_pPlanes, bufferSize);

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::Gray2, m_pCommandBuffer, 12 + bufferSize);
}

void ZeDMD::RenderGray4(uint8_t *pFrame)
{
   if (!m_available || !UpdateFrameBuffer8(pFrame))
      return;

   int bufferSize = Scale(m_pScaledFrameBuffer, m_pFrameBuffer, 1) / 8 * 4;
   Split(m_pPlanes, m_zedmdComm.GetWidth(), m_zedmdComm.GetHeight(), 4, m_pScaledFrameBuffer);

   memcpy(m_pCommandBuffer, m_palette, 48);
   memcpy(m_pCommandBuffer + 48, m_pPlanes, bufferSize);

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::ColGray4, m_pCommandBuffer, 48 + bufferSize);
}

void ZeDMD::RenderColoredGray6(uint8_t *pFrame, uint8_t *pPalette, uint8_t *pRotations)
{
   if (!m_available)
      return;

   bool change = UpdateFrameBuffer8(pFrame);

   if (memcmp(&m_palette, pPalette, 192))
   {
      memcpy(&m_palette, pPalette, 192);
      change = true;
   }

   if (!change)
      return;

   int bufferSize = Scale(m_pScaledFrameBuffer, m_pFrameBuffer, 1) / 8 * 6;
   Split(m_pPlanes, m_zedmdComm.GetWidth(), m_zedmdComm.GetHeight(), 6, m_pScaledFrameBuffer);

   memcpy(m_pCommandBuffer, pPalette, 192);
   memcpy(m_pCommandBuffer + 192, m_pPlanes, bufferSize);

   if (pRotations)
      memcpy(m_pCommandBuffer + 192 + bufferSize, pRotations, 24);
   else
      memset(m_pCommandBuffer + 192 + bufferSize, 255, 24);

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::ColGray6, m_pCommandBuffer, 192 + bufferSize + 24);
}

void ZeDMD::RenderRgb24(uint8_t *pFrame)
{
   if (!m_available || !UpdateFrameBuffer24(pFrame))
      return;

   int bufferSize = Scale(m_pCommandBuffer, pFrame, 3);

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::RGB24, m_pFrameBuffer, bufferSize);
}

bool ZeDMD::UpdateFrameBuffer8(uint8_t *pFrame)
{
   if (!memcmp(m_pFrameBuffer, pFrame, m_width * m_height))
      return false;

   memcpy(m_pFrameBuffer, pFrame, m_width * m_height);
   return true;
}

bool ZeDMD::UpdateFrameBuffer24(uint8_t *pFrame)
{
   if (!memcmp(m_pFrameBuffer, pFrame, m_width * m_height * 3))
      return false;

   memcpy(m_pFrameBuffer, pFrame, m_width * m_height * 3);
   return true;
}

/**
 * Derived from https://github.com/freezy/dmd-extensions/blob/master/LibDmd/Common/FrameUtil.cs
 */

void ZeDMD::Split(uint8_t *pPlanes, int width, int height, int bitlen, uint8_t *pFrame)
{
   int planeSize = width * height / 8;
   int pos = 0;
   uint8_t bd[bitlen];

   for (int y = 0; y < height; y++)
   {
      for (int x = 0; x < width; x += 8)
      {
         memset(bd, 0, bitlen * sizeof(uint8_t));

         for (int v = 7; v >= 0; v--)
         {
            uint8_t pixel = pFrame[(y * width) + (x + v)];
            for (int i = 0; i < bitlen; i++)
            {
               bd[i] <<= 1;
               if ((pixel & (1 << i)) != 0)
                  bd[i] |= 1;
            }
         }

         for (int i = 0; i < bitlen; i++)
            pPlanes[i * planeSize + pos] = bd[i];

         pos++;
      }
   }
}

bool ZeDMD::CmpColor(uint8_t *px1, uint8_t *px2, uint8_t colors)
{
   if (colors == 3)
   {
      return (px1[0] == px2[0]) &&
             (px1[1] == px2[1]) &&
             (px1[2] == px2[2]);
   }

   return px1[0] == px2[0];
}

void ZeDMD::SetColor(uint8_t *px1, uint8_t *px2, uint8_t colors)
{
   px1[0] = px2[0];

   if (colors == 3)
   {
      px1[1] = px2[1];
      px1[2] = px2[2];
   }
}

int ZeDMD::Scale(uint8_t *pScaledFrame, uint8_t *pFrame, uint8_t colors)
{
   int xoffset = 0;
   int yoffset = 0;
   int scale = 0; // 0 - no scale, 1 - half scale, 2 - double scale
   int frameWidth = m_zedmdComm.GetWidth();
   int frameHeight = m_zedmdComm.GetHeight();
   int bufferSize = frameWidth * frameHeight * colors;

   if (m_upscaling && m_width == 192 && frameWidth == 256)
   {
      xoffset = 32;
   }
   else if (m_downscaling && m_width == 192)
   {
      xoffset = 16;
      scale = 1;
   }
   else if (m_upscaling && m_height == 16 && frameHeight == 32)
   {
      yoffset = 8;
   }
   else if (m_upscaling && m_height == 16 && frameHeight == 64)
   {
      yoffset = 16;
      scale = 2;
   }
   else if (m_downscaling && m_width == 256 && frameWidth == 128)
   {
      scale = 1;
   }
   else if (m_upscaling && m_width == 128 && frameWidth == 256)
   {
      scale = 2;
   }
   else
   {
      memcpy(pScaledFrame, pFrame, bufferSize);
      return bufferSize;
   }

   memset(pScaledFrame, 0, bufferSize);

   if (scale == 1)
   {
      // for half scaling we take the 4 points and look if there is one colour repeated
      for (int y = 0; y < m_height; y += 2)
      {
         for (int x = 0; x < m_width; x += 2)
         {
            uint16_t upper_left = y * m_width * colors + x * colors;
            uint16_t upper_right = upper_left + colors;
            uint16_t lower_left = upper_left + m_width * colors;
            uint16_t lower_right = lower_left + colors;
            uint16_t target = (xoffset + (x / 2) + (y / 2) * frameWidth) * colors;

            // Prefer most outer upper_lefts.
            if (x < m_width / 2)
            {
               if (y < m_height / 2)
               {
                  if (CmpColor(&pFrame[upper_left], &pFrame[upper_right], colors) || CmpColor(&pFrame[upper_left], &pFrame[lower_left], colors) || CmpColor(&pFrame[upper_left], &pFrame[lower_right], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
                  }
                  else if (CmpColor(&pFrame[upper_right], &pFrame[lower_left], colors) || CmpColor(&pFrame[upper_right], &pFrame[lower_right], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
                  }
                  else if (CmpColor(&pFrame[lower_left], &pFrame[lower_right], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
                  }
                  else
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
                  }
               }
               else
               {
                  if (CmpColor(&pFrame[lower_left], &pFrame[lower_right], colors) || CmpColor(&pFrame[lower_left], &pFrame[upper_left], colors) || CmpColor(&pFrame[lower_left], &pFrame[upper_right], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
                  }
                  else if (CmpColor(&pFrame[lower_right], &pFrame[upper_left], colors) || CmpColor(&pFrame[lower_right], &pFrame[upper_right], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
                  }
                  else if (CmpColor(&pFrame[upper_left], &pFrame[upper_right], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
                  }
                  else
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
                  }
               }
            }
            else
            {
               if (y < m_height / 2)
               {
                  if (CmpColor(&pFrame[upper_right], &pFrame[upper_left], colors) || CmpColor(&pFrame[upper_right], &pFrame[lower_right], colors) || CmpColor(&pFrame[upper_right], &pFrame[lower_left], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
                  }
                  else if (CmpColor(&pFrame[upper_left], &pFrame[lower_right], colors) || CmpColor(&pFrame[upper_left], &pFrame[lower_left], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
                  }
                  else if (CmpColor(&pFrame[lower_right], &pFrame[lower_left], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
                  }
                  else
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
                  }
               }
               else
               {
                  if (CmpColor(&pFrame[lower_right], &pFrame[lower_left], colors) || CmpColor(&pFrame[lower_right], &pFrame[upper_right], colors) || CmpColor(&pFrame[lower_right], &pFrame[upper_left], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
                  }
                  else if (CmpColor(&pFrame[lower_left], &pFrame[upper_right], colors) || CmpColor(&pFrame[lower_left], &pFrame[upper_left], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
                  }
                  else if (CmpColor(&pFrame[upper_right], &pFrame[upper_left], colors))
                  {
                     SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
                  }
                  else
                  {
                     SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
                  }
               }
            }
         }
      }
   }
   else if (scale == 2)
   {
      // we implement scale2x http://www.scale2x.it/algorithm
      uint16_t row = m_width * colors;
      for (int x = 0; x < m_height; x++)
      {
         for (int y = 0; y < m_width; y++)
         {
            uint8_t a[colors], b[colors], c[colors], d[colors], e[colors], f[colors], g[colors], h[colors], i[colors];
            for (uint8_t tc = 0; tc < colors; tc++)
            {
               if (y == 0 && x == 0)
               {
                  a[tc] = b[tc] = d[tc] = e[tc] = pFrame[tc];
                  c[tc] = f[tc] = pFrame[colors + tc];
                  g[tc] = h[tc] = pFrame[row + tc];
                  i[tc] = pFrame[row + colors + tc];
               }
               else if ((y == 0) && (x == m_height - 1))
               {
                  a[tc] = b[tc] = pFrame[(x - 1) * row + tc];
                  c[tc] = pFrame[(x - 1) * row + colors + tc];
                  d[tc] = g[tc] = h[tc] = e[tc] = pFrame[x * row + tc];
                  f[tc] = i[tc] = pFrame[x * row + colors + tc];
               }
               else if ((y == m_width - 1) && (x == 0))
               {
                  a[tc] = d[tc] = pFrame[y * colors - colors + tc];
                  b[tc] = c[tc] = f[tc] = e[tc] = pFrame[y * colors + tc];
                  g[tc] = pFrame[row + y * colors - colors + tc];
                  h[tc] = i[tc] = pFrame[row + y * colors + tc];
               }
               else if ((y == m_width - 1) && (x == m_height - 1))
               {
                  a[tc] = pFrame[x * row - 2 * colors + tc];
                  b[tc] = c[tc] = pFrame[x * row - colors + tc];
                  d[tc] = g[tc] = pFrame[m_height * row - 2 * colors + tc];
                  e[tc] = f[tc] = h[tc] = i[tc] = pFrame[m_height * row - colors + tc];
               }
               else if (y == 0)
               {
                  a[tc] = b[tc] = pFrame[(x - 1) * row + tc];
                  c[tc] = pFrame[(x - 1) * row + colors + tc];
                  d[tc] = e[tc] = pFrame[x * row + tc];
                  f[tc] = pFrame[x * row + colors + tc];
                  g[tc] = h[tc] = pFrame[(x + 1) * row + tc];
                  i[tc] = pFrame[(x + 1) * row + colors + tc];
               }
               else if (y == m_width - 1)
               {
                  a[tc] = pFrame[x * row - 2 * colors + tc];
                  b[tc] = c[tc] = pFrame[x * row - colors + tc];
                  d[tc] = pFrame[(x + 1) * row - 2 * colors + tc];
                  e[tc] = f[tc] = pFrame[(x + 1) * row - colors + tc];
                  g[tc] = pFrame[(x + 2) * row - 2 * colors + tc];
                  h[tc] = i[tc] = pFrame[(x + 2) * row - colors + tc];
               }
               else if (x == 0)
               {
                  a[tc] = d[tc] = pFrame[y * colors - colors + tc];
                  b[tc] = e[tc] = pFrame[y * colors + tc];
                  c[tc] = f[tc] = pFrame[y * colors + colors + tc];
                  g[tc] = pFrame[row + y * colors - colors + tc];
                  h[tc] = pFrame[row + y * colors + tc];
                  i[tc] = pFrame[row + y * colors + colors + tc];
               }
               else if (x == m_height - 1)
               {
                  a[tc] = pFrame[(x - 1) * row + y * colors - colors + tc];
                  b[tc] = pFrame[(x - 1) * row + y * colors + tc];
                  c[tc] = pFrame[(x - 1) * row + y * colors + colors + tc];
                  d[tc] = g[tc] = pFrame[x * row + y * colors - colors + tc];
                  e[tc] = h[tc] = pFrame[x * row + y * colors + tc];
                  f[tc] = i[tc] = pFrame[x * row + y * colors + colors + tc];
               }
               else
               {
                  a[tc] = pFrame[(x - 1) * row + y * colors - colors + tc];
                  b[tc] = pFrame[(x - 1) * row + y * colors + tc];
                  c[tc] = pFrame[(x - 1) * row + y * colors + colors + tc];
                  d[tc] = pFrame[x * row + y * colors - colors + tc];
                  e[tc] = pFrame[x * row + y * colors + tc];
                  f[tc] = pFrame[x * row + y * colors + colors + tc];
                  g[tc] = pFrame[(x + 1) * row + y * colors - colors + tc];
                  h[tc] = pFrame[(x + 1) * row + y * colors + tc];
                  i[tc] = pFrame[(x + 1) * row + y * colors + colors + tc];
               }
            }

            if (!CmpColor(b, h, colors) && !CmpColor(d, f, colors))
            {
               SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth + y * 2 + xoffset) * colors], CmpColor(d, b, colors) ? d : e, colors);
               SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth + y * 2 + 1 + xoffset) * colors], CmpColor(b, f, colors) ? f : e, colors);
               SetColor(&pScaledFrame[(yoffset * frameWidth + (x * 2 + 1) * frameWidth + y * 2 + xoffset) * colors], CmpColor(d, h, colors) ? d : e, colors);
               SetColor(&pScaledFrame[(yoffset * frameWidth + (x * 2 + 1) * frameWidth + y * 2 + 1 + xoffset) * colors], CmpColor(h, f, colors) ? f : e, colors);
            }
            else
            {
               SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth + y * 2 + xoffset) * colors], e, colors);
               SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth + y * 2 + 1 + xoffset) * colors], e, colors);
               SetColor(&pScaledFrame[(yoffset * frameWidth + (x * 2 + 1) * frameWidth + y * 2 + xoffset) * colors], e, colors);
               SetColor(&pScaledFrame[(yoffset * frameWidth + (x * 2 + 1) * frameWidth + y * 2 + 1 + xoffset) * colors], e, colors);
            }
         }
      }
   }
   else // offset!=0
   {
      for (int y = 0; y < m_height; y++)
      {
         for (int x = 0; x < m_width; x++)
         {
            for (uint8_t c = 0; c < colors; c++)
            {
               pScaledFrame[((yoffset + y) * frameWidth + xoffset + x) * colors + c] = pFrame[(y * m_width + x) * colors + c];
            }
         }
      }
   }

   return bufferSize;
}
