#pragma once

#include "ofxCore.h"
#include "ofxImageEffect.h"

struct Globals {
	// Host main pointer
	OfxHost					*pHost = NULL;

	// Host function "suites"
	OfxImageEffectSuiteV1	*pEffectSuite = NULL;
	OfxPropertySuiteV1		*pPropSuite = NULL;
	OfxParameterSuiteV1		*pParamSuite = NULL;
	OfxMemorySuiteV1		*pMemorySuite = NULL;
	OfxMultiThreadSuiteV1	*pThreadSuite = NULL;
	OfxMessageSuiteV1		*pMessageSuite = NULL;
	OfxInteractSuiteV1		*pInteractSuite = NULL;

	// some flags about the host's behaviour
	bool iHostSupportsMultipleBitDepths = false;
};
extern Globals g;
