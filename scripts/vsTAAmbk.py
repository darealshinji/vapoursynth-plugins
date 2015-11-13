##==========================================================
## 2015.11.9			vsTAAmbk 0.3.2					
##			Port from TAAmbk 0.7.0 by Evalyn
##			Email: pov@mahou-shoujo.moe			
##			Thanks (author)kewenyu for help				
##==========================================================
##			Requirements:								
##						EEDI2							
##						nnedi3							
##						RemoveGrain/Repair				
##						fmtconv							
##						GenericFilters					
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
##	"mtype" and "mtype2" = 5 are completely useless.	
##	 Add lsb[bool] to control nnedi3 input bitdepth.
##	"False" means input depth for nnedi3 is always 8bit.
##	"thin" and "dark" are now removed.
##  add aatype 7 using sangnom					
##		 												
##==========================================================	 												
##		 												
##	Output bitdepth is always 16bit INTEGER.						
##	AA precision is 16bit (8bit if necessary).	
##	Mask precision depend on the input.					
##	(if 8 then 8, if 16 then 16)						
##	Other parts are all 16bit.							
##														
##==========================================================

import vapoursynth as vs
import havsfunc as haf


def TAAmbk(input, aatype=1, lsb=False, preaa=0, sharp=0, postaa=None, mtype=None, mthr=32, src=None,
			 cycle=0, eedi3sclip=None, predown=False, repair=None, stabilize=0, p1=None, p2=None, 
		   p3=None, p4=None, p5=None, p6=None, showmask=False, mtype2=0, mthr2=32, auxmthr=None):
	core = vs.get_core()
	
	#constant value
	funcname = 'TAAmbk'
	w = input.width
	h = input.height
	upw4 = (round(w*0.09375)*16) # mod16(w*1.5)
	uph4 = (round(h*0.09375)*16) # mod16(h*1.5)
	downw4 = (round(w*0.046875)*16) # mod16(w*0.75)
	downh4 = (round(h*0.046875)*16) # mod16(h*0.75)
	
	# border to add for SangNomMod when aatype = 6 or 7
	if aatype == 6 or aatype == 7:
		# mod16 or not
		if w % 16 == 0:
			mod16w = True
		else:
			mod16w = False
			borderW = (16 - w % 16) 
		if h % 16 == 0:
			mod16h = True
		else:
			mod16h = False
			borderH = (16 - h % 16)
	
	
	
	#generate paramerters if None
	if mtype == None:
		if preaa == 0 and aatype == 0:
			mtype = 0
		else:
			mtype = 1
	
	if auxmthr == None:
		if mtype == 1:
			auxmthr = 1.2
		else:
			if mtype ==3:
				auxmthr = 8
			else:
				auxmthr = 0.0
							
	absSh = abs(sharp)
	if postaa == None:
		if absSh > 70 or (absSh > 0.4 and absSh < 1):
			postaa = True
		else:
			postaa = False
			
	if repair == None:
		if (aatype != 1 and aatype != 2 and aatype != 3):
			repair = 20
		else:
			repair = 0
	
	if isinstance(mtype, vs.VideoNode):
		rp = 20
	else:
		if mtype == 5:
			rp = 0
		else:
			rp = 20
	
	if eedi3sclip is None:
		eedi3sclip = False
	else:
		if not isinstance(eedi3sclip, bool):
			raise TypeError(funcname + ': \"eedi3sclip\" must be bool !')
			
	
	# p1~p6 preset groups	
	pindex = aatype + 3
	#				 aatype =		-3       -2		-1		0	   1	  2	 	3		 4		 5		 6		7
	if p1	is None: p1		=	[	48,	    48,		48,		0,	   10,	 0.5, 	 3,		48,		48,		48,		48][pindex]
	if p2	is None: p2		=	[	 3,	   0.5,		10,		0,	   20,	 0.2, 	 1,		 1,		 0,		rp,		rp][pindex]
	if p3	is None: p3		=	[	 1,	   0.2,		20,		0,	   20,	  20, 	 2,		 3,		 0,		 0,		 0][pindex]
	if p4	is None: p4		=	[	 2,	    20,		20,		0,	   24,	   3, 	 0,		 2,		 0,		 0,		 0][pindex]
	if p4	is None: p4		=	[	 2,	    20,		20,		0,	   24,	   3, 	 0,		 2,		 0,		 0,		 0][pindex]
	if p5	is None: p5		=	[	 0,	     3,		24,		0,	   50,	  30, 	 0,		 0,		 0,		 0,		 0][pindex]
	if p6	is None: p6		=	[	 0,	    30,		50,		0,	    0,	   0, 	 0,		 0,		 0,		 0,		 0][pindex]
	
	
	#paramerters check
	#input type check
	if not isinstance(input, vs.VideoNode):
		raise ValueError(funcname + ': \"input\" must be a clip !')
	#YUV constant value
	inputFormatid = input.format.id  							# A unique id identifying the format.
	sColorFamily = input.format.color_family					# Which group of colorspaces the format describes.
	sbits_per_sample = int(input.format.bits_per_sample)		# How many bits are used to store one sample in one plane.
	sSType = input.format.sample_type							# source sample type
	#format check
	if sColorFamily == vs.YUV or sColorFamily == vs.GRAY:
		if sSType != vs.INTEGER:
			raise TypeError(funcname + ': \"input\" must be INTEGER format !')
		else:
			if not (sbits_per_sample == 8 or sbits_per_sample == 16):
				raise TypeError(funcname + ': \"input\" must be 8bit or 16bit INTEGER !')
	else:
		raise TypeError(funcname + ': Only YUV colorfmaily is supported !')
	
	#aatype check
	if not isinstance(aatype, int) or (aatype < -3 or aatype > 7):
		raise ValueError(funcname + ': \"aatype\" (int: -3~7) invalid !')
	#lsb check
	if not isinstance(lsb, bool):
		raise TypeError(funcname + ': \"lsb\" must be BOOL !')
	#preaa check
	if not isinstance(preaa, int) or (preaa < 0 or preaa > 1):
		raise ValueError(funcname + ': \"preaa\" (int: 0~1) invalid !')
	#mtype check
	if not isinstance(mtype, int):
		if not isinstance(mtype, vs.VideoNode):
			raise TypeError(funcname + ': \"mtype\" is not a clip !')
		else:
			if mtype.format.id != inputFormatid :
				raise TypeError(funcname + ': \"input\" and \"mclip(mtype)\" must be of the same format !')
			else:
				if mtype.width != w or mtype.height != h:
					raise TypeError(funcname + ': resolution of \"input\" and your custome mask clip \"mtype\" must match !')
	else:
		if mtype < 0 or mtype > 6:
			raise ValueError(funcname + ': \"mtype\" (int: 0~6) invalid !')
	#mthr check
	if not isinstance(mthr, int) or (mthr < 0 or mthr > 255):
		raise ValueError(funcname + ': \"mthr\" (int: 0~255) invalid !')
	#repair check
	if not isinstance(repair, int) or (repair < -24 or repair > 24):
		raise ValueError(funcname + ': \"repair\" (int: -24~24) invalid !')
	#src clip check
	if src is not None and isinstance(src, vs.VideoNode):
		if src.format.id != inputFormatid :
			raise TypeError(funcname + ': \"input\" and \"src\" must be of the same format !')
		else:
			if src.width != w or src.height != h:
				raise TypeError(funcname + ': resolution of \"input\" and \"src\" must match !')
	elif src is not None:
		raise ValueError(funcname + ': \"src\" is not a clip !')
	#cycle check
	if not isinstance(cycle, int) or cycle < 0:
		raise ValueError(funcname + ': \"cycle\" must be non-negative int !')
	#stabilize check
	if not isinstance(stabilize, int) or (stabilize < -3 or stabilize > 3):
		raise ValueError(funcname + ': \"stabilize\" (int: -3~3) invalid !')
	if showmask and mtype == 0:
		raise ValueError(funcname + ': There is NO mask to show when \"mtype\" = 0 !')
		
	###bugs
	if mtype == 5 or mtype2 == 5:
		raise ValueError(funcname + ': \"mtype\" or \"mtype2\" = 5 (Roberts) unavailable now !')
		
	###################################
	###  Small functions ##############
	###################################
	
	# average two clips of 3 yuv planes
	def average(clipa, clipb):
		return (core.std.Expr(clips=[clipa,clipb], expr=["x y + 2 /"]))
		
	# bitdepth conversion from mvsfunc, mawen1250 Thanks!
	def Depth(input, depth=None):
		sbitPS = input.format.bits_per_sample
		if sbitPS == depth:
			return input
		else:
			return core.fmtc.bitdepth(input,bits=depth,flt=0,dmode=3)
			
	# fast PointResize from mvsfunc
	def PointPower(input, vpow=1):
		for i in range(vpow):
			clip = core.std.Interleave([input,input]).std.DoubleWeave(tff=True).std.SelectEvery(2,0)
		return clip
		
	
	###################################
	
	# src clip issue
	#======================
	if src == None:
		if predown:
			if lsb:
				src = core.nnedi3.nnedi3(core.fmtc.resample(input, w=downw4, h=downh4,kernel="spline36"),field=1,dh=True)
				src = core.std.Transpose(core.fmtc.resample(src,w=downw4,h=h,sx=0,sy=[-0.5,-0.5*(1<<input.format.subsampling_h)],kernel="spline36"))
				src = core.std.Transpose(core.fmtc.resample(core.nnedi3.nnedi3(src,field=1,dh=True),w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<input.format.subsampling_h)],kernel="spline36"))
			else:
				src = core.nnedi3.nnedi3(Depth(core.fmtc.resample(input, w=downw4, h=downh4,kernel="spline36"),8),field=1,dh=True)
				src = core.std.Transpose(core.fmtc.resample(src,w=downw4,h=h,sx=0,sy=[-0.5,-0.5*(1<<input.format.subsampling_h)],kernel="spline36"))
				src = core.std.Transpose(core.fmtc.resample(core.nnedi3.nnedi3(Depth(src,8),field=1,dh=True),w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<input.format.subsampling_h)],kernel="spline36"))
		else:
			src = input
	#======================
	

	
	#internal function
	def TAAmbk_prepass(clip, predown=predown, downw4=downw4, downh4=downh4, thin=0, dark=0, preaa=preaa):
		if predown:
			pdclip = core.resize.Spline(clip, downw4, downh4)
		else:
			pdclip = clip
		
		if preaa == 1:
			if lsb:
				nn = core.nnedi3.nnedi3(pdclip, field=3)
				nnt = core.std.Transpose(core.nnedi3.nnedi3(core.std.Transpose(pdclip), field=3))
			else:
				nn = core.nnedi3.nnedi3(Depth(pdclip,8), field=3)
				nnt = core.std.Transpose(core.nnedi3.nnedi3(Depth(core.std.Transpose(pdclip),8), field=3))
			#nnedi3 double rate start with top
			clph = average(core.std.SelectEvery(nn, cycle=2, offsets=0), core.std.SelectEvery(nn, cycle=2, offsets=1))
			clpv = average(core.std.SelectEvery(nnt, cycle=2, offsets=0), core.std.SelectEvery(nnt, cycle=2, offsets=1))
			clp = average(clph, clpv)
			
			preaaB = clp
		else:
			preaaB = pdclip
		preaaC = preaaB
		#filters unavailable
		#=======================================
		# if thin == 0 and dark == 0:
			# preaaC = preaaB
		
		# else:
			# if dark == 0:
				# preaaC = core.warp.AWarpSharp2(preaaB,depth=thin)
			# elif thin == 0:
				# preaaC = Toon(preaaB,dark) #?
			# else:
				# preaaC = Toon(core.warp.AWarpSharp2(preaaB,depth=thin),dark)  #?
		#=======================================
		
		
		return preaaC
		
		
		
		
		
	#internal functions
	def TAAmbk_mainpass(preaaC, aatype=aatype, cycle=cycle, p1=p1, p2=p2, p3=p3, p4=p4, p5=p5, p6=p6, w=w, h=h,
						uph4=uph4, upw4=upw4, eedi3sclip=eedi3sclip):
		# generate eedi3 sclip using nnedi3 double height				
		if eedi3sclip is True:
			if aatype == -2:
				if lsb:
					sclip = core.nnedi3.nnedi3(preaaC,field=1,dh=True)
					sclip_r = core.resize.Spline(sclip,w,uph4)
					sclip_r = core.std.Transpose(sclip_r)
					sclip_r = core.nnedi3.nnedi3(sclip_r,field=1,dh=True)
					sclip = Depth(sclip,8)
					sclip_r = Depth(sclip_r,8)
				else:
					sclip = core.nnedi3.nnedi3(Depth(preaaC,8),field=1,dh=True)
					sclip_r = core.resize.Spline(sclip,w,uph4)
					sclip_r = core.std.Transpose(sclip_r)
					sclip_r = core.nnedi3.nnedi3(sclip_r,field=1,dh=True)
			elif aatype == 2:
				if lsb:
					sclip = core.nnedi3.nnedi3(preaaC,field=1,dh=True)
					sclip_r = sclip_r = core.resize.Spline(sclip,w,h)
					sclip_r = core.std.Transpose(sclip_r)
					sclip_r = core.nnedi3.nnedi3(sclip_r,field=1,dh=True)
					sclip = Depth(sclip,8)
					sclip_r = Depth(sclip_r,8)
				else:
					sclip = core.nnedi3.nnedi3(Depth(preaaC,8),field=1,dh=True)
					sclip_r = sclip_r = core.resize.Spline(sclip,w,h)
					sclip_r = core.std.Transpose(sclip_r)
					sclip_r = core.nnedi3.nnedi3(sclip_r,field=1,dh=True)
		
		# generate aa_clip
		##########################
		# # # AAtype -3 or 4 # # #
		##########################
		if aatype == -3 or aatype == 4:
			if lsb:
				aa_clip = core.nnedi3.nnedi3(preaaC, dh=True, field=1, nsize=int(p2), nns=int(p3), qual=int(p4))
				aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=w,h=uph4,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"))
				aa_clip = core.fmtc.resample(core.nnedi3.nnedi3(aa_clip, dh=True, field=1, nsize=int(p2), nns=int(p3), qual=int(p4)),w=uph4,h=upw4,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = Depth(aa_clip,depth=8)
				aa_clip = core.sangnom.SangNomMod(core.std.Transpose(core.sangnom.SangNomMod(aa_clip,aa=int(p1))),aa=int(p1))
				aa_clip = core.fmtc.resample(aa_clip,w=w,h=h,kernel=["spline36","spline36"])
			else:
				aa_clip = core.nnedi3.nnedi3(Depth(preaaC,8), dh=True, field=1, nsize=int(p2), nns=int(p3), qual=int(p4))
				aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=w,h=uph4,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"))
				aa_clip = core.fmtc.resample(core.nnedi3.nnedi3(Depth(aa_clip,8), dh=True, field=1, nsize=int(p2), nns=int(p3), qual=int(p4)),w=uph4,h=upw4,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = Depth(aa_clip,depth=8)
				aa_clip = core.sangnom.SangNomMod(core.std.Transpose(core.sangnom.SangNomMod(aa_clip,aa=int(p1))),aa=int(p1))
				aa_clip = core.fmtc.resample(aa_clip,w=w,h=h,kernel=["spline36","spline36"])
		######################
		# # # AA type -2 # # #
		######################
		elif aatype == -2:
			if eedi3sclip == False:
					
				aa_clip = core.fmtc.resample(core.eedi3.eedi3(Depth(preaaC,8), dh=True, field=1, alpha=p2, beta=p3, gamma=p4, nrad=int(p5), mdis=int(p6)), w=w, h=uph4, sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = Depth(aa_clip,depth=8)
				aa_clip = core.eedi3.eedi3(core.std.Transpose(aa_clip), dh=True, field=1, alpha=p2, beta=p3, gamma=p4, nrad=int(p5), mdis=int(p6))
				aa_clip = core.sangnom.SangNomMod(Depth(core.fmtc.resample(aa_clip, w=uph4, h=upw4, sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"),depth=8),aa=int(p1))
				aa_clip = core.sangnom.SangNomMod(core.std.Transpose(aa_clip),aa=int(p1))
				aa_clip = core.fmtc.resample(aa_clip,w=w,h=h,kernel=["spline36","spline36"])
			else:
				# EEDI3 need w * h
				aa_clip = core.fmtc.resample(core.eedi3.eedi3(Depth(preaaC,8), dh=True, field=1, alpha=p2, beta=p3, gamma=p4, nrad=int(p5), mdis=int(p6), sclip=sclip), w=w, h=uph4, sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				# output w * uph4
				aa_clip = Depth(aa_clip,depth=8)
				# EEDI3 need uph4 * w
				aa_clip = core.eedi3.eedi3(core.std.Transpose(aa_clip), dh=True, field=1, alpha=p2, beta=p3, gamma=p4, nrad=int(p5), mdis=int(p6), sclip=sclip_r)
				aa_clip = core.sangnom.SangNomMod(Depth(core.fmtc.resample(aa_clip, w=uph4, h=upw4, sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"),depth=8),aa=int(p1))
				aa_clip = core.sangnom.SangNomMod(core.std.Transpose(aa_clip),aa=int(p1))
				aa_clip = core.fmtc.resample(aa_clip,w=w,h=h,kernel=["spline36","spline36"])
		######################
		# # # AA type -1 # # #
		######################
		elif aatype == -1:
			aa_clip = core.fmtc.resample(core.eedi2.EEDI2(preaaC, field=1, mthresh=int(p2), lthresh=int(p3), vthresh=int(p4), maxd=int(p5), nt=int(p6)),w=w,h=uph4,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
			aa_clip = core.eedi2.EEDI2(core.std.Transpose(aa_clip),field=1, mthresh=int(p2), lthresh=int(p3), vthresh=int(p4), maxd=int(p5), nt=int(p6))
			aa_clip = core.sangnom.SangNomMod(Depth(core.fmtc.resample(aa_clip,w=uph4,h=upw4,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"),depth=8),aa=int(p1))
			aa_clip = core.sangnom.SangNomMod(core.std.Transpose(aa_clip),aa=int(p1))
			aa_clip = core.fmtc.resample(aa_clip,w=w,h=h,kernel=["spline36","spline36"])
		######################
		# # # AA type 1  # # #
		######################
		elif aatype == 1:
			aa_clip = core.fmtc.resample(core.eedi2.EEDI2(preaaC,field=1,mthresh=int(p1), lthresh=int(p2), vthresh=int(p3), maxd=int(p4), nt=int(p5)),w=w,h=h,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
			aa_clip = core.eedi2.EEDI2(core.std.Transpose(aa_clip),field=1,mthresh=int(p1), lthresh=int(p2), vthresh=int(p3), maxd=int(p4), nt=int(p5))
			aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"))
		######################
		# # # AA type 2  # # #
		######################
		elif aatype == 2:
			if eedi3sclip == False:
				aa_clip = core.fmtc.resample(core.eedi3.eedi3(Depth(preaaC,8),dh=True, field=1, alpha=p1, beta=p2, gamma=p3, nrad=int(p4), mdis=int(p5)),w=w,h=h,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = Depth(core.std.Transpose(aa_clip),depth=8)
				aa_clip = core.fmtc.resample(core.eedi3.eedi3(aa_clip,dh=True, field=1, alpha=p1, beta=p2, gamma=p3, nrad=int(p4), mdis=int(p5)),w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = core.std.Transpose(aa_clip)
			else:
				#EEDI3 need w * h
				aa_clip = core.fmtc.resample(core.eedi3.eedi3(Depth(preaaC,8),dh=True, field=1, alpha=p1, beta=p2, gamma=p3, nrad=int(p4), mdis=int(p5), sclip=sclip),w=w,h=h,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				#output w * h
				aa_clip = Depth(core.std.Transpose(aa_clip),depth=8)
				#EEDI3 need h * w
				aa_clip = core.fmtc.resample(core.eedi3.eedi3(aa_clip,dh=True, field=1, alpha=p1, beta=p2, gamma=p3, nrad=int(p4), mdis=int(p5), sclip=sclip_r),w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = core.std.Transpose(aa_clip)
		######################
		# # # AA type 3  # # #
		######################
		elif aatype == 3:
			if lsb:
				aa_clip = core.fmtc.resample(core.nnedi3.nnedi3(preaaC, dh=True, field=1, nsize=int(p1), nns=int(p2), qual=int(p3)),w=w,h=h,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = core.nnedi3.nnedi3(core.std.Transpose(aa_clip), dh=True, field=1, nsize=int(p1), nns=int(p2), qual=int(p3))
				aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"))
			else:
				aa_clip = core.fmtc.resample(core.nnedi3.nnedi3(Depth(preaaC,8), dh=True, field=1, nsize=int(p1), nns=int(p2), qual=int(p3)),w=w,h=h,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
				aa_clip = core.nnedi3.nnedi3(Depth(core.std.Transpose(aa_clip),8), dh=True, field=1, nsize=int(p1), nns=int(p2), qual=int(p3))
				aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"))
		######################
		# # # AA type 5  # # #
		######################
		elif aatype == 5:
			aa_clip = Depth(core.fmtc.resample(preaaC, w=upw4, h=uph4 ,kernel=["lanczos","bicubic"]),depth=8)
			aa_clip = core.std.Transpose(core.sangnom.SangNomMod(aa_clip,aa=int(p1)))
			aa_clip = core.fmtc.resample(core.sangnom.SangNomMod(aa_clip,aa=int(p1)),w=h,h=w,kernel="spline36")
			aa_clip = core.std.Transpose(aa_clip)
		######################
		# # # AA type 6  # # #
		######################
		elif aatype == 6:
			aa_clip = Depth(core.fmtc.resample(preaaC, w=w, h=uph4 ,kernel=["lanczos","bicubic"]),depth=8)
			if mod16w is True:
				aa_clip = core.fmtc.resample(core.sangnom.SangNomMod(aa_clip,aa=int(p1)),w=w,h=h,kernel="spline36")
			else:
				aa_clip = core.std.AddBorders(aa_clip,borderW)
				aa_clip = core.fmtc.resample(core.sangnom.SangNomMod(aa_clip,aa=int(p1)),w=w,h=h,kernel="spline36")
				aa_clip = core.std.CropRel(aa_clip,borderW)
			aa_clip = core.fmtc.resample(core.std.Transpose(aa_clip),w=h,h=upw4,kernel=["lanczos","bicubic"])
			if mod16h is True:
				aa_clip = core.sangnom.SangNomMod(Depth(aa_clip,depth=8),aa=int(p1))
			else:
				aa_clip = core.std.AddBorders(aa_clip,borderH)
				aa_clip = core.sangnom.SangNomMod(Depth(aa_clip,depth=8),aa=int(p1))
				aa_clip = core.std.CropRel(aa_clip,borderH)
			aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=h,h=w,kernel="spline36"))
			aa_clip = core.rgvs.Repair(aa_clip, core.fmtc.resample(preaaC,w=w,h=h,kernel="spline64"), mode=int(p2))
		######################
		# # # AA type 7  # # #
		######################
		elif aatype == 7:
			aa_clip = PointPower(Depth(preaaC,8))
			
			if mod16w and not predown:
				aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
				aa_clip = core.std.Transpose(aa_clip)
			elif predown:
				if aa_clip.width == downw4:
					aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
					aa_clip = core.std.Transpose(aa_clip)
				elif mod16w:
					aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
					aa_clip = core.std.Transpose(aa_clip)
				else:
					aa_clip = core.std.AddBorders(aa_clip,borderW)
					aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
					aa_clip = core.std.CropRel(aa_clip,borderW)
					aa_clip = core.std.Transpose(aa_clip)
			else:
				aa_clip = core.std.AddBorders(aa_clip,borderW)
				aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
				aa_clip = core.std.CropRel(aa_clip,borderW)
				aa_clip = core.std.Transpose(aa_clip)
			aa_clip = PointPower(aa_clip)
			
			if mod16h and not predown:
				aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
			elif predown:
				if aa_clip.width == downh4 * 2:
					aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
				elif mod16h:
					aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
				else:
					aa_clip = core.std.AddBorders(aa_clip,(16 - h * 2 % 16))
					aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
					aa_clip = core.std.CropRel(aa_clip,(16 - h * 2 % 16))
			else:
				aa_clip = core.std.AddBorders(aa_clip,(16 - h * 2 % 16))
				aa_clip = core.sangnom.SangNomMod(aa_clip,aa=int(p1))
				aa_clip = core.std.CropRel(aa_clip,(16 - h * 2 % 16))
			aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=h,h=w,kernel="spline36"))
			
			if predown:
				aa_clip = core.rgvs.Repair(aa_clip, core.fmtc.resample(preaaC,w=w,h=h,kernel="spline64"), mode=int(p2))
			else:
				aa_clip = core.rgvs.Repair(aa_clip, Depth(preaaC,16), mode=int(p2))
			
		# if predown and no aa, use nnedi3 to recover
		else:
			if predown:
				if lsb:
					aa_clip = core.fmtc.resample(core.nnedi3.nnedi3(preaaC,dh=True, field=1, nsize=1, nns=3, qual=2),w=preaaC.width,h=h,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
					aa_clip = core.nnedi3.nnedi3(core.std.Transpose(aa_clip),dh=True, field=1, nsize=1, nns=3, qual=2)
					aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"))
				else:
					aa_clip = core.fmtc.resample(core.nnedi3.nnedi3(Depth(preaaC,8),dh=True, field=1, nsize=1, nns=3, qual=2),w=preaaC.width,h=h,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36")
					aa_clip = core.nnedi3.nnedi3(Depth(core.std.Transpose(aa_clip),8),dh=True, field=1, nsize=1, nns=3, qual=2)
					aa_clip = core.std.Transpose(core.fmtc.resample(aa_clip,w=h,h=w,sx=0,sy=[-0.5,-0.5*(1<<preaaC.format.subsampling_h)],kernel="spline36"))
		
		return aa_clip if cycle == 0 else TAAmbk_mainpass(aa_clip, aatype=aatype ,cycle=cycle-1, p1=p1, p2=p2, p3=p3, p4=p4, p5=p5, p6=p6, w=w, h=h, uph4=uph4, upw4=upw4, eedi3sclip=eedi3sclip)
	
	
	
	
	#Internal functions
	def TAAmbk_mask(input, mtype=mtype, mthr=mthr, w=w, mtype2=mtype2, mthr2=mthr2, auxmthr=auxmthr):
	
		#generate edge_mask_1
		if mtype == 1:
			edge_mask_1 = core.tcanny.TCanny(input, sigma=auxmthr, mode=1, op=2, planes=0)
			exprY = "x "+str(mthr)+" <= x 2 / x 2 * ?"
			if input.format.num_planes == 1:
				edge_mask_1 = core.std.Expr(edge_mask_1, expr=[exprY])
			else:
				edge_mask_1 = core.std.Expr(edge_mask_1, expr=[exprY,""])
			if w > 1100:
				edge_mask_1 = core.rgvs.RemoveGrain(edge_mask_1, mode=20)
			else:
				edge_mask_1 = core.rgvs.RemoveGrain(edge_mask_1, mode=11)
			edge_mask_1 = core.generic.Inflate(edge_mask_1, planes=0)
		elif mtype == 3:
			edge_mask_1 = core.generic.TEdge(input, min=auxmthr, planes=0)
			exprY = "x "+str(mthr//5)+" <= x 2 / x 16 * ?"
			if input.format.num_planes == 1:
				edge_mask_1 = core.std.Expr(edge_mask_1, expr=[exprY])
			else:
				edge_mask_1 = core.std.Expr(edge_mask_1, expr=[exprY,""])
			edge_mask_1 = core.generic.Deflate(edge_mask_1, planes=0)
			if w > 1100:
				edge_mask_1 = core.rgvs.RemoveGrain(edge_mask_1, mode=20)
			else:
				edge_mask_1 = core.rgvs.RemoveGrain(edge_mask_1, mode=11)
		elif mtype == 2:
			edge_mask_1 = core.msmoosh.MSharpen(input, threshold=mthr//5, strength=0, mask=True, planes=0)
		elif mtype == 4:
			edge_mask_1 = core.generic.Sobel(input, min=5, max=7, planes=0)
			edge_mask_1 = core.generic.Inflate(edge_mask_1, planes=0)
		elif mtype == 5:
		#=======================
			edge_mask_1 = input # roberts kernel unavailable
			edge_mask_1 = core.generic.Inflate(edge_mask_1, planes=0)
		#=======================
		elif mtype == 6:
			edge_mask_1 = core.generic.Prewitt(input, min=0, max=255, planes=0)
			exprY = "x "+str(mthr)+" <= x 2 / x 2.639015821545 * ?"
			if input.format.num_planes == 1:
				edge_mask_1 = core.std.Expr(edge_mask_1, expr=[exprY])
			else:
				edge_mask_1 = core.std.Expr(edge_mask_1, expr=[exprY,""])
			edge_mask_1 = core.rgvs.RemoveGrain(edge_mask_1, mode=4)
			edge_mask_1 = core.generic.Inflate(edge_mask_1, planes=0)
		else:
			edge_mask_1 == None
			
		#generate edge_mask_2
		if mtype2 == 0:
			edge_mask_2 = None
		elif mtype2 == 1:
			edge_mask_2 = core.tcanny.TCanny(input, sigma=1.2, mode=1, op=0, planes=0)
			exprY = "x "+str(mthr2)+" <= x 2 / x 2 * ?"
			if input.format.num_planes == 1:
				edge_mask_2 = core.std.Expr(edge_mask_2, expr=[exprY])
			else:
				edge_mask_2 = core.std.Expr(edge_mask_2, expr=[exprY,""])
			if w > 1100:
				edge_mask_2 = core.rgvs.RemoveGrain(edge_mask_2, mode=20)
			else:
				edge_mask_2 = core.rgvs.RemoveGrain(edge_mask_2, mode=11)
			edge_mask_1 = core.generic.Inflate(edge_mask_2, planes=0)
		elif mtype2 == 3:
			edge_mask_2 = core.generic.TEdge(input, planes=0)
			exprY = "x "+str(mthr2//5)+" <= x 2 / x 16 * ?"
			if input.format.num_planes == 1:
				edge_mask_2 = core.std.Expr(edge_mask_2, expr=[exprY])
			else:
				edge_mask_2 = core.std.Expr(edge_mask_2, expr=[exprY,""])
			edge_mask_2 = core.generic.Deflate(edge_mask_2, planes=0)
			if w > 1100:
				edge_mask_2 = core.rgvs.RemoveGrain(edge_mask_2, mode=20)
			else:
				edge_mask_2 = core.rgvs.RemoveGrain(edge_mask_2, mode=11)
		elif mtype2 == 2:
			edge_mask_2 = core.msmoosh.MSharpen(input, threshold=mthr2//5, strength=0, mask=True, planes=0)
		elif mtype2 == 4:
			edge_mask_2 = core.generic.Sobel(input, min=5, max=7, planes=0)
			edge_mask_2 = core.generic.Inflate(edge_mask_2, planes=0)
		elif mtype2 == 5:
		#=======================
			edge_mask_2 = input
			edge_mask_2 = core.generic.Inflate(edge_mask_2, planes=0)
		#=======================
		else:
			edge_mask_2 = core.generic.Prewitt(input, min=0, max=255, planes=0)
			exprY = "x "+str(mthr2)+" <= x 2 / x 2.639015821545 * ?"
			edge_mask_2 = core.std.Expr(edge_mask_2, expr=[exprY])
			edge_mask_2 = core.rgvs.RemoveGrain(edge_mask_2, mode=4)
			edge_mask_2 = core.generic.Inflate(edge_mask_2, planes=0)
			
		#generate final_mask
		if mtype2 == 0:
			final_mask = edge_mask_1
		else:
			final_mask = core.std.Expr(clips=[edge_mask_1,edge_mask_2], expr=["x y max"])
			
		return final_mask
	
	
	#temporal stabilizer of sharped clip
	def Soothe(sharp, origin, keep=24):
		if keep > 100:
			keep = 100
		if keep < 0:
			keep = 0
		KP = str(keep)
		diff = core.std.Expr(clips=[origin,sharp], expr=["x y - 128 +"])
		diff2 = core.focus.TemporalSoften(diff, radius=1, luma_threshold=255, chroma_threshold=255, scenechange=32, mode=2)
		expr = "x 128 - y 128 - * 0 < x 128 - 100 / "+KP+" * 128 + x 128 - abs y 128 - abs > x "+KP+" * y 100 "+KP+" - * + 100 / x ? ?"
		diff3 = core.std.Expr(clips=[diff,diff2], expr=[expr])
		return core.std.Expr(clips=[origin,diff3], expr=["x y 128 - -"])
		
	#internal functions
	def TAAmbk_stabilize(input, aaedsharp, stabilize):
		aadiff = core.std.MakeDiff(Depth(input,16), aaedsharp)
		if(stabilize < 0):
			aadiff_stab = core.rgvs.Repair(core.focus.TemporalSoften(aadiff,abs(stabilize), 255, 255, 254, 2),aadiff,4)
		else:
			inputsuper = core.mv.Super(input,pel=1)
			diffsuper = core.mv.Super(aadiff,pel=1,levels=1)
			if stabilize == 3:
				fv3 = core.mv.Analyse(inputsuper,isb=False,delta=3,overlap=8,blksize=16)
				bv3 = core.mv.Analyse(inputsuper,isb=True,delta=3,overlap=8,blksize=16)
			if stabilize >= 2:
				fv2 = core.mv.Analyse(inputsuper,isb=False,delta=2,overlap=8,blksize=16)
				bv2 = core.mv.Analyse(inputsuper,isb=True,delta=2,overlap=8,blksize=16)
			if stabilize >= 1:
				fv1 = core.mv.Analyse(inputsuper,isb=False,delta=1,overlap=8,blksize=16)
				bv1 = core.mv.Analyse(inputsuper,isb=True,delta=1,overlap=8,blksize=16)
				
			if stabilize == 1:
				stabilized_diff = core.mv.Degrain1(aadiff,diffsuper,bv1,fv1)
			elif stabilize == 2:
				stabilized_diff = core.mv.Degrain2(aadiff,diffsuper,bv1,fv1,bv2,fv2)
			elif stabilize == 3:
				stabilized_diff = core.mv.Degrain3(aadiff,diffsuper,bv1,fv1,bv2,fv2,bv3,fv3)
			else:
				stabilized_diff = None
			aadiff_stab = core.std.Expr(clips=[aadiff,stabilized_diff], expr=["x 128 - abs y 128 - abs < x y ?"])
			if aadiff_stab.format.num_planes == 1:
				aadiff_stab = core.std.Merge(aadiff_stab, stabilized_diff, weight=[0.6])
			else:
				aadiff_stab = core.std.Merge(aadiff_stab, stabilized_diff, weight=[0.6,0])
		aaed_stab = core.std.MakeDiff(Depth(input,16), aadiff_stab)
		
		return aaed_stab
	#==============================		
	#main functions
	#==============================
	preaaC = TAAmbk_prepass(input, predown=predown, downw4=downw4, downh4=downh4, preaa=preaa)
	
	aa_clip = TAAmbk_mainpass(preaaC,aatype=aatype, cycle=cycle, p1=p1, p2=p2, p3=p3, p4=p4, p5=p5, p6=p6, w=w, h=h, uph4=uph4, upw4=upw4, eedi3sclip=eedi3sclip)
	
	#sharp
	if sharp == 0:
		aaedsp = aa_clip
	elif sharp >= 1:
		aaedsp = haf.LSFmod(aa_clip,strength=int(absSh), defaults="old", source=Depth(src,16))
	elif sharp > 0:
		per = int(40*absSh)
		matrix = [-1, -2, -1, -2, 52-per , -2, -1, -2, -1]
		aaedsp = core.generic.Convolution(aa_clip,matrix)
	elif sharp > -1:
		aaedsp = haf.LSFmod(aa_clip,strength=round(absSh*100), defaults="fast", source=Depth(src,16))
	elif sharp == -1:
		if w > 1100:
			clipb = core.std.MakeDiff(aa_clip, core.rgvs.RemoveGrain(aa_clip, mode=20))
		else:
			clipb = core.std.MakeDiff(aa_clip, core.rgvs.RemoveGrain(aa_clip, mode=11))
		clipb = core.rgvs.Repair(clipb, core.std.MakeDiff(Depth(src,16), aa_clip),mode=13)
		aaedsp = core.std.MergeDiff(aa_clip, clipb)
	else:
		aaedsp = haf.LSFmod(aa_clip,strength=int(absSh), defaults="slow", source=Depth(src,16))
	#postAA
	if postaa:
		aaedsp = Soothe(aaedsp,aa_clip,keep=48)
		
	#stabilize
	if stabilize != 0:
		aaedstab = TAAmbk_stabilize(input, aaedsp, stabilize)
	else:
		aaedstab = aaedsp
	#masked merge
	if isinstance(mtype, vs.VideoNode):
		edge_mask = mtype
		aamerge = core.std.MaskedMerge(Depth(input,16),aaedstab,Depth(edge_mask,16),first_plane=True)
	elif mtype != 0:
		edge_mask = TAAmbk_mask(input, mtype=mtype, mthr=mthr, w=w, mtype2=mtype2, mthr2=mthr2, auxmthr=auxmthr)
		aamerge = core.std.MaskedMerge(Depth(input,16),aaedstab,Depth(edge_mask,16),first_plane=True)
	else:
		aamerge = aaedstab
	# output
	if showmask:
		return edge_mask
	else:
		if repair == 0 or aatype == 0:
			return aamerge
		elif(repair > 0):
			return core.rgvs.Repair(aamerge, Depth(input,depth=16), mode=repair)
		else:
			return core.rgvs.Repair(Depth(input,depth=16), aamerge, mode=abs(repair))

