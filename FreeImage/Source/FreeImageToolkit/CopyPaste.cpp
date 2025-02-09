// ==========================================================
// Copy / paste routines
//
// - Floris van den Berg (flvdberg@wxs.nl)
// - Alexander Dymerets (sashad@te.net.ua)
// - Herv� Drolon (drolon@infonie.fr)
// - Manfred Tausch (manfred.tausch@t-online.de)
// - Riley McNiff (rmcniff@marexgroup.com)
//
// This file is part of FreeImage 3
//
// COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN "AS IS" BASIS, WITHOUT WARRANTY
// OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, WITHOUT LIMITATION, WARRANTIES
// THAT THE COVERED CODE IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE
// OR NON-INFRINGING. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED
// CODE IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT, YOU (NOT
// THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE COST OF ANY NECESSARY
// SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL
// PART OF THIS LICENSE. NO USE OF ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER
// THIS DISCLAIMER.
//
// Use at your own risk!
// ==========================================================

#include "FreeImage.h"
#include "Utilities.h"

// ----------------------------------------------------------
//   Macros + structures
// ----------------------------------------------------------

#define RGB555(r, g, b) ((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10))
#define RGB565(r, g, b) ((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11))

// ----------------------------------------------------------
//   Helpers
// ----------------------------------------------------------

/////////////////////////////////////////////////////////////
// Alpha blending / combine functions

// ----------------------------------------------------------
/// 1-bit
static BOOL Combine1(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha);
/// 4-bit
static BOOL Combine4(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha);
/// 8-bit
static BOOL Combine8(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha);
/// 16-bit 555
static BOOL Combine16_555(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha);
/// 16-bit 565
static BOOL Combine16_565(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha);
/// 24-bit
static BOOL Combine24(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha);
/// 32- bit
static BOOL Combine32(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha);
// ----------------------------------------------------------

// ----------------------------------------------------------
//   1-bit
// ----------------------------------------------------------

static BOOL 
Combine1(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha) {
	BOOL value;

	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 1) || (FreeImage_GetBPP(src_dib) != 1)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) * FreeImage_GetPitch(dst_dib));
	BYTE *src_bits = FreeImage_GetBits(src_dib);	

	// combine images
	for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
		for(WORD cols = 0; cols < FreeImage_GetWidth(src_dib); cols++) {
			// get bit at (rows, cols) in src image
			value = (src_bits[cols >> 3] & (0x80 >> (cols & 0x07))) != 0;
			// set bit at (rows, x+cols) in dst image	
			value ? dst_bits[(x + cols) >> 3] |= (0x80 >> ((x + cols) & 0x7)) : dst_bits[(x + cols) >> 3] &= (0xFF7F >> ((x + cols) & 0x7));
		}

		dst_bits += FreeImage_GetPitch(dst_dib);
		src_bits += FreeImage_GetPitch(src_dib);
	}

	return TRUE;
}

// ----------------------------------------------------------
//   4-bit
// ----------------------------------------------------------

static BOOL 
Combine4(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha) {

	int swapTable[16];
	BOOL bOddStart, bOddEnd;

	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 4) || (FreeImage_GetBPP(src_dib) != 4)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	// get src and dst palettes
	RGBQUAD *src_pal = FreeImage_GetPalette(src_dib);
	RGBQUAD *dst_pal = FreeImage_GetPalette(dst_dib);
	if (src_pal == NULL || dst_pal == NULL) {
		return FALSE;
	}

	// build a swap table for the closest color match from the source palette to the destination palette

	for (int i = 0; i < 16; i++)	{
		WORD min_diff = (WORD)-1;

		for (int j = 0; j < 16; j++)	{
			// calculates the color difference using a Manhattan distance
			WORD abs_diff = (WORD)(
				abs(src_pal[i].rgbBlue - dst_pal[j].rgbBlue)
				+ abs(src_pal[i].rgbGreen - dst_pal[j].rgbGreen)
				+ abs(src_pal[i].rgbRed - dst_pal[j].rgbRed)
				);

			if (abs_diff < min_diff)	{
				swapTable[i] = j;
				min_diff = abs_diff;
				if (abs_diff == 0) {
					break;
				}
			}
		}
	}

	BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) *	FreeImage_GetPitch(dst_dib)) + (x >> 1);
	BYTE *src_bits = FreeImage_GetBits(src_dib);    

	// combine images

	// allocate space for our temporary row
	unsigned src_line   = FreeImage_GetLine(src_dib);
	unsigned src_width  = FreeImage_GetWidth(src_dib);
	unsigned src_height = FreeImage_GetHeight(src_dib);

	BYTE *buffer = (BYTE *)malloc(src_line * sizeof(BYTE));
	if (buffer == NULL) {
		return FALSE;
	}

	bOddStart = (x & 0x01) ? TRUE : FALSE;

	if ((bOddStart && !(src_width & 0x01)) || (!bOddStart && (src_width & 0x01)))	{
		bOddEnd = TRUE;
	}
	else {
		bOddEnd = FALSE;
	}

	for(WORD rows = 0; rows < src_height; rows++) {
		memcpy(buffer, src_bits, src_line);
		
		// change the values in the temp row to be those from the swap table
		
		for (WORD cols = 0; cols < src_line; cols++) {
			buffer[cols] = (BYTE)((swapTable[HINIBBLE(buffer[cols]) >> 4] << 4) + swapTable[LOWNIBBLE(buffer[cols])]);
		}

		if (bOddStart) {	
			buffer[0] = HINIBBLE(dst_bits[0]) + LOWNIBBLE(buffer[0]);
		}
		
		if (bOddEnd)	{
			buffer[src_line - 1] = HINIBBLE(buffer[src_line - 1]) + LOWNIBBLE(dst_bits[src_line - 1]);
		}

		memcpy(dst_bits, buffer, src_line);
		
		dst_bits += FreeImage_GetPitch(dst_dib);
		src_bits += FreeImage_GetPitch(src_dib);
	}

	free(buffer);

	return TRUE;

}

// ----------------------------------------------------------
//   8-bit
// ----------------------------------------------------------

static BOOL 
Combine8(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha) {
	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 8) || (FreeImage_GetBPP(src_dib) != 8)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) * FreeImage_GetPitch(dst_dib)) + (x);
	BYTE *src_bits = FreeImage_GetBits(src_dib);	

	if(alpha > 255) {
		// combine images
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			memcpy(dst_bits, src_bits, FreeImage_GetLine(src_dib));

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	} else {
		// alpha blend images
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			for (WORD cols = 0; cols < FreeImage_GetLine(src_dib); cols++) {							
				dst_bits[cols] = (BYTE)(((src_bits[cols] - dst_bits[cols]) * alpha + (dst_bits[cols] << 8)) >> 8);
			}

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	}

	return TRUE;
}

// ----------------------------------------------------------
//   16-bit
// ----------------------------------------------------------

static BOOL 
Combine16_555(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha) {
	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 16) || (FreeImage_GetBPP(src_dib) != 16)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) * FreeImage_GetPitch(dst_dib)) + (x * 2);
	BYTE *src_bits = FreeImage_GetBits(src_dib);	

	if (alpha > 255) {
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			memcpy(dst_bits, src_bits, FreeImage_GetLine(src_dib));

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	} else {
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			for(WORD cols = 0; cols < FreeImage_GetLine(src_dib); cols += 2) {
				RGBTRIPLE color_s;
				RGBTRIPLE color_t;
				
				WORD *tmp1 = (WORD *)&dst_bits[cols];
				WORD *tmp2 = (WORD *)&src_bits[cols];

				// convert 16-bit colors to 24-bit

				color_s.rgbtRed = (BYTE)(((*tmp1 & FI16_555_RED_MASK) >> FI16_555_RED_SHIFT) << 3);
				color_s.rgbtGreen = (BYTE)(((*tmp1 & FI16_555_GREEN_MASK) >> FI16_555_GREEN_SHIFT) << 3);
				color_s.rgbtBlue = (BYTE)(((*tmp1 & FI16_555_BLUE_MASK) >> FI16_555_BLUE_SHIFT) << 3);

				color_t.rgbtRed = (BYTE)(((*tmp2 & FI16_555_RED_MASK) >> FI16_555_RED_SHIFT) << 3);
				color_t.rgbtGreen = (BYTE)(((*tmp2 & FI16_555_GREEN_MASK) >> FI16_555_GREEN_SHIFT) << 3);
				color_t.rgbtBlue = (BYTE)(((*tmp2 & FI16_555_BLUE_MASK) >> FI16_555_BLUE_SHIFT) << 3);

				// alpha blend

				color_s.rgbtRed = (BYTE)(((color_t.rgbtRed - color_s.rgbtRed) * alpha + (color_s.rgbtRed << 8)) >> 8);
				color_s.rgbtGreen = (BYTE)(((color_t.rgbtGreen - color_s.rgbtGreen) * alpha + (color_s.rgbtGreen << 8)) >> 8);
				color_s.rgbtBlue = (BYTE)(((color_t.rgbtBlue - color_s.rgbtBlue) * alpha + (color_s.rgbtBlue << 8)) >> 8);

				// convert 24-bit color back to 16-bit

				*tmp1 = RGB555(color_s.rgbtRed, color_s.rgbtGreen, color_s.rgbtBlue);
			}

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	}

	return TRUE;
}

static BOOL 
Combine16_565(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha) {
	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 16) || (FreeImage_GetBPP(src_dib) != 16)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) * FreeImage_GetPitch(dst_dib)) + (x * 2);
	BYTE *src_bits = FreeImage_GetBits(src_dib);	

	if (alpha > 255) {
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			memcpy(dst_bits, src_bits, FreeImage_GetLine(src_dib));

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	} else {
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			for(WORD cols = 0; cols < FreeImage_GetLine(src_dib); cols += 2) {
				RGBTRIPLE color_s;
				RGBTRIPLE color_t;
				
				WORD *tmp1 = (WORD *)&dst_bits[cols];
				WORD *tmp2 = (WORD *)&src_bits[cols];

				// convert 16-bit colors to 24-bit

				color_s.rgbtRed = (BYTE)(((*tmp1 & FI16_565_RED_MASK) >> FI16_565_RED_SHIFT) << 3);
				color_s.rgbtGreen = (BYTE)(((*tmp1 & FI16_565_GREEN_MASK) >> FI16_565_GREEN_SHIFT) << 2);
				color_s.rgbtBlue = (BYTE)(((*tmp1 & FI16_565_BLUE_MASK) >> FI16_565_BLUE_SHIFT) << 3);

				color_t.rgbtRed = (BYTE)(((*tmp2 & FI16_565_RED_MASK) >> FI16_565_RED_SHIFT) << 3);
				color_t.rgbtGreen = (BYTE)(((*tmp2 & FI16_565_GREEN_MASK) >> FI16_565_GREEN_SHIFT) << 2);
				color_t.rgbtBlue = (BYTE)(((*tmp2 & FI16_565_BLUE_MASK) >> FI16_565_BLUE_SHIFT) << 3);

				// alpha blend

				color_s.rgbtRed = (BYTE)(((color_t.rgbtRed - color_s.rgbtRed) * alpha + (color_s.rgbtRed << 8)) >> 8);
				color_s.rgbtGreen = (BYTE)(((color_t.rgbtGreen - color_s.rgbtGreen) * alpha + (color_s.rgbtGreen << 8)) >> 8);
				color_s.rgbtBlue = (BYTE)(((color_t.rgbtBlue - color_s.rgbtBlue) * alpha + (color_s.rgbtBlue << 8)) >> 8);

				// convert 24-bit color back to 16-bit

				*tmp1 = RGB565(color_s.rgbtRed, color_s.rgbtGreen, color_s.rgbtBlue);
			}

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	}
	
	return TRUE;
}

// ----------------------------------------------------------
//   24-bit
// ----------------------------------------------------------

static BOOL 
Combine24(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha) {
	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 24) || (FreeImage_GetBPP(src_dib) != 24)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) * FreeImage_GetPitch(dst_dib)) + (x * 3);
	BYTE *src_bits = FreeImage_GetBits(src_dib);	

	if(alpha > 255) {
		// combine images
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			memcpy(dst_bits, src_bits, FreeImage_GetLine(src_dib));

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	} else {
		// alpha blend images
		for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
			for (WORD cols = 0; cols < FreeImage_GetLine(src_dib); cols++) {							
				dst_bits[cols] = (BYTE)(((src_bits[cols] - dst_bits[cols]) * alpha + (dst_bits[cols] << 8)) >> 8);
			}

			dst_bits += FreeImage_GetPitch(dst_dib);
			src_bits += FreeImage_GetPitch(src_dib);
		}
	}

	return TRUE;
}

// ----------------------------------------------------------
//   32-bit
// ----------------------------------------------------------

static BOOL 
BlendCombine32(FIBITMAP *dst_dib, FIBITMAP *src_dib, int x, int y) {

	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 32) || (FreeImage_GetBPP(src_dib) != 32)) {
		return FALSE;
	}

  int src_w = FreeImage_GetWidth(src_dib), src_h = FreeImage_GetHeight(src_dib);
  int dst_w = FreeImage_GetWidth(dst_dib), dst_h = FreeImage_GetHeight(dst_dib);

  if (x + src_w < 0 || y + src_h < 0)
    return FALSE;

  if (x > dst_w || y > dst_h)
    return FALSE;

  unsigned int src_line_width = 0;
  for(int src_y = 0; src_y < src_h; src_y++) 
  {
    int dst_y = src_y + y;
    unsigned int dst_line_width = dst_y * FreeImage_GetPitch(dst_dib);

    for(int src_x = 0; src_x < src_w; src_x++) 
    { 
      int dst_x = src_x + x;

      //-- ligne condamn�e
      if (dst_y < 0 || src_y >= FreeImage_GetHeight(src_dib) || dst_y >= FreeImage_GetHeight(dst_dib))
        break;
      //--

      if (dst_x < 0)
        continue;

      if (src_x >= FreeImage_GetWidth(src_dib) || dst_x >= FreeImage_GetWidth(dst_dib))
        continue;

      BYTE *src_bits = FreeImage_GetBits(src_dib) + src_line_width + (src_x << 2);
      BYTE *dst_bits = FreeImage_GetBits(dst_dib) + dst_line_width + (dst_x << 2);

      if (src_bits[FI_RGBA_ALPHA] == 0) // rien � blender
        ;
      else if (src_bits[FI_RGBA_ALPHA] == 255 || dst_bits[FI_RGBA_ALPHA] == 0) // copie � 100%
      {
        memcpy(dst_bits, src_bits, 4);
      }
      else // blend
      {
        float src_alpha = src_bits[FI_RGBA_ALPHA] / 255.f;
        float dst_alpha = dst_bits[FI_RGBA_ALPHA] / 255.f;
        float one_minus_src_alpha = 1.0f - src_alpha;
        float composit_alpha = one_minus_src_alpha * dst_alpha;
        float result_alpha = src_alpha + composit_alpha;
        dst_bits[FI_RGBA_RED] = (dst_bits[FI_RGBA_RED] * composit_alpha + src_bits[FI_RGBA_RED] * src_alpha) / result_alpha;
        dst_bits[FI_RGBA_GREEN] = (dst_bits[FI_RGBA_GREEN] * composit_alpha + src_bits[FI_RGBA_GREEN] * src_alpha) / result_alpha;
        dst_bits[FI_RGBA_BLUE] = (dst_bits[FI_RGBA_BLUE] * composit_alpha + src_bits[FI_RGBA_BLUE] * src_alpha) / result_alpha;
        dst_bits[FI_RGBA_ALPHA] = result_alpha * 255.f;
      }
    }
    src_line_width += FreeImage_GetPitch(src_dib);
  }
	return TRUE;
}


static BOOL 
Combine32(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y, WORD alpha) {
	// check the bit depth of src and dst images
	if((FreeImage_GetBPP(dst_dib) != 32) || (FreeImage_GetBPP(src_dib) != 32)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	BYTE *src_bits = FreeImage_GetBits(src_dib);	
  BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) * FreeImage_GetPitch(dst_dib)) + (x * 4);
		
  WORD block_width = FreeImage_GetPitch(src_dib);
  WORD dst_line_jump_length = FreeImage_GetPitch(dst_dib) - block_width;

  for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) 
  {
    for(WORD cols = 0; cols < FreeImage_GetWidth(src_dib); cols++) 
    {
        if (src_bits[FI_RGBA_ALPHA] == 0) // rien � blender
          ;
        else if (src_bits[FI_RGBA_ALPHA] == 255) // copie � 100%
        {
          memcpy(dst_bits, src_bits, 4);
        }
        else // blend
        {
          float src_alpha = ((float)src_bits[FI_RGBA_ALPHA]) / 255.f;
          float dst_alpha = ((float)dst_bits[FI_RGBA_ALPHA]) / 255.f;
          dst_bits[FI_RGBA_RED] = dst_bits[FI_RGBA_RED] * (dst_alpha - src_alpha) + src_bits[FI_RGBA_RED] * src_alpha;
          dst_bits[FI_RGBA_GREEN] = dst_bits[FI_RGBA_GREEN] * (dst_alpha - src_alpha) + src_bits[FI_RGBA_GREEN] * src_alpha;
          dst_bits[FI_RGBA_BLUE] = dst_bits[FI_RGBA_BLUE] * (dst_alpha - src_alpha) + src_bits[FI_RGBA_BLUE] * src_alpha;
          if (dst_bits[FI_RGBA_ALPHA] + src_bits[FI_RGBA_ALPHA] > 255) dst_bits[FI_RGBA_ALPHA] = 255;
          else dst_bits[FI_RGBA_ALPHA] += src_bits[FI_RGBA_ALPHA];
        }
        dst_bits += 4;
        src_bits += 4;
    }
    dst_bits += dst_line_jump_length;
  }

	return TRUE;
}

// ----------------------------------------------------------
//   Any type other than FIBITMAP
// ----------------------------------------------------------

static BOOL 
CombineSameType(FIBITMAP *dst_dib, FIBITMAP *src_dib, WORD x, WORD y) {
	// check the bit depth of src and dst images
	if(FreeImage_GetImageType(dst_dib) != FreeImage_GetImageType(src_dib)) {
		return FALSE;
	}

	// check the size of src image
	if((x + FreeImage_GetWidth(src_dib) > FreeImage_GetWidth(dst_dib)) || (y + FreeImage_GetHeight(src_dib) > FreeImage_GetHeight(dst_dib))) {
		return FALSE;
	}

	BYTE *dst_bits = FreeImage_GetBits(dst_dib) + ((FreeImage_GetHeight(dst_dib) - FreeImage_GetHeight(src_dib) - y) * FreeImage_GetPitch(dst_dib)) + (x);
	BYTE *src_bits = FreeImage_GetBits(src_dib);	

	// combine images
	for(WORD rows = 0; rows < FreeImage_GetHeight(src_dib); rows++) {
		memcpy(dst_bits, src_bits, FreeImage_GetLine(src_dib));

		dst_bits += FreeImage_GetPitch(dst_dib);
		src_bits += FreeImage_GetPitch(src_dib);
	}

	return TRUE;
}

// ----------------------------------------------------------
//   FreeImage interface
// ----------------------------------------------------------

/**
Copy a sub part of the current image and returns it as a FIBITMAP*.
Works with any bitmap type.
@param left Specifies the left position of the cropped rectangle. 
@param top Specifies the top position of the cropped rectangle. 
@param right Specifies the right position of the cropped rectangle. 
@param bottom Specifies the bottom position of the cropped rectangle. 
@return Returns the subimage if successful, NULL otherwise.
*/
FIBITMAP * DLL_CALLCONV 
FreeImage_Copy(FIBITMAP *src, int left, int top, int right, int bottom) {

	if(!src) 
		return NULL;

	// normalize the rectangle
	if(right < left) {
		INPLACESWAP(left, right);
	}
	if(bottom < top) {
		INPLACESWAP(top, bottom);
	}
	// check the size of the sub image
	int src_width  = FreeImage_GetWidth(src);
	int src_height = FreeImage_GetHeight(src);
	if((left < 0) || (right > src_width) || (top < 0) || (bottom > src_height)) {
		return NULL;
	}

	// allocate the sub image
	unsigned bpp = FreeImage_GetBPP(src);
	int dst_width = (right - left);
	int dst_height = (bottom - top);

	FIBITMAP *dst = 
		FreeImage_AllocateT(FreeImage_GetImageType(src), 
							dst_width, 
							dst_height, 
							bpp, 
							FreeImage_GetRedMask(src), FreeImage_GetGreenMask(src), FreeImage_GetBlueMask(src));

	if(NULL == dst) return NULL;

	// get the dimensions
	int dst_line = FreeImage_GetLine(dst);
	int dst_pitch = FreeImage_GetPitch(dst);
	int src_pitch = FreeImage_GetPitch(src);

	// get the pointers to the bits and such

	BYTE *src_bits = FreeImage_GetScanLine(src, src_height - top - dst_height);
	switch(bpp) {
		case 1:
			// point to x = 0
			break;

		case 4:
			// point to x = 0
			break;

		default:
		{
			// calculate the number of bytes per pixel
			unsigned bytespp = FreeImage_GetLine(src) / FreeImage_GetWidth(src);
			// point to x = left
			src_bits += left * bytespp;
		}
		break;
	}

	// point to x = 0
	BYTE *dst_bits = FreeImage_GetBits(dst);

	// copy the palette

	memcpy(FreeImage_GetPalette(dst), FreeImage_GetPalette(src), FreeImage_GetColorsUsed(src) * sizeof(RGBQUAD));

	// copy the bits
	if(bpp == 1) {
		BOOL value;
		unsigned y_src, y_dst;

		for(int y = 0; y < dst_height; y++) {
			y_src = y * src_pitch;
			y_dst = y * dst_pitch;
			for(int x = 0; x < dst_width; x++) {
				// get bit at (y, x) in src image
				value = (src_bits[y_src + ((left+x) >> 3)] & (0x80 >> ((left+x) & 0x07))) != 0;
				// set bit at (y, x) in dst image
				value ? dst_bits[y_dst + (x >> 3)] |= (0x80 >> (x & 0x7)) : dst_bits[y_dst + (x >> 3)] &= (0xff7f >> (x & 0x7));
			}
		}
	}

	else if(bpp == 4) {
		BYTE shift, value;
		unsigned y_src, y_dst;

		for(int y = 0; y < dst_height; y++) {
			y_src = y * src_pitch;
			y_dst = y * dst_pitch;
			for(int x = 0; x < dst_width; x++) {
				// get nibble at (y, x) in src image
				shift = (BYTE)((1 - (left+x) % 2) << 2);
				value = (src_bits[y_src + ((left+x) >> 1)] & (0x0F << shift)) >> shift;
				// set nibble at (y, x) in dst image
				shift = (BYTE)((1 - x % 2) << 2);
				dst_bits[y_dst + (x >> 1)] &= ~(0x0F << shift);
				dst_bits[y_dst + (x >> 1)] |= ((value & 0x0F) << shift);
			}
		}
	}

	else if(bpp >= 8) {
		for(int y = 0; y < dst_height; y++) {
			memcpy(dst_bits + (y * dst_pitch), src_bits + (y * src_pitch), dst_line);
		}
	}

	return dst;
}

/**
Alpha blend a sub part image with the current image.
The bit depth of dst bitmap must be greater than or equal to the bit depth of src. 
Upper promotion of src is done internally. Supported bit depth equals to 32.
@param src Source subimage
@param left Specifies the left position of the sub image. 
@param top Specifies the top position of the sub image. 
@return Returns TRUE if successful, FALSE otherwise.
*/
BOOL DLL_CALLCONV 
FreeImage_BlendPaste(FIBITMAP *dst, FIBITMAP *src, int left, int top) {
  BOOL bResult = FALSE;

  if(!src || !dst) return FALSE;

  // check data type
  FREE_IMAGE_TYPE image_type = FreeImage_GetImageType(dst);
  if(image_type != FreeImage_GetImageType(src)) {
    // no conversion between data type is done
    return FALSE;
  }

  if (FreeImage_GetBPP(dst) != 32)
    return FALSE;

  FIBITMAP *clone = NULL;

  // check the bit depth of src and dst images
  unsigned bpp_src = FreeImage_GetBPP(src);
  unsigned bpp_dst = FreeImage_GetBPP(dst);

  BOOL isRGB565 = FALSE;

  if ((FreeImage_GetRedMask(dst) == FI16_565_RED_MASK) && (FreeImage_GetGreenMask(dst) == FI16_565_GREEN_MASK) && (FreeImage_GetBlueMask(dst) == FI16_565_BLUE_MASK)) {
    isRGB565 = TRUE;
  } else {
    // includes case where all the masks are 0
    isRGB565 = FALSE;
  }

  // perform promotion if needed
  if(bpp_dst == bpp_src) {
    clone = src;
  } else if(bpp_dst > bpp_src) {
    switch(bpp_dst) {
        case 32:
          clone = FreeImage_ConvertTo32Bits(src);
          break;
        default:
          return FALSE;
    }
  } else {
    return FALSE;
  }

  if(!clone) return FALSE;

  bResult = BlendCombine32(dst, clone, left, top);

  if(clone != src)
    FreeImage_Unload(clone);

  return bResult;
}


BOOL DLL_CALLCONV 
FreeImage_Paste(FIBITMAP *dst, FIBITMAP *src, int left, int top, int alpha) {
	BOOL bResult = FALSE;

	if(!src || !dst) return FALSE;

	// check the size of src image
	if((left + FreeImage_GetWidth(src) > FreeImage_GetWidth(dst)) || (top + FreeImage_GetHeight(src) > FreeImage_GetHeight(dst))) {
		return FALSE;
	}

	// check data type
	FREE_IMAGE_TYPE image_type = FreeImage_GetImageType(dst);
	if(image_type != FreeImage_GetImageType(src)) {
		// no conversion between data type is done
		return FALSE;
	}

	if(image_type == FIT_BITMAP) {
		FIBITMAP *clone = NULL;

		// check the bit depth of src and dst images
		unsigned bpp_src = FreeImage_GetBPP(src);
		unsigned bpp_dst = FreeImage_GetBPP(dst);
		BOOL isRGB565 = FALSE;

		if ((FreeImage_GetRedMask(dst) == FI16_565_RED_MASK) && (FreeImage_GetGreenMask(dst) == FI16_565_GREEN_MASK) && (FreeImage_GetBlueMask(dst) == FI16_565_BLUE_MASK)) {
			isRGB565 = TRUE;
		} else {
			// includes case where all the masks are 0
			isRGB565 = FALSE;
		}

		// perform promotion if needed
		if(bpp_dst == bpp_src) {
			clone = src;
		} else if(bpp_dst > bpp_src) {
			// perform promotion
			switch(bpp_dst) {
				case 4:
					clone = FreeImage_ConvertTo4Bits(src);
					break;
				case 8:
					clone = FreeImage_ConvertTo8Bits(src);
					break;
				case 16:
					if (isRGB565) {
						clone = FreeImage_ConvertTo16Bits565(src);
					} else {
						// includes case where all the masks are 0
						clone = FreeImage_ConvertTo16Bits555(src);
					}
					break;
				case 24:
					clone = FreeImage_ConvertTo24Bits(src);
					break;
				case 32:
					clone = FreeImage_ConvertTo32Bits(src);
					break;
				default:
					return FALSE;
			}
		} else {
			return FALSE;
		}

		if(!clone) return FALSE;

		// paste src to dst
		switch(FreeImage_GetBPP(dst)) {
			case 1:
				bResult = Combine1(dst, clone, (WORD)left, (WORD)top, (WORD)alpha);
				break;
			case 4:
				bResult = Combine4(dst, clone, (WORD)left, (WORD)top, (WORD)alpha);
				break;
			case 8:
				bResult = Combine8(dst, clone, (WORD)left, (WORD)top, (WORD)alpha);
				break;
			case 16:
				if (isRGB565) {
					bResult = Combine16_565(dst, clone, (WORD)left, (WORD)top, (WORD)alpha);
				} else {
					// includes case where all the masks are 0
					bResult = Combine16_555(dst, clone, (WORD)left, (WORD)top, (WORD)alpha);
				}
				break;
			case 24:
				bResult = Combine24(dst, clone, (WORD)left, (WORD)top, (WORD)alpha);
				break;
			case 32:
				bResult = Combine32(dst, clone, (WORD)left, (WORD)top, (WORD)alpha);
				break;
		}

		if(clone != src)
			FreeImage_Unload(clone);

		}
	else {	// any type other than FITBITMAP
		bResult = CombineSameType(dst, src, (WORD)left, (WORD)top);
	}

	return bResult;
}

