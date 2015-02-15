import vapoursynth as vs

def TempLinearApproximateMC(clip, meclip = None, radius = 2, planes = None, subpel = 4, subpelinterp = None, dct = None, refine = False, blocksize = None,
	overlap = None, search = None, searchparam = None, pelsearch = None, chromamotion = True, truemotion = True, _lambda = None, lsad = None, pnew = None,
	plevel = None, globalmotion = True, thsad = None, thscd1 = None, thscd2 = None, badrange = None, isse = None, gamma = None):
	#-------------------------------------------------------------------------------------------------------------------------------------
	
	core = vs.get_core()
	
	if not isinstance(clip, vs.VideoNode) or clip.format.id != vs.YUV420P8:
		raise ValueError('MCDenoise.TempLinearApproximate: Input format must be YUV420P8.')
	if not isinstance(radius, int) or radius < 1:
		raise ValueError("MCDenoise.TempLinearApproximate: 'radius' must be larger than 1.")
		
	#-------------------------------------------------------------------------------------------------------------------------------------
	
	tla_arguments = {"radius": radius, "planes": planes, "gamma": gamma}
	super_arguments = {"pel": subpel, "sharp": subpelinterp}
	compensate_arguments = {"thsad":thsad, "thscd1":thscd1, "thscd2":thscd2}
	recalculate_arguments = {"chroma":chromamotion, "search":search,  "searchparam":searchparam, "truemotion":truemotion, "lambda":_lambda}
	analyze_arguments = {
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
		"isse":isse
	}
	
	if not refine:
		analyze_arguments['blksize'] = blocksize
		analyze_arguments['overlap'] = overlap
	
	if dct is not None:
		analyze_arguments['dct'] = dct
		recalculate_arguments['dct'] = dct
	
	input_super = core.mv.Super(clip, **super_arguments)
	me_super = input_super if not meclip else core.mv.Super(meclip, **super_arguments)
		
	#-------------------------------------------------------------------------------------------------------------------------------------
	def compensate(isb, delta):
		if refine:
			vector = core.mv.Analyse(me_super, isb=isb, delta=delta, blksize=32, overlap=16, **analyze_arguments)
			vector = core.mv.Recalculate(me_super, vector, blksize=16, overlap=8, **recalculate_arguments)
			vector = core.mv.Recalculate(me_super, vector, blksize=8, overlap=4, **recalculate_arguments)
			vector = core.mv.Recalculate(me_super, vector, blksize=4, overlap=2, **recalculate_arguments)
		else:
			vector = core.mv.Analyse(me_super, isb=isb, delta=delta, **analyze_arguments)
		return core.mv.Compensate(clip, input_super, vector, **compensate_arguments)

	compensate_back = [compensate(True, i) for i in range(radius, 0, -1)]
	compensate_forward = [compensate(False, i) for i in range(1, radius+1)]
	
	sequence = compensate_back + [clip] + compensate_forward
	interleaved = core.std.Interleave(sequence)		 
	filtered = core.tla.TempLinearApproximate(interleaved, **tla_arguments)
	
	return core.std.SelectEvery(filtered, radius * 2 + 1, radius)