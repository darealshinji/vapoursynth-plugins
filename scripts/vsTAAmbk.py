##==========================================================
## 2016.02.09			vsTAAmbk 0.6.1					
##			Ported from TAAmbk 0.7.0 by Evalyn
##			Maintained by Evalyn pov@mahou-shoujo.moe		
##							kewenyu 1059902659@qq.com		
##==========================================================
##			Requirements:								
##						EEDI2							
##						nnedi3							
##						RemoveGrain/Repair				
##						fmtconv										
##						MSmoosh							
##						MVTools							
##						TemporalSoften					
##						sangnom							
##						HAvsFunc(and its requirements)	
##			VapourSynth R28 or newer
##
##==========================================================
##==========================================================
##														
##	#### Only YUV colorfmaily is supported ! 	
##	#### And input bitdepth must be 8 or 16 INT !			
##		 												
##==========================================================	 	

import vapoursynth as vs
import havsfunc as haf
import mvsfunc as mvf

def TAAmbkX(input, aatype=1, strength=0.0, preaa=0, cycle=0,
            mtype=None, mclip=None, mthr=None, mthr2=None, mlthresh=None, mpand=[2, 1], txtprt=None,
            thin=0, dark=0.0,
            sharp=0, repair=0, postaa=None, src=None, stabilize=0,
            down8=False, showmask=0, eedi3m=True, **pn):
    
    
    core = vs.get_core()
    
    
    # Constant Values
    FUNCNAME = 'TAAmbkX'    # 'X' refer to 'Experimental'. Will change to vsTAAmbk when it is stable
    W = input.width
    H = input.height
    BPS = input.format.bits_per_sample
    COLOR_FAMILY = input.format.color_family
    SAMPLE_TYPE = input.format.sample_type
    ID = input.format.id
    SUBSAMPLE = input.format.subsampling_h
    IS_GRAY = True if input.format.num_planes == 1 else False
    ABS_SHARP = abs(sharp)
    STR = strength    # Strength of predown
    PROCE_DEPTH = BPS if down8 is False else 8    # Process bitdepth
    
    
    # Associated Default Values
    if mtype is None:
        mtype = 0 if preaa == 0 and aatype == 0 else 1
    if postaa is None:
        postaa = True if ABS_SHARP > 70 or (ABS_SHARP > 0.4 and ABS_SHARP < 1) else False
    
        
    if src is None:
        src = input
    else:
        if src.format.id != ID or src.width != W or src.height != H:
            raise RuntimeError(FUNCNAME + ': format of src and input mismatch !')
    
    
    # Input Check
    if not isinstance(input, vs.VideoNode):
        raise ValueError(FUNCNAME + ': input must be a clip !')
    if COLOR_FAMILY == vs.YUV or COLOR_FAMILY == vs.GRAY:
        if SAMPLE_TYPE != vs.INTEGER:
            raise TypeError(FUNCNAME + ': input must be integer format !')
        if not (BPS == 8 or BPS == 16):
            raise TypeError(FUNCNAME + ': input must be 8bit or 16bit !')
    else:
        raise TypeError(FUNCNAME + ': input must be YUV or GRAY !')        
        
    
############### Internal Functions ###################
    def Preaa(input, mode):
        nn = None if mode == 2 else core.nnedi3.nnedi3(input, field=3)
        nnt = None if mode == 1 else core.nnedi3.nnedi3(core.std.Transpose(input), field=3).std.Transpose()
        clph = None if mode == 2 else core.std.Merge(core.std.SelectEvery(nn, cycle=2, offsets=0), 
                                                     core.std.SelectEvery(nn, cycle=2, offsets=1))
        clpv = None if mode == 1 else core.std.Merge(core.std.SelectEvery(nnt, cycle=2, offsets=0), 
                                                     core.std.SelectEvery(nnt, cycle=2, offsets=1))
        clp = core.std.Merge(clph, clpv) if mode == -1 else None
        if mode == 1:
            return clph
        elif mode == 2:
            return clpv
        else:
            return clp
    
    
    def Lineplay(input, thin, dark):
        if thin != 0 and dark != 0:
            return haf.Toon(core.warp.AWarpSharp2(input, depth=int(thin)), str=float(dark))
        elif thin == 0:
            return haf.Toon(input, str=float(dark))
        else:
            return core.warp.AWarpSharp2(input, depth=int(thin))
    
    
    def Stabilize(clip, src, delta):
        aaDiff = core.std.MakeDiff(src, input, planes=[0, 1, 2])
        inSuper = core.mv.Super(clip, pel=1)
        diffSuper = core.mv.Super(aaDiff, pel=1, levels=1)
        
        fv3 = core.mv.Analyse(inSuper, isb=False, delta=3, overlap=8, blksize=16) if delta == 3 else None
        fv2 = core.mv.Analyse(inSuper, isb=False, delta=2, overlap=8, blksize=16) if delta >= 2 else None
        fv1 = core.mv.Analyse(inSuper, isb=False, delta=1, overlap=8, blksize=16) if delta >= 1 else None
        bv1 = core.mv.Analyse(inSuper, isb=True, delta=1, overlap=8, blksize=16) if delta >= 1 else None
        bv2 = core.mv.Analyse(inSuper, isb=True, delta=2, overlap=8, blksize=16) if delta >= 2 else None
        bv3 = core.mv.Analyse(inSuper, isb=True, delta=3, overlap=8, blksize=16) if delta == 3 else None
        
        if stabilize == 1:
            diffStab = core.mv.Degrain1(aaDiff, diffSuper, bv1, fv1)
        elif stabilize == 2:
            diffStab = core.mv.Degrain2(aaDiff, diffSuper, bv1, fv1, bv2, fv2)
        elif stabilize == 3:
            diffStab = core.mv.Degrain3(aaDiff, diffSuper, bv1, fv1, bv2, fv2, bv3, fv3)
        else:
            raise ValueError(FUNCNAME + ': \"stabilize\" int(0~3) invaild !')
        
        # Caculate for high-bitdepth support
        neutral = 128 << (BPS - 8)
        
        compareExpr = "x {neutral} - abs y {neutral} - abs < x y ?".format(neutral=neutral)
        diffCompare = core.std.Expr([aaDiff, diffStab], compareExpr)
        diffCompare = core.std.Merge(diffCompare, diffStab, 0.6)
        
        aaStab = core.std.MakeDiff(src, diffCompare, planes=[0, 1, 2])
        return aaStab
    
    
    def Soothe(sharped, src):
        neutral = 128 << (BPS - 8)
        peak = (1 << BPS) - 1
        multiple = peak / 255
        const = 100 * multiple
        keep = 24
        kp = keep * multiple
        
        diff1Expr = "x y - {neutral} +".format(neutral=neutral)
        diff1 = core.std.Expr([src, sharped], diff1Expr)
        
        diff2 = core.focus.TemporalSoften(diff1, radius=1, luma_threshold=255, chroma_threshold=255, scenechange=32, mode=2)
        
        diff3Expr = "x {neutral} - y {neutral} - * 0 < x {neutral} - {const} / {kp} * {neutral} + x {neutral} - abs y {neutral} - abs > x {kp} * y {const} {kp} - * + {const} / x ? ?".format(neutral=neutral, const=const, kp=kp)
        diff3 = core.std.Expr([diff1, diff2], diff3Expr)
        
        finalExpr = "x y {neutral} - -".format(neutral=neutral)
        final = core.std.Expr([src, diff3], finalExpr)
        
        return final
    
    
    def Sharp(aaclip, aasrc):
        if sharp >= 1:
            sharped = haf.LSFmod(aaclip, strength=int(ABS_SHARP), defaults="old", source=aasrc)
        elif sharp > 0:
            per = int(40*ABS_SHARP)
            matrix = [-1, -2, -1, -2, 52 - per, -2, -1, -2, -1]
            sharped = core.std.Convolution(aaclip, matrix)
        elif sharp > -1:
            sharped = haf.LSFmod(aaclip, strength=round(ABS_SHARP*100), defaults="fast", source=aasrc)
        elif sharp == -1:
            clipb = core.std.MakeDiff(aaclip, core.rgvs.RemoveGrain(aaclip, mode=20 if W > 1100 else 11))
            clipb = core.rgvs.Repair(clipb, core.std.MakeDiff(aasrc, aaclip), mode=13)
            sharped = core.std.MergeDiff(aaclip, clipb)
        else:
            sharped = haf.LSFmod(aaclip, strength=int(ABS_SHARP), defaults="slow", source=aasrc)
            
        return sharped
        
        
        
        
######################### Begin of Main AAtype#########################    
    class aaParent:
        def __init__(self):
            self.dfactor = 1 - min(STR, 0.5)
            self.dw = round(W * self.dfactor / 4) * 4
            self.dh = round(H * self.dfactor / 4) * 4
            self.upw4 = round(self.dw * 0.375) * 4
            self.uph4 = round(self.dh * 0.375) * 4
    
        @staticmethod
        def aaResizer(clip, w, h, shift):
            try:
                y = core.std.ShufflePlanes(clip, 0, vs.GRAY)
                u = core.std.ShufflePlanes(clip, 1, vs.GRAY)
                v = core.std.ShufflePlanes(clip, 2, vs.GRAY)
                y_resized = core.resize.Spline36(y, w, h, src_top=shift)
                u_resized = core.resize.Spline36(u, int(w / (SUBSAMPLE + 1)), int(h / (SUBSAMPLE + 1)), src_top=shift)
                v_resized = core.resize.Spline36(v, int(w / (SUBSAMPLE + 1)), int(h / (SUBSAMPLE + 1)), src_top=shift)
                resized = core.std.ShufflePlanes([y_resized, u_resized, v_resized], [0, 0, 0], vs.YUV)
                if resized.format.bits_per_sample != PROCE_DEPTH:
                    resized = mvf.Depth(resized, PROCE_DEPTH)
                return resized
            except vs.Error:
                resized = core.fmtc.resample(clip, w, h, sy=[shift, shift * (1 << SUBSAMPLE)])
                return resized
    
    class aaNnedi3(aaParent):
        def __init__(self, args):
            super(aaNnedi3, self).__init__()
            self.nsize = args.get('nsize', 3)
            self.nns = args.get('nns', 1)
            self.qual = args.get('qual', 2)
    
        def AA(self, clip):
            aaed = core.nnedi3.nnedi3(clip, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
            aaed = self.aaResizer(aaed, W, H, -0.5)
            aaed = core.std.Transpose(aaed)
            aaed = core.nnedi3.nnedi3(aaed, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
            aaed = self.aaResizer(aaed, H, W, -0.5)
            aaed = core.std.Transpose(aaed)
            return aaed
        

    class aaNnedi3SangNom(aaNnedi3):
        def __init__(self, args):
            super(aaNnedi3SangNom, self).__init__(args)
            self.aa = args.get('aa', 48)
        
        def AA(self, clip):
            aaed = core.nnedi3.nnedi3(clip, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
            aaed = self.aaResizer(aaed, W, self.uph4, -0.5)
            aaed = core.std.Transpose(aaed)
            aaed = core.nnedi3.nnedi3(aaed, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
            aaed = self.aaResizer(aaed, self.uph4, self.upw4, -0.5)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(aaed)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = self.aaResizer(aaed, W, H, 0)
            return aaed
    
    
    class aaNnedi3UpscaleSangNom(aaNnedi3SangNom):
        def __init__(self, args):
            super(aaNnedi3UpscaleSangNom, self).__init__(args)
            self.nsize = args.get('nsize', 1)
            self.nns = args.get('nns', 3)
            self.qual = args.get('qual', 2)


    class aaEedi3(aaParent):
        def __init__(self, eedi3m, args):
            super(aaEedi3, self).__init__()
            self.alpha = args.get('alpha', 0.5)
            self.beta = args.get('beta', 0.2)
            self.gamma = args.get('gamma', 20)
            self.nrad = args.get('nrad', 3)
            self.mdis = args.get('mdis', 30)
            self.eedi3m = eedi3m
            try:
                self.eedi3 = core.eedi3_092.eedi3    # Check whether eedi3_092 is available
            except AttributeError:
                self.eedi3 = core.eedi3.eedi3
                self.eedi3m = False    # Disable eedi3m if eedi3_092 is not available

        @staticmethod
        def down8(clip):
            if BPS == 16 and PROCE_DEPTH != 8:
                return mvf.Depth(clip, 8)
            else:
                return clip
        
        def build_eedi3_mask(self, clip):
            clip = self.down8(clip)
            eedi3_mask = core.nnedi3.nnedi3(clip, field=1, show_mask=True)
            eedi3_mask = core.std.Expr([eedi3_mask, clip], "x 254 > x y - 0 = not and 255 0 ?")
            eedi3_mask_turn = core.std.Transpose(eedi3_mask)
            return [eedi3_mask, eedi3_mask_turn]
        
        def AA(self, clip):
            if self.eedi3m is False:
                aaed = self.eedi3(self.down8(clip), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis)
                aaed = self.aaResizer(aaed, W, H, -0.5)
                aaed = core.std.Transpose(aaed)
                aaed = self.eedi3(self.down8(aaed), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis)
                aaed = self.aaResizer(aaed, H, W, -0.5)
                aaed = core.std.Transpose(aaed)
                return mvf.Depth(aaed, PROCE_DEPTH)
            else:
                mask = self.build_eedi3_mask(clip)
                aaed = self.eedi3(self.down8(clip), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis, mclip=mask[0])
                aaed = self.aaResizer(aaed, W, H, -0.5)
                aaed = core.std.Transpose(aaed)
                aaed = self.eedi3(self.down8(aaed), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis, mclip=mask[1])
                aaed = self.aaResizer(aaed, H, W, -0.5)
                aaed = core.std.Transpose(aaed)
                return mvf.Depth(aaed, PROCE_DEPTH)

    class aaEedi3SangNom(aaEedi3):
        def __init__(self, eedi3m, args):
            super(aaEedi3SangNom, self).__init__(eedi3m, args)
            self.aa = args.get('aa', 48)

        def build_eedi3_mask(self, clip):
            clip = self.down8(clip)
            eedi3_mask = core.nnedi3.nnedi3(clip, field=1, show_mask=True)
            eedi3_mask = core.std.Expr([eedi3_mask, clip], "x 254 > x y - 0 = not and 255 0 ?")
            eedi3_mask_turn = core.std.Transpose(eedi3_mask)
            eedi3_mask_turn = core.resize.Bicubic(eedi3_mask_turn, self.uph4, W)
            return [eedi3_mask, eedi3_mask_turn]
        
        def AA(self, clip):
            if self.eedi3m is False:
                aaed = core.eedi3.eedi3(self.down8(clip), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis)
                aaed = self.aaResizer(aaed, W, self.uph4, -0.5)
                aaed = core.std.Transpose(aaed)
                aaed = core.eedi3.eedi3(self.down8(aaed), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis)
                aaed = self.aaResizer(aaed, self.uph4, self.upw4, -0.5)
                aaed = core.sangnom.SangNom(aaed, aa=self.aa)
                aaed = core.std.Transpose(aaed)
                aaed = core.sangnom.SangNom(aaed, aa=self.aa)
                aaed = self.aaResizer(aaed, W, H, 0)
                return mvf.Depth(aaed, PROCE_DEPTH)
            else:
                mask = self.build_eedi3_mask(clip)
                aaed = self.eedi3(self.down8(clip), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis, mclip=mask[0])
                aaed = self.aaResizer(aaed, W, self.uph4, -0.5)
                aaed = core.std.Transpose(aaed)
                aaed = self.eedi3(self.down8(aaed), field=1, dh=True, alpha=self.alpha, beta=self.beta, gamma=self.gamma, nrad=self.nrad, mdis=self.mdis, mclip=mask[1])
                aaed = self.aaResizer(aaed, self.uph4, self.upw4, -0.5)
                aaed = core.sangnom.SangNom(aaed, aa=self.aa)
                aaed = core.std.Transpose(aaed)
                aaed = core.sangnom.SangNom(aaed, aa=self.aa)
                aaed = self.aaResizer(aaed, W, H, 0)
                return mvf.Depth(aaed, PROCE_DEPTH)


    class aaEedi2(aaParent):
        def __init__(self, args):
            super(aaEedi2, self).__init__()
            self.mthresh = args.get('mthresh', 10)
            self.lthresh = args.get('lthresh', 20)
            self.vthresh = args.get('vthresh', 20)
            self.maxd = args.get('maxd', 24)
            self.nt = args.get('nt', 50)
        def AA(self, clip):
            aaed = core.eedi2.EEDI2(clip, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
            aaed = self.aaResizer(aaed, W, H, -0.5)
            aaed = core.std.Transpose(aaed)
            aaed = core.eedi2.EEDI2(aaed, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
            aaed = self.aaResizer(aaed, H, W, -0.5)
            aaed = core.std.Transpose(aaed)
            return aaed


    class aaEedi2SangNom(aaEedi2):
        def __init__(self, args):
            super(aaEedi2SangNom, self).__init__(args)
            self.aa = args.get('aa', 48)
        
        def AA(self, clip):
            aaed = core.eedi2.EEDI2(clip, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
            aaed = self.aaResizer(aaed, W, self.uph4, -0.5)
            aaed = core.std.Transpose(aaed)
            aaed = core.eedi2.EEDI2(aaed, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
            aaed = self.aaResizer(aaed, self.uph4, self.upw4, -0.5)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(aaed)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = self.aaResizer(aaed, W, H, 0)
            return aaed


    class aaSpline64NrSangNom(aaParent):
        def __init__(self, args):
            super(aaSpline64NrSangNom, self).__init__()
            self.aa = args.get('aa', 48)
    
        def AA(self, clip):
            aaed = core.fmtc.resample(clip, self.upw4, self.uph4, kernel="spline64")
            aaed = mvf.Depth(aaed, PROCE_DEPTH)
            aaGaussian = core.fmtc.resample(clip, self.upw4, self.uph4, kernel="gaussian", a1=100)
            aaGaussian = mvf.Depth(aaGaussian, PROCE_DEPTH)
            aaed = core.rgvs.Repair(aaed, aaGaussian, 1)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(aaed)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(self.aaResizer(aaed, H, W, 0))
            return aaed


    class aaSpline64SangNom(aaParent):
        def __init__(self, args):
            super(aaSpline64SangNom, self).__init__()
            self.aa = args.get('aa', 48)
        
        def AA(self, clip):
            aaed = core.fmtc.resample(clip, W, self.uph4, kernel="spline64")
            aaed = mvf.Depth(aaed, PROCE_DEPTH)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(self.aaResizer(aaed, W, H, 0))
            aaed = core.fmtc.resample(aaed, H, self.upw4, kernel="spline64")
            aaed = mvf.Depth(aaed, PROCE_DEPTH)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(self.aaResizer(aaed, H, W, 0))
            return aaed


    class aaPointSangNom(aaParent):
        def __init__(self, args):
            super(aaPointSangNom, self).__init__()
            self.aa = args.get('aa', 48)
            self.upw = self.dw * 2
            self.uph = self.dh * 2
    
        def AA(self, clip):
            aaed = core.resize.Point(clip, self.upw, self.uph)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(aaed)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = self.aaResizer(core.std.Transpose(aaed), W, H, 0)
            return aaed
            
            
    # An AA method from VCB-S
    class aaEedi2PointSangNom(aaEedi2SangNom):
        def __init__(self, args):
            super(aaEedi2PointSangNom, self).__init__(args)
        
        def AA(self, clip):
            aaed = core.eedi2.EEDI2(clip, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
            aaed = self.aaResizer(aaed, W, H*2, -0.5)
            aaed = core.std.Transpose(aaed)
            aaed = core.resize.Point(aaed, H*2, W*2)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(aaed)
            aaed = core.sangnom.SangNom(aaed, aa=self.aa)
            aaed = core.std.Transpose(aaed)
            aaed = self.aaResizer(aaed, H, W, -0.5)
            aaed = core.std.Transpose(aaed)
            return aaed
            
            

##################### End of Main AAType ########################


##################### Begin of Mtype ###########################

    class mParent:
        def __init__(self):
            self.outdepth = vs.GRAY8 if PROCE_DEPTH == 8 else vs.GRAY16
            self.multi = 1 if PROCE_DEPTH == 8 else 257    # Multiple factor of Expr

        @staticmethod
        def mParaInit(mthr, default):
            if mthr is not None:
                return mthr
            else:
                return default

        @staticmethod
        def mCheckList(list1, list2, list3):
            if len(list1) != (len(list2) - 1):
                raise ValueError(FUNCNAME + ': num of mthr and mlthresh mismatch !')
            if len(list2) != len(list3):
                raise ValueError(FUNCNAME + ': num of mthr and mthr2 mismatch !')
        
        @staticmethod    
        def mGetGray(clip):
            return core.std.ShufflePlanes(clip, 0, vs.GRAY)
    
        '''
        def mBlankClip(clip):
            blc = core.std.BlankClip(clip, format=vs.GRAY8 if PROCE_DEPTH == 8 else vs.GRAY16)
            return blc 
        '''  
    
        @staticmethod
        def mExpand(clip, time):
            for i in range(time):
                clip = core.std.Maximum(clip, 0)
            return clip
    
        @staticmethod
        def mInpand(clip, time):
            for i in range(time):
                clip = core.std.Minimum(clip, 0)
            return clip
        
    class mCanny(mParent):
        def __init__(self, mthr, mthr2, mlthresh, mpand):
            super(mCanny, self).__init__()
            self.sigma = self.mParaInit(mthr, 1.2)
            self.t_h = self.mParaInit(mthr2, 8.0)
            self.mlthresh = mlthresh
            self.mpand = mpand
    
        def getMask(self, clip):
            clip = self.mGetGray(clip)
            if isinstance(self.sigma, list):
                self.mCheckList(self.mlthresh, self.sigma, self.t_h)
                mask = core.tcanny.TCanny(clip, sigma=self.sigma[0], t_h=self.t_h[0], mode=0, planes=0)
                
                for i in range(len(self.mlthresh)):
                    tmask = core.tcanny.TCanny(clip, sigma=self.sigma[i+1], t_h=self.t_h[i+1], mode=0, planes=0)
                    expr = "x " + str(self.mlthresh[i]) + " < z y ?"
                    mask = core.std.Expr([clip, tmask, mask], expr)
                mask = core.std.Expr(mask, "x " + str(self.multi) + " *", self.outdepth)            
            
            else:
                mask = core.tcanny.TCanny(clip, sigma=self.sigma, t_h=self.t_h, mode=0, planes=0)
        
            if self.mpand is not 0:
                mask = self.mExpand(mask, self.mpand[0])
                mask = self.mInpand(mask, self.mpand[1])
            
            return core.rgvs.RemoveGrain(mask, 20)

    class mCannySobel(mParent):
        def __init__(self, mthr, mthr2, mlthresh, mpand):
            super(mCannySobel, self).__init__()
            self.binarize = self.mParaInit(mthr, 48)
            self.sigma = self.mParaInit(mthr2, 1.2)
            self.mlthresh = mlthresh
            self.mpand = mpand
        
        @staticmethod
        def mCheckList(list1, list2):
            if len(list1) != (len(list2) - 1):
                raise ValueError(FUNCNAME + ': num of mthr and mlthresh mismatch !')
    
        def getMask(self, clip):
            clip = self.mGetGray(clip)
            eemask = core.tcanny.TCanny(clip, sigma=self.sigma, mode=1, op=2, planes=0)
            if isinstance(self.binarize, list):
                self.mCheckList(self.mlthresh, self.binarize)
                expr = "x " + str(self.binarize[0]) + " < 0 255 ?"
                mask = core.std.Expr(eemask, expr, self.outdepth)
            
                for i in range(len(self.mlthresh)):
                    texpr = "x " + str(self.binarize[i+1]) + " < 0 255 ?"
                    tmask = core.std.Expr(eemask, texpr)
                    expr = "x " + str(self.mlthresh[i]) + " < z y ?"
                    mask = core.std.Expr([clip, tmask, mask], expr) 
                mask = core.std.Expr(mask, "x " + str(self.multi) + " *", self.outdepth)
            
            else:
                expr = "x " + str(self.binarize) + " < 0 255 " + str(self.multi) + " * ?"
                mask = core.std.Expr(eemask, expr, self.outdepth)
            
            if self.mpand is not 0:
                mask = self.mExpand(mask, self.mpand[0])
                mask = self.mInpand(mask, self.mpand[1])

            return core.rgvs.RemoveGrain(mask, 20)

    class mPrewitt(mParent):
        def __init__(self, mthr, mlthresh, mpand):
            super(mPrewitt, self).__init__()
            self.mfactor = self.mParaInit(mthr, 62)
            self.mlthresh = mlthresh
            self.mpand = mpand

        @staticmethod
        def mCheckList(list1, list2):
            if len(list1) != (len(list2) - 1):
                raise ValueError(FUNCNAME + ': num of mthr and mlthresh mismatch !')

        @staticmethod
        def mInflate(clip, time):
            for i in range(time):
                clip = core.std.Inflate(clip)
            return clip

        @staticmethod
        def mDeflate(clip, time):
            for i in range(time):
                clip = core.std.Deflate(clip)
            return clip

        def getMask(self, clip):
            clip = self.mGetGray(clip)
            emask_1 = core.std.Convolution(clip, [1, 1, 0, 1, 0, -1, 0, -1, -1], divisor=1, saturate=False)
            emask_2 = core.std.Convolution(clip, [1, 1, 1, 0, 0, 0, -1, -1, -1], divisor=1, saturate=False)
            emask_3 = core.std.Convolution(clip, [1, 0, -1, 1, 0, -1, 1, 0, -1], divisor=1, saturate=False)
            emask_4 = core.std.Convolution(clip, [0, -1, -1, 1, 0, -1, 1, 1, 0], divisor=1, saturate=False)
            expr = "x y max z max a max"
            emask = core.std.Expr([emask_1, emask_2, emask_3, emask_4], expr)

            if isinstance(self.mfactor, list):
                self.mCheckList(self.mlthresh, self.mfactor)
                expr = "x " + str(self.mfactor[0]) + " <= x 2 / x 1.4 pow ?"
                mask = core.std.Expr(emask, expr, self.outdepth)
                for i in range(len(self.mlthresh)):
                    texpr = "x " + str(self.mfactor[i+1]) + " <= x 2 / x 1.4 pow ?"
                    tmask = core.std.Expr(emask, texpr)
                    expr = "x " + str(self.mlthresh[i]) + " < z y ?"
                    mask = core.std.Expr([clip, tmask, mask], expr)
                mask = core.std.Expr(mask, "x " + str(self.multi) + " *", self.outdepth)
            else:
                expr = "x {factor} <= x 2 / {multi} * x 1.4 pow {multi} * ?".format(factor=self.mfactor, multi=self.multi)
                mask = core.std.Expr(emask, expr)
            if self.mpand is not 0:
                mask = self.mInflate(mask, self.mpand[0])
                mask = self.mDeflate(mask, self.mpand[1])
            return core.rgvs.RemoveGrain(mask, 20)

    
    class mText(mParent):
        def __init__(self, luma):
            super(mText, self).__init__()
            self.luma = luma
            self.uvdiff = 1
            #self.neutral = 128
            #self.max = 255
        
        def getMask(self, clip):
            y = core.std.ShufflePlanes(clip, 0, vs.GRAY)
            u = mvf.Depth(core.fmtc.resample(core.std.ShufflePlanes(clip, 1, vs.GRAY), W, H, sx=0.25), PROCE_DEPTH)
            v = mvf.Depth(core.fmtc.resample(core.std.ShufflePlanes(clip, 2, vs.GRAY), W, H, sx=0.25), PROCE_DEPTH)
            txtExpr = "x {luma} > y 128 - abs {uvdiff} <= and z 128 - abs {uvdiff} <= and 255 0 ?".format(luma=self.luma, uvdiff=self.uvdiff)
            txtmask = core.std.Expr([y, u, v], txtExpr, self.outdepth)
            if BPS == 16:
                txtmask = core.std.Expr(txtmask, "x 257 *", vs.GRAY16)
            txtmask = core.std.Maximum(txtmask).std.Maximum().std.Maximum()
            return txtmask
            
################### End of Mtype ######################


################### Begin of Main Workflow ################

    input8 = input if BPS == 8 else mvf.Depth(input, 8)
    if BPS == 16 and down8 is True:
        input = input8
    
    # Pre-Antialiasing
    preaaClip = input if preaa == 0 else Preaa(input, preaa)
    
    # Pre-Lineplay (Considered useless)
    lineplayClip = preaaClip if (thin == 0 and dark == 0) else Lineplay(preaaClip, thin, dark)
    
    # Instantiate Main Anti-Aliasing Object, pn is a dict
    if aatype == 1 or aatype == "Eedi2":
        aaObj = aaEedi2(pn)
        
    elif aatype == 2 or aatype == "Eedi3":
        aaObj = aaEedi3(eedi3m, pn)
        
    elif aatype == 3 or aatype == "Nnedi3":
        aaObj = aaNnedi3(pn)
        
    elif aatype == 4 or aatype == "Nnedi3UpscaleSangNom":
        aaObj = aaNnedi3UpscaleSangNom(pn)
        
    elif aatype == 5 or aatype == "Spline64NrSangNom":
        aaObj = aaSpline64NrSangNom(pn)
        
    elif aatype == 6 or aatype == "Spline64SangNom":
        aaObj = aaSpline64SangNom(pn)
        
    elif aatype == "PointSangNom":
        aaObj = aaPointSangNom(pn)
        
    elif aatype == -1 or aatype == "Eedi2SangNom":
        aaObj = aaEedi2SangNom(pn)
        
    elif aatype == -2 or aatype == "Eedi3SangNom":
        aaObj = aaEedi3SangNom(eedi3m, pn)
        
    elif aatype == -3 or aatype == "Nnedi3SangNom":
        aaObj = aaNnedi3SangNom(pn)
        
    elif aatype == 'Eedi2PointSangNom':
        aaObj = aaEedi2PointSangNom(pn)
        
    elif aatype == 0:
        pass

    else:
        raise ValueError(FUNCNAME + ': Unknown AAtype !')
        
    # Get Anti-Aliasing Clip
    if aatype != 0:
        if strength != 0:
            lineplayClip = aaObj.aaResizer(lineplayClip, aaObj.dw, aaObj.dh, 0)
        aaedClip = aaObj.AA(lineplayClip)
        # Cycle it as you will
        while cycle > 0:
            aaedClip = aaObj.AA(aaedClip)
            cycle = cycle - 1
            
    else:
        aaedClip = lineplayClip
        
    # Back 16 if BPS is 16 and down8
    if BPS == 16 and down8 is True:
        aaedClip = mvf.Depth(aaedClip, 16)
        
    # Sharp it
    sharpedClip = aaedClip if sharp == 0 else Sharp(aaedClip, src)
    
    # PostAA
    if postaa is True:
        sharpedClip = Soothe(sharpedClip, src)
    
    # Repair it
    repairedClip = sharpedClip if repair == 0 else core.rgvs.Repair(src, sharpedClip, repair)
    
    # Stabilize it
    stabedClip = repairedClip if stabilize == 0 else Stabilize(repairedClip, src, stabilize)
    
    
    # Build AA Mask First then Merge it. Output depth is BPS
          # ! Mask always being built under 8 bit ! #
    if mclip is not None:
        aaMask = mclip
        try:
            mergedClip = core.std.MaskedMerge(src, stabedClip, aaMask, planes=[0, 1, 2], first_plane=True)
        except:
            raise RuntimeError(FUNCNAME + ': Something wrong with your mclip. Check the resolution and bitdepth of mclip !')
    else:
        if mtype != 0:
            if mtype == 1 or mtype == "Canny":
                maskObj = mCanny(mthr, mthr2, mlthresh, mpand)
                aaMask = maskObj.getMask(input8)
            elif mtype == 2 or mtype == "CannySobel":
                maskObj = mCannySobel(mthr, mthr2, mlthresh, mpand)
                aaMask = maskObj.getMask(input8)
            elif mtype == 3 or mtype == "Prewitt":
                maskObj = mPrewitt(mthr, mlthresh, mpand)
                aaMask = maskObj.getMask(input8)
            else:
                raise ValueError(FUNCNAME + ': Unknown mtype')
            
            # Let it back to 16 if input is 16bit
            if BPS == 16:
                aaMask = core.std.Expr(aaMask, "x 257 *", vs.GRAY16)
                
            mergedClip = core.std.MaskedMerge(src, stabedClip, aaMask, planes=[0, 1, 2], first_plane=True)
        else:
            mergedClip = stabedClip
    
    # Build Text Mask if input is not GRAY
    if IS_GRAY is False and txtprt is not None:
        txtmObj = mText(txtprt)
        txtMask = txtmObj.getMask(input8)
        txtprtClip = core.std.MaskedMerge(mergedClip, src, txtMask, planes=[0, 1, 2], first_plane=True)
    else:
        txtprtClip = mergedClip
    
    
    # Clamp loss if input is 16bit and down8 is set
    if BPS == 16 and down8 is True:
        outClip = mvf.LimitFilter(src, txtprtClip, thr=1.0, elast=2.0)
    else:
        outClip = txtprtClip
        
    # Showmask or output
    if showmask == -1:
        try:
            return txtMask
        except UnboundLocalError:
            raise RuntimeError(FUNCNAME + ': No txtmask to show if you don\'t have one.')
            
    elif showmask == 1:
        try:
            return aaMask
        except UnboundLocalError:
            raise RuntimeError(FUNCNAME + ': No mask to show if you don\'t have one.')
    elif showmask == 2:
        try:
            return core.std.StackVertical([core.std.ShufflePlanes([aaMask, core.std.BlankClip(src)], [0, 1, 2], vs.YUV), src])
        except UnboundLocalError:
            raise RuntimeError(FUNCNAME + ': No mask to show if you don\'t have one.')
    elif showmask == 3:
        try:
            return core.std.Interleave([core.std.ShufflePlanes([aaMask, core.std.BlankClip(src)], [0, 1, 2], vs.YUV), src])
        except UnboundLocalError:
            raise RuntimeError(FUNCNAME + ': No mask to show if you don\'t have one.')
    else:
        return outClip