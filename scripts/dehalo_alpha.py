# dehalo_alpha.py (2012-11-29)
# requirements: mt_masktools.dll(avs)
#               fmtconv.dll(vs)
#               Repair.dll(avs)

import vapoursynth as vs

def clamp(minimum, x, maximum):
    return int(max(minimum, min(round(x), maximum)))

def m4(x):
	res = lambda x: 16 if x < 16 else int(round(x / 4.0) * 4)
	return res(x)

class InvalidArgument(Exception):
	def __init__(self, value):
		self.value = value
	def __str__(self):
		return repr(self.value)

class DeHalo_alpha(object):
	def __init__(self, core):
		self.std       = core.std
		self.repair    = core.avs.Repair
		self.expand    = core.avs.mt_expand
		self.inpand    = core.avs.mt_inpand
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
	
	def mt_logic(self, c1, c2, mode, th1=0, th2=0, planes=0):
		if mode == 'min':
			expr = lambda x, y: min(x + th1, y + th2)
		elif mode == 'max':
			expr = lambda x, y: max(x + th1, y + th2)
		else:
			raise InvalidArgument('%s is not a valid mode for mt_logic' % mode)
		return self.mt_lutxy(c1, c2, expr, planes)
	
	def dehalo_alpha(self, src, rx=2.0, ry=2.0, darkstr=1.0, brightstr=1.0, lowsens=50, highsens=50, ss=1.5):
		his  = highsens / 100.0
		ox   = src.width
		oy   = src.height
		
		self.max = 2 ** src.format.bits_per_sample - 1
		self.mid = self.max // 2 + 1
		self.lut_range = range(self.max + 1)
		
		this   = self.resample(clip=src, w=m4(ox/rx), h=m4(oy/ry), kernel='bicubic')
		halos  = self.bitdepth(self.resample(clip=this, w=ox, h=oy, kernel='bicubic', a1=1, a2=0), csp=vs.YUV420P8)
		are    = self.mt_lutxy(self.expand(src, U=1, V=1), self.inpand(src, U=1, V=1), lambda x, y: x - y)
		ugly   = self.mt_lutxy(self.expand(halos, U=1, V=1), self.inpand(halos, U=1, V=1), lambda x, y: x - y)
		so     = self.mt_lutxy(ugly, are, lambda x, y: ((((y - x) / (y + 0.001)) * 255) - lowsens) * (((y + 256) / 512) + his))
		lets   = self.std.MaskedMerge([halos, src], so)
		if ss == 1:
			remove = self.repair(src, lets, 1, 0)
		else:
			remove = self.bitdepth(self.resample(clip=src, w=m4(ox*ss), h=m4(oy*ss)), csp=vs.YUV420P8)
			remove = self.mt_logic(remove, self.bitdepth(self.resample(self.expand(lets, U=1, V=1), m4(ox * ss), m4(oy * ss)), csp=vs.YUV420P8), 'min')
			remove = self.mt_logic(remove, self.bitdepth(self.resample(self.inpand(lets, U=1, V=1), m4(ox * ss), m4(oy * ss)), csp=vs.YUV420P8), 'max')
			remove = self.bitdepth(self.resample(remove, ox, oy), csp=vs.YUV420P8)
		them   = self.mt_lutxy(src, remove, lambda x, y: x - ((x - y) * darkstr) if x < y else x - ((x - y) * brightstr))
		
		return them
	
	def usage(self):
		usage = '''
		Reduce halo artifacts that can occur when sharpening.
		
		dehalo_alpha(src, rx=2.0, ry=2.0, darkstr=1.0, brightstr=1.0, lowsens=50.0, highsens=50.0, ss=1.5)
			rx, ry [float, 1.0 ... 2.0 ... ~3.0]
				As usual, the radii for halo removal. Note: this function is rather sensitive to the radius settings.
				Set it as low as possible! If radius is set too high, it will start missing small spots.
			darkstr, brightstr [float, 0.0 ... 1.0] [<0.0 and >1.0 possible]
				The strength factors for processing dark and bright halos. Default 1.0 both for symmetrical processing.
				On Comic/Anime, darkstr=0.4~0.8 sometimes might be better ... sometimes.
				In General, the function seems to preserve dark lines rather well.
			lowsens, highsens [int, 0 ... 50 ... 100] 
				Sensitivity settings, not that easy to describe them exactly ... in a sense, they define a window
				between how weak an achieved effect has to be to get fully accepted, and how strong an achieved
				effect has to be to get fully discarded. Defaults are 50 and 50 ... try and see for yourself. 
			ss [float, 1.0 ... 1.5 ...]
				Supersampling factor, to avoid creation of aliasing.
		'''
		return usage