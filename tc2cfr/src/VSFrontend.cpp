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

#include "Backend.h"

#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct{
  VSNodeRef *node;
  const VSVideoInfo *vi;
  FrameMap_t **frameMap;
  int mapLength;
  int fpsNum,fpsDen;
}tc2cfr_t;

static void VS_CC VStc2cfrInit(VSMap *in,VSMap *out,void **instanceData,VSNode *node,VSCore *core,const VSAPI *vsapi){
  tc2cfr_t *tmp=(tc2cfr_t *)*instanceData;
  VSVideoInfo *vi=(VSVideoInfo *)malloc(sizeof(VSVideoInfo));
  vi->flags=tmp->vi->flags;
  vi->format=tmp->vi->format;
  vi->fpsNum=tmp->fpsNum;
  vi->fpsDen=tmp->fpsDen;
  vi->height=tmp->vi->height;
  vi->width=tmp->vi->width;
  vi->numFrames=tmp->frameMap[tmp->mapLength-1]->transFrame;
  vi->format=vsapi->registerFormat(tmp->vi->format->colorFamily,tmp->vi->format->sampleType,tmp->vi->format->bitsPerSample,tmp->vi->format->subSamplingW,tmp->vi->format->subSamplingH,core);
  vsapi->setVideoInfo(vi,1,node);
}

static const VSFrameRef *VS_CC VStc2cfrGetFrame(int n,int activationReason,void **instanceData,void **frameData,VSFrameContext *frameCtx,VSCore *core,const VSAPI *vsapi){
  tc2cfr_t *data=(tc2cfr_t *)*instanceData;
  int origFrame=getOrigFrame(data->frameMap,data->mapLength,n);
  if(activationReason==arInitial){
    vsapi->requestFrameFilter(origFrame,data->node,frameCtx);
    return NULL;
  }
  if(activationReason!=arAllFramesReady)
    return NULL;
  return vsapi->getFrameFilter(origFrame,data->node,frameCtx);
}

static void VS_CC VStc2cfrFree(void *instanceData,VSCore *core,const VSAPI *vsapi){
  tc2cfr_t *tmp=(tc2cfr_t *)instanceData;
  if(tmp==NULL)
    return;
  if(tmp->node!=NULL){
    vsapi->freeNode(tmp->node);
    tmp->node=NULL;
  }
  if(tmp->frameMap!=NULL){
    for(int i=0;i<tmp->mapLength;i++)
      if(tmp->frameMap[i]!=NULL){
        free(tmp->frameMap[i]);
        tmp->frameMap[i]=NULL;
      }
    free(tmp->frameMap);
    tmp->frameMap=NULL;
  }
  return;
}

#define FAIL_IF_ERROR(cond,msg){\
  if(cond){\
    vsapi->setError(out,msg);\
    VStc2cfrFree(data,core,vsapi);\
    return;\
  }\
}

void VS_CC VStc2cfrCreate(const VSMap *in,VSMap *out,void *userData,VSCore *core,const VSAPI *vsapi){
  tc2cfr_t *data=(tc2cfr_t *)malloc(sizeof(tc2cfr_t));
  FAIL_IF_ERROR(data==NULL,"failed to allocate memory when init");
  data->node=NULL;
  data->frameMap=NULL;

  int err=0;
  data->node=vsapi->propGetNode(in,"clip",0,&err);
  FAIL_IF_ERROR(err!=0,"lack input clip");
  data->vi=(const VSVideoInfo *)malloc(sizeof(VSVideoInfo));
  data->vi=vsapi->getVideoInfo(data->node);

  const char *timecodeFilePath=vsapi->propGetData(in,"timecode",0,&err);
  FAIL_IF_ERROR(err!=0,"lack timecode file path");
  
  data->fpsNum=(int)vsapi->propGetInt(in,"fpsNum",0,&err);
  FAIL_IF_ERROR(err!=0,"lack of output fpsNum");

  data->fpsDen=(int)vsapi->propGetInt(in,"fpsDen",0,&err);
  if(err!=0)
    data->fpsDen=data->vi->fpsDen;

  char *errmsg=NULL;
  data->frameMap=genFrameMap(timecodeFilePath,data->vi->numFrames,data->fpsNum,data->fpsDen,&(data->mapLength),&errmsg);
  FAIL_IF_ERROR(errmsg!=NULL,errmsg);

  vsapi->createFilter(in,out,"readtcv1",VStc2cfrInit,VStc2cfrGetFrame,VStc2cfrFree,fmParallel,0,data,core);
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc,VSRegisterFunction registerFunc,VSPlugin *plugin){
  configFunc("org.vapoursynth.tc2cfr","tc2cfr","timecode to cfr convert",VAPOURSYNTH_API_VERSION,1,plugin);
  registerFunc("readtcv1","clip:clip;timecode:data;fpsNum:int;fpsDen:int:opt;",VStc2cfrCreate,0,plugin);
}