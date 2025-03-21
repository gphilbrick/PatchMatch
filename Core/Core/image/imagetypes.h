#ifndef CORE_IMAGE_IMAGETYPES
#define CORE_IMAGE_IMAGETYPES

namespace core {
	
template<typename T >
class TwoDArray;

class Vector3;
class Vector4;

/// Per-channel values in [0,1]
using ImageRGB = TwoDArray< Vector3 >;
/// Per-channel values in [0,1]
using ImageRGBA = TwoDArray< Vector4 >;
using ImageBinary = TwoDArray< bool >;
using ImageScalar = TwoDArray< double >;
	
} // core
#endif // #include