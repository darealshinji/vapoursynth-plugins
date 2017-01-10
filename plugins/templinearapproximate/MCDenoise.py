import vapoursynth as vs

def TempLinearApproximateMC(clip, meclip = None, radius = 2, planes = None, denoise = None, subpel = 4, subpelinterp = None, dct = None, refine = True, blocksize = None, overlap = None, search = None, searchparam = None, pelsearch = None, chromamotion = True, truemotion = True, _lambda = None, lsad = None, pnew = None, plevel = None, globalmotion = True, thsad = None, thscd1 = None, thscd2 = None, badsad = None, badrange = None, trymany = None, gamma = None, ftype = None, sigma = None, sigma2 = None, pmin = None, pmax = None, sbsize = None, smode = None, sosize = None, swin = None, twin = None, sbeta = None, tbeta = None, zmean = None, f0beta = None, nstring = None, sstring = None, ssx = None, ssy = None, sst = None):
	#-------------------------------------------------------------------------------------------------------------------------------------
	
	core = vs.get_core()
	
	if not isinstance(radius, int) or radius < 1:
		raise ValueError("MCDenoise.TempLinearApproximate: 'radius' must be an integer larger than 1.")
		
	if denoise is None:
		denoise = 'tla'
		
	if denoise not in ['tla', 'dfttest']:
		raise ValueError("MCDenoise.TempLinearApproximate: invalid value for 'denoise'. Valid values are: 'tla', 'dfttest'.")
		
	mv = core.mv
	if clip.format.sample_type == vs.FLOAT:
		mv = core.mvsf
		
	#-------------------------------------------------------------------------------------------------------------------------------------
	
	tla_arguments = {'radius': radius, 'planes': planes, 'gamma': gamma}
	dfttest_arguments = {
		'ftype': ftype,
		'sigma': sigma,
		'sigma2': sigma2,
		'pmin': pmin,
		'pmax': pmax,
		'sbsize': sbsize,
		'smode': smode,
		'sosize': sosize,
		'tbsize': radius * 2 + 1,
		'swin': swin,
		'twin': twin,
		'sbeta': sbeta,
		'tbeta': tbeta,
		'zmean': zmean,
		'f0beta': f0beta,
		'nstring': nstring,
		'sstring': sstring,
		'ssx': ssx,
		'ssy': ssy,
		'sst': sst,
		'planes': planes,
	}
	super_arguments = {'pel': subpel, 'sharp': subpelinterp}
	compensate_arguments = {'thsad': thsad, 'thscd1': thscd1, 'thscd2': thscd2}
	recalculate_arguments = {'chroma': chromamotion, 'search': search,  'searchparam': searchparam, 'truemotion': truemotion, 'lambda': _lambda, 'dct': dct}
	analyse_arguments = {
		'chroma': chromamotion, 
		'search': search, 
		'searchparam': searchparam, 
		'truemotion': truemotion,
		'lambda': _lambda,  
		'pelsearch': pelsearch,
		'lsad': lsad,
		'pnew': pnew,
		'plevel': plevel,
		'global': globalmotion,
		'badsad': badsad,
		'badrange': badrange,
		'trymany': trymany,
		'dct': dct,
	}
	
	if not refine:
		analyse_arguments['blksize'] = blocksize
		analyse_arguments['overlap'] = overlap

	input_super = mv.Super(clip, **super_arguments)

	me_super = input_super if not meclip else mv.Super(meclip, **super_arguments)
		
	#-------------------------------------------------------------------------------------------------------------------------------------
	def compensate(isb, delta):
		if refine:
			vector = mv.Analyse(me_super, isb=isb, delta=delta, blksize=32, overlap=16, **analyse_arguments)
			vector = mv.Recalculate(me_super, vector, blksize=16, overlap=8, **recalculate_arguments)
			vector = mv.Recalculate(me_super, vector, blksize=8, overlap=4, **recalculate_arguments)
			vector = mv.Recalculate(me_super, vector, blksize=4, overlap=2, **recalculate_arguments)
		else:
			vector = mv.Analyse(me_super, isb=isb, delta=delta, **analyse_arguments)
		return mv.Compensate(clip, input_super, vector, **compensate_arguments)

	compensate_back = [compensate(True, i) for i in range(radius, 0, -1)]
	compensate_forward = [compensate(False, i) for i in range(1, radius+1)]
	
	sequence = compensate_back + [clip] + compensate_forward
	interleaved = core.std.Interleave(sequence)		 
	if denoise == 'tla':
		filtered = core.tla.TempLinearApproximate(interleaved, **tla_arguments)
	elif denoise == 'dfttest':
		filtered = core.dfttest.DFTTest(interleaved, **dfttest_arguments)
	
	return core.std.SelectEvery(filtered, radius * 2 + 1, radius)