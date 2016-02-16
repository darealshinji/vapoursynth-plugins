/*
  IsCombedTIVTCport.cpp: Copyright (C) 2013  Acfun (www dot acfun dot tv)
  
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

#define CTHRESH_DEFAULT 9
#define MI_DEFAULT 80
#define CHROMA_DEFAULT 0
#define BLOCKX_DEFAULT 16
#define BLOCKY_DEFAULT 16
#define METRIC_DEFAULT 0
#define OPT_DEFAULT 0

#define PIXEL_COMBED 1
#define PIXEL_UNCOMBED 0

#define lowbit(x) (x)&(-(x))

static int bitNum(int num){
  if(num==128)
    return 7;
  if(num<128){
    if(num==16)
      return 4;
    if(num<16){
      if(num==8)
        return 3;
      if(num<8)
        return 2;
    }
    else{
      if(num==64)
        return 6;
      else
        return 5;
    }
  }
  else{
    if(num==1024)
      return 10;
    if(num<1024){
      if(num==512)
        return 9;
      else
        return 8;
    }
    else
      return 11;
  }
  return -1;
}

typedef struct{
  VSNodeRef *node;
  const VSVideoInfo *vi;
  int cthresh,MI,chroma,blockx,blocky,metric,opt;
}IsCombedTIVTC_parameter_t;

static void VS_CC IsCombedTIVTCFree(void *instanceData,VSCore *core,const VSAPI *vsapi){
  IsCombedTIVTC_parameter_t *tmp=(IsCombedTIVTC_parameter_t *)instanceData;
  if(tmp==NULL){
    return;
  }
  if(tmp->node==NULL){
    vsapi->freeNode(tmp->node);
    tmp->node=NULL;
  }
  free(tmp);
  return;
}

static void VS_CC IsCombedTIVTCInit(VSMap *in,VSMap *out,void **instanceData,VSNode *node,VSCore *core,const VSAPI *vsapi){
  IsCombedTIVTC_parameter_t *tmp=(IsCombedTIVTC_parameter_t *)*instanceData;
  VSVideoInfo vi=*(tmp->vi);
  vi.height=1;
  vi.width=1;
  vsapi->setVideoInfo(&vi,1,node);
  return;
}

typedef char (VS_CC *metricFuncType)(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,int cthresh);
metricFuncType *metricFuncArray;

static char VS_CC metricWay1(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,int cthresh){
  int d1=c-b,d2=c-d;
  if((d1>cthresh)&&(d2>cthresh)||(d1<-cthresh)&&(d2<-cthresh))
    if(abs(a+(c<<2)+e+3*(b+d))>cthresh*6)
      return PIXEL_COMBED;
  return PIXEL_UNCOMBED;
}

static char VS_CC metricWay2(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,int cthresh){
  if((c-b)*(c-d)>cthresh*cthresh)
    return PIXEL_COMBED;
  return PIXEL_UNCOMBED;
}

#define FAIL_IF_ERROR(cond,msg){\
  if(cond){\
    vsapi->setError(out,msg);\
    IsCombedTIVTCFree(data,core,vsapi);\
    return;\
  }\
}

static const VSFrameRef *VS_CC IsCombedTIVTCGetFrame(int n,int activationReason,void **instanceData,void **frameData,VSFrameContext *frameCtx,VSCore *core,const VSAPI *vsapi){
//main function
  IsCombedTIVTC_parameter_t *combprop=(IsCombedTIVTC_parameter_t *)*instanceData;
  if(activationReason==arInitial){
    vsapi->requestFrameFilter(n,combprop->node,frameCtx);
    return NULL;
  }
  if(activationReason!=arAllFramesReady)
    return NULL;
  
  const int plane=0;
  const VSFrameRef *src=vsapi->getFrameFilter(n,combprop->node,frameCtx);
  const VSFormat *fi=vsapi->getFrameFormat(src);
  VSFrameRef *dst=vsapi->newVideoFrame(fi,1,1,src,core);
  uint8_t *dstp;
  for(int i=0;i<fi->numPlanes;i++){
    dstp=vsapi->getWritePtr(dst,i);
    dstp[0]=0;
  }

  //pixel
  uint8_t *srcp=(uint8_t *)vsapi->getReadPtr(src,plane);
  int srcStride=vsapi->getStride(src,plane);
  const int height=vsapi->getFrameHeight(src,plane);
  const int width=vsapi->getFrameWidth(src,plane);
  uint8_t *srcpp=srcp-srcStride;
  uint8_t *srcppp=srcpp-srcStride;
  uint8_t *srcpn=srcp+srcStride;
  uint8_t *srcpnn=srcpn+srcStride;
  
  int y,x;
  char **res=(char **)malloc(sizeof(char *)*height);
  metricFuncType metricFunc=metricFuncArray[combprop->metric];
  int cthresh=combprop->cthresh;
  
  for(y=0;y<height;y++){
    res[y]=(char *)malloc(sizeof(char)*width);
    int sizeof_res_y = sizeof(res[y]);
    memset(res[y],0,sizeof_res_y);
    //memset(res[y],0,sizeof(res[y]));
  }
  for(x=0;x<width;x++)
    res[0][x]=metricFunc(srcpnn[x],srcpn[x],srcp[x],srcpn[x],srcpnn[x],cthresh);

  srcppp=srcppp+srcStride;
  srcpp=srcpp+srcStride;
  srcp=srcp+srcStride;
  srcpn=srcpn+srcStride;
  srcpnn=srcpnn+srcStride;
  for(x=0;x<width;x++)
    res[1][x]=metricFunc(srcpnn[x],srcpp[x],srcp[x],srcpn[x],srcpnn[x],cthresh);
  
  for(y=2;y+2<height;y++){
    srcppp=srcppp+srcStride;
    srcpp=srcpp+srcStride;
    srcp=srcp+srcStride;
    srcpn=srcpn+srcStride;
    srcpnn=srcpn+srcStride;
    for(x=0;x<width;x++)
      res[y][x]=metricFunc(srcppp[x],srcpp[x],srcp[x],srcpn[x],srcpnn[x],cthresh);
  }

  srcppp=srcppp+srcStride;
  srcpp=srcpp+srcStride;
  srcp=srcp+srcStride;
  srcpn=srcp+srcStride;
  for(x=0;x<width;x++)
    res[height-2][x]=metricFunc(srcppp[x],srcpp[x],srcp[x],srcpn[x],srcppp[x],cthresh);

  srcppp=srcppp+srcStride;
  srcpp=srcpp+srcStride;
  srcp=srcp+srcStride;
  for(x=0;x<width;x++)
    res[height-1][x]=metricFunc(srcppp[x],srcpp[x],srcp[x],srcpp[x],srcppp[x],cthresh);

  vsapi->freeFrame(src);

  //blocks
  const int xblock=combprop->blockx;
  const int yblock=combprop->blocky;
  const int xshift=bitNum(xblock);
  const int yshift=bitNum(yblock);
  const int xhalf=xblock>>1;
  const int yhalf=yblock>>1;
  const int xblocks=((width+xhalf)>>xshift)+1;
  const int yblocks=((height+yhalf)>>yshift)+1;
  const int xblocks4=xblocks<<2;
  const int arraysize=(xblocks*yblocks)<<2;
  int *count=(int *)malloc(sizeof(int)*arraysize);
  memset(count,0,sizeof(int)*arraysize);

  int tmp1,tmp2,box1,box2;
  for(y=1;y+1<height;y++){
    tmp1=(y>>yshift)*xblocks4;
    tmp2=((y+yhalf)>>yshift)*xblocks4;
    for(x=0;x<width;x++)
      if((res[y-1][x]==PIXEL_COMBED)&&(res[y][x]==PIXEL_COMBED)&&(res[y+1][x]==PIXEL_COMBED)){
        box1=(x>>xshift)<<2;
        box2=((x+xhalf)>>xshift)<<2;
        ++count[tmp1+box1+0];
        ++count[tmp1+box2+1];
        ++count[tmp2+box1+2];
        ++count[tmp2+box2+3];
      }
  }

  for(y=0;y<height;y++)
    free(res[y]);
  free(res);

  //check
  char flag=0;
  for(int i=0;i<arraysize;i++)
    if(count[i]>combprop->MI){
      flag=1;
      break;
    }
  free(count);
  
  vsapi->propSetInt(vsapi->getFramePropsRW(dst),"_IsCombedTIVTC",flag,paReplace);
  return dst;
}

void VS_CC IsCombedTIVTCCreate(const VSMap *in,VSMap *out,void *userData,VSCore *core,const VSAPI *vsapi){
  IsCombedTIVTC_parameter_t *data=(IsCombedTIVTC_parameter_t *)malloc(sizeof(IsCombedTIVTC_parameter_t));
  FAIL_IF_ERROR(data==NULL,"failed to allocate handler when init");
  
  int err;
  
  data->node=vsapi->propGetNode(in,"clip",0,0);
  data->vi=vsapi->getVideoInfo(data->node);
  
  FAIL_IF_ERROR(!isConstantFormat(data->vi),"Clip format must be constant");
  FAIL_IF_ERROR(data->vi->format->sampleType!=stInteger,"float samples unsupported");
  FAIL_IF_ERROR((data->vi->format->colorFamily!=cmGray)&&(data->vi->format->colorFamily!=cmYUV),"need a luma plane");
  FAIL_IF_ERROR(data->vi->format->bitsPerSample!=8,"only 8-bit integer pixel value is supported");
  
  data->cthresh=(int)vsapi->propGetInt(in,"cthresh",0,&err);
  if(err!=0)
    data->cthresh=CTHRESH_DEFAULT;
  FAIL_IF_ERROR((data->cthresh<-1)||(data->cthresh>255),"undefined cthresh value");

  data->chroma=(int)vsapi->propGetInt(in,"chroma",0,&err);
  if(err!=0)
    data->chroma=CHROMA_DEFAULT;
  FAIL_IF_ERROR((data->chroma!=0)&&(data->chroma!=1),"undefined chroma value");

  data->blockx=(int)vsapi->propGetInt(in,"blockx",0,&err);
  if(err!=0)
    data->blockx=BLOCKX_DEFAULT;
  FAIL_IF_ERROR((data->blockx<4)||(data->blockx>2048)||(lowbit(data->blockx)!=(data->blockx)),"undefined blockx value");
  
  data->blocky=(int)vsapi->propGetInt(in,"blocky",0,&err);
  if(err!=0)
    data->blocky=BLOCKY_DEFAULT;
  FAIL_IF_ERROR((data->blocky<4)||(data->blocky>2048)||(lowbit(data->blocky)!=(data->blocky)),"undefined blocky value");
  
  data->MI=(int)vsapi->propGetInt(in,"MI",0,&err);
  if(err!=0)
    data->MI=MI_DEFAULT;
  FAIL_IF_ERROR((data->MI<0)||(data->MI>data->blockx*data->blocky),"undefined MI value");
  
  data->metric=(int)vsapi->propGetInt(in,"metric",0,&err);
  if(err!=0)
    data->metric=METRIC_DEFAULT;
  FAIL_IF_ERROR((data->metric!=0)&&(data->metric!=1),"undefined metric value");
  
  data->opt=(int)vsapi->propGetInt(in,"opt",0,&err);
  if(err!=0)
    data->opt=OPT_DEFAULT;
  FAIL_IF_ERROR((data->opt<0)||(data->opt>4),"undefined opt value");
  
  metricFuncArray=(metricFuncType *)malloc(sizeof(metricFuncType)*2);
  metricFuncArray[0]=metricWay1;
  metricFuncArray[1]=metricWay2;
  
  vsapi->createFilter(in,out,"IsCombedTIVTC",IsCombedTIVTCInit,IsCombedTIVTCGetFrame,IsCombedTIVTCFree,fmParallel,0,data,core);
  return;
}