#ifndef XCODEC_GLOBAL_H
#define XCODEC_GLOBAL_H

#ifdef _WIN32
#ifdef XCodec_STATIC
#define XCODEC_EXPORT
#else
#ifdef XCodec_EXPORTS
#define XCODEC_EXPORT __declspec(dllexport)
#else
#define XCODEC_EXPORT __declspec(dllimport)
#endif
#endif
#else
#define XCODEC_EXPORT
#endif


#endif // XCODEC_GLOBAL_H
