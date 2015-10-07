# EdgeCleaner.py (2013-12-23)
# Dependencies: RemoveGrain (vs)
#		Repair (vs)
#		GeneritFilters (vs)
#		MaskTools2 (avs)
#		deen.dll (avs)
#		aWarpSharp2.dll (avs)

import vapoursynth as vs

def clamp(minimum, x, maximum):
	return int(max(minimum, min(round(x), maximum)))

class EdgeCleaner():
	def __init__(self):
		self.core   = vs.get_core()
		self.max    = 255
		self.mid    = 128
	
	def invert(self, c1, planes=[0]):
		expr = ('{max} x -').format(max=self.max)
		expr = [(i in planes) * expr for i in range(3)]
		return self.core.std.Expr([c1], expr)
	
	def subtract(self, c1, c2, luma=126, planes=[0]):
		expr = ('{luma} x + y -').format(luma=luma)
		expr = [(i in planes) * expr for i in range(3)]
		return self.core.std.Expr([c1, c2], expr)
	
	def starmask(self, src, mode=1):
		if mode == 1:
			clean = self.core.rgvs.RemoveGrain(src, 17)
			diff  = self.core.std.MakeDiff(src, clean)
			final = self.core.resize.Point(self.core.std.ShufflePlanes(diff, 0, vs.GRAY), format=vs.YUV420P8)
			final = self.core.generic.Levels(final, 40, 168, 0.350, 0, 255)
			final = self.core.rgvs.RemoveGrain(final, [7, 0])
			final = self.core.avs.mt_edge(final, 'prewitt', 4, 16, 4, 16)
		else:
			clean  = self.core.rgvs.RemoveGrain(self.core.rgvs.Repair(self.core.avs.deen(src, 'a3d', 4, 12, 0), src, 15), 21)
			pmask  = self.invert(self.core.avs.mt_expand(self.core.avs.mt_edge(src, 'roberts', 0, 2, 0, 2), mode=self.core.avs.mt_circle(1)))
			fmask  = self.core.std.MaskedMerge(clean, src, pmask)
			subt   = self.subtract(fmask, src)
			final  = self.core.generic.Deflate(self.core.avs.mt_edge(subt, 'roberts', 0, 0, 0, 0))
		
		return final

	def edgecleaner(self, src, strength=8, smode=0, rmode=17, rep=True, hot=False):
		smode = clamp(0, smode, 2)
		
		if src.format.id != vs.YUV420P8:
			raise ValueError('Input video format should be YUV420P8.')
		
		if smode != 0:
			strength = strength + 2
		
		main = self.core.avs.aWarpSharp2(src, 128, 1, 0, strength)
		
		if rep == True:
			main = self.core.rgvs.Repair(main, src, rmode)
		
		mask  = self.core.avs.mt_convolution(self.invert(self.core.avs.mt_edge(src, 'prewitt', 4, 32, 4, 32)))
		
		final = self.core.std.MaskedMerge(src, main, mask, planes=0)
		
		if hot == True:
			final = self.core.rgvs.Repair(final, src, 2)
		
		if smode != 0:
			stmask = self.starmask(src, smode)
			final  = self.core.std.MaskedMerge(final, src, stmask)
		
		return final
	
	def usage(self):
		usage = '''
		A simple edge cleaning and weak dehaloing function ported to vapoursynth. Ported from:
		http://pastebin.com/7TCR7W4x
		
		edgecleaner(src, strength=16, rep=True, rmode=17, smode=0, hot=False)
			strength (float)      - specifies edge denoising strength (8.0)
			rep (boolean)         - actives Repair for the aWarpSharped clip (true; requires Repair).
			rmode (integer)       - specifies the Repair mode; 1 is very mild and good for halos, 
						16 and 18 are good for edge structure preserval 
						on strong settings but keep more halos and edge noise,
						17 is similar to 16 but keeps much less haloing,
						other modes are not recommended (17; requires Repair).
			smode (integer)       - specifies what method will be used for finding small particles,
						ie stars; 0 is disabled, 1 uses RemoveGrain and 2 uses Deen 
						(0; requires RemoveGrain/Repair/Deen).
			hot (boolean)         - specifies whether removal of hot pixels should take place (false).
		'''
		return usage
