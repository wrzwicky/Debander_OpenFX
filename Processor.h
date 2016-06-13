#pragma once

#include "ofxCore.h"
#include "ofxImageEffect.h"


////////////////////////////////////////////////////////////////////////////////
// This is a set of utility functions that got placed here as I got tired of
// typing the same thing.
// If anyone uses this for real functionality in real plugins, you are nuts.

template <class T> inline T Maximum(T a, T b) { return a > b ? a : b; }
template <class T> inline T Minimum(T a, T b) { return a < b ? a : b; }


////////////////////////////////////////////////////////////////////////////////
// base class to process images with
class Processor {
protected:
	OfxImageEffectHandle  instance;
	void *srcV, *dstV, *maskV;
	OfxRectI srcRect, dstRect, maskRect;
	int srcBytesPerLine, dstBytesPerLine, maskBytesPerLine;
	OfxRectI  window;

public:
	Processor(OfxImageEffectHandle  inst,
		void *src, OfxRectI sRect, int sBytesPerLine,
		void *dst, OfxRectI dRect, int dBytesPerLine,
		void *mask, OfxRectI mRect, int mBytesPerLine,
		OfxRectI  win)
		: instance(inst)
		, srcV(src)
		, dstV(dst)
		, maskV(mask)
		, srcRect(sRect)
		, dstRect(dRect)
		, maskRect(mRect)
		, srcBytesPerLine(sBytesPerLine)
		, dstBytesPerLine(dBytesPerLine)
		, maskBytesPerLine(mBytesPerLine)
		, window(win)
	{}

	static void multiThreadProcessing(unsigned int threadId, unsigned int nThreads, void *arg);
	void process(OfxMultiThreadSuiteV1 *pThreadSuite);

	virtual void doProcessing(OfxRectI window) = 0;
};
