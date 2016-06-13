#include "debander.h"

#include <stdexcept>
#include <new>
#include <cstring>
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxUtilities.H" // example support utils

#include "guicon.h"
#include "ProcessRGBA.h"
#include "ProcessAlpha.h"


#if defined __APPLE__ || defined linux || defined __FreeBSD__
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

#define OFX_PLUGIN_UID   "wrz.DebandingPlugin"
#define OFX_PLUGIN_GROUP "Uncle Bill's Pretty Good Software"
#define OFX_PLUGIN_NAME  "Debander"


// ===================================================== //
Globals g;

// private instance data type
struct MyInstanceData {
  bool isGeneralEffect;

  // handles to the clips we deal with
  OfxImageClipHandle sourceClip;
  OfxImageClipHandle maskClip;
  OfxImageClipHandle outputClip;
};

/* mandatory function to set up the host structures */


// Convinience wrapper to get private data 
static MyInstanceData *
getMyInstanceData( OfxImageEffectHandle effect)
{
  // get the property handle for the plugin
  OfxPropertySetHandle effectProps;
  g.pEffectSuite->getPropertySet(effect, &effectProps);

  // get my data pointer out of that
  MyInstanceData *myData = 0;
  g.pPropSuite->propGetPointer(effectProps,  kOfxPropInstanceData, 0, 
			    (void **) &myData);
  return myData;
}



// ===================================================== //
// MAIN ACTION FUNCTIONS

/** @brief Called at load */
static OfxStatus
onLoad(void)
{
	if (!g.pHost)
		return kOfxStatErrMissingHostFeature;

#ifdef _DEBUG
	RedirectIOToConsole();
	printf("Debug console is active.\n");
#endif

	g.pEffectSuite = (OfxImageEffectSuiteV1 *)g.pHost->fetchSuite(g.pHost->host, kOfxImageEffectSuite, 1);
	g.pPropSuite = (OfxPropertySuiteV1 *)g.pHost->fetchSuite(g.pHost->host, kOfxPropertySuite, 1);
	g.pParamSuite = (OfxParameterSuiteV1 *)g.pHost->fetchSuite(g.pHost->host, kOfxParameterSuite, 1);
	g.pMemorySuite = (OfxMemorySuiteV1 *)g.pHost->fetchSuite(g.pHost->host, kOfxMemorySuite, 1);
	g.pThreadSuite = (OfxMultiThreadSuiteV1 *)g.pHost->fetchSuite(g.pHost->host, kOfxMultiThreadSuite, 1);
	g.pMessageSuite = (OfxMessageSuiteV1 *)g.pHost->fetchSuite(g.pHost->host, kOfxMessageSuite, 1);
	g.pInteractSuite = (OfxInteractSuiteV1 *)g.pHost->fetchSuite(g.pHost->host, kOfxInteractSuite, 1);

	if (!g.pEffectSuite || !g.pPropSuite || !g.pParamSuite || !g.pMemorySuite || !g.pThreadSuite || !g.pMessageSuite || !g.pInteractSuite )
		return kOfxStatErrMissingHostFeature;

	// record a few host features
	int prop;
	g.pPropSuite->propGetInt(g.pHost->host, kOfxImageEffectPropSupportsMultipleClipDepths, 0, &prop);
	g.iHostSupportsMultipleBitDepths = (prop != 0);

	return kOfxStatOK;
}

/** @brief Called before unload */
static OfxStatus
onUnLoad(void)
{
	return kOfxStatOK;
}

// provide the plugin's name and features
static OfxStatus
describe(OfxImageEffectHandle  effect)
{
	// get the property handle for the plugin
	OfxPropertySetHandle effectProps;
	g.pEffectSuite->getPropertySet(effect, &effectProps);

	// set some labels and the group it belongs to
	g.pPropSuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, OFX_PLUGIN_GROUP);
	g.pPropSuite->propSetString(effectProps, kOfxPropLabel, 0, OFX_PLUGIN_NAME);

	// define the context(s) we can be used in
	g.pPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);

	// We can render both fields in a fielded images in one hit if there is no animation
	// So set the flag that allows us to do this
	g.pPropSuite->propSetInt(effectProps, kOfxImageEffectPluginPropFieldRenderTwiceAlways, 0, 0);

	// say we can support multiple pixel depths and let the clip preferences action deal with it all.
	g.pPropSuite->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 1);

	// disable tiling; we need the whole image
	g.pPropSuite->propSetInt(effectProps, kOfxImageEffectPropSupportsTiles, 0, 0);

	// set the bit depths the plugin can handle
	g.pPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
	g.pPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
	g.pPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthFloat);

	return kOfxStatOK;
}

// describe each of the plugin contexts availble
static OfxStatus
describeInContext(OfxImageEffectHandle  effect, OfxPropertySetHandle inArgs)
{
	// get the context from the inArgs handle
	char *context;
	g.pPropSuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);
	bool isGeneralContext = strcmp(context, kOfxImageEffectContextGeneral) == 0;

	OfxPropertySetHandle props;
	// define the single output clip in both contexts
	g.pEffectSuite->clipDefine(effect, kOfxImageEffectOutputClipName, &props);

	// set the component types we can handle on out output
	g.pPropSuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
	g.pPropSuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);

	// define the single source clip in both contexts
	g.pEffectSuite->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &props);

	// set the component types we can handle on our main input
	g.pPropSuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
	g.pPropSuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);

	if (isGeneralContext) {
		// define a second input that is a mask, alpha only and is optional
		g.pEffectSuite->clipDefine(effect, "Mask", &props);
		g.pPropSuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentAlpha);
		g.pPropSuite->propSetInt(props, kOfxImageClipPropOptional, 0, 1);
	}

	return kOfxStatOK;
}

//  instance construction
static OfxStatus
createInstance(OfxImageEffectHandle effect)
{
	// get a pointer to the effect properties
	OfxPropertySetHandle effectProps;
	g.pEffectSuite->getPropertySet(effect, &effectProps);

	// get a pointer to the effect's parameter set
	OfxParamSetHandle paramSet;
	g.pEffectSuite->getParamSet(effect, &paramSet);

	// make my private instance data
	MyInstanceData *myData = new MyInstanceData;
	char *context = 0;

	// is this instance a general effect ?
	g.pPropSuite->propGetString(effectProps, kOfxImageEffectPropContext, 0, &context);
	myData->isGeneralEffect = context && (strcmp(context, kOfxImageEffectContextGeneral) == 0);

	// cache away our clip handles
	g.pEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &myData->sourceClip, 0);
	g.pEffectSuite->clipGetHandle(effect, kOfxImageEffectOutputClipName, &myData->outputClip, 0);

	if (myData->isGeneralEffect) {
		g.pEffectSuite->clipGetHandle(effect, "Mask", &myData->maskClip, 0);
	}
	else
		myData->maskClip = 0;

	// set my private instance data
	g.pPropSuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *)myData);

	return kOfxStatOK;
}

// instance destruction
static OfxStatus
destroyInstance(OfxImageEffectHandle  effect)
{
	// get my instance data
	MyInstanceData *myData = getMyInstanceData(effect);

	// and delete it
	if (myData)
	{
		delete myData;

		OfxPropertySetHandle effectProps;
		g.pEffectSuite->getPropertySet(effect, &effectProps);
		g.pPropSuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *)myData);
	}
	return kOfxStatOK;
}

// are the settings of the effect performing an identity operation
static OfxStatus
isIdentity(OfxImageEffectHandle  effect,
	OfxPropertySetHandle inArgs,
	OfxPropertySetHandle outArgs)
{
	// Debander never does the identity op
	// (or at least, we won't know til we've processed the image.)
	return kOfxStatReplyDefault;
}

// the process code  that the host sees
static OfxStatus render(OfxImageEffectHandle  handle,
	OfxPropertySetHandle inArgs,
	OfxPropertySetHandle /*outArgs*/)
{
	// get the render window and the time from the inArgs
	OfxTime time;
	OfxRectI renderWindow;
	OfxStatus status = kOfxStatOK;

	g.pPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
	g.pPropSuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

	// retrieve any instance data associated with this effect
	MyInstanceData *myData = getMyInstanceData(handle);

	// property handles and members of each image
	// in reality, we would put this in a struct as the C++ support layer does
	OfxPropertySetHandle sourceImg = NULL, outputImg = NULL, maskImg = NULL;
	int srcRowBytes, srcBitDepth, dstRowBytes, dstBitDepth, maskRowBytes = 0, maskBitDepth;
	bool srcIsAlpha, dstIsAlpha, maskIsAlpha = false;
	OfxRectI dstRect, srcRect, maskRect = { 0 };
	void *src, *dst, *mask = NULL;

	try {
		// get the source image
		sourceImg = ofxuGetImage(myData->sourceClip, time, srcRowBytes, srcBitDepth, srcIsAlpha, srcRect, src);
		if (sourceImg == NULL) throw OfxuNoImageException();

		// get the output image
		outputImg = ofxuGetImage(myData->outputClip, time, dstRowBytes, dstBitDepth, dstIsAlpha, dstRect, dst);
		if (outputImg == NULL) throw OfxuNoImageException();

		if (myData->isGeneralEffect) {
			// is the mask connected?
			if (ofxuIsClipConnected(handle, "Mask")) {
				maskImg = ofxuGetImage(myData->maskClip, time, maskRowBytes, maskBitDepth, maskIsAlpha, maskRect, mask);

				if (maskImg != NULL) {
					// and see that it is a single component
					if (!maskIsAlpha || maskBitDepth != srcBitDepth) {
						throw OfxuStatusException(kOfxStatErrImageFormat);
					}
				}
			}
		}

		// see if they have the same depths and bytes and all
		if (srcBitDepth != dstBitDepth || srcIsAlpha != dstIsAlpha) {
			throw OfxuStatusException(kOfxStatErrImageFormat);
		}

		// do the rendering
		if (!dstIsAlpha) {
			switch (dstBitDepth) {
#if 0
// int types not supported by debander
			case 8: {
				ProcessRGBA<OfxRGBAColourB, unsigned char, 255, 0> fred(handle,
					src, srcRect, srcRowBytes,
					dst, dstRect, dstRowBytes,
					mask, maskRect, maskRowBytes,
					renderWindow);
				fred.process(g.pThreadSuite);
				break;
			}

			case 16: {
				ProcessRGBA<OfxRGBAColourS, unsigned short, 65535, 0> fred(handle,
					src, srcRect, srcRowBytes,
					dst, dstRect, dstRowBytes,
					mask, maskRect, maskRowBytes,
					renderWindow);
				fred.process(g.pThreadSuite);
				break;
			}
#endif

			case 32: {
				ProcessRGBA<OfxRGBAColourF, float, 1, 1> fred(handle,
					src, srcRect, srcRowBytes,
					dst, dstRect, dstRowBytes,
					mask, maskRect, maskRowBytes,
					renderWindow);
				fred.process(g.pThreadSuite);
				break;
			}

			default:
				throw OfxuStatusException(kOfxStatErrImageFormat);
			}
		}
		else {
			switch (dstBitDepth) {
#if 0
// int types not supported by debander
			case 8: {
				ProcessAlpha<unsigned char, unsigned char, 255, 0> fred(handle,
					src, srcRect, srcRowBytes,
					dst, dstRect, dstRowBytes,
					mask, maskRect, maskRowBytes,
					renderWindow);
				fred.process(g.pThreadSuite);
				break;
			}

			case 16: {
				ProcessAlpha<unsigned short, unsigned short, 65535, 0> fred(handle,
					src, srcRect, srcRowBytes,
					dst, dstRect, dstRowBytes,
					mask, maskRect, maskRowBytes,
					renderWindow);
				fred.process(g.pThreadSuite);
				break;
			}
#endif

			case 32: {
				ProcessAlpha<float, float, 1, 1> fred(handle,
					src, srcRect, srcRowBytes,
					dst, dstRect, dstRowBytes,
					mask, maskRect, maskRowBytes,
					renderWindow);
				fred.process(g.pThreadSuite);
				break;
			}
			default:
				throw OfxuStatusException(kOfxStatErrImageFormat);
			}
		}
	}
	catch (OfxuNoImageException &ex) {
		// if we were interrupted, the failed fetch is fine, just return kOfxStatOK
		// otherwise, something wierd happened
		if (!g.pEffectSuite->abort(handle)) {
			status = kOfxStatFailed;
		}
	}
	catch (OfxuStatusException &ex) {
		status = ex.status();
	}

	// release the data pointers
	if (maskImg)
		g.pEffectSuite->clipReleaseImage(maskImg);
	if (sourceImg)
		g.pEffectSuite->clipReleaseImage(sourceImg);
	if (outputImg)
		g.pEffectSuite->clipReleaseImage(outputImg);

	return status;
}

// tells the host what region we are capable of filling
OfxStatus
getSpatialRoD(OfxImageEffectHandle  effect, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs)
{
	// retrieve any instance data associated with this effect
	MyInstanceData *myData = getMyInstanceData(effect);

	OfxTime time;
	g.pPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

	// my RoD is the same as my input's
	OfxRectD rod;
	g.pEffectSuite->clipGetRegionOfDefinition(myData->sourceClip, time, &rod);

	// note that the RoD is _not_ dependant on the Mask clip

	// set the rod in the out args
	g.pPropSuite->propSetDoubleN(outArgs, kOfxImageEffectPropRegionOfDefinition, 4, &rod.x1);

	return kOfxStatOK;
}

// tells the host how much of the input we need to fill the given window
OfxStatus
getSpatialRoI(OfxImageEffectHandle  effect, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs)
{
	// get the RoI the effect is interested in from inArgs
	OfxRectD roi;
	g.pPropSuite->propGetDoubleN(inArgs, kOfxImageEffectPropRegionOfInterest, 4, &roi.x1);

	// the input needed is the same as the output, so set that on the source clip
	g.pPropSuite->propSetDoubleN(outArgs, "OfxImageClipPropRoI_Source", 4, &roi.x1);

	// retrieve any instance data associated with this effect
	MyInstanceData *myData = getMyInstanceData(effect);

	// if a general effect, we need to know the mask as well
	if (myData->isGeneralEffect && ofxuIsClipConnected(effect, "Mask")) {
		g.pPropSuite->propSetDoubleN(outArgs, "OfxImageClipPropRoI_Mask", 4, &roi.x1);
	}
	return kOfxStatOK;
}

// Tells the host how many frames we can fill, only called in the general context.
// This is actually redundant as this is the default behaviour, but for illustrative
// purposes.
OfxStatus
getTemporalDomain(OfxImageEffectHandle  effect, OfxPropertySetHandle /*inArgs*/, OfxPropertySetHandle outArgs)
{
	MyInstanceData *myData = getMyInstanceData(effect);

	double sourceRange[2];

	// get the frame range of the source clip
	OfxPropertySetHandle props; g.pEffectSuite->clipGetPropertySet(myData->sourceClip, &props);
	g.pPropSuite->propGetDoubleN(props, kOfxImageEffectPropFrameRange, 2, sourceRange);

	// set it on the out args
	g.pPropSuite->propSetDoubleN(outArgs, kOfxImageEffectPropFrameRange, 2, sourceRange);

	return kOfxStatOK;
}

// Set our clip preferences 
static OfxStatus
getClipPreferences(OfxImageEffectHandle  effect, OfxPropertySetHandle /*inArgs*/, OfxPropertySetHandle outArgs)
{
	// retrieve any instance data associated with this effect
	MyInstanceData *myData = getMyInstanceData(effect);

	// get the component type and bit depth of our main input
	int  bitDepth;
	bool isRGBA;
	ofxuClipGetFormat(myData->sourceClip, bitDepth, isRGBA, true); // get the unmapped clip component

																   // get the strings used to label the various bit depths
	const char *bitDepthStr = bitDepth == 8 ? kOfxBitDepthByte : (bitDepth == 16 ? kOfxBitDepthShort : kOfxBitDepthFloat);
	const char *componentStr = isRGBA ? kOfxImageComponentRGBA : kOfxImageComponentAlpha;

	// set out output to be the same same as the input, component and bitdepth
	g.pPropSuite->propSetString(outArgs, "OfxImageClipPropComponents_Output", 0, componentStr);
	if (g.iHostSupportsMultipleBitDepths)
		g.pPropSuite->propSetString(outArgs, "OfxImageClipPropDepth_Output", 0, bitDepthStr);

	// if a general effect, we may have a mask input, check that for types
	if (myData->isGeneralEffect) {
		if (ofxuIsClipConnected(effect, "Mask")) {
			// set the mask input to be a single channel image of the same bitdepth as the source
			g.pPropSuite->propSetString(outArgs, "OfxImageClipPropComponents_Mask", 0, kOfxImageComponentAlpha);
			if (g.iHostSupportsMultipleBitDepths)
				g.pPropSuite->propSetString(outArgs, "OfxImageClipPropDepth_Mask", 0, bitDepthStr);
		}
	}

	return kOfxStatOK;
}

// function called when the instance has been changed by anything
static OfxStatus
instanceChanged(OfxImageEffectHandle  effect,
	OfxPropertySetHandle inArgs,
	OfxPropertySetHandle /*outArgs*/)
{
	// see why it changed
	char *changeReason;
	g.pPropSuite->propGetString(inArgs, kOfxPropChangeReason, 0, &changeReason);

	// we are only interested in user edits
	if (strcmp(changeReason, kOfxChangeUserEdited) != 0) return kOfxStatReplyDefault;

	// fetch the type of the object that changed
	char *typeChanged;
	g.pPropSuite->propGetString(inArgs, kOfxPropType, 0, &typeChanged);

	// was it a clip or a param?
	bool isClip = strcmp(typeChanged, kOfxTypeClip) == 0;
	bool isParam = strcmp(typeChanged, kOfxTypeParameter) == 0;

	// get the name of the thing that changed
	char *objChanged;
	g.pPropSuite->propGetString(inArgs, kOfxPropName, 0, &objChanged);

	// don't trap any others
	return kOfxStatReplyDefault;
}



// ===================================================== //
// MAIN PLUGIN/HOST INTERFACE

// function to set the host structure
static void
setHostFunc(OfxHost *hostStruct)
{
	g.pHost = hostStruct;
}

static OfxStatus
pluginMain(const char *action, const void *handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs)
{
#ifdef _DEBUG
	printf("pluginMain \"%s\" %p\n", action, (void*)handle);
#endif

	try {
		// cast to appropriate type
		OfxImageEffectHandle effect = (OfxImageEffectHandle)handle;

		if (strcmp(action, kOfxActionDescribe) == 0) {
			return describe(effect);
		}
		else if (strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
			return describeInContext(effect, inArgs);
		}
		else if (strcmp(action, kOfxActionLoad) == 0) {
			return onLoad();
		}
		else if (strcmp(action, kOfxActionUnload) == 0) {
			return onUnLoad();
		}
		else if (strcmp(action, kOfxActionCreateInstance) == 0) {
			return createInstance(effect);
		}
		else if (strcmp(action, kOfxActionDestroyInstance) == 0) {
			return destroyInstance(effect);
		}
		else if (strcmp(action, kOfxImageEffectActionIsIdentity) == 0) {
			return isIdentity(effect, inArgs, outArgs);
		}
		else if (strcmp(action, kOfxImageEffectActionRender) == 0) {
			return render(effect, inArgs, outArgs);
			//return RenderAction(effect, inArgs, outArgs);
		}
		else if (strcmp(action, kOfxImageEffectActionGetRegionOfDefinition) == 0) {
			return getSpatialRoD(effect, inArgs, outArgs);
		}
		else if (strcmp(action, kOfxImageEffectActionGetRegionsOfInterest) == 0) {
			return getSpatialRoI(effect, inArgs, outArgs);
		}
		else if (strcmp(action, kOfxImageEffectActionGetClipPreferences) == 0) {
			return getClipPreferences(effect, inArgs, outArgs);
		}
		else if (strcmp(action, kOfxActionInstanceChanged) == 0) {
			return instanceChanged(effect, inArgs, outArgs);
		}
		else if (strcmp(action, kOfxImageEffectActionGetTimeDomain) == 0) {
			return getTemporalDomain(effect, inArgs, outArgs);
		}
	}
	catch (std::bad_alloc) {
		// catch memory
		//std::cout << "OFX Plugin Memory error." << std::endl;
		return kOfxStatErrMemory;
	}
	catch (const std::exception& e) {
		// standard exceptions
		//std::cout << "OFX Plugin error: " << e.what() << std::endl;
		return kOfxStatErrUnknown;
	}
	catch (int err) {
		// ho hum, gone wrong somehow
		return err;
	}
	catch (...) {
		// everything else
		//std::cout << "OFX Plugin error" << std::endl;
		return kOfxStatErrUnknown;
	}

	// other actions to take the default value
	return kOfxStatReplyDefault;
}

static OfxPlugin debanderPlugin =
{
	kOfxImageEffectPluginApi,	// API
	1,							// API version
	OFX_PLUGIN_UID,				// Plugin UID
	1,							// Plugin version major
	0,							// Plugin version minor
	setHostFunc,				// Callback to tell us about host
	pluginMain					// Main entry point to our plugin.
};



// ===================================================== //
// BOOTSTRAPPER FUNCTIONS

EXPORT int
OfxGetNumberOfPlugins(void)
{
	return 1;
}

EXPORT OfxPlugin *
OfxGetPlugin(int nth)
{
	if (nth == 0)
		return &debanderPlugin;
	return 0;
}
