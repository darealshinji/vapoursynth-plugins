import vapoursynth as vs

class MCDenoise():
	
	def __init__(self):
		self.core = vs.get_core()
		
	def TempLinearApproximate(self, input, meclip = None, radius = 2, planes = None, subpel = 4, subpelinterp = None, dct = None, refine = None, blocksize = None,
		overlap = None, search = None, searchparam = None, pelsearch = None, chromamotion = True, truemotion = True, _lambda = None, lsad = None, pnew = None,
		plevel = None, globalmotion = True, thsad = None, thscd1 = None, thscd2 = None, badrange = None, isse = None):
		#-------------------------------------------------------------------------------------------------------------------------------------
		
		if not isinstance(input, vs.VideoNode) or input.format.id != vs.YUV420P8:
			raise ValueError('MCDenoise.TempLinearApproximate: Input format must be YUV420P8.')
		if not isinstance(radius, int) or radius < 1:
			raise ValueError("MCDenoise.TempLinearApproximate: 'radius' must be larger than 1.")
			
		#-------------------------------------------------------------------------------------------------------------------------------------
		
		if refine is None:
			refine = False
		
		tlaArguments = dict(radius=radius)
		superArguments = dict()
		analyzeArguments = dict()
		compensateArguments = dict()
		recalculateArguments = dict()
		
		if planes is not None:
			tlaArguments['planes'] = planes
		if subpel is not None:
			superArguments['pel'] = subpel
		if subpelinterp is not None:
			superArguments['sharp'] = subpelinterp
		if dct is not None:
			analyzeArguments['dct'] = dct
			recalculateArguments['dct'] = dct
		if chromamotion is not None:
			analyzeArguments['chroma'] = chromamotion	
			recalculateArguments['chroma'] = chromamotion
		if (blocksize is not None) and (refine == False):
			analyzeArguments['blksize'] = blocksize
		if (overlap is not None) and (refine == False):
			analyzeArguments['overlap'] = overlap
		if search is not None:
			analyzeArguments['search'] = search
			recalculateArguments['search'] = search
		if searchparam is not None:
			analyzeArguments['searchparam'] = searchparam
			recalculateArguments['searchparam'] = searchparam
		if pelsearch is not None:
			analyzeArguments['pelsearch'] = pelsearch
		if truemotion is not None:
			analyzeArguments['truemotion'] = truemotion
			recalculateArguments['truemotion'] = truemotion
		if _lambda is not None:
			analyzeArguments['lambda'] = _lambda
			recalculateArguments['lambda'] = _lambda
		if lsad is not None:
			analyzeArguments['lsad'] = lsad
		if pnew is not None:
			analyzeArguments['pnew'] = pnew
		if plevel is not None:
			analyzeArguments['plevel'] = plevel
		if globalmotion is not None:
			analyzeArguments['global'] = globalmotion
		if badrange is not None:
			analyzeArguments['badrange'] = badrange
		if thsad is not None:
			compensateArguments['thsad'] = thsad
		if thscd1 is not None:
			compensateArguments['thscd1'] = thscd1
		if thscd2 is not None:
			compensateArguments['thscd2'] = thscd2
		if isse is not None:
			analyzeArguments['isse'] = isse
		
		inputSuper = self.core.mv.Super(input, **superArguments)
		
		if meclip is None:
			MESuper = inputSuper
		else:
			MESuper = self.core.mv.Super(meclip, **superArguments)
			
		#-------------------------------------------------------------------------------------------------------------------------------------
		
		vectorsBack = []
		for i in range(radius, 0, -1):
			if refine:
				vector = self.core.mv.Analyse(MESuper, isb=True, delta=i, blksize=32, overlap=16, **analyzeArguments)
				vector = self.core.mv.Recalculate(MESuper, vector, blksize=16, overlap=8, **recalculateArguments)
				vector = self.core.mv.Recalculate(MESuper, vector, blksize=8, overlap=4, **recalculateArguments)
				vector = self.core.mv.Recalculate(MESuper, vector, blksize=4, overlap=2, **recalculateArguments)
				vectorsBack.append(vector)
			else:
				vectorsBack.append(self.core.mv.Analyse(MESuper, isb=True, delta=i, **analyzeArguments))
				
		vectorsForward = []
		for i in range(1, radius + 1):
			if refine:
				vector = self.core.mv.Analyse(MESuper, isb=False, delta=i, blksize=32, overlap=16, **analyzeArguments)
				vector = self.core.mv.Recalculate(MESuper, vector, blksize=16, overlap=8, **recalculateArguments)
				vector = self.core.mv.Recalculate(MESuper, vector, blksize=8, overlap=4, **recalculateArguments)
				vector = self.core.mv.Recalculate(MESuper, vector, blksize=4, overlap=2, **recalculateArguments)
				vectorsForward.append(vector)
			else:
				vectorsForward.append(self.core.mv.Analyse(MESuper, isb=False, delta=i, **analyzeArguments))
		
		compensateBack = []
		compensateForward = []
		for i in range(0, radius):
			compensateBack.append(self.core.mv.Compensate(input, inputSuper, vectorsBack[i], **compensateArguments))
			compensateForward.append(self.core.mv.Compensate(input, inputSuper, vectorsForward[i], **compensateArguments))
		
		sequence = compensateBack + [input] + compensateForward
		interleaved = self.core.std.Interleave(sequence)			
		filtered = self.core.tla.TempLinearApproximate(interleaved, **tlaArguments)
		
		output = self.core.std.SelectEvery(filtered, radius * 2 + 1, radius)
		return output
		