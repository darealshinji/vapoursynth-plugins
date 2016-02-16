/*
  Copyright (c) 2013 gnaggnoyil All rights reserved.
  
  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:
  
    1.Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    2.Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
  OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#if defined(_WIN32) && !defined(_WIN64)  //Avisynth plugin loader should only be compiled under 32-bit Windows platform
#include <Windows.h>

#include "Backend.h"

#include "avisynth.h"

class AVStc2cfr:public GenericVideoFilter{
  private:
    FrameMap_t **frameMap;
    int mapLength;
    //VideoInfo orig_vi;
  public:
    AVStc2cfr(PClip _child,const char *timecode,int fpsNum,int fpsDen,IScriptEnvironment *env);
    ~AVStc2cfr();
    PVideoFrame __stdcall GetFrame(int n,IScriptEnvironment *env);
};

AVStc2cfr::AVStc2cfr(PClip _child,const char *timecode,int fpsNum,int fpsDen,IScriptEnvironment *env):GenericVideoFilter(_child){
  char *errmsg=NULL;
  frameMap=genFrameMap(timecode,vi.num_frames,fpsNum,fpsDen,&mapLength,&errmsg);
  if(errmsg!=NULL){
    env->ThrowError(errmsg);
    return ;
  }
  vi.fps_numerator=fpsNum;
  vi.fps_denominator=fpsDen;
  vi.num_frames=frameMap[mapLength-1]->transFrame;
}

AVStc2cfr::~AVStc2cfr(){
  if(frameMap!=NULL){
    for(int i=0;i<mapLength;i++)
      if(frameMap[i]!=NULL)
        free(frameMap[i]);
    free(frameMap);
  }
}

PVideoFrame __stdcall AVStc2cfr::GetFrame(int n,IScriptEnvironment *env){
  int origFrame=getOrigFrame(frameMap,mapLength,n);
  return child->GetFrame(origFrame,env);
}

AVSValue __cdecl AVStc2cfrCreate(AVSValue args,void *userData,IScriptEnvironment *env){
  if(!args[1].Defined()){
    env->ThrowError("lack timecode file path");
    return NULL;
  }
  if(!args[2].Defined()){
    env->ThrowError("lack of output fpsNum");
    return NULL;
  }

  PClip tmp=args[0].AsClip();
  return new AVStc2cfr(tmp,args[1].AsString(),args[2].AsInt(),args[3].AsInt(tmp->GetVideoInfo().fps_denominator),env);
}

extern "C" __declspec(dllexport) const char * __stdcall AvisynthPluginInit2(IScriptEnvironment *env){
  env->AddFunction("readtcv1","c[timecode]s[fpsNum]i[fpsDen]i",AVStc2cfrCreate,0);
  return "timecode to cfr convert";
}
#endif //#if defined(_WIN32) && !defined(_WIN64)