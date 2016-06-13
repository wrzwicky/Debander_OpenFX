#pragma once
#include "Processor.h"

// template to do the Alpha processing
template <class PIX, class MASK, int max, int isFloat>
class ProcessAlpha : public Processor
{
public:
	ProcessAlpha(OfxImageEffectHandle handle,
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

	void doProcessing(OfxRectI window)
	{
		// copy stuff from ProcessRGBA here
	}
};
