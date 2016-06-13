#pragma once
#include "Processor.h"

#define PROCESS_ROWS 1
#define PROCESS_COLUMNS 1



// template to do the RGBA processing
// FULL IMPLEMENTATION GOES HERE -- C++ templates are done this way
template <class PIX, class MASK, int max, int isFloat>
class ProcessRGBA : public Processor {
public:
	ProcessRGBA(OfxImageEffectHandle handle,
		void *srcV, OfxRectI srcRect, int srcBytesPerLine,
		void *dstV, OfxRectI dstRect, int dstBytesPerLine,
		void *maskV, OfxRectI maskRect, int maskBytesPerLine,
		OfxRectI  window)
		: Processor(handle,
			srcV, srcRect, srcBytesPerLine,
			dstV, dstRect, dstBytesPerLine,
			maskV, maskRect, maskBytesPerLine,
			window)
	{
	}

	inline static
	bool equals(PIX *one, PIX *two)
	{
		return one->r == two->r && one->g == two->g && one->b == two->b && one->a == two->a;
	}

	template <class T> inline static
		T Clamp(T v, int min, int max)
	{
		if (v < T(min)) return T(min);
		if (v > T(max)) return T(max);
		return v;
	}

	//=======================================================================
	//
	// signum function, thanks to:
	// https://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
	//
	template <typename T> inline constexpr static
		int signum(T x, std::false_type is_signed) {
		return T(0) < x;
	}
	template <typename T> inline constexpr static
		int signum(T x, std::true_type is_signed) {
		return (T(0) < x) - (x < T(0));
	}
	template <typename T> inline constexpr static
		int signum(T x) {
		return signum(x, std::is_signed<T>());
	}
	//
	//=======================================================================



	// look up a pixel in the image, does bounds checking to see if it is in the image rectangle
	static
	PIX *pixelAddress(PIX *img, OfxRectI rect, int x, int y, int bytesPerLine)
	{
		if (x < rect.x1 || x >= rect.x2 || y < rect.y1 || y >= rect.y2 || !img)
			return 0;
		PIX *pix = (PIX *)(((char *)img) + (y - rect.y1) * bytesPerLine);
		pix += x - rect.x1;
		return pix;
	}



	void doProcessing(OfxRectI window)
	{
#ifdef _DEBUG
		printf("  doProcessing( x=%d-%d  y=%d-%d)\n", window.x1, window.x2, window.y1, window.y2);
#endif

		PIX *src = (PIX *)srcV;
		PIX *dst = (PIX *)dstV;

#if PROCESS_ROWS
		//=======================================================================
		//
		// PROCESS ROWS
		//

		for (int y = window.y1; y < window.y2; y++) {
			if (g.pEffectSuite->abort(instance))
				break;

			PIX *pDst = pixelAddress(dst, dstRect, window.x1, y, dstBytesPerLine);
			PIX *pSrc = pixelAddress(src, srcRect, window.x1, y, srcBytesPerLine);

			// need:
			//   xLeft (1st pixel)
			//   'start' color (left of xLeft)
			//   'center' color (middle of band)
			//   xRight (last pixel)
			//   'end' color (right of last pixel)

			int wMain = window.x2 - window.x1;  //actual num pixels to process
			for (int xMain = 0; xMain < wMain; xMain++)
			{
				// don't forget -- pSrc already points to pixel at window.x1

				// Hunt for start of band
				int xLeft = xMain;
				for (; xLeft < wMain-1; xLeft++)
				{
					if (equals(&pSrc[xLeft], &pSrc[xLeft + 1]))
						break;
					else
						pDst[xLeft] = pSrc[xLeft];
				}
//TODO Copy the last pixel in the line

				// Hunt for end of band
				int xRight = xLeft + 1;
				for (; xRight < wMain; xRight++)
				{
					if (!equals(&pSrc[xLeft], &pSrc[xRight]))
						break;
				}

				// Either way, xRight is one pixel too far
				xRight--;

				// This is deband mode: adjustments are limited to +/- 0.5 step.
				// pColorLeft and -Right represent colors one pixel *outside* the band.
				PIX pColorLeft = pSrc[xLeft];
				if (xLeft > 0)
				{
					// look at pixel left of band to adjust start color
					PIX *pOut = &pSrc[xLeft - 1];
#if 0
					// Since source image is float, I don't know how big a step is. :(
					pColorLeft.r += signum(pOut->r - pColorLeft.r) * 0.5f;
					pColorLeft.g += signum(pOut->g - pColorLeft.g) * 0.5f;
					pColorLeft.b += signum(pOut->b - pColorLeft.b) * 0.5f;
					pColorLeft.a += signum(pOut->a - pColorLeft.a) * 0.5f;
#endif
					// This will deband if color values are 1 'step' apart; deblock if farther.
					pColorLeft.r = (pOut->r + pColorLeft.r) * 0.5f;
					pColorLeft.g = (pOut->g + pColorLeft.g) * 0.5f;
					pColorLeft.b = (pOut->b + pColorLeft.b) * 0.5f;
					pColorLeft.a = (pOut->a + pColorLeft.a) * 0.5f;
				}
				else
					; // Leave color as-is.
				PIX pColorRight = pSrc[xRight];
				if (xRight + 1 < wMain)
				{
					// look at pixel left of band to adjust start color
					PIX *pOut = &pSrc[xRight + 1];
#if 0
					// Since source image is float, I don't know how big a step is. :(
					pColorRight.r += signum(pOut->r - pColorRight.r) * 0.5f;
					pColorRight.g += signum(pOut->g - pColorRight.g) * 0.5f;
					pColorRight.b += signum(pOut->b - pColorRight.b) * 0.5f;
					pColorRight.a += signum(pOut->a - pColorRight.a) * 0.5f;
#endif
					// This will deband if color values are 1 'step' apart; deblock if farther.
					pColorRight.r = (pOut->r + pColorRight.r) * 0.5f;
					pColorRight.g = (pOut->g + pColorRight.g) * 0.5f;
					pColorRight.b = (pOut->b + pColorRight.b) * 0.5f;
					pColorRight.a = (pOut->a + pColorRight.a) * 0.5f;
				}
				else
					; // Leave color as-is.

//TODO can I *read* from outside the window?

#if 0
//TODO deblock mode: use actual colors from outside band to fix band
//  as opposed to 'deband' which limits adjustments to +/- one half step regardless of actual jump from band to adjacent pixels.
				// copy starting color from one pixel to the left of band, if possible
				PIX *pColorLeft = &pSrc[xLeft > 0 ? xLeft - 1 : xLeft];
				// copy ending color from one pixel to the right of band, if possible
				PIX *pColorRight = &pSrc[xRight + 1 < wMain ? xRight + 1 : xRight];
#endif

				for (int ix = xLeft; ix <= xRight; ix++)
				{
					// denom is one more than band size
					int denom = (xRight - xLeft + 1) + 1;
					// numer ranges [1 .. (size-1)]
					int numer = (ix - xLeft) + 1;

					//pDst[ix] = pSrc[ix];
					//pDst[ix].r = (float)max * (denom - numer) / denom;
					pDst[ix].r = pColorLeft.r * (denom - numer) / denom + pColorRight.r * numer / denom;
					pDst[ix].g = pColorLeft.g * (denom - numer) / denom + pColorRight.g * numer / denom;
					pDst[ix].b = pColorLeft.b * (denom - numer) / denom + pColorRight.b * numer / denom;
					pDst[ix].a = pColorLeft.a * (denom - numer) / denom + pColorRight.a * numer / denom;
				}

				// Skip main loop past this band.
				xMain = xRight;
			}
		}
#endif PROCESS_ROWS



#if PROCESS_COLUMNS
		//=======================================================================
		//
		// PROCESS COLUMNS
		//
#define addrows_src(addr,n) (PIX *)(((char *)(addr)) + (n) * srcBytesPerLine)
#define addrows_dst(addr,n) (PIX *)(((char *)(addr)) + (n) * dstBytesPerLine)

		for (int x = window.x1; x < window.x2; x++)
		{
			if (g.pEffectSuite->abort(instance))
				break;

			PIX *pDst = pixelAddress(dst, dstRect, x, window.y1, dstBytesPerLine);
			PIX *pSrc = pixelAddress(src, srcRect, x, window.y1, srcBytesPerLine);

			int hMain = window.y2 - window.y1;  //actual num pixels to process
			for (int yMain = 0; yMain < hMain; yMain++)
			{
				// don't forget -- pSrc already points to pixel at window.y1

				// Hunt for start of band
				int yTop = yMain;
				for (; yTop < hMain - 1; yTop++)
				{
					if (equals(addrows_src(pSrc, yTop), addrows_src(pSrc, yTop + 1)))
						break;
					else
						*addrows_dst(pDst, yTop) = *addrows_src(pSrc, yTop);
				}

				// Hunt for end of band
				int yBot = yTop + 1;
				for (; yBot < hMain; yBot++)
				{
					if (!equals(addrows_src(pSrc, yTop), addrows_src(pSrc, yBot)))
						break;
				}

				// Either way, yBot is one pixel too far
				yBot--;

				// See row mode for docs and notes.
				PIX pColorTop = *addrows_src(pSrc, yTop);
				if (yTop > 0)
				{
					// look at pixel above band to adjust start color
					PIX *pOut = addrows_src(pSrc, yTop - 1);
					// This will deband if color values are 1 'step' apart; deblock if farther.
					pColorTop.r = (pOut->r + pColorTop.r) * 0.5f;
					pColorTop.g = (pOut->g + pColorTop.g) * 0.5f;
					pColorTop.b = (pOut->b + pColorTop.b) * 0.5f;
					pColorTop.a = (pOut->a + pColorTop.a) * 0.5f;
				}
				else
					; // Leave color as-is.
				PIX pColorBot = *addrows_src(pSrc, yBot);
				if (yBot + 1 < hMain)
				{
					// look at pixel left of band to adjust start color
					PIX *pOut = addrows_src(pSrc, yBot + 1);
					// This will deband if color values are 1 'step' apart; deblock if farther.
					pColorBot.r = (pOut->r + pColorBot.r) * 0.5f;
					pColorBot.g = (pOut->g + pColorBot.g) * 0.5f;
					pColorBot.b = (pOut->b + pColorBot.b) * 0.5f;
					pColorBot.a = (pOut->a + pColorBot.a) * 0.5f;
				}
				else
					; // Leave color as-is.

				for (int iy = yTop; iy <= yBot; iy++)
				{
					// denom is one more than band size
					int denom = (yBot - yTop + 1) + 1;
					// numer ranges [1 .. (size-1)]
					int numer = (iy - yTop) + 1;

					PIX *ps = addrows_src(pSrc, iy);
					PIX *pd = addrows_dst(pDst, iy);

					//pDst[ix] = pSrc[ix];
					//pDst[ix].r = (float)max * (denom - numer) / denom;
#if PROCESS_ROWS
					// average color with row mode result
					pd->r += pColorTop.r * (denom - numer) / denom + pColorBot.r * numer / denom;
					pd->g += pColorTop.g * (denom - numer) / denom + pColorBot.g * numer / denom;
					pd->b += pColorTop.b * (denom - numer) / denom + pColorBot.b * numer / denom;
					pd->a += pColorTop.a * (denom - numer) / denom + pColorBot.a * numer / denom;
					pd->r /= 2.f;
					pd->g /= 2.f;
					pd->b /= 2.f;
					pd->a /= 2.f;
#else
					// dump color into dest image
					pd->r = pColorTop.r * (denom - numer) / denom + pColorBot.r * numer / denom;
					pd->g = pColorTop.g * (denom - numer) / denom + pColorBot.g * numer / denom;
					pd->b = pColorTop.b * (denom - numer) / denom + pColorBot.b * numer / denom;
					pd->a = pColorTop.a * (denom - numer) / denom + pColorBot.a * numer / denom;
#endif
				}

				// Skip main loop past this band.
				yMain = yBot;
			}
		}
#endif PROCESS_COLUMNS

	}
};
