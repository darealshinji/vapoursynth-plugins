
enum {
   PLANAR_Y=1<<0,
   PLANAR_U=1<<1,
   PLANAR_V=1<<2,
   PLANAR_ALIGNED=1<<3,
   PLANAR_Y_ALIGNED=PLANAR_Y|PLANAR_ALIGNED,
   PLANAR_U_ALIGNED=PLANAR_U|PLANAR_ALIGNED,
   PLANAR_V_ALIGNED=PLANAR_V|PLANAR_ALIGNED,
   PLANAR_A=1<<4,
   PLANAR_R=1<<5,
   PLANAR_G=1<<6,
   PLANAR_B=1<<7,
   PLANAR_A_ALIGNED=PLANAR_A|PLANAR_ALIGNED,
   PLANAR_R_ALIGNED=PLANAR_R|PLANAR_ALIGNED,
   PLANAR_G_ALIGNED=PLANAR_G|PLANAR_ALIGNED,
   PLANAR_B_ALIGNED=PLANAR_B|PLANAR_ALIGNED,
  };


struct VideoInfo {
  int width, height;    // width=0 means no video
  unsigned fps_numerator, fps_denominator;
  int num_frames;
  // This is more extensible than previous versions. More properties can be added seeminglesly.

  // Colorspace properties.
/*
7<<0  Planar Width Subsampling bits
      Use (X+1) & 3 for GetPlaneWidthSubsampling
        000 => 1        YV12, YV16
        001 => 2        YV411, YUV9
        010 => reserved
        011 => 0        YV24
        1xx => reserved

1<<3  VPlaneFirst YV12, YV16, YV24, YV411, YUV9
1<<4  UPlaneFirst I420

7<<8  Planar Height Subsampling bits
      Use ((X>>8)+1) & 3 for GetPlaneHeightSubsampling
        000 => 1        YV12
        001 => 2        YUV9
        010 => reserved
        011 => 0        YV16, YV24, YV411
        1xx => reserved

7<<16 Sample resolution bits
        000 => 8
        001 => 16
        010 => 32
        011 => reserved
        1xx => reserved

Planar match mask  1111.0000.0000.0111.0000.0111.0000.0111
Planar signature   10xx.0000.0000.00xx.0000.00xx.00xx.00xx
Planar filter mask 1111.1111.1111.1111.1111.1111.1100.1111
*/
  enum {
    CS_BGR = 1<<28,
    CS_YUV = 1<<29,
    CS_INTERLEAVED = 1<<30,
    CS_PLANAR = 1<<31,

    CS_Shift_Sub_Width   =  0,
    CS_Shift_Sub_Height  =  8,
    CS_Shift_Sample_Bits = 16,

    CS_Sub_Width_Mask    = 7 << CS_Shift_Sub_Width,
    CS_Sub_Width_1       = 3 << CS_Shift_Sub_Width, // YV24
    CS_Sub_Width_2       = 0 << CS_Shift_Sub_Width, // YV12, I420, YV16
    CS_Sub_Width_4       = 1 << CS_Shift_Sub_Width, // YUV9, YV411

    CS_VPlaneFirst       = 1 << 3, // YV12, YV16, YV24, YV411, YUV9
    CS_UPlaneFirst       = 1 << 4, // I420

    CS_Sub_Height_Mask   = 7 << CS_Shift_Sub_Height,
    CS_Sub_Height_1      = 3 << CS_Shift_Sub_Height, // YV16, YV24, YV411
    CS_Sub_Height_2      = 0 << CS_Shift_Sub_Height, // YV12, I420
    CS_Sub_Height_4      = 1 << CS_Shift_Sub_Height, // YUV9

    CS_Sample_Bits_Mask  = 7 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_8     = 0 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_16    = 1 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_32    = 2 << CS_Shift_Sample_Bits,

    CS_PLANAR_MASK       = CS_PLANAR | CS_INTERLEAVED | CS_YUV | CS_BGR | CS_Sample_Bits_Mask | CS_Sub_Height_Mask | CS_Sub_Width_Mask,
    CS_PLANAR_FILTER     = ~( CS_VPlaneFirst | CS_UPlaneFirst ),

  // Specific colorformats
    CS_UNKNOWN = 0,
    CS_BGR24 = 1<<0 | CS_BGR | CS_INTERLEAVED,
    CS_BGR32 = 1<<1 | CS_BGR | CS_INTERLEAVED,
    CS_YUY2  = 1<<2 | CS_YUV | CS_INTERLEAVED,
//  CS_YV12  = 1<<3  Reserved
//  CS_I420  = 1<<4  Reserved
    CS_RAW32 = 1<<5 | CS_INTERLEAVED,

//  YV12 must be 0xA000008 2.5 Baked API will see all new planar as YV12
//  I420 must be 0xA000010

    CS_YV24  = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_1,  // YUV 4:4:4 planar
    CS_YV16  = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_2,  // YUV 4:2:2 planar
    CS_YV12  = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_VPlaneFirst | CS_Sub_Height_2 | CS_Sub_Width_2,  // y-v-u, 4:2:0 planar
    CS_I420  = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_UPlaneFirst | CS_Sub_Height_2 | CS_Sub_Width_2,  // y-u-v, 4:2:0 planar
    CS_IYUV  = CS_I420,
    CS_YUV9  = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_VPlaneFirst | CS_Sub_Height_4 | CS_Sub_Width_4,  // YUV 4:1:0 planar
    CS_YV411 = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_4,  // YUV 4:1:1 planar

    CS_Y8    = CS_PLANAR | CS_INTERLEAVED | CS_YUV | CS_Sample_Bits_8,                                     // Y   4:0:0 planar
/*
    CS_YV48  = CS_PLANAR | CS_YUV | CS_Sample_Bits_16 | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_1, // YUV 4:4:4 16bit samples
    CS_Y16   = CS_PLANAR | CS_INTERLEAVED | CS_YUV | CS_Sample_Bits_16,                                    // Y   4:0:0 16bit samples

    CS_YV96  = CS_PLANAR | CS_YUV | CS_Sample_Bits_32 | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_1, // YUV 4:4:4 32bit samples
    CS_Y32   = CS_PLANAR | CS_INTERLEAVED | CS_YUV | CS_Sample_Bits_32,                                    // Y   4:0:0 32bit samples

    CS_PRGB  = CS_PLANAR | CS_RGB | CS_Sample_Bits_8,                                                      // Planar RGB
    CS_RGB48 = CS_PLANAR | CS_RGB | CS_Sample_Bits_16,                                                     // Planar RGB 16bit samples
    CS_RGB96 = CS_PLANAR | CS_RGB | CS_Sample_Bits_32,                                                     // Planar RGB 32bit samples
*/
  };

  int pixel_type;                // changed to int as of 2.5


  int audio_samples_per_second;   // 0 means no audio
  int sample_type;                // as of 2.5
  __int64 num_audio_samples;      // changed as of 2.5
  int nchannels;                  // as of 2.5

  // Imagetype properties

  int image_type;

  enum {
    IT_BFF = 1<<0,
    IT_TFF = 1<<1,
    IT_FIELDBASED = 1<<2
  };

  // Chroma placement bits 20 -> 23  ::FIXME:: Really want a Class to support this
  enum {
    CS_UNKNOWN_CHROMA_PLACEMENT = 0 << 20,
    CS_MPEG1_CHROMA_PLACEMENT   = 1 << 20,
    CS_MPEG2_CHROMA_PLACEMENT   = 2 << 20,
    CS_YUY2_CHROMA_PLACEMENT    = 3 << 20,
    CS_TOPLEFT_CHROMA_PLACEMENT = 4 << 20
  };


  // useful functions of the above
  bool HasVideo() const;
  bool HasAudio() const;
  bool IsRGB() const;
  bool IsRGB24() const;
  bool IsRGB32() const;
  bool IsYUV() const;
  bool IsYUY2() const;

  bool IsYV24()  const;
  bool IsYV16()  const;
  bool IsYV12()  const;
  bool IsYV411() const;
//bool IsYUV9()  const;
  bool IsY8()    const;

  bool IsColorSpace(int c_space) const;

  bool Is(int property) const;
  bool IsPlanar() const;
  bool IsFieldBased() const;
  bool IsParityKnown() const;
  bool IsBFF() const;
  bool IsTFF() const;

  bool IsVPlaneFirst() const;  // Don't use this
  int BytesFromPixels(int pixels) const;   // Will not work on planar images, but will return only luma planes
  int RowSize(int plane=0) const;
  int BMPSize() const;

  __int64 AudioSamplesFromFrames(int frames) const;
  int FramesFromAudioSamples(__int64 samples) const;
  __int64 AudioSamplesFromBytes(__int64 bytes) const;
  __int64 BytesFromAudioSamples(__int64 samples) const;
  int AudioChannels() const;
  int SampleType() const;
  bool IsSampleType(int testtype) const;
  int SamplesPerSecond() const;
  int BytesPerAudioSample() const;
  void SetFieldBased(bool isfieldbased);
  void Set(int property);
  void Clear(int property);

  int GetPlaneWidthSubsampling(int plane) const;   // Subsampling in bitshifts!
  int GetPlaneHeightSubsampling(int plane) const;   // Subsampling in bitshifts!
  int BitsPerPixel() const;

  int BytesPerChannelSample() const;

  // useful mutator
  void SetFPS(unsigned numerator, unsigned denominator);

  // Range protected multiply-divide of FPS
  void MulDivFPS(unsigned multiplier, unsigned divisor);

  // Test for same colorspace
  bool IsSameColorspace(const VideoInfo& vi) const;

}; // endstruct VideoInfo
