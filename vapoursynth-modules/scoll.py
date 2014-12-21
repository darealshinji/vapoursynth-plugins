''' SColl.py
Various functions for VapourSynth:
	ssaa
	nediAA
	naa
	dehalo_alpha
	yahr
	fastlinedarken
	contrasharpening
	RemoveDirt
	UnsharpMask
	ModerateSharpen
	SharpenBelow
	RemoveDust
'''
import vapoursynth as vs

def clamp(minimum, x, maximum):
	return int(max(minimum, min(round(x), maximum)))

def m4(x):
	return 16 if x < 16 else int(round(x / 4.0) * 4)

class SColl():
	def __init__(self):
		self.core = vs.get_core()
	
	''' ssaa
		Small antialiasing function for 1080p. Input must be vs.YUV420P8.

		ssaa(clip, th_mask=11, sharpen=False, smask=False)
			th_mask: The lower the threshold, more pixels will be antialiased.
			aamode:  Antialiasing metoth: 0: SangNom; 1: nnedi3; 2: RemoveGrain.
			ssmode:  Supersampling metoth: 0:Spline36; 1: nnedi3_rpow2.
			sharpen: Performs contra-sharpening on the antialiased zones.
			smask:   Shows the zones antialiasing will be applied.
		
		Dependencies:
			SangNom (avs)
			Repair (vs)
			RemoveGrain (vs)
			GenericFilters (vs)
			fmtconv (vs)
			nnedi3 (vs)
		
		Todo:
			- Make it work on 16bit mode when aviable.
			
		Changelog:
			3.0	Added RemoveGrain aamode and nnedi3 ssmode.
			2.0	Adapted to use native repair.
				Memory/speed optimizations.
			1.0	Initial release.
	'''
	def ssaa(self, clip, th_mask=15, aamode=0, ssmode=1, sharpen=False, smask=False):
		fw   = clip.width
		fh   = clip.height
		
		if clip.format.id != vs.YUV420P8:
			raise ValueError('Input video format should be YUV420P8.')
		
		cluma = self.core.rgvs.RemoveGrain(self.get_luma(clip), 1)
		mask  = self.ssaa_BuildMask(c=cluma, edgelvl=th_mask)
		
		#Super Sampling
		if ssmode == 0:
			aac = self.core.fmtc.resample(cluma, fw*2, fh*2)
			aac = self.core.fmtc.bitdepth(aac, dmode=0, bits=8)
		elif ssmode == 1:
			aac = self.core.nnedi3.nnedi3_rpow2(clip=cluma, rfactor=2, correct_shift=1, qual=2)
		else:
			raise ValueError('Wrong ssmode, it should be 0 or 1.')
		
		#Antialiasing
		if aamode == 0:
			aac = self.core.std.Transpose(self.core.avs.SangNom2(c1=aac))
			aac = self.core.std.Transpose(self.core.avs.SangNom2(c1=aac))
			aac = self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(aac, fw, fh), bits=8)
		elif aamode == 1:
			aac = self.core.std.Transpose(self.core.nnedi3.nnedi3(clip=aac, field=0, nns=2))
			aac = self.core.std.Transpose(self.core.nnedi3.nnedi3(clip=aac, field=0, nns=2))
			aac = self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(aac, fw, fh), bits=8)
		elif aamode == 2:
			aac = self.core.rgvs.RemoveGrain(self.core.rgvs.RemoveGrain(self.core.rgvs.RemoveGrain(aac, 5), 21), 19)
			aac = self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(aac, fw, fh), bits=8)
			aac = self.core.rgvs.RemoveGrain(aac, 22)
		else:
			raise ValueError('Wrong aamode, it should be 0, 1 or 2.')
		
		#Sharpening
		if sharpen is True:
			aaD   = self.core.std.MakeDiff(cluma, aac)
			shrpD = self.core.std.MakeDiff(aac, self.core.rgvs.RemoveGrain(aac, 20))
			DD    = self.core.rgvs.Repair(shrpD, aaD, 13)
			aac   = self.core.std.MergeDiff(aac, DD)
		
		#Merge chroma
		if smask is True:
			return mask
		else:
			last = self.core.std.MaskedMerge(cluma, aac, mask, planes=0)
			return self.merge_chroma(last, clip)
	
	''' nediAA - antialiasing function using nnedi3
		pm: parity mode, should be 2 or 3.
	'''
	def nediAA(self, c, pm=2):
		pm = clamp(2, pm, 3)
		ret = self.core.nnedi3.nnedi3(c, field=pm)
		ret = self.core.std.Merge(ret[::2], ret[1::2])
		return ret
	
	''' naa - antialiasing function using nnedi3
		ss: supersampling value, must be even.
		cp: if false chroma will not be altered.
	'''
	def naa(self, c, ss=2, cp=True):
		src = c
		if cp is False:
			c = self.get_luma(c)
		ret = self.core.nnedi3.nnedi3_rpow2(clip=c, rfactor=2, correct_shift=1, qual=2)
		ret = self.core.nnedi3.nnedi3(clip=ret, field=0, nns=2)
		ret = self.core.std.Transpose(ret)
		ret = self.core.nnedi3.nnedi3(clip=ret, field=0, nns=2)
		ret = self.core.std.Transpose(ret)
		ret = self.core.fmtc.resample(ret, c.width, c.height)
		if ret.format.bits_per_sample != c.format.bits_per_sample:
			ret = self.core.fmtc.bitdepth(ret, bits=c.format.bits_per_sample)
		if cp is False:
			ret = self.merge_chroma(ret, src)
		return ret
	
	''' dehalo_alpha
		Reduce halo artifacts that can occur when sharpening. Ported from:
		http://forum.doom9.org/showpost.php?p=738264&postcount=43
		
		Input must be vs.YUV420P8.
		
		dehalo_alpha(src, rx=2.0, ry=2.0, darkstr=1.0, brightstr=1.0,
			lowsens=50, highsens=50, ss=1.5)
			rx, ry [float, 1.0 ... 2.0 ... ~3.0]
				As usual, the radii for halo removal.
				Note: this function is rather sensitive to 
				the radius settings. Set it as low as possible! 
				If radius is set too high, it will start missing small spots.
			darkstr, brightstr [float, 0.0 ... 1.0] [<0.0 and >1.0 possible]
				The strength factors for processing
				dark and bright halos. Default 1.0 both for
				symmetrical processing. On Comic/Anime, 
				darkstr=0.4~0.8 sometimes might be better... sometimes.
				In General, the function seems to preserve
				dark lines rather well.
			lowsens, highsens [int, 0 ... 50 ... 100] 
				Sensitivity settings, not that easy to describe
				them exactly ... in a sense, they define a window
				between how weak an achieved effect has to be to 
				get fully accepted, and how strong an achieved
				effect has to be to get fully discarded. 
				Defaults are 50 and 50 ... try and see for yourself. 
			ss [float, 1.0 ... 1.5 ...]
				Supersampling factor, to avoid creation of aliasing.
		
		Dependencies: 
			GenericFilters (vs)
			Repair (vs)
			fmtconv (vs)
		
		Changelog:
			2.0	Adapted to use native repair.
				Memory/speed optimizations.
			1.0	Initial release.
	'''
	def dehalo_alpha(self, src, rx=2.0, ry=2.0, darkstr=1.0, brightstr=1.0, lowsens=50, highsens=50, ss=1.5):
		his  = highsens / 100.0
		ox   = src.width
		oy   = src.height
		
		if src.format.id != vs.YUV420P8:
			raise ValueError('Input video format should be YUV420P8.')
			
		self.vmax      = 2 ** src.format.bits_per_sample - 1
		self.vmid      = self.vmax // 2 + 1
		self.lut_range = range(self.vmax + 1)
		
		src_l  = self.get_luma(src)
		
		this   = self.core.fmtc.resample(clip=src_l, w=m4(ox/rx), h=m4(oy/ry), kernel='bicubic')
		halos  = self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(clip=this, w=ox, h=oy, kernel='bicubic', a1=1, a2=0), bits=8, dmode=0)
		are    = self.core.std.Expr([self.core.generic.Maximum(src_l), self.core.generic.Minimum(src_l)], 'x y -')
		ugly   = self.core.std.Expr([self.core.generic.Maximum(halos), self.core.generic.Minimum(halos)], 'x y -')
		so     = self.lutxy(c1=ugly, c2=are, expr=lambda x, y: ((((y - x) / (y + 0.001)) * 255) - lowsens) * (((y + 256) / 512) + his))
		lets   = self.core.std.MaskedMerge(halos, src_l, mask=so)
		if ss == 1:
			remove = self.core.rgvs.Repair(src_l, lets, [1, 0])
		else:
			remove = self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(clip=src_l, w=m4(ox*ss), h=m4(oy*ss)), bits=8, dmode=0)
			remove = self.logic(c1=remove, c2=self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(clip=self.core.generic.Maximum(lets, planes=[0]), w=m4(ox*ss), h=m4(oy*ss)), bits=8, dmode=0), mode='min')
			remove = self.logic(c1=remove, c2=self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(clip=self.core.generic.Minimum(lets, planes=[0]), w=m4(ox*ss), h=m4(oy*ss)), bits=8, dmode=0), mode='max')
			remove = self.core.fmtc.bitdepth(clip=self.core.fmtc.resample(clip=remove, w=ox, h=oy), bits=8, dmode=0)
		them   = self.lutxy(c1=src_l, c2=remove, expr=lambda x, y: x - ((x - y) * darkstr) if x < y else x - ((x - y) * brightstr))
		
		return self.merge_chroma(them, src)
	
	'''yahr
		And Y'et A'nother H'alo R'educing function. Ported from:
		http://forum.doom9.org/showpost.php?p=1205653&postcount=9
		
		yahr(src)
		
		Dependencies: 
			GenericFilters (vs)
			RemoveGrain (vs)
			aWarpSharp2 (avs)
		
		Changelog:
			2.0	Adapted to use native repair.
				Memory/speed optimizations.
			1.0	Initial release.
	'''
	def yahr(self, src):
		if src.format.id != vs.YUV420P8:
			raise ValueError('Input video format should be YUV420P8.')
		
		c   = src
		src = get_luma(src)
		
		b1    = self.core.rgvs.RemoveGrain(self.minblur(src, 2), 11)
		b1D   = self.core.std.MakeDiff(src, b1)
		w1    = self.core.avs.aWarpSharp2(c1=src, depth=32, blur=2, thresh=128)
		w1b1  = self.core.rgvs.RemoveGrain(self.minblur(w1, 2), 11)
		w1b1D = self.core.std.MakeDiff(w1, w1b1)
		DD    = self.core.rgvs.Repair(b1D, w1b1D, 13) 
		DD2   = self.core.std.MakeDiff(b1D, DD)
		
		return self.merge_chroma(self.core.std.MakeDiff(src, DD2), c)
	
	''' fastlinedarken
		Line darkening script for vapoursynth ported from avisynth. Port from:
		http://forum.doom9.org/showthread.php?t=82125
		
		fastlinedarken(clip, strength=48, luma_cap=191, threshold=4, prot=5, thinning=0)
		
		Parameters are:
		strength (integer)  - Line darkening amount, 0-256. Default 48. Represents the _maximum_ amount
				that the luma will be reduced by, weaker lines will be reduced by
				proportionately less.
		luma_cap (integer)  - value from 0 (black) to 255 (white), used to stop the darkening
				determination from being 'blinded' by bright pixels, and to stop grey
				lines on white backgrounds being darkened. Any pixels brighter than
				luma_cap are treated as only being as bright as luma_cap. Lowering
				luma_cap tends to reduce line darkening. 255 disables capping. Default 191.
		threshold (integer) - any pixels that were going to be darkened by an amount less than
				threshold will not be touched. setting this to 0 will disable it, setting
				it to 4 (default) is recommended, since often a lot of random pixels are
				marked for very slight darkening and a threshold of about 4 should fix
				them. Note if you set threshold too high, some lines will not be darkened
		prot     (integer)  - Prevents the darkest lines from being darkened. Protection acts as a threshold.
				Values range from 0 (no prot) to ~50 (protect everything)
		thinning (integer)  - optional line thinning amount, 0-256. Setting this to 0 will disable it,
				which is gives a _big_ speed increase. Note that thinning the lines will
				inherently darken the remaining pixels in each line a little. Default 24.
		
		Dependencies:
			RemoveGrain (vs)
			GenericFilters (vs)

		Changelog:
			2.0	Adapted to use native removegrain.
				Memory/speed optimizations.
				Added protection from 1.4x MOD.
			1.0	Initial release.
	'''
	def fastlinedarken(self, src, strength=48, luma_cap=191, threshold=4, prot=5, thinning=0):
		strf = float(strength)/128.0
		thn  = float(thinning)/16.0
		self.vmax      = 2 ** src.format.bits_per_sample - 1
		self.vmid      = self.vmax // 2 + 1
		self.lut_range = range(self.vmax + 1)
		
		src_luma = self.get_luma(src)
		
		exin = self.core.generic.Minimum(self.core.generic.Maximum(src_luma, threshold=int(self.vmax/(prot+1))))
		
		expr  = lambda x, y: (((x - y if y < luma_cap else x - luma_cap) if (y if y < luma_cap else luma_cap) > (x + threshold) else 0) * strf) + x
		thick = self.lutxy(src_luma, exin, expr)
		
		if thinning > 0:
			expr  = lambda x, y: ((x - y if y < luma_cap else x - luma_cap) if (y if y < luma_cap else luma_cap) > (x + threshold) else 0) + (self.vmid - 1)
			diff  = self.lutxy(src_luma, exin, expr)
			expr     = lambda x: ((x - (self.vmid - 1)) * thn) + self.vmax
			linemask = self.core.rgvs.RemoveGrain(self.lut(self.core.generic.Minimum(diff), expr), [20])
			expa     = self.core.generic.Maximum(src_luma, [0])
			expr     = ('x y {vmid} - {strf} 1 + * +').format(vmid=self.vmid - 1, strf=strf)
			thin     = self.core.std.Expr([expa, diff], [expr])
			pre      = self.core.std.MaskedMerge(thin, thick, mask=linemask, planes=[0])
		else:
			pre = thick
		
		return self.merge_chroma(pre, src)
	
	''' contrasharpening
	    contrasharpeninghd-1.0 ()
		contrasharpening ported from:
		http://forum.doom9.org/showpost.php?p=1474191&postcount=337
		
		contrasharpening(filtered, original)
			filtered:  A clip blured due the filtering done on it.
			original:  The same clip before the filtering.
			hd:        Use HD mode.
			overshoot: This can be used to lower the effect.
		
		Dependencies: 
			RemoveGrain (vs)
			Repair (vs)

		Changelog:
			2.0	Adapted to use native removegrain/repair.
				Memory/speed optimizations.
			1.0	Initial release
		'''
	def contrasharpening(self, filtered, original):
		self.vmax      = 2 ** filtered.format.bits_per_sample - 1
		self.vmid      = self.vmax // 2 + 1
		self.lut_range = range(self.vmax + 1)
		
		l_filtered = self.get_luma(filtered)
		l_original = self.get_luma(original)
		
		s    = self.minblur(l_filtered)
		allD = self.core.std.MakeDiff(l_original, l_filtered)
		ssD  = self.core.std.MakeDiff(s, self.core.rgvs.RemoveGrain(s, [11]))
		expr = lambda x, y: x if abs(x - self.vmid) < abs(y - self.vmid) else y
		ssDD = self.core.rgvs.Repair(ssD, allD, [1])
		ssDD = self.lutxy(ssDD, ssD, expr)
		
		lret  =  self.core.std.MergeDiff(l_filtered, ssDD)
		
		return self.merge_chroma(lret, filtered)
	''' 
	RemoveDirt
	'''
	def RemoveDirt(self, c, repmode=16):
		if c.format.num_planes > 1:
			repmodes = [repmode,repmode,1]
			greymode = 0
		else:
			repmodes = [repmode]
			greymode = 1
		cleansed    = self.core.rgvs.Clense(c)
		sbegin      = self.core.rgvs.ForwardClense(c)
		send        = self.core.rgvs.BackwardClense(c)
		scenechange = self.core.rdvs.SCSelect(c, sbegin, send, cleansed)
		alt         = self.core.rgvs.Repair(scenechange, c, mode=repmodes)
		restore     = self.core.rgvs.Repair(cleansed, c, mode=repmodes)
		corrected   = self.core.rdvs.RestoreMotionBlocks(cleansed, restore, neighbour=c, alternative=alt, 
								gmthreshold=70, dist=1, dmode=2, noise=10, noisy=12,
								grey=greymode)
		return self.core.rgvs.RemoveGrain(corrected, mode=[17,17,1])
	'''
	UnsharpMask
	'''
	def UnsharpMask(self, clip, strength=None):
		if strength is None:
			blur_clip = self.core.generic.Blur(clip, planes=[0])
		else:
			blur_clip = self.core.generic.GBlur(clip, sigma=strength, planes=[0])
		diff_clip = self.core.std.MakeDiff(clip, blur_clip, planes=[0])
		return self.core.std.MergeDiff(clip, diff_clip, planes=[0])
	'''
	ModerateSharpen
	'''
	def ModerateSharpen(self, c, strength=1.0, repmode=16):
		blurred   = self.core.generic.GBlur(c, sigma=strength)
		sharpened = self.core.std.Expr([c, blurred], ['x x + y -', ''])
		return self.core.rgvs.Repair(sharpened, c, mode=repmode, planes=[0])
	
	'''
	SharpenBelow
	'''
	def SharpenBelow(self, c, strength=1.0):
		clean = self.core.rgvs.RemoveGrain(c, 4)
		sharp = self.UnsharpMask(c, strength)
		return self.core.std.MakeDiff(c, self.core.std.MakeDiff(clean, sharp))
	'''
	RemoveDust
	'''
	def RemoveDust(self, c, mode=4, repmode=16):
		clensed = self.core.rgvs.Clense(c)
		rep     = self.core.rgvs.Repair(clensed, c, mode=repmode)
		return self.core.rgvs.RemoveGrain(rep, mode=mode)
	'''
	Helper functions:
		add_diff DEPRECATED
		make_diff DEPRECATED
		ssaa_BuildMask
		get_luma
		merge_chroma
		lut
		lutxy
		logic
		minblur FIXME
	'''
	def add_diff(self, c1, c2, planes=[0]):
		expr = ('x y + {mid} -').format(mid=(2 ** c1.format.bits_per_sample - 1) // 2 + 1)
		expr = [(i in planes) * expr for i in range(c1.format.num_planes)]
		return self.core.std.Expr([c1, c2], expr)

	def make_diff(self, c1, c2, planes=[0]):
		expr = ('x y - {mid} +').format(mid=(2 ** c1.format.bits_per_sample - 1) // 2 + 1)
		expr = [(i in planes) * expr for i in range(c1.format.num_planes)]
		return self.core.std.Expr([c1, c2], expr)
	
	def ssaa_BuildMask(self, c, edgelvl=15, th=255):
		if c.format.id != vs.GRAY:
			c = self.core.std.ShufflePlanes(clips=c, planes=[0], colorfamily=vs.GRAY)
		m  = self.core.generic.Prewitt(c, edgelvl, edgelvl)
		m  = self.core.generic.Maximum(m, threshold=th)
		return m
	
	def get_luma(self, c1):
		return self.core.std.ShufflePlanes(clips=c1, planes=[0], colorfamily=vs.GRAY)
	
	def merge_chroma(self, c1, c2):
		return self.core.std.ShufflePlanes(clips=[c1, c2], planes=[0, 1, 2], colorfamily=c2.format.color_family)
	
	def lut(self, c1, expr, planes=[0]):
		vmax = 2 ** c1.format.bits_per_sample - 1
		vmid = vmax // 2 + 1
		lut_range = range(vmax + 1)
		lut = [clamp(0, expr(x), vmax) for x in lut_range]
		return self.core.std.Lut(clip=c1, lut=lut, planes=planes)
	
	def lutxy(self, c1, c2, expr, planes=[0]):
		vmax = 2 ** c1.format.bits_per_sample - 1
		vmid = vmax // 2 + 1
		lut_range = range(vmax + 1)
		lut = []
		for y in lut_range:
			for x in lut_range:
				lut.append(clamp(0, expr(x, y), vmax))
		return self.core.std.Lut2(c1, c2, lut=lut, planes=planes)
	
	def logic(self, c1, c2, mode, th1=0, th2=0):
		if mode == 'min':
			expr = ('x {th1} + y {th2} + min').format(th1=th1, th2=th2)
		elif mode == 'max':
			expr = ('x {th1} + y {th2} + max').format(th1=th1, th2=th2)
		else:
			raise ValueError('%s is not a valid mode for logic' % mode)
		return self.core.std.Expr([c1, c2], expr=expr)
	
	def minblur(self, clp, r=1):
		vmax = 2 ** clp.format.bits_per_sample - 1
		vmid = vmax // 2 + 1
		if r == 1:
			RG11D = self.core.std.MakeDiff(clp, self.core.rgvs.RemoveGrain(clp, [11]))
			RG4D  = self.core.std.MakeDiff(clp, self.core.rgvs.RemoveGrain(clp, [4]))
		elif r == 2: #FIXME: mode incorrectly implemented
			RG11D = self.core.std.MakeDiff(clp, self.core.rgvs.RemoveGrain(self.core.rgvs.RemoveGrain(clp, [11]), [20]))
			RG4D  = self.core.std.MakeDiff(clp, self.core.generic.Median(clp))
		elif r == 3: #FIXME: mode incorrectly implemented
			RG11D = self.core.std.MakeDiff(clp, self.core.rgvs.RemoveGrain(self.core.rgvs.RemoveGrain(self.core.rgvs.RemoveGrain(clp, [11]), [20]), [20]))
			RG4D  = self.core.std.MakeDiff(clp, self.core.generic.Median(clp))
		else:
			raise ValueError('minblur Wrong value for "r", it should be 1, 2 or 3.')
		
		expr = lambda x, y: vmid if ((x - vmid) * (y - vmid)) < 0 else (x if abs(x - vmid) < abs(y - vmid) else y)
		DD   = self.lutxy(RG11D, RG4D, expr)
		
		return self.core.std.MakeDiff(clp, DD)
