
#include "PsImage.h"
#include "FreeImage.h"

/*
** Creation d'une FIBITMAP à partir d'un buffer
*/
FIBITMAP *FreeImage_FromBuffer(unsigned char *realbuffer, int sx, int sy, int bpp)
{
  FIBITMAP *im = FreeImage_Allocate(sx, sy, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
  FreeImage_SetTransparent(im, true);
  int bytespp = FreeImage_GetLine(im) / FreeImage_GetWidth(im);
  for(unsigned int y = 0; y < FreeImage_GetHeight(im); y++) 
  {
    BYTE *bits = FreeImage_GetScanLine(im, y);
    for(unsigned int x = 0; x < FreeImage_GetWidth(im); x++) 
    {
      unsigned char* location = realbuffer + (x + (y * (int)sx)) * bpp;
#ifndef __BIG_ENDIAN__
      bits[FI_RGBA_RED] = location[2];
      bits[FI_RGBA_BLUE] = location[0];
#else
		bits[FI_RGBA_RED] = location[0];
      bits[FI_RGBA_BLUE] = location[2];
#endif
      bits[FI_RGBA_GREEN] = location[1];
      if (bpp == 3) bits[FI_RGBA_ALPHA] = 255;
      else bits[FI_RGBA_ALPHA] = location[3];
      bits += bytespp;
    }
  }
  FreeImage_FlipVertical(im);
  return im;
}

/*
** Creation d'une FIBITMAP blanche
*/
FIBITMAP *FreeImage_CreateWhiteRGBA(int w, int h)
{
  FIBITMAP *im = FreeImage_Allocate(w, h, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
  FreeImage_SetTransparent(im, true);
  int bytespp = FreeImage_GetLine(im) / FreeImage_GetWidth(im);
  for(unsigned int y = 0; y < FreeImage_GetHeight(im); y++) 
  {
    BYTE *bits = FreeImage_GetScanLine(im, y);
    for(unsigned int x = 0; x < FreeImage_GetWidth(im); x++) 
    {
      bits[FI_RGBA_RED] = 255;
      bits[FI_RGBA_GREEN] = 255;
      bits[FI_RGBA_BLUE] = 255;
      bits[FI_RGBA_ALPHA] = 255;
      bits += bytespp;
    }
  }
  return im;
}

/*
** Creation d'une FIBITMAP à partir d'un objet Image
*/
FIBITMAP *FreeImage_FromImage(PsImage& image)
{
  int dsx = (int)abs(image.w);
	int dsy = (int)abs(image.h);
  int sx, sy, bpp;
	image.GetTexture().GetSize(sx, sy);	
	unsigned char *realbuffer = image.GetTexture().GetBufferUncompressed(bpp);
  FIBITMAP *im = FreeImage_FromBuffer(realbuffer, sx, sy, bpp);
  delete[] realbuffer; 
  return im;
}



