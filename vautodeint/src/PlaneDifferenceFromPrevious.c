/*
  PlaneDifferenceFromPrevious.cpp: Copyright (C) 2013  Acfun (www dot acfun dot tv)
  
  Author: Li Yonggang (gnaggnoyil at gmail dot com)
  
  This file is part of the VAutoDeint project.
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 3 of
  the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with the author; if not, write to the Free Software Foundation,
  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct{
  VSNodeRef *node;
  const VSVideoInfo *vi;
  const char *prop;
  int plane;
}PlaneDifferenceFromPrevious_t;

static void VS_CC PlaneDifferenceFromPreviousFree(void *instanceData,VSCore *core,const VSAPI *vsapi){
  PlaneDifferenceFromPrevious_t *tmp=(PlaneDifferenceFromPrevious_t *)instanceData;
  if(tmp==NULL)
    return;
  if(tmp->node==NULL){
    vsapi->freeNode(tmp->node);
    tmp->node=NULL;
  }
  free(tmp);
  return;
}

static void VS_CC PlaneDifferenceFromPreviousInit(VSMap *in,VSMap *out,void **instanceData,VSNode *node,VSCore *core,const VSAPI *vsapi){
  PlaneDifferenceFromPrevious_t *tmp=(PlaneDifferenceFromPrevious_t *)*instanceData;
  VSVideoInfo vi=*(tmp->vi);
  vi.height=1;
  vi.width=1;
  vsapi->setVideoInfo(&vi,1,node);
  return;
}

#define FAIL_IF_ERROR(cond,msg){\
  if(cond){\
    vsapi->setError(out,msg);\
    PlaneDifferenceFromPreviousFree(data,core,vsapi);\
    return;\
  }\
}

static const VSFrameRef *VS_CC PlaneDifferenceFromPreviousGetFrame(int n,int activationReason,void **instanceData,void **frameData,VSFrameContext *frameCtx,VSCore *core,const VSAPI *vsapi){
  PlaneDifferenceFromPrevious_t *diffprop=(PlaneDifferenceFromPrevious_t *)*instanceData;
  const int nminus1=(n==0)?0:(n-1);
  if(activationReason==arInitial){
    vsapi->requestFrameFilter(nminus1,diffprop->node,frameCtx);
    vsapi->requestFrameFilter(n,diffprop->node,frameCtx);
    return NULL;
  }
  if(activationReason!=arAllFramesReady)
    return NULL;

  const VSFrameRef *now=vsapi->getFrameFilter(n,diffprop->node,frameCtx);
  const VSFormat *fi=vsapi->getFrameFormat(now);
  VSFrameRef *dst=vsapi->newVideoFrame(fi,1,1,now,core);
  uint8_t *dstp;
  for(int i=0;i<fi->numPlanes;i++){
    dstp=vsapi->getWritePtr(dst,i);
    dstp[0]=0;
  }

  if(n==0){
    vsapi->propSetFloat(vsapi->getFramePropsRW(dst),diffprop->prop,0.000000,paReplace);
    vsapi->freeFrame(now);
    return dst;
  }

  const VSFrameRef *prev=vsapi->getFrameFilter(nminus1,diffprop->node,frameCtx);
  const int width=vsapi->getFrameWidth(now,diffprop->plane);
  const int height=vsapi->getFrameHeight(now,diffprop->plane);
  const uint8_t *srcnow=vsapi->getReadPtr(now,diffprop->plane);
  const uint8_t *srcprev=vsapi->getReadPtr(prev,diffprop->plane);
  int srcStride=vsapi->getStride(now,diffprop->plane);

  int64_t acc=0;
  int y,x;
  for(y=0;y<height;y++){
    for(x=0;x<width;x++)
      acc+=abs(srcnow[x]-srcprev[x]);
    srcnow+=srcStride;
    srcprev+=srcStride;
  }

  vsapi->propSetFloat(vsapi->getFramePropsRW(dst),diffprop->prop,acc/((double)width*height),paReplace);
  vsapi->freeFrame(prev);
  vsapi->freeFrame(now);
  return dst;
}

void VS_CC PlaneDifferenceFromPreviousCreate(const VSMap *in,VSMap *out,void *userData,VSCore *core,const VSAPI *vsapi){
  PlaneDifferenceFromPrevious_t *data=(PlaneDifferenceFromPrevious_t *)malloc(sizeof(PlaneDifferenceFromPrevious_t));
  FAIL_IF_ERROR(data==NULL,"failed to allocate handler when init");

  data->node=vsapi->propGetNode(in,"clip",0,0);
  data->vi=vsapi->getVideoInfo(data->node);

  FAIL_IF_ERROR(!isConstantFormat(data->vi),"Clip format must be constant");
  FAIL_IF_ERROR(data->vi->format->sampleType!=stInteger,"float samples unsupported");
  FAIL_IF_ERROR((data->vi->format->colorFamily!=cmGray)&&(data->vi->format->colorFamily!=cmYUV),"need a luma plane");
  FAIL_IF_ERROR(data->vi->format->bitsPerSample!=8,"only 8-bit integer pixel value is supported");

  data->plane=(int)vsapi->propGetInt(in,"plane",0,0);
  FAIL_IF_ERROR((data->plane<0)||(data->plane>data->vi->format->numPlanes),"invalid plane specified");

  int err;

  data->prop=vsapi->propGetData(in,"prop",0,&err);
  if(err!=0)
    data->prop="PlaneDifference";

  vsapi->createFilter(in,out,"PlaneDifferenceFromPrevious",PlaneDifferenceFromPreviousInit,PlaneDifferenceFromPreviousGetFrame,PlaneDifferenceFromPreviousFree,fmParallel,0,data,core);
  return;
}