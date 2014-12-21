#include "stdafx.h"
#include "avisynth.h"

// copied from interface.cpp in avisynth 2.6 source code

/**********************************************************************/

// struct VideoInfo

// useful functions of the above
inline bool VideoInfo::HasVideo() const { return (width!=0); }
inline bool VideoInfo::HasAudio() const { return (audio_samples_per_second!=0); }
inline bool VideoInfo::IsRGB() const { return !!(pixel_type&CS_BGR); }
inline bool VideoInfo::IsRGB24() const { return (pixel_type&CS_BGR24)==CS_BGR24; } // Clear out additional properties
inline bool VideoInfo::IsRGB32() const { return (pixel_type & CS_BGR32) == CS_BGR32 ; }
inline bool VideoInfo::IsYUV() const { return !!(pixel_type&CS_YUV ); }
inline bool VideoInfo::IsYUY2() const { return (pixel_type & CS_YUY2) == CS_YUY2; }

inline bool VideoInfo::IsYV24()  const { return (pixel_type & CS_PLANAR_MASK) == (CS_YV24  & CS_PLANAR_FILTER); }
inline bool VideoInfo::IsYV16()  const { return (pixel_type & CS_PLANAR_MASK) == (CS_YV16  & CS_PLANAR_FILTER); }
inline bool VideoInfo::IsYV12()  const { return (pixel_type & CS_PLANAR_MASK) == (CS_YV12  & CS_PLANAR_FILTER); }
inline bool VideoInfo::IsY8()    const { return (pixel_type & CS_PLANAR_MASK) == (CS_Y8    & CS_PLANAR_FILTER); }

inline bool VideoInfo::IsYV411() const { return (pixel_type & CS_PLANAR_MASK) == (CS_YV411 & CS_PLANAR_FILTER); }
//bool VideoInfo::IsYUV9()  const { return (pixel_type & CS_PLANAR_MASK) == (CS_YUV9  & CS_PLANAR_FILTER); }

/* Baked ********************
inline bool VideoInfo::IsColorSpace(int c_space) const { return ((pixel_type & c_space) == c_space); }
   Baked ********************/
inline bool VideoInfo::IsColorSpace(int c_space) const {
  return IsPlanar() ? ((pixel_type & CS_PLANAR_MASK) == (c_space & CS_PLANAR_FILTER)) : ((pixel_type & c_space) == c_space);
}

inline bool VideoInfo::Is(int property) const { return ((pixel_type & property)==property ); }
inline bool VideoInfo::IsPlanar() const { return !!(pixel_type & CS_PLANAR); }
inline bool VideoInfo::IsFieldBased() const { return !!(image_type & IT_FIELDBASED); }
inline bool VideoInfo::IsParityKnown() const { return ((image_type & IT_FIELDBASED)&&(image_type & (IT_BFF|IT_TFF))); }
inline bool VideoInfo::IsBFF() const { return !!(image_type & IT_BFF); }
inline bool VideoInfo::IsTFF() const { return !!(image_type & IT_TFF); }

/* Baked ********************
inline bool VideoInfo::IsVPlaneFirst() const {return ((pixel_type & CS_YV12) == CS_YV12); }  // Don't use this
inline int VideoInfo::BytesFromPixels(int pixels) const { return pixels * (BitsPerPixel()>>3); }   // Will not work on planar images, but will return only luma planes
inline int VideoInfo::RowSize() const { return BytesFromPixels(width); }  // Also only returns first plane on planar images
inline int VideoInfo::BMPSize() const { if (IsPlanar()) {int p = height * ((RowSize()+3) & ~3); p+=p>>1; return p;  } return height * ((RowSize()+3) & ~3); }
inline __int64 VideoInfo::AudioSamplesFromFrames(__int64 frames) const { return (fps_numerator && HasVideo()) ? ((__int64)(frames) * audio_samples_per_second * fps_denominator / fps_numerator) : 0; }
   Baked ********************/
inline __int64 VideoInfo::AudioSamplesFromFrames(int frames) const { return (fps_numerator && HasVideo()) ? ((__int64)(frames) * audio_samples_per_second * fps_denominator / fps_numerator) : 0; }
inline int VideoInfo::FramesFromAudioSamples(__int64 samples) const { return (fps_denominator && HasAudio()) ? (int)((samples * (__int64)fps_numerator)/((__int64)fps_denominator * (__int64)audio_samples_per_second)) : 0; }
inline __int64 VideoInfo::AudioSamplesFromBytes(__int64 bytes) const { return HasAudio() ? bytes / BytesPerAudioSample() : 0; }
inline __int64 VideoInfo::BytesFromAudioSamples(__int64 samples) const { return samples * BytesPerAudioSample(); }
inline int VideoInfo::AudioChannels() const { return HasAudio() ? nchannels : 0; }
inline int VideoInfo::SampleType() const{ return sample_type;}
inline bool VideoInfo::IsSampleType(int testtype) const{ return !!(sample_type&testtype);}
inline int VideoInfo::SamplesPerSecond() const { return audio_samples_per_second; }
inline int VideoInfo::BytesPerAudioSample() const { return nchannels*BytesPerChannelSample();}
inline void VideoInfo::SetFieldBased(bool isfieldbased)  { if (isfieldbased) image_type|=IT_FIELDBASED; else  image_type&=~IT_FIELDBASED; }
inline void VideoInfo::Set(int property)  { image_type|=property; }
inline void VideoInfo::Clear(int property)  { image_type&=~property; }

/* Baked ********************
inline int VideoInfo::BitsPerPixel() const {
  switch (pixel_type) {
    case CS_BGR24:
      return 24;
    case CS_BGR32:
      return 32;
    case CS_YUY2:
      return 16;
    case CS_YV12:
    case CS_I420:
      return 12;
    default:
      return 0;
  }
}
   Baked ********************/

inline int VideoInfo::BytesPerChannelSample() const {
  switch (sample_type) {
  case SAMPLE_INT8:
    return sizeof(unsigned char);
  case SAMPLE_INT16:
    return sizeof(signed short);
  case SAMPLE_INT24:
    return 3;
  case SAMPLE_INT32:
    return sizeof(signed int);
  case SAMPLE_FLOAT:
    return sizeof(SFLOAT);
  default:
    _ASSERTE("Sample type not recognized!");
    return 0;
  }
}

inline bool VideoInfo::IsVPlaneFirst() const {
  return !IsY8() && IsPlanar() && (pixel_type & (CS_VPlaneFirst | CS_UPlaneFirst)) == CS_VPlaneFirst;   // Shouldn't use this
}

inline int VideoInfo::BytesFromPixels(int pixels) const {
  return !IsY8() && IsPlanar() ? pixels << ((pixel_type>>CS_Shift_Sample_Bits) & 3) : pixels * (BitsPerPixel()>>3);   // For planar images, will return luma plane
}

inline int VideoInfo::RowSize(int plane) const {
  const int rowsize = BytesFromPixels(width);

  switch (plane) {
    case PLANAR_U: case PLANAR_V:
      return (!IsY8() && IsPlanar()) ? rowsize>>GetPlaneWidthSubsampling(plane) : 0;

    case PLANAR_U_ALIGNED: case PLANAR_V_ALIGNED:
      return (!IsY8() && IsPlanar()) ? ((rowsize>>GetPlaneWidthSubsampling(plane))+FRAME_ALIGN-1)&(~(FRAME_ALIGN-1)) : 0; // Aligned rowsize

    case PLANAR_Y_ALIGNED:
      return (rowsize+FRAME_ALIGN-1)&(~(FRAME_ALIGN-1)); // Aligned rowsize
  }
  return rowsize;
}

inline int VideoInfo::BMPSize() const {
  if (!IsY8() && IsPlanar()) {
    // Y plane
    const int Ybytes  = ((RowSize(PLANAR_Y)+3) & ~3) * height;
    const int UVbytes = ((RowSize(PLANAR_U)+3) & ~3) * height >> GetPlaneHeightSubsampling(PLANAR_U);
    return Ybytes + UVbytes*2;
  }
  return height * ((RowSize()+3) & ~3);
}

inline int VideoInfo::GetPlaneWidthSubsampling(int plane) const {  // Subsampling in bitshifts!
  if (plane == PLANAR_Y)  // No subsampling
    return 0;
  if (IsY8())
    throw AvisynthError("Filter error: GetPlaneWidthSubsampling not available on Y8 pixel type.");
  if (plane == PLANAR_U || plane == PLANAR_V) {
    if (IsYUY2())
      return 1;
    else if (IsPlanar())
      return ((pixel_type>>CS_Shift_Sub_Width)+1) & 3;
    else
      throw AvisynthError("Filter error: GetPlaneWidthSubsampling called with unsupported pixel type.");
  }
  throw AvisynthError("Filter error: GetPlaneWidthSubsampling called with unsupported plane.");
}

inline int VideoInfo::GetPlaneHeightSubsampling(int plane) const {  // Subsampling in bitshifts!
  if (plane == PLANAR_Y)  // No subsampling
    return 0;
  if (IsY8())
    throw AvisynthError("Filter error: GetPlaneHeightSubsampling not available on Y8 pixel type.");
  if (plane == PLANAR_U || plane == PLANAR_V) {
    if (IsYUY2())
      return 0;
    else if (IsPlanar())
      return ((pixel_type>>CS_Shift_Sub_Height)+1) & 3;
    else
      throw AvisynthError("Filter error: GetPlaneHeightSubsampling called with unsupported pixel type.");
  }
  throw AvisynthError("Filter error: GetPlaneHeightSubsampling called with supported plane.");
}

inline int VideoInfo::BitsPerPixel() const {
// Lookup Interleaved, calculate PLANAR's
    switch (pixel_type) {
      case CS_BGR24:
        return 24;
      case CS_BGR32:
        return 32;
      case CS_YUY2:
        return 16;
      case CS_Y8:
        return 8;
//    case CS_Y16:
//      return 16;
//    case CS_Y32:
//      return 32;
    }
    if (IsPlanar()) {
      const int S = IsYUV() ? GetPlaneWidthSubsampling(PLANAR_U) + GetPlaneHeightSubsampling(PLANAR_U) : 0;
      return ( ((1<<S)+2) * (8<<((pixel_type>>CS_Shift_Sample_Bits) & 3)) ) >> S;
    }
    return 0;
}

// useful mutator
inline void VideoInfo::SetFPS(unsigned numerator, unsigned denominator) {
  if ((numerator == 0) || (denominator == 0)) {
    fps_numerator = 0;
    fps_denominator = 1;
  }
  else {
    unsigned x=numerator, y=denominator;
    while (y) {   // find gcd
      unsigned t = x%y; x = y; y = t;
    }
    fps_numerator = numerator/x;
    fps_denominator = denominator/x;
  }
}

// Range protected multiply-divide of FPS
inline void VideoInfo::MulDivFPS(unsigned multiplier, unsigned divisor) {
  unsigned __int64 numerator   = UInt32x32To64(fps_numerator,   multiplier);
  unsigned __int64 denominator = UInt32x32To64(fps_denominator, divisor);

  unsigned __int64 x=numerator, y=denominator;
  while (y) {   // find gcd
    unsigned __int64 t = x%y; x = y; y = t;
  }
  numerator   /= x; // normalize
  denominator /= x;

  unsigned __int64 temp = numerator | denominator; // Just looking top bit
  unsigned u = 0;
  while (temp & 0xffffffff80000000) { // or perhaps > 16777216*2
    temp = Int64ShrlMod32(temp, 1);
    u++;
  }
  if (u) { // Scale to fit
    const unsigned round = 1 << (u-1);
    SetFPS( (unsigned)Int64ShrlMod32(numerator   + round, u),
            (unsigned)Int64ShrlMod32(denominator + round, u) );
  }
  else {
    fps_numerator   = (unsigned)numerator;
    fps_denominator = (unsigned)denominator;
  }
}

// Test for same colorspace
inline bool VideoInfo::IsSameColorspace(const VideoInfo& vi) const {
  if (vi.pixel_type == pixel_type) return TRUE;
  if (IsYV12() && vi.IsYV12()) return TRUE;
  return FALSE;
}

// end struct VideoInfo

/**********************************************************************/
