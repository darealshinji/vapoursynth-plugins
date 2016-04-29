import vapoursynth as vs

def TempLinearApproximateMC(clip, meclip = None, radius = 2, planes = None, subpel = 4, subpelinterp = None, dct = None, refine = False, blocksize = None,
	overlap = None, search = None, searchparam = None, pelsearch = None, chromamotion = True, truemotion = True, _lambda = None, lsad = None, pnew = None,
	plevel = None, globalmotion = True, thsad = None, thscd1 = None, thscd2 = None, badrange = None, isse = None, gamma = None):
	#-------------------------------------------------------------------------------------------------------------------------------------
	
	core = vs.get_core()
	
	if not isinstance(radius, int) or radius < 1:
		raise ValueError("MCDenoise.TempLinearApproximate: 'radius' must be an integer larger than 1.")
		
	mv = core.mv
	if clip.format.sample_type == vs.FLOAT:
		mv = core.mvsf
		
	#-------------------------------------------------------------------------------------------------------------------------------------
	
	tla_arguments = {"radius": radius, "planes": planes, "gamma": gamma}
	super_arguments = {"pel": subpel, "sharp": subpelinterp}
	compensate_arguments = {"thsad":thsad, "thscd1":thscd1, "thscd2":thscd2}
	recalculate_arguments = {"chroma":chromamotion, "search":search,  "searchparam":searchparam, "truemotion":truemotion, "lambda":_lambda}
	analyse_arguments = {
		"chroma":chromamotion, 
		"search":search, 
		"searchparam":searchparam, 
		"truemotion":truemotion,
		"lambda":_lambda,  
		"pelsearch":pelsearch,
		"lsad":lsad,
		"pnew":pnew,
		"plevel":plevel,
		"global":globalmotion,
		"badrange":badrange,
	}
	
	if not refine:
		analyse_arguments['blksize'] = blocksize
		analyse_arguments['overlap'] = overlap
	
	if dct is not None:
		analyse_arguments['dct'] = dct
		recalculate_arguments['dct'] = dct
		
	if isse is not None:
		analyse_arguments['isse'] = isse
		
	analyse_arguments_32 = analyse_arguments
	
	input_super = mv.Super(clip, **super_arguments)
	if refine and dct is not None:
		if dct == 5:
			analyse_arguments_32['dct'] = 0
		elif dct == 6:
			analyse_arguments_32['dct'] = 2
		elif dct == 7:
			analyse_arguments_32['dct'] = 3
		elif dct == 8:
			analyse_arguments_32['dct'] = 4
		elif dct == 9:
			analyse_arguments_32['dct'] = 2
		elif dct == 10:
			analyse_arguments_32['dct'] = 3
	
	me_super = input_super if not meclip else mv.Super(meclip, **super_arguments)
		
	#-------------------------------------------------------------------------------------------------------------------------------------
	def compensate(isb, delta):
		if refine:
			vector = mv.Analyse(me_super, isb=isb, delta=delta, blksize=32, overlap=16, **analyse_arguments_32)
			vector = mv.Recalculate(me_super, vector, blksize=16, overlap=8, **recalculate_arguments)
			vector = mv.Recalculate(me_super, vector, blksize=8, overlap=4, **recalculate_arguments)
			vector = mv.Recalculate(me_super, vector, blksize=4, overlap=2, **recalculate_arguments)
		elif blocksize == 32:
			vector = mv.Analyse(me_super, isb=isb, delta=delta, **analyse_arguments_32)
		else:
			vector = mv.Analyse(me_super, isb=isb, delta=delta, **analyse_arguments)
		return mv.Compensate(clip, input_super, vector, **compensate_arguments)

	compensate_back = [compensate(True, i) for i in range(radius, 0, -1)]
	compensate_forward = [compensate(False, i) for i in range(1, radius+1)]
	
	sequence = compensate_back + [clip] + compensate_forward
	interleaved = core.std.Interleave(sequence)		 
	filtered = core.tla.TempLinearApproximate(interleaved, **tla_arguments)
	
	return core.std.SelectEvery(filtered, radius * 2 + 1, radius)