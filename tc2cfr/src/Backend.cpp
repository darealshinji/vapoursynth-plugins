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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <queue>
#include <stack>

#include "Backend.h"

/*
inline int round(double x){
  return (int)(floor(x+0.5));
}
*/

inline bool isInt(double x){
  return abs(x-round(x))<=0.0001;
}

int comp(const void *a,const void *b){
  if((*(const v1event_t **)a)->st<(*(const v1event_t **)b)->st)
    return -1;
  if((*(const v1event_t **)a)->st>(*(const v1event_t **)b)->st)
    return 1;
  return 0;
}

FrameMap_t **genFrameMap(const char *timecodeFilePath,int origFrameTot,int fpsNum,int fpsDen,int *returnLen,char **msg){
  //read a timecode v1 file and return a frame map or report error
  if(*msg!=NULL)
    free(*msg);
  *msg=NULL;

  FILE *timecode=fopen(timecodeFilePath,"r");
  if(timecode==NULL){
    const char *errmsg="the given timecode file does not exist";
    *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
    strcpy(*msg,errmsg);
    return NULL;
  }

  bool isv1=false;
  char *line=(char *)malloc(sizeof(char)*256);
  for(;fgets(line,255,timecode)!=NULL;)
    if(strcmp(line,"# timecode format v1\n")==0){
      isv1=true;
      break;
    }
  if(!isv1){
    const char *errmsg="not timecode v1 format";
    *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
    strcpy(*msg,errmsg);
    fclose(timecode);
    free(line);
    return NULL;
  }

  double assumeFPS=-1;
  std::queue<v1event_t *> *equeue=new std::queue<v1event_t *>();
  int a,b;
  double c;
  int assumecount=0;
  for(;fgets(line,255,timecode)!=NULL;){
    if(strncmp(line,"Assume",6)==0){
      if(assumecount==0){
        sscanf(line,"Assume %lf\n",&assumeFPS);
        ++assumecount;
      }
      else{
        const char *errmsg="duplicate assumeFPS is set";
        *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
        strcpy(*msg,errmsg);
        break;
      }
    }
    if(sscanf(line,"%d,%d,%lf",&a,&b,&c)>0){
      if(c<=0){
        const char *errmsg="invalid fps in timecode";
        *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
        strcpy(*msg,errmsg);
        break;
      }
      if(!isInt(fpsNum/(c*fpsDen))){
        const char *errmsg="invalid target fps";
        *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
        strcpy(*msg,errmsg);
        break;
      }
      if(a>b){
        const char *errmsg="invalid fps interval in timecode";
        *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
        strcpy(*msg,errmsg);
        break;
      }
      v1event_t *event=(v1event_t *)malloc(sizeof(v1event_t));
      event->st=a;
      event->en=b;
      event->dupNum=round(fpsNum/(fpsDen*c));
      equeue->push(event);
    }
  }
  if(*msg!=NULL){
    for(int i=equeue->size();i>0;--i){
      v1event_t *tmp=equeue->front();
      equeue->pop();
      free(tmp);
    }
    delete equeue;
    fclose(timecode);
    free(line);
    return NULL;
  }
  fclose(timecode);
  free(line);
  line=NULL;

  int n=equeue->size();
  v1event_t **events=(v1event_t **)malloc(sizeof(v1event_t *)*(n+1));
  events[0]=(v1event_t *)malloc(sizeof(v1event_t));
  events[0]->st=-1;
  events[0]->en=-1;
  events[0]->dupNum=-1;
  for(int i=1;i<=n;i++){
    events[i]=equeue->front();
    equeue->pop();
  }
  delete equeue;

  qsort(events+1,n,sizeof(v1event_t *),comp);
  
  if(assumeFPS<=0)
    for(int i=0;i<n;i++)
      if(events[i]->en+1<events[i+1]->st){
        const char *errmsg="invalid assumeFPS in timecode";
        *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
        strcpy(*msg,errmsg);
        for(int j=0;j<=n;j++)
          free(events[i]);
        free(events);
        return NULL;
      }

  std::stack<FrameMap_t *> *frameMap=new std::stack<FrameMap_t *>();
  FrameMap_t *tmpMap=(FrameMap_t *)malloc(sizeof(FrameMap_t));
  tmpMap->origFrame=0;
  tmpMap->transFrame=0;
  tmpMap->dupNum=-1;
  frameMap->push(tmpMap);
  for(int i=1;i<=n;i++){
    if(events[i-1]->en+1<events[i]->st){
      tmpMap=(FrameMap_t *)malloc(sizeof(FrameMap_t));
      tmpMap->origFrame=events[i]->st-1;
      if(!isInt(fpsNum/(fpsDen*assumeFPS))){
        const char *errmsg="invalid target fps";
        *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
        strcpy(*msg,errmsg);
        free(tmpMap);
        break;
      }
      tmpMap->dupNum=round(fpsNum/(assumeFPS*fpsDen));
      tmpMap->transFrame=(events[i]->st-1-events[i-1]->en)*tmpMap->dupNum+frameMap->top()->transFrame;
      if(tmpMap->transFrame<0){
        const char *errmsg="output numFrames must be a 32bit integer";
        *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
        strcpy(*msg,errmsg);
        free(tmpMap);
        break;
      }
      frameMap->push(tmpMap);
    }
    tmpMap=(FrameMap_t *)malloc(sizeof(FrameMap_t));
    tmpMap->origFrame=events[i]->en;
    tmpMap->dupNum=events[i]->dupNum;
    tmpMap->transFrame=(events[i]->en-events[i]->st+1)*tmpMap->dupNum+frameMap->top()->transFrame;
    if(tmpMap->transFrame<0){
      const char *errmsg="output numFrames must be a 32bit integer";
      *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
      strcpy(*msg,errmsg);
      free(tmpMap);
      break;
    }
    frameMap->push(tmpMap);
  }

  for(int i=0;i<=n;i++)
    free(events[i]);
  free(events);

  if(*msg!=NULL){
    for(FrameMap_t *tmp;frameMap->size()>0;){
      tmp=frameMap->top();
      frameMap->pop();
      free(tmp);
    }
    delete frameMap;
    return NULL;
  }

  if(frameMap->top()->origFrame<origFrameTot){
    FrameMap_t *tmp=(FrameMap_t *)malloc(sizeof(FrameMap_t));
    tmp->origFrame=origFrameTot;
    tmp->dupNum=round(fpsNum/(fpsDen*assumeFPS));
    tmp->transFrame=(origFrameTot-frameMap->top()->origFrame)*tmp->dupNum+frameMap->top()->transFrame;
    if(tmp->transFrame<0){
      const char *errmsg="output numFrames must be a 32bit integer";
      *msg=(char *)malloc(sizeof(char)*(strlen(errmsg)+1));
      strcpy(*msg,errmsg);
      free(tmp);
      for(;frameMap->size()>0;){
        tmp=frameMap->top();
        frameMap->pop();
        free(tmp);
      }
      delete frameMap;
      return NULL;
    }
  }

  *returnLen=frameMap->size();
  FrameMap_t **ret=(FrameMap_t **)malloc(sizeof(FrameMap_t *)*(*returnLen));
  for(int i=(*returnLen)-1;i>=0;--i){
    ret[i]=frameMap->top();
    frameMap->pop();
  }
  delete frameMap;

  return ret;
}//genFrameMap

int binsearch(FrameMap_t **arr,int len,int object){
  int st=0,en=len-1,mid=0;
  for(;st<en;){
    mid=(st+en)>>1;
    if(arr[mid]->transFrame==object)
      return mid;
    if(arr[mid]->transFrame>object)
      en=mid;
    else
      st=mid+1;
  }
  return en;
}

int getOrigFrame(FrameMap_t **map,int mapLen,int n){
  if(n==0)
    return 0;
  int index=binsearch(map,mapLen,n);
  if(n==map[index]->transFrame)
    return map[index]->origFrame;
  return (n-map[index-1]->transFrame-1)/map[index]->dupNum+map[index-1]->origFrame+1;
}