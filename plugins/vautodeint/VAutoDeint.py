#VAutoDeint.py: Copyright (C) 2013  Acfun (www dot acfun dot tv)
#
#Author: Li Yonggang (gnaggnoyil at gmail dot com)
#
#This file is part of the VAutoDeint project.
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License as
#published by the Free Software Foundation; either version 3 of
#the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with the author; if not, write to the Free Software Foundation,
#Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

import vapoursynth
import sys

#####################################################################################################
#                                                                                                   #
#        VAutoDeint - auto interlace detection and deinterlace vapoursynth python script            #
#                                                                                                   #
#  Introduction:                                                                                    #
#                                                                                                   #
#      VAutoDeint is an auto interlace detection and deinterlace script that can be used in a       #
#  vapoursynth python script. It can recognize progressive, interlaced, telecined sources, and      #
#  hybrids thereof. For telecined and interlaced sources, it can also determine the field order.    #
#  For progressive sources, it can recognise if the source has had the framerate upconverted by     #
#  simple repitition (eg 24p->60p). While in some cases it can be inaccurate or even totally        #
#  incorrect, it can meet most common deinterlace demands and thus may be helpful for users to      #
#  check source types and write correct scripts.                                                    #
#                                                                                                   #
#      The project can be considered as a port to vapoursynth of MeGUI's auto interlace detection   #
#  algorithm, which is written by berrinam and based on descriptions of AutoGK's algorithm. The     #
#  project also contains algorithms from tritical's tivtc avisynth plugin and vapoursynth's         #
#  built-in filters. For these reasons above, the project would be released under GPL v3.           #
#                                                                                                   #
#  Installation and usage:                                                                          #
#                                                                                                   #
#      Actually there is no so-called "installation" step for this project. Simply copy the         #
#  externalfilters.dll to a path X, and copy VAutoDeint.py to YOUR_PYTHON3_PATH\Lib\site-packages   #
#  and then you can use this script.                                                                #
#                                                                                                   #
#      This script is a vapoursynth python script, so if you don't know how to write a vapoursynth  #
#  python script you can go to http://www.vapoursynth.com/doc/index.html to check for the           #
#  vapoursynth documents.                                                                           #
#                                                                                                   #
#      The __main__ module in VAutoDeint.py shows a example of a way to use this script. In         #
#  general, you can use VAutoDeint like this:                                                       #
#                                                                                                   #
#      import vapoursynth                                                                           #
#      import sys                                                                                   #
#      import VAutoDeint                                                                            #
#                                                                                                   #
#      core=vapoursynth.Core()                                                                      #
#      core.std.LoadPlugin(externalfilter.dllpathX+"externalfilters.dll")                           #
#      core.avs.LoadPlugin(TIVTC.dllpath+"tivtc.dll")                                               #
#      core.avs.LoadPlugin(TDeint.dllpath+"tdeint.dll")                                             #
#      """Load other plugins"""                                                                     #
#      clip=THE_CLIP_YOU_WANT_TO_DETECT                                                             #
#      autodeint=VAutoDeint.VAutoDeint(core)                                                        #
#      dict=autodeint.autoDeintDetect(clip)#get a dictionary describing the source type of the      #
#                                          #clip. You can print it to see what it contains.         #
#      func=autdeint.getProcessFilter(dict)#get a processing filter according to the dictionary     #
#                                          #given, which can be used later.                         #
#      video1=func(clip)                                                                            #
#      """postprocessing"""                                                                         #
#                                                                                                   #
#####################################################################################################

class VAutoDeint():

    def __init__(self,core,path="C:\\Program Files\\vapoursynth\\filters\\",avspath="C:\\Program Files\\avisynth\\plugins\\"):
        self.avs=core.avs;
        self.std=core.std;
        if core.list_functions().find("deintconf")+1==0:
            self.std.LoadPlugin(path+"externalfilters.dll")
        self.deintconf=core.deintconf;
        if core.list_functions().find("TFM")+1==0:
            self.avs.LoadPlugin(avspath+"TIVTC.dll")
        if core.list_functions().find("TDeint")+1==0:
            self.avs.LoadPlugin(avspath+"TDeint.dll")
        self.SECTION_LENGTH=15
        self.HYBRID_TYPE_PERCENT=5
        self.MINIMUM_USEFUL_SECTIONS=30
        self.ANALYSE_PERCENT=3
        self.MINIMUM_ANALYSE_SECTIONS=30
        self.DECIMATION_THRESHOLD=2.0
        self.FIELD_SECTION_LENGTH=5
        self.HYBRID_FIELD_ORDER_PERCENT=10
 
    def __get_short_section_type(self,comb_data,motion_data):
        if False in motion_data:
            return "SectionType_UNKNOWN"
        if not(True in comb_data):
            return "SectionType_PROGRESSIVE"
        for i in range(5):
            if comb_data[i]:
                break
        if [comb_data[j%5] for j in range(i,i+5)]==[True,True,False,False,False]:
            return "SectionType_FILM"
        if not([comb_data[j%5] for j in range(i,i+5)]==[True,False,False,False,False]):
            return "SectionType_INTERLACED"
        return "SectionType_UNKNOWN"
 
    def __get_section_type(self,comb_data,motion_data):
        sectionTypes=[self.__get_short_section_type(comb_data[i:(i+5)],motion_data[i:(i+5)]) for i in range(0,self.SECTION_LENGTH,5)]
        if sectionTypes[0]==sectionTypes[1]:
            return sectionTypes[0]
        if sectionTypes[1]==sectionTypes[2]:
            return sectionTypes[1]
        if sectionTypes[2]==sectionTypes[0]:
            return sectionTypes[2]
        return "SectionType_UNKNOWN"
 
    def __get_field_type(self,field_dataB,field_dataT):
        countA=0
        countB=0
        for i in range(1,self.FIELD_SECTION_LENGTH):
            if field_dataB[i]>field_dataT[i]:
                countA+=1
            elif field_dataB[i]<field_dataT[i]:
                countB+=1
        if (countA>countB)and countB==0:
            return "FieldOrder_TFF"
        if (countA<countB)and countA==0:
            return "FieldOrder_BFF"
        return "FieldOrder_UNKNOWN"
 
    def autoDeintDetect(self,clip,info=False,infoOutput=sys.stderr):
        tot_frames=clip.num_frames
        diff=self.deintconf.PlaneDifferenceFromPrevious(clip,0,prop="YPlaneDifference")
        IsCombedTIVTCClip=self.deintconf.IsCombedTIVTC(clip)
        motion_data=[]
        comb_data=[]
        if info:
            for i in range(tot_frames):
                motion_data.append((diff.get_frame(i).props.YPlaneDifference[0]>=1.0))
                comb_data.append(bool(IsCombedTIVTCClip.get_frame(i).props._IsCombedTIVTC[0]))
                print("\rcollecting motion and comb data...%d of %d completed." %(i,tot_frames),end="",file=infoOutput)
            print(file=infoOutput)
        else:
            for i in range(tot_frames):
                motion_data.append((diff.get_frame(i).props.YPlaneDifference[0]>=1.0))
                comb_data.append(bool(IsCombedTIVTCClip.get_frame(i).props._IsCombedTIVTC[0]))
        """check for interlace type"""
        sectionLength=self.SECTION_LENGTH
        numSections=tot_frames//sectionLength
        newLength=numSections*sectionLength
        all_section_type=[self.__get_section_type(comb_data[i:(i+sectionLength)],motion_data[i:(i+sectionLength)]) for i in range(0,newLength,sectionLength)]
        section_counts={}
        for i in range(0,numSections):
            if all_section_type[i] in section_counts.keys():
                section_counts[all_section_type[i]]+=1
            else:
                section_counts[all_section_type[i]]=1
        def count(type):
            if not(type in section_counts.keys()):
                section_counts[type]=0
            return section_counts[type]
        num_known_sections=numSections-count("SectionType_UNKNOWN")
        mostly_film=False
        if num_known_sections<self.MINIMUM_USEFUL_SECTIONS:
            source_type="SourceType_UNKNOWN"
        else:
            def hasType(type):
                return bool(count(type)*100>=self.HYBRID_TYPE_PERCENT*numSections)
            has_film=hasType("SectionType_FILM")
            has_interlace=hasType("SectionType_INTERLACED")
            has_progressive=hasType("SectionType_PROGRESSIVE")
            need_fo=True
            if has_film:
                if has_interlace:
                    source_type="SourceType_HYBRID_FILM"
                    mostly_film=bool(count("SectionType_FILM")>=count("SectionType_INTERLACED")+count("SectionType_PROGRESSIVE"))
                elif has_progressive:
                    source_type="SourceType_PARTIALLY_FILM"
                else:
                    source_type="SourceType_FILM"
            elif has_interlace:
                if has_progressive:
                    source_type="SourceType_PARTIALLY_INTERLACED"
                else:
                    source_type="SourceType_INTERLACED"
            else:
                need_fo=False
                source_type="SourceType_PROGRESSIVE"
        """check for decimation"""
        decimation_frame=0
        if (source_type=="SourceType_UNKNOWN") or (source_type=="SourceType_PROGRESSIVE"):
            def numMotionFrames(motion_data):
                ret=0
                for i in range(0,5):
                    if motion_data[i]:
                      ret+=1
                return ret
            def maxandmax(list):
                max1=-1
                max2=-2
                maxi=-1
                for i in range(len(list)):
                    if max1<list[i]:
                        max2=max1
                        max1=list[i]
                        maxi=i
                        continue
                    if max2<list[i]:
                        max2=list[i]
                return (max1,max2,maxi)
            numSectionsWithMotionPattern=[0 for i in range(0,6)]
            for i in range(0,newLength,self.SECTION_LENGTH):
                numMovingFrames=[numMotionFrames(motion_data[i+j:i+j+5]) for j in range(0,self.SECTION_LENGTH,5)]
                (max1,max2,maxi)=maxandmax(numMovingFrames)
                if max1==max2:
                    numSectionsWithMotionPattern[max1]+=1
            (max1,max2,maxi)=maxandmax(numSectionsWithMotionPattern)
            if (max1>max2*self.DECIMATION_THRESHOLD) and not maxi in (0,5):
                source_type="SourceType_DECIMATING"
                decimation_frame=5-maxi
        """check for field order"""
        field_order="FieldOrder_UNKNOWN"
        if need_fo:
            abff=self.std.SeparateFields(clip,tff=False)
            tot_frames=abff.num_frames
            field_dataB=[]
            diff=self.deintconf.PlaneDifferenceFromPrevious(abff,0,prop="YDifferencePrevious")
            if info:
                for i in range(tot_frames):
                    field_dataB.append(diff.get_frame(i).props.YDifferencePrevious[0])
                    print("\rcollecting bff field difference data...%d of %d completed." %(i,tot_frames),end="",file=infoOutput)
                print(file=infoOutput)
            else:
                for i in range(tot_frames):
                    field_dataB.append(diff.get_frame(i).props.YDifferencePrevious[0])
            atff=self.std.SeparateFields(clip,tff=True)
            field_dataT=[]
            diff=self.deintconf.PlaneDifferenceFromPrevious(atff,0,prop="YDifferencePrevious")
            if info:
                for i in range(tot_frames):
                    field_dataT.append(diff.get_frame(i).props.YDifferencePrevious[0])
                    print("\rcollecting tff field difference data...%d of %d completed." %(i,tot_frames),end="",file=infoOutput)
                print(file=infoOutput)
            else:
                for i in range(tot_frames):
                    field_dataT.append(diff.get_frame(i).props.YDifferencePrevious[0])
            num_field_sections=tot_frames//self.FIELD_SECTION_LENGTH
            new_field_section_length=num_field_sections*self.FIELD_SECTION_LENGTH
            all_field_section_type=[self.__get_field_type(field_dataB[i:(i+self.FIELD_SECTION_LENGTH)],field_dataT[i:(i+self.FIELD_SECTION_LENGTH)]) for i in range(0,new_field_section_length,self.FIELD_SECTION_LENGTH)]
            field_section_counts={}
            for i in range(0,num_field_sections):
                if all_field_section_type[i] in field_section_counts.keys():
                    field_section_counts[all_field_section_type[i]]+=1
                else:
                    field_section_counts[all_field_section_type[i]]=1
            def countField(type):
                if not(type in field_section_counts.keys()):
                    field_section_counts[type]=0
                return field_section_counts[type]
            def hasFieldType(type):
                return bool(countField(type)*100>=self.HYBRID_FIELD_ORDER_PERCENT*num_field_sections)
            hasTFF=hasFieldType("FieldOrder_TFF")
            hasBFF=hasFieldType("FieldOrder_BFF")
            if hasTFF and not hasBFF:
                field_order="FieldOrder_TFF"
            elif hasBFF and not hasTFF:
                field_order="FieldOrder_BFF"
            elif hasTFF and hasBFF:
                field_order="FieldOrder_VARIABLE"
            else:
                field_order="FieldOrder_UNKNOWN"
        return {"source_type":source_type,"field_order":field_order,"mostly_film":mostly_film,"decimation_frame":decimation_frame}
 
    def getProcessFilter(self,type):
        if type["source_type"]=="SourceType_PROGRESSIVE":
            return lambda x:x
        if type["source_type"] in ("SourceType_INTERLACED","SourceType_PARTIALLY_INTERLACED"):
            if type["field_order"]=="FieldOrder_TFF":
                return lambda x:self.avs.TDeint(c1=x,order=1)
            if type["field_order"]=="FieldOrder_BFF":
                return lambda x:self.avs.TDeint(c1=x,order=0)
            if type["field_order"]=="FieldOrder_VARIABLE":
                return lambda x:self.avs.TDeint(c1=x,order=-1)
        if type["source_type"] in ("SourceType_FILM", "SourceType_PARTIALLY_FILM"):
            if type["field_order"]=="FieldOrder_TFF":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=1))
            if type["field_order"]=="FieldOrder_BFF":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=0))
            if type["field_order"]=="FieldOrder_VARIABLE":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=-1))
        if type["source_type"]=="SourceType_DECIMATING":
            return lambda x:self.avs.TDecimate(c1=x,cycleR=type["decimation_frame"])
        if type["source_type"]=="SourceType_HYBRID_FILM" and type["mostly_film"]:
            if type["field_order"]=="FieldOrder_TFF":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=1),hybrid=1)
            if type["field_order"]=="FieldOrder_BFF":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=0),hybrid=1)
            if type["field_order"]=="FieldOrder_VARIABLE":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=-1),hybrid=1)
        if type["source_type"]=="SourceType_HYBRID_FILM" and not type["mostly_film"]:
            if type["field_order"]=="FieldOrder_TFF":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=1),hybrid=3)
            if type["field_order"]=="FieldOrder_BFF":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=0),hybrid=3)
            if type["field_order"]=="FieldOdrer_VARIABLE":
                return lambda x:self.avs.TDecimate(c1=self.avs.TFM(c1=x,order=-1),hybrid=3)
        return None
 
if __name__=="__main__":
    core=vapoursynth.Core(threads=6)
    core.std.LoadPlugin(r"H:\useful things\vsproject\externalfilters\externalfilters\Release\externalfilters.dll")
    core.avs.LoadPlugin(r"H:\Program Files (x86)\MeGUI\tools\avisynth_plugin\TIVTC.dll")
    core.avs.LoadPlugin(r"H:\Program Files (x86)\MeGUI\tools\dgindex\DGDecode.dll")
    core.avs.LoadPlugin(r"H:\Program Files (x86)\MeGUI\tools\avisynth_plugin\TDeint.dll")
    video=core.avs.MPEG2Source(d2v=r"H:\video\TamakoMarket\Tamakomarket_12.TSSplit.2-2.d2v",cpu=4)
    autodeint=VAutoDeint(core)
    dict=autodeint.autoDeintDetect(video)
    print(video.num_frames,video.fps_num)
    print(dict)
    video1=autodeint.getProcessFilter(dict)(video)
    print(video1.num_frames,video1.fps_num)
