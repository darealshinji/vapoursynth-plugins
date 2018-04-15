import vapoursynth as vs
import mvsfunc as mvf
import havsfunc as haf
import functools

MODULE_NAME = 'vsTAAmbk'


class Clip:
    def __init__(self, clip):
        self.core = vs.get_core()
        self.clip = clip
        if not isinstance(clip, vs.VideoNode):
            raise TypeError(MODULE_NAME + ': clip is invalid.')
        self.clip_width = clip.width
        self.clip_height = clip.height
        self.clip_bits = clip.format.bits_per_sample
        self.clip_color_family = clip.format.color_family
        self.clip_sample_type = clip.format.sample_type
        self.clip_id = clip.format.id
        self.clip_subsample_w = clip.format.subsampling_w
        self.clip_subsample_h = clip.format.subsampling_h
        self.clip_is_gray = True if clip.format.num_planes == 1 else False
        # Register format for GRAY10
        vs.GRAY10 = self.core.register_format(vs.GRAY, vs.INTEGER, 10, 0, 0).id


class AAParent(Clip):
    def __init__(self, clip, strength=0.0, down8=False):
        super(AAParent, self).__init__(clip)
        self.dfactor = 1 - min(strength, 0.5)
        self.dw = round(self.clip_width * self.dfactor / 4) * 4
        self.dh = round(self.clip_height * self.dfactor / 4) * 4
        self.upw4 = round(self.dw * 0.375) * 4
        self.uph4 = round(self.dh * 0.375) * 4
        self.down8 = down8
        self.process_depth = self.clip_bits
        if down8 is True:
            self.down_8()
        if self.dfactor != 1:
            self.clip = self.resize(self.clip, self.dw, self.dh, shift=0)
        if self.clip_color_family is vs.GRAY:
            if self.clip_sample_type is not vs.INTEGER:
                raise TypeError(MODULE_NAME + ': clip must be integer format.')
        else:
            raise TypeError(MODULE_NAME + ': clip must be GRAY family.')

    def resize(self, clip, w, h, shift):
        try:
            resized = self.core.resize.Spline36(clip, w, h, src_top=shift)
        except vs.Error:
            resized = self.core.fmtc.resample(clip, w, h, sy=shift)
            if resized.format.bits_per_sample != self.process_depth:
                mvf.Depth(resized, self.process_depth)
        return resized

    def down_8(self):
        self.process_depth = 8
        self.clip = mvf.Depth(self.clip, 8)


class AANnedi3(AAParent):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AANnedi3, self).__init__(clip, strength, down8)
        self.nsize = args.get('nsize', 3)
        self.nns = args.get('nns', 1)
        self.qual = args.get('qual', 2)
        self.opencl = args.get('opencl', False)
        if self.opencl is True:
            try:
                self.nnedi3 = self.core.nnedi3cl.NNEDI3CL
            except AttributeError:
                try:
                    self.nnedi3 = self.core.znedi3.nnedi3
                except AttributeError:
                    self.nnedi3 = self.core.nnedi3.nnedi3
        else:
            try:
                self.nnedi3 = self.core.znedi3.nnedi3
            except AttributeError:
                self.nnedi3 = self.core.nnedi3.nnedi3

    def out(self):
        aaed = self.nnedi3(self.clip, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
        aaed = self.resize(aaed, self.clip_width, self.clip_height, -0.5)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.nnedi3(aaed, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
        aaed = self.resize(aaed, self.clip_height, self.clip_width, -0.5)
        aaed = self.core.std.Transpose(aaed)
        return aaed


class AANnedi3SangNom(AANnedi3):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AANnedi3SangNom, self).__init__(clip, strength, down8, **args)
        self.aa = args.get('aa', 48)

    def out(self):
        aaed = self.nnedi3(self.clip, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
        aaed = self.resize(aaed, self.clip_width, self.uph4, shift=-0.5)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.nnedi3(aaed, field=1, dh=True, nsize=self.nsize, nns=self.nns, qual=self.qual)
        aaed = self.resize(aaed, self.uph4, self.upw4, shift=-0.5)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.resize(aaed, self.clip_width, self.clip_height, shift=0)
        return aaed


class AANnedi3UpscaleSangNom(AANnedi3SangNom):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AANnedi3UpscaleSangNom, self).__init__(clip, strength, down8, **args)
        self.nsize = args.get('nsize', 1)
        self.nns = args.get('nns', 3)
        self.qual = args.get('qual', 2)


class AAEedi3(AAParent):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AAEedi3, self).__init__(clip, strength, down8)
        self.eedi3_args = {'alpha': args.get('alpha', 0.5), 'beta': args.get('beta', 0.2),
                           'gamma': args.get('gamma', 20), 'nrad': args.get('nrad', 3), 'mdis': args.get('mdis', 30)}

        self.opencl = args.get('opencl', False)
        if self.opencl is True:
            try:
                self.eedi3 = self.core.eedi3m.EEDI3CL
                self.eedi3_args['device'] = args.get('opencl_device', 0)
            except AttributeError:
                self.eedi3 = self.core.eedi3.eedi3
                if self.process_depth > 8:
                    self.clip = mvf.Depth(self.clip, 8)
        else:
            try:
                self.eedi3 = self.core.eedi3m.EEDI3
            except AttributeError:
                self.eedi3 = self.core.eedi3.eedi3
                if self.process_depth > 8:
                    self.clip = mvf.Depth(self.clip, 8)

    '''
    def build_eedi3_mask(self, clip):
        eedi3_mask = self.core.nnedi3.nnedi3(clip, field=1, show_mask=True)
        eedi3_mask = self.core.std.Expr([eedi3_mask, clip], "x 254 > x y - 0 = not and 255 0 ?")
        eedi3_mask_turn = self.core.std.Transpose(eedi3_mask)
        if self.dfactor != 1:
            eedi3_mask_turn = self.core.resize.Bicubic(eedi3_mask_turn, self.clip_height, self.dw)
        return eedi3_mask, eedi3_mask_turn
    '''

    def out(self):
        aaed = self.eedi3(self.clip, field=1, dh=True, **self.eedi3_args)
        aaed = self.resize(aaed, self.dw, self.clip_height, shift=-0.5)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.eedi3(aaed, field=1, dh=True, **self.eedi3_args)
        aaed = self.resize(aaed, self.clip_height, self.clip_width, shift=-0.5)
        aaed = self.core.std.Transpose(aaed)
        aaed_bits = aaed.format.bits_per_sample
        return aaed if aaed_bits == self.process_depth else mvf.Depth(aaed, self.process_depth)


class AAEedi3SangNom(AAEedi3):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AAEedi3SangNom, self).__init__(clip, strength, down8, **args)
        self.aa = args.get('aa', 48)

    '''
    def build_eedi3_mask(self, clip):
        eedi3_mask = self.core.nnedi3.nnedi3(clip, field=1, show_mask=True)
        eedi3_mask = self.core.std.Expr([eedi3_mask, clip], "x 254 > x y - 0 = not and 255 0 ?")
        eedi3_mask_turn = self.core.std.Transpose(eedi3_mask)
        eedi3_mask_turn = self.core.resize.Bicubic(eedi3_mask_turn, self.uph4, self.dw)
        return eedi3_mask, eedi3_mask_turn
    '''

    def out(self):
        aaed = self.eedi3(self.clip, field=1, dh=True, **self.eedi3_args)
        aaed = self.resize(aaed, self.dw, self.uph4, shift=-0.5)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.eedi3(aaed, field=1, dh=True, **self.eedi3_args)
        aaed = self.resize(aaed, self.uph4, self.upw4, shift=-0.5)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.resize(aaed, self.clip_width, self.clip_height, shift=0)
        aaed_bits = aaed.format.bits_per_sample
        return aaed if aaed_bits == self.process_depth else mvf.Depth(aaed, self.process_depth)


class AAEedi2(AAParent):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AAEedi2, self).__init__(clip, strength, down8)
        self.mthresh = args.get('mthresh', 10)
        self.lthresh = args.get('lthresh', 20)
        self.vthresh = args.get('vthresh', 20)
        self.maxd = args.get('maxd', 24)
        self.nt = args.get('nt', 50)

    def out(self):
        aaed = self.core.eedi2.EEDI2(self.clip, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
        aaed = self.resize(aaed, self.dw, self.clip_height, shift=-0.5)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.core.eedi2.EEDI2(aaed, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
        aaed = self.resize(aaed, self.clip_height, self.clip_width, shift=-0.5)
        aaed = self.core.std.Transpose(aaed)
        return aaed


class AAEedi2SangNom(AAEedi2):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AAEedi2SangNom, self).__init__(clip, strength, down8, **args)
        self.aa = args.get('aa', 48)

    def out(self):
        aaed = self.core.eedi2.EEDI2(self.clip, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
        aaed = self.resize(aaed, self.dw, self.uph4, shift=-0.5)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.core.eedi2.EEDI2(aaed, 1, self.mthresh, self.lthresh, self.vthresh, maxd=self.maxd, nt=self.nt)
        aaed = self.resize(aaed, self.uph4, self.upw4, shift=-0.5)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.resize(aaed, self.clip_width, self.clip_height, shift=0)
        return aaed


class AASpline64NRSangNom(AAParent):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AASpline64NRSangNom, self).__init__(clip, strength, down8)
        self.aa = args.get('aa', 48)

    def out(self):
        aa_spline64 = self.core.fmtc.resample(self.clip, self.upw4, self.uph4, kernel='spline64')
        aa_spline64 = mvf.Depth(aa_spline64, self.process_depth)
        aa_gaussian = self.core.fmtc.resample(self.clip, self.upw4, self.uph4, kernel='gaussian', a1=100)
        aa_gaussian = mvf.Depth(aa_gaussian, self.process_depth)
        aaed = self.core.rgvs.Repair(aa_spline64, aa_gaussian, 1)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.resize(aaed, self.clip_width, self.clip_height, shift=0)
        return aaed


class AASpline64SangNom(AAParent):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AASpline64SangNom, self).__init__(clip, strength, down8)
        self.aa = args.get('aa', 48)

    def out(self):
        aaed = self.core.fmtc.resample(self.clip, self.clip_width, self.uph4, kernel="spline64")
        aaed = mvf.Depth(aaed, self.process_depth)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(self.resize(aaed, self.clip_width, self.clip_height, 0))
        aaed = self.core.fmtc.resample(aaed, self.clip_height, self.upw4, kernel="spline64")
        aaed = mvf.Depth(aaed, self.process_depth)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(self.resize(aaed, self.clip_height, self.clip_width, 0))
        return aaed


class AAPointSangNom(AAParent):
    def __init__(self, clip, strength=0, down8=False, **args):
        super(AAPointSangNom, self).__init__(clip, 0, down8)
        self.aa = args.get('aa', 48)
        self.upw = self.clip_width * 2
        self.uph = self.clip_height * 2
        self.strength = strength  # Won't use this

    def out(self):
        aaed = self.core.resize.Point(self.clip, self.upw, self.uph)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.core.sangnom.SangNom(aaed, aa=self.aa)
        aaed = self.core.std.Transpose(aaed)
        aaed = self.resize(aaed, self.clip_width, self.clip_height, 0)
        return aaed


class MaskParent(Clip):
    def __init__(self, clip):
        super(MaskParent, self).__init__(clip)
        if clip.format.color_family is not vs.GRAY:
            self.clip = self.core.std.ShufflePlanes(self.clip, 0, vs.GRAY)
        self.clip = mvf.Depth(self.clip, 8)  # Mask will always be processed in 8bit scale
        self.mask = None
        self.multi = ((1 << self.clip_bits) - 1) // 255

    def __add__(self, mask_b):
        if not isinstance(mask_b, MaskParent):
            raise TypeError(MODULE_NAME + ': Incorrect mask_b type.')
        self.mask = self.core.std.Expr([self.mask, mask_b.mask], "x y max", vs.GRAY8)
        return self

    def expand(self, time):
        for i in range(time):
            self.mask = self.core.std.Maximum(self.mask)

    def inpand(self, time):
        for i in range(time):
            self.mask = self.core.std.Minimum(self.mask)

    def blur(self):
        self.mask = self.core.rgvs.RemoveGrain(self.mask, 20)

    def out(self):
        out_type = eval('vs.GRAY{depth}'.format(depth=self.clip_bits))
        return self.core.std.Expr(self.mask, 'x ' + str(self.multi) + ' *', out_type)


class MaskCanny(MaskParent):
    def __init__(self, clip, **kwargs):
        super(MaskCanny, self).__init__(clip)
        self.sigma = kwargs.get('sigma', 1.2)
        self.t_h = kwargs.get('t_h', 8.0)
        self.lthresh = kwargs.get('lthresh', None)
        self.mpand = kwargs.get('mpand', [1, 0])
        self.opencl = kwargs.get('opencl', False)
        self.opencl_device = kwargs.get('opencl_devices', 0)

        if isinstance(self.sigma, (list, tuple)) and isinstance(self.t_h, (list, tuple)) \
                and isinstance(self.lthresh, (list, tuple)):
            if len(self.sigma) != len(self.t_h) or len(self.lthresh) != len(self.sigma) - 1:
                raise ValueError(MODULE_NAME + ': incorrect length of sigma, t_h or lthresh.')
            self.mask = self.tcanny(self.clip, sigma=self.sigma[0], t_h=self.t_h[0], mode=0, planes=0)
            for i in range(len(self.lthresh)):
                temp_mask = self.tcanny(self.clip, sigma=self.sigma[i + 1], t_h=self.t_h[i + 1],
                                        mode=0, planes=0)
                expr = "x " + str(self.lthresh[i]) + " < z y ?"
                self.mask = self.core.std.Expr([self.clip, temp_mask, self.mask], expr)
        elif not isinstance(self.sigma, (list, tuple)) and not isinstance(self.t_h, (list, tuple)):
            self.mask = self.tcanny(self.clip, sigma=self.sigma, t_h=self.t_h, mode=0, planes=0)
        else:
            raise ValueError(MODULE_NAME + ': sigma, t_h, lthresh shoule be same type (num, list or tuple).')

        if not isinstance(self.mpand, (list, tuple)):
            self.mpand = [self.mpand, self.mpand]
        if self.mpand != [0, 0] and self.mpand != (0, 0):
            self.expand(self.mpand[0])
            self.inpand(self.mpand[1])

    def tcanny(self, clip, sigma, t_h, mode, planes):
        if self.opencl is True:
            try:
                return self.core.tcanny.TCannyCL(clip, sigma=sigma, t_h=t_h, mode=mode, planes=planes,
                                                 device=self.opencl_device)
            except AttributeError:
                return self.core.tcanny.TCanny(clip, sigma=sigma, t_h=t_h, mode=mode, planes=planes)
        else:
            return self.core.tcanny.TCanny(clip, sigma=sigma, t_h=t_h, mode=mode, planes=planes)


class MaskSobel(MaskParent):
    def __init__(self, clip, **kwargs):
        super(MaskSobel, self).__init__(clip)
        self.binarize = kwargs.get('binarize', 48)
        self.sigma = kwargs.get('sigma', 1.2)
        self.lthresh = kwargs.get('lthresh', None)
        self.mpand = kwargs.get('mpand', [1, 1])

        eemask = self.core.tcanny.TCanny(self.clip, sigma=self.sigma, mode=1, op=2, planes=0)
        if isinstance(self.binarize, (list, tuple)) and isinstance(self.lthresh, (list, tuple)):
            if len(self.lthresh) != len(self.binarize) - 1:
                raise ValueError(MODULE_NAME + ': incorrect length of sigma, binarize or lthresh.')
            expr = 'x {binarize} < 0 255 ?'.format(binarize=self.binarize[0])
            self.mask = self.core.std.Expr(eemask, expr)
            for i in range(len(self.lthresh)):
                temp_expr = 'x {binarize} < 0 255 ?'.format(binarize=self.binarize[i + 1])
                temp_mask = self.core.std.Expr(eemask, temp_expr)
                luma_expr = 'x {thresh} < z y ?'.format(thresh=self.lthresh[i])
                self.mask = self.core.std.Expr([self.clip, temp_mask, self.mask], luma_expr)
        elif not isinstance(self.binarize, (list, tuple)):
            expr = 'x {binarize} < 0 255 ?'.format(binarize=self.binarize)
            self.mask = self.core.std.Expr(eemask, expr)
        else:
            raise ValueError(MODULE_NAME + ': binarzie and lthresh should be same type (num, list, tuple).')

        if not isinstance(self.mpand, (list, tuple)):
            self.mpand = [self.mpand, self.mpand]
        if self.mpand != [0, 0] and self.mpand != (0, 0):
            self.expand(self.mpand[0])
            self.inpand(self.mpand[1])


class MaskPrewitt(MaskParent):
    def __init__(self, clip, **kwargs):
        super(MaskPrewitt, self).__init__(clip)
        self.factor = kwargs.get('factor', 62)
        self.lthresh = kwargs.get('lthresh', None)
        self.mpand = kwargs.get('mpand', 0)

        eemask_1 = self.core.std.Convolution(self.clip, [1, 1, 0, 1, 0, -1, 0, -1, -1], divisor=1, saturate=False)
        eemask_2 = self.core.std.Convolution(self.clip, [1, 1, 1, 0, 0, 0, -1, -1, -1], divisor=1, saturate=False)
        eemask_3 = self.core.std.Convolution(self.clip, [1, 0, -1, 1, 0, -1, 1, 0, -1], divisor=1, saturate=False)
        eemask_4 = self.core.std.Convolution(self.clip, [0, -1, -1, 1, 0, -1, 1, 1, 0], divisor=1, saturate=False)
        eemask = self.core.std.Expr([eemask_1, eemask_2, eemask_3, eemask_4], "x y max z max a max")
        if isinstance(self.factor, (list, tuple)) and isinstance(self.lthresh, (list, tuple)):
            if len(self.lthresh) != len(self.factor) - 1:
                raise ValueError(MODULE_NAME + ': incorrect length of lthresh or factor.')
            expr = 'x {factor} <= x 2 / x 1.4 pow ?'.format(factor=self.factor[0])
            self.mask = self.core.std.Expr(eemask, expr)
            for i in range(len(self.lthresh)):
                temp_expr = "x {factor} <= x 2 / x 1.4 pow ?".format(factor=self.factor[i + 1])
                temp_mask = self.core.std.Expr(eemask, temp_expr)
                luma_expr = "x {lthresh} < z y ?".format(lthresh=self.lthresh[i])
                self.mask = self.core.std.Expr([self.clip, temp_mask, self.mask], luma_expr)
        elif not isinstance(self.factor, (list, tuple)):
            expr = "x {factor} <= x 2 / x 1.4 pow ?".format(factor=self.factor)
            self.mask = self.core.std.Expr(eemask, expr)
        else:
            raise ValueError(MODULE_NAME + ': factor and lthresh should be same type (num, list or tuple).')

        if not isinstance(self.mpand, (list, tuple)):
            self.mpand = [self.mpand, self.mpand]
        if self.mpand != [0, 0] and self.mpand != (0, 0):
            self.expand(self.mpand[0])
            self.inpand(self.mpand[1])


class FadeTextMask(MaskParent):
    def __init__(self, clip, **kwargs):
        super(FadeTextMask, self).__init__(clip)
        self.clip = mvf.Depth(clip, 8)
        self.lthr = kwargs.get('lthr', 225)
        self.cthr = kwargs.get('cthr', 2)
        self.mexpand = kwargs.get('expand', 2)
        self.fade_nums = kwargs.get('fade_num', 8)
        self.apply_range = kwargs.get('apply_range', None)

        if self.clip_color_family is not vs.YUV:
            raise TypeError(MODULE_NAME + ': clip should be a YUV clip')

        y = self.core.std.ShufflePlanes(self.clip, 0, vs.GRAY)
        u = self.core.std.ShufflePlanes(self.clip, 1, vs.GRAY)
        v = self.core.std.ShufflePlanes(self.clip, 2, vs.GRAY)
        try:
            u = self.core.resize.Bicubic(u, self.clip_width, self.clip_height, src_left=0.25)
            v = self.core.resize.Bicubic(v, self.clip_width, self.clip_height, src_left=0.25)
        except vs.Error:
            u = mvf.Depth(self.core.fmtc.resample(u, self.clip_width, self.clip_height, sx=0.25), 8)
            v = mvf.Depth(self.core.fmtc.resample(v, self.clip_width, self.clip_height, sx=0.25), 8)

        expr = "x {lthr} > y 128 - abs {cthr} < and z 128 - abs {cthr} < and 255 0 ?".format(lthr=self.lthr,
                                                                                             cthr=self.cthr)
        self.mask = self.core.std.Expr([y, u, v], expr)
        if self.mexpand > 0:
            self.expand(self.mexpand)

        frame_count = self.clip.num_frames

        def shift_backward(n, clip, num):
            if n + num > frame_count - 1:
                return clip[frame_count - 1]
            else:
                return clip[n + num]

        def shift_forward(n, clip, num):
            if n - num < 0:
                return clip[0]
            else:
                return clip[n - num]

        if isinstance(self.fade_nums, int):
            in_num = self.fade_nums
            out_num = self.fade_nums
        elif isinstance(self.fade_nums, (list, tuple)):
            if len(self.fade_nums) != 2:
                raise ValueError(MODULE_NAME + ': incorrect fade_nums setting.')
            in_num = self.fade_nums[0]
            out_num = self.fade_nums[1]
        else:
            raise TypeError(MODULE_NAME + ': fade_num can only be int, tuple or list.')

        if self.fade_nums is not 0:
            fade_in = self.core.std.FrameEval(self.mask, functools.partial(shift_backward, clip=self.mask, num=in_num))
            fade_out = self.core.std.FrameEval(self.mask, functools.partial(shift_forward, clip=self.mask, num=out_num))
            self.mask = self.core.std.Expr([self.mask, fade_in, fade_out], " x y max z max")

        if self.apply_range is not None:
            if not isinstance(self.apply_range, (list, tuple)):
                raise TypeError(MODULE_NAME + ': apply range can only be list or tuple.')
            elif len(self.apply_range) != 2:
                raise ValueError(MODULE_NAME + ': incorrect apply range setting.')
            else:
                try:
                    blank_clip = self.core.std.BlankClip(self.mask)
                    if 0 in self.apply_range:
                        self.mask = self.mask[self.apply_range[0]:self.apply_range[1]] + \
                                    blank_clip[self.apply_range[1]:]
                    elif frame_count in self.apply_range:
                        self.mask = blank_clip[0:self.apply_range[0]] + \
                                    self.mask[self.apply_range[0]:self.apply_range[1]]
                    else:
                        self.mask = blank_clip[0:self.apply_range[0]] + \
                                    self.mask[self.apply_range[0]:self.apply_range[1]] + \
                                    blank_clip[self.apply_range[1]:]
                except vs.Error:
                    raise ValueError(MODULE_NAME + ': incorrect apply range setting. Possible end less than start.')


def daa(clip, mode=-1, opencl=False):
    core = vs.get_core()
    if opencl is True:
        try:
            daa_nnedi3 = core.nnedi3cl.NNEDI3CL
        except AttributeError:
            daa_nnedi3 = core.nnedi3.nnedi3
    else:
        try:
            daa_nnedi3 = core.znedi3.nnedi3
        except AttributeError:
            daa_nnedi3 = core.nnedi3.nnedi3
    if mode == -1:
        nn = daa_nnedi3(clip, field=3)
        nnt = daa_nnedi3(core.std.Transpose(clip), field=3).std.Transpose()
        clph = core.std.Merge(core.std.SelectEvery(nn, cycle=2, offsets=0),
                              core.std.SelectEvery(nn, cycle=2, offsets=1))
        clpv = core.std.Merge(core.std.SelectEvery(nnt, cycle=2, offsets=0),
                              core.std.SelectEvery(nnt, cycle=2, offsets=1))
        clp = core.std.Merge(clph, clpv)
    elif mode == 1:
        nn = daa_nnedi3(clip, field=3)
        clp = core.std.Merge(core.std.SelectEvery(nn, cycle=2, offsets=0),
                             core.std.SelectEvery(nn, cycle=2, offsets=1))
    elif mode == 2:
        nnt = daa_nnedi3(core.std.Transpose(clip), field=3).std.Transpose()
        clp = core.std.Merge(core.std.SelectEvery(nnt, cycle=2, offsets=0),
                             core.std.SelectEvery(nnt, cycle=2, offsets=1))
    else:
        raise ValueError(MODULE_NAME + ': daa: at least one direction should be processed.')
    return clp


def temporal_stabilize(clip, src, delta=3, pel=1, retain=0.6):
    core = vs.get_core()
    clip_bits = clip.format.bits_per_sample
    src_bits = src.format.bits_per_sample
    if clip_bits != src_bits:
        raise ValueError(MODULE_NAME + ': temporal_stabilize: bits depth of clip and src mismatch.')
    if delta not in [1, 2, 3]:
        raise ValueError(MODULE_NAME + ': temporal_stabilize: delta (1~3) invalid.')

    diff = core.std.MakeDiff(src, clip)
    clip_super = core.mv.Super(clip, pel=pel)
    diff_super = core.mv.Super(diff, pel=pel, levels=1)

    backward_vectors = [core.mv.Analyse(clip_super, isb=True, delta=i+1, overlap=8, blksize=16) for i in range(delta)]
    forward_vectors = [core.mv.Analyse(clip_super, isb=False, delta=i+1, overlap=8, blksize=16) for i in range(delta)]
    vectors = [vector for vector_group in zip(backward_vectors, forward_vectors) for vector in vector_group]

    stabilize_func = {
        1: core.mv.Degrain1,
        2: core.mv.Degrain2,
        3: core.mv.Degrain3
    }
    diff_stabilized = stabilize_func[delta](diff, diff_super, *vectors)

    neutral = 1 << (clip_bits - 1)
    expr = 'x {neutral} - abs y {neutral} - abs < x y ?'.format(neutral=neutral)
    diff_stabilized_limited = core.std.Expr([diff, diff_stabilized], expr)
    diff_stabilized = core.std.Merge(diff_stabilized_limited, diff_stabilized, retain)
    clip_stabilized = core.std.MakeDiff(src, diff_stabilized)
    return clip_stabilized


def soothe(clip, src, keep=24):
    core = vs.get_core()
    clip_bits = clip.format.bits_per_sample
    src_bits = src.format.bits_per_sample
    if clip_bits != src_bits:
        raise ValueError(MODULE_NAME + ': temporal_stabilize: bits depth of clip and src mismatch.')

    neutral = 1 << (clip_bits - 1)
    ceil = (1 << clip_bits) - 1
    multiple = ceil // 255
    const = 100 * multiple
    kp = keep * multiple

    diff = core.std.MakeDiff(src, clip)
    try:
        diff_soften = core.misc.AverageFrame(diff, weights=[1, 1, 1], scenechange=32)
    except AttributeError:
        diff_soften = core.focus.TemporalSoften(diff, radius=1, luma_threshold=255,
                                                chroma_threshold=255, scenechange=32, mode=2)
    diff_soothed_expr = "x {neutral} - y {neutral} - * 0 < x {neutral} - {const} / {kp} * {neutral} + " \
                        "x {neutral} - abs y {neutral} - abs > " \
                        "x {kp} * y {const} {kp} - * + {const} / x ? ?".format(neutral=neutral, const=const, kp=kp)
    diff_soothed = core.std.Expr([diff, diff_soften], diff_soothed_expr)
    clip_soothed = core.std.MakeDiff(src, diff_soothed)
    return clip_soothed


def TAAmbk(clip, aatype=1, aatypeu=None, aatypev=None, preaa=0, strength=0.0, cycle=0, mtype=None, mclip=None,
           mthr=None, mthr2=None, mlthresh=None, mpand=(1, 0), txtmask=0, txtfade=0, thin=0, dark=0.0, sharp=0,
           aarepair=0, postaa=None, src=None, stabilize=0, down8=True, showmask=0, opencl=False, opencl_device=0,
           **args):
    core = vs.get_core()
    aatypeu = aatype if aatypeu is None else aatypeu
    aatypev = aatype if aatypev is None else aatypev

    if mtype is None:
        mtype = 0 if preaa == 0 and True not in (aatype, aatypeu, aatypev) else 1

    if postaa is None:
        postaa = True if abs(sharp) > 70 or (0.4 < abs(sharp) < 1) else False

    if src is None:
        src = clip
    else:
        if clip.format.id != src.format.id:
            raise ValueError(MODULE_NAME + ': clip format and src format mismatch.')
        elif clip.width != src.width or clip.height != src.height:
            raise ValueError(MODULE_NAME + ': clip resolution and src resolution mismatch.')

    preaa_clip = clip if preaa == 0 else daa(clip, preaa, opencl)

    if thin == 0 and dark == 0:
        edge_enhanced_clip = preaa_clip
    elif thin != 0 and dark != 0:
        edge_enhanced_clip = haf.Toon(core.warp.AWarpSharp2(preaa_clip, depth=int(thin)), str=float(dark))
    elif thin == 0:
        edge_enhanced_clip = haf.Toon(preaa_clip, str=float(dark))
    else:
        edge_enhanced_clip = core.warp.AWarpSharp2(preaa_clip, depth=int(thin))

    aa_kernel = {
        1: AAEedi2,
        2: AAEedi3,
        3: AANnedi3,
        4: AANnedi3UpscaleSangNom,
        5: AASpline64NRSangNom,
        6: AASpline64SangNom,
        -1: AAEedi2SangNom,
        -2: AAEedi3SangNom,
        -3: AANnedi3SangNom,
        'Eedi2': AAEedi2,
        'Eedi3': AAEedi3,
        'Nnedi3': AANnedi3,
        'Nnedi3UpscaleSangNom': AANnedi3UpscaleSangNom,
        'Spline64NrSangNom': AASpline64NRSangNom,
        'Spline64SangNom': AASpline64SangNom,
        'Eedi2SangNom': AAEedi2SangNom,
        'Eedi3SangNom': AAEedi3SangNom,
        'Nnedi3SangNom': AANnedi3SangNom,
        'PointSangNom': AAPointSangNom
    }

    aaed_clip = None
    if clip.format.color_family is vs.YUV:
        y = core.std.ShufflePlanes(edge_enhanced_clip, 0, vs.GRAY)
        u = core.std.ShufflePlanes(edge_enhanced_clip, 1, vs.GRAY)
        v = core.std.ShufflePlanes(edge_enhanced_clip, 2, vs.GRAY)
        if aatype != 0:
            try:
                y = aa_kernel[aatype](y, strength, down8, opencl=opencl, opencl_device=opencl_device, **args).out()
                cycle_y = cycle
                while cycle_y > 0:
                    y = aa_kernel[aatype](y, strength, down8, opencl=opencl, opencl_device=opencl_device, **args).out()
                    cycle_y -= 1
                y = mvf.Depth(y, clip.format.bits_per_sample) if down8 is True else y
            except KeyError:
                raise ValueError(MODULE_NAME + ': unknown aatype.')
        if aatypeu != 0:
            try:
                u = aa_kernel[aatypeu](u, 0, down8, opencl=opencl, opencl_device=opencl_device,
                                       **args).out()  # Won't do predown for u plane
                cycle_u = cycle
                while cycle_u > 0:
                    u = aa_kernel[aatypeu](u, 0, down8, opencl=opencl, opencl_device=opencl_device, **args).out()
                    cycle_u -= 1
                u = mvf.Depth(u, clip.format.bits_per_sample) if down8 is True else u
            except KeyError:
                raise ValueError(MODULE_NAME + ': unknown aatypeu.')
        if aatypev != 0:
            try:
                v = aa_kernel[aatypev](v, 0, down8, opencl=opencl, opencl_device=opencl_device,
                                       **args).out()  # Won't do predown for v plane
                cycle_v = cycle
                while cycle_v > 0:
                    v = aa_kernel[aatypev](v, 0, down8, opencl=opencl, opencl_device=opencl_device, **args).out()
                    cycle_v -= 1
                v = mvf.Depth(v, clip.format.bits_per_sample) if down8 is True else v
            except KeyError:
                raise ValueError(MODULE_NAME + ': unknown aatypev.')
        aaed_clip = core.std.ShufflePlanes([y, u, v], [0, 0, 0], vs.YUV)
    elif clip.format.color_family is vs.GRAY:
        y = edge_enhanced_clip
        if aatype != 0:
            try:
                y = aa_kernel[aatype](y, strength, down8, **args).out()
                cycle_y = cycle
                while cycle_y > 0:
                    y = aa_kernel[aatype](y, strength, down8, **args).out()
                    cycle_y -= 1
                aaed_clip = mvf.Depth(y, clip.format.bits_per_sample) if down8 is True else y
            except KeyError:
                raise ValueError(MODULE_NAME + ': unknown aatype.')
    else:
        raise ValueError(MODULE_NAME + ': Unsupported color family.')

    abs_sharp = abs(sharp)
    if sharp >= 1:
        sharped_clip = haf.LSFmod(aaed_clip, strength=int(abs_sharp), defaults='old', source=src)
    elif sharp > 0:
        per = int(40 * abs_sharp)
        matrix = [-1, -2, -1, -2, 52 - per, -2, -1, -2, -1]
        sharped_clip = core.std.Convolution(aaed_clip, matrix)
    elif sharp == 0:
        sharped_clip = aaed_clip
    elif sharp > -1:
        sharped_clip = haf.LSFmod(aaed_clip, strength=round(abs_sharp * 100), defaults='fast', source=src)
    elif sharp == -1:
        blured = core.rgvs.RemoveGrain(aaed_clip, mode=20 if aaed_clip.width > 1100 else 11)
        diff = core.std.MakeDiff(aaed_clip, blured)
        diff = core.rgvs.Repair(diff, core.std.MakeDiff(src, aaed_clip), mode=13)
        sharped_clip = core.std.MergeDiff(aaed_clip, diff)
    else:
        sharped_clip = aaed_clip

    postaa_clip = sharped_clip if postaa is False else soothe(sharped_clip, src, 24)
    repaired_clip = postaa_clip if aarepair == 0 else core.rgvs.Repair(src, postaa_clip, aarepair)
    stabilized_clip = repaired_clip if stabilize == 0 else temporal_stabilize(repaired_clip, src, stabilize)

    if mclip is not None:
        mask = mclip
        try:
            masked_clip = core.std.MaskedMerge(src, stabilized_clip, mask, first_plane=True)
        except vs.Error:
            raise RuntimeError(MODULE_NAME + ': Something wrong with your mclip. '
                                             'Maybe resolution or bit_depth mismatch.')
    elif mtype != 0:
        if mtype == 1 or mtype is 'Canny':
            opencl_device = args.get('opencl_device', 0)
            mthr = 1.2 if mthr is None else mthr
            mthr2 = 8.0 if mthr2 is None else mthr2
            mask = MaskCanny(clip, sigma=mthr, t_h=mthr2, lthresh=mlthresh, mpand=mpand, opencl=opencl,
                             opencl_devices=opencl_device).out()
        elif mtype == 2 or mtype is 'Sobel':
            mthr = 1.2 if mthr is None else mthr
            mthr2 = 48 if mthr2 is None else mthr2
            mask = MaskSobel(clip, sigma=mthr, binarize=mthr2, lthresh=mlthresh, mpand=mpand).out()
        elif mtype == 3 or mtype is 'prewitt':
            mthr = 62 if mthr is None else mthr
            mask = MaskPrewitt(clip, factor=mthr, lthresh=mlthresh, mpand=mpand).out()
        else:
            raise ValueError(MODULE_NAME + ': unknown mtype.')
        masked_clip = core.std.MaskedMerge(src, stabilized_clip, mask)
    else:
        masked_clip = stabilized_clip

    if txtmask > 0 and clip.format.color_family is not vs.GRAY:
        text_mask = FadeTextMask(clip, lthr=txtmask, fade_nums=txtfade).out()
        txt_protected_clip = core.std.MaskedMerge(masked_clip, src, text_mask, first_plane=True)
    else:
        txt_protected_clip = masked_clip

    if clip.format.bits_per_sample > 8 and down8 is True:
        clamped_clip = mvf.LimitFilter(src, txt_protected_clip, thr=1.0, elast=2.0)
    else:
        clamped_clip = txt_protected_clip

    try:
        if showmask == -1:
            return text_mask
        elif showmask == 1:
            return mask
        elif showmask == 2:
            return core.std.StackVertical(
                [core.std.ShufflePlanes([mask, core.std.BlankClip(src)], [0, 1, 2], vs.YUV), src])
        elif showmask == 3:
            return core.std.Interleave(
                [core.std.ShufflePlanes([mask, core.std.BlankClip(src)], [0, 1, 2], vs.YUV), src])
        else:
            return clamped_clip
    except UnboundLocalError:
        raise RuntimeError(MODULE_NAME + ': No mask to show if you don\'t have one.')
