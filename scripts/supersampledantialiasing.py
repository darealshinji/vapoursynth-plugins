# supersampledantialiasing.py (2012-11-25)
# requirements: mt_masktools.dll(avs)
#               SangNom.dll(avs)
#               fmtconv.dll(vs)
#               RemoveGrain.dll(avs)(opt:sharpen)
#               Repair.dll(avs)(opt:sharpen)

import vapoursynth as vs

def clamp(minimum, x, maximum):
    return int(max(minimum, min(round(x), maximum)))

class SupersampledAntialiasing(object):
	def __init__(self, core):
		self.std       = core.std
		self.rgrain    = core.avs.RemoveGrain
		self.repair    = core.avs.Repair
		self.snom      = core.avs.SangNom
		self.edge      = core.avs.mt_edge
		self.expand    = core.avs.mt_expand
		self.resample  = core.fmtc.resample
		self.bitdepth  = core.fmtc.bitdepth
		self.lut_range = None
		self.max       = 0
		self.mid       = 0
		
	def mt_lutxy(self, c1, c2, expr, planes=0):
		lut = []
		for y in self.lut_range:
			for x in self.lut_range:
				lut.append(clamp(0, expr(x, y), self.max))
		return self.std.Lut2([c1, c2], lut, planes)

	def mt_adddiff(self, c1, c2, planes=0):
		expr = lambda x, y: x + y - self.mid
		return self.mt_lutxy(c1, c2, expr, planes)

	def mt_makediff(self, c1, c2, planes=0):
		expr = lambda x, y: x - y + self.mid
		return self.mt_lutxy(c1, c2, expr, planes)

	def ssaa_BuildMask(self, clip, edgelvl=11, radius=2, thY=255, thC=255, mode='rectangle'):
		srad = radius-1
		
		m = self.edge(clip, 'prewitt', edgelvl, edgelvl, edgelvl, edgelvl, chroma='copy')
		
		if srad > 0:
			if ((mode == 'losange') or ((mode == 'ellipse') and ((srad % 3) != 1))):
				mode_m = 'both'
			else:
				mode_m = 'square'
		else:
			mode_m = ''
		
		if mode_m != '':
			while srad > 0:
				m = self.expand(m, thY, thC, mode_m)
				srad = srad - 1
		
		return m
	
	def ssaa(self, clip, th_mask=11, sharpen=False, smask=False):
		fw   = clip.width
		fh   = clip.height
		
		if clip.format.id != vs.YUV420P8:
			self.bitdepth(clip=clip, csp=vs.YUV420P8)
		
		mask = self.ssaa_BuildMask(clip=clip, edgelvl=th_mask)
		
		aac  = self.bitdepth(clip=self.resample(clip, fw*2, fh*2), csp=vs.YUV420P8)
		aac  = self.std.Transpose(self.snom(c1=aac))
		aac  = self.std.Transpose(self.snom(c1=aac))
		aac  = self.bitdepth(clip=self.resample(aac, fw, fh, kernel='blackman'), csp=vs.YUV420P8)
		
		if sharpen == True:
			self.max = 2 ** clip.format.bits_per_sample - 1
			self.mid = self.max // 2 + 1
			self.lut_range = range(self.max + 1)
			aaD   = self.mt_makediff(clip, aac)
			shrpD = self.mt_makediff(aac, self.rgrain(aac, 20))
			DD    = self.repair(shrpD, aaD, 13)
			aac   = self.mt_adddiff(aac, DD)
			
		if smask == True:
			return mask
		else:
			return self.std.MaskedMerge([clip, aac], mask, planes=0)
	
	def usage(self):
		usage = '''
		Small antialiasing function for 1080p.
		
		ssaa(clip, th_mask=11, sharpen=False, smask=False)
			th_mask: The lower the threshold, more pixels
			         will be antialiased.
			sharpen: performs contra-sharpening on the antialiased zones.
			smask:   shows the zones antialiasing will be applied.
		'''
		return usage