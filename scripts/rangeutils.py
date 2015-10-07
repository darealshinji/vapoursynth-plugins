''' Functions that operate on ranges in a clip:
	replacerange
	deleterange
	freezeloop
	blankit
'''
import vapoursynth as vs

class RangeUtils():
	def __init__(self):
		self.core = vs.get_core() 
	'''
	Replaces a range of frames of a clip with the same range of
	frames from another clip.
	If no end frame is given, it will only replace the start frame. '''
	def replacerange(self, clip1, clip2, start, end=None):
		if end is None: end = start
		
		if start < 0 or start > clip1.num_frames - 1:
			raise ValueError('start frame out of bounds: %i.' % start)
		if end < start or end > clip1.num_frames - 1:
			raise ValueError('end frame out of bounds: %i.' % end)
		
		if start > 0:
			temp = 'self.core.std.Trim(clip1, 0, start - 1) + '
		else:
			temp = ''
		temp += 'self.core.std.Trim(clip2, start, end)'
		if end < clip1.num_frames - 1:
			temp += '+ self.core.std.Trim(clip1, end + 1)'
		
		final = eval(temp)
		
		if clip1.num_frames != final.num_frames:
			raise ValueError('input / output framecount missmatch (got: %i; expected: %i).' %
					(final.num_frames, clip1.num_frames))
		
		return final
	'''
	Deletes a range of frames from a clip.
	If no end frame is given, it will only delete the start frame. '''
	def deleterange(self, src, start, end=None):
		if end is None: end = start
		
		if start < 0 or start > src.num_frames - 1:
			raise ValueError('start frame out of bounds: %i.' % start)
		if end < start or end > src.num_frames - 1:
			raise ValueError('end frame out of bounds: %i.' % end)

		if start != 0:
			final = src[:start]
			if end < src.num_frames - 1:
				final = final + src[end + 1:]
		else:
			final = src[end + 1:]
		
		if src.num_frames != final.num_frames + (end - start + 1):
			raise ValueError('output expected framecount missmatch.')
		
		return final
	'''
	Freezes a range of frames form start to end using the frames
	comprended between loopStart and loopEnd.
	If no end frames are provided for the range or the loop,
	start frames will be used instead. '''
	def freezeloop(self, src, start, end, loopStart, loopEnd=None):
		if loopEnd is None: loopEnd = loopStart
		
		if start < 0 or start > src.num_frames - 1:
			raise ValueError('start frame out of bounds: %i.' % start)
		if loopStart < 0 or loopStart > src.num_frames - 1:
			raise ValueError('loop start frame out of bounds: %i.' % loopStart)
		if end < start or end > src.num_frames - 1:
			raise ValueError('end frame out of bounds: %i.' % end)
		if loopEnd < loopStart or loopEnd > src.num_frames - 1:
			raise ValueError('loop end out of bounds: %i.' % loopEnd)
		
		loop = self.core.std.Loop(src[loopStart:loopEnd + 1], 0)
		
		span = end - start + 1
		
		if start != 0:
			final = src[:start] + loop[:span]
		else:
			final = loop[:span]
		if end < src.num_frames - 1:
			final = final + src[end + 1:]
		
		if src.num_frames != final.num_frames:
			raise ValueError('input / output framecount missmatch (got: %i; expected: %i).' %
					(final.num_frames, src.num_frames))
			
		return final
	'''
	Blanks a range of frames in a clip, by default to pure balck.
	If no endframe is provided start frame will be used. '''
	def blankit(self, src, start, end=None):
		if end is None: end = start
		
		e   = self.core.std.BlankClip(src, color=color)
		
		if start != 0:
			z = src[:start] + e[start:end + 1]
		else:
			z = e[start:end + 1]
		if end < src.num_frames - 1:
			z = z + src[end + 1:]
		
		if src.num_frames != z.num_frames:
			raise ValueError('input / output framecount missmatch (got: %i; expected: %i).' %
					(z.num_frames, src.num_frames))
		return z
