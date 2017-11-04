Description
===========

EEDI3 works by finding the best non-decreasing (non-crossing) warping between two lines by minimizing a cost functional. The cost is based on neighborhood similarity (favor connecting regions that look similar), the vertical difference created by the interpolated values (favor small differences), the interpolation directions (favor short connections vs long), and the change in interpolation direction from pixel to pixel (favor small changes).

Ported from AviSynth plugin http://bengal.missouri.edu/~kes25c/ and http://ldesoras.free.fr/prod.html#src_eedi3


Usage
=====

    eedi3m.EEDI3(clip clip, int field[, bint dh=False, int[] planes, float alpha=0.2, float beta=0.25, float gamma=20.0, int nrad=2, int mdis=20, bint hp=False, bint ucubic=True, bint cost3=True, int vcheck=2, float vthresh0=32.0, float vthresh1=64.0, float vthresh2=4.0, clip sclip=None, int opt=0])

* clip: Clip to process. Any planar format with either integer sample type of 8-16 bit depth or float sample type of 32 bit depth is supported.

* field: Controls the mode of operation (double vs same rate) and which field is kept.
  * 0 = same rate, keep bottom field
  * 1 = same rate, keep top field
  * 2 = double rate (alternates each frame), starts with bottom
  * 3 = double rate (alternates each frame), starts with top

* dh: Doubles the height of the input. Each line of the input is copied to every other line of the output and the missing lines are interpolated. If field=0, the input is copied to the odd lines of the output. If field=1, the input is copied to the even lines of the output. field must be set to either 0 or 1 when using dh=True.

* planes: A list of the planes to process. By default all planes are processed.

* alpha/beta/gamma: These trade off line/edge connection vs artifacts created. alpha and beta must be in the range [0,1], and the sum alpha+beta must be in the range [0,1]. alpha is the weight given to connecting similar neighborhoods. The larger alpha is the more lines/edges should be connected. beta is the weight given to vertical difference created by the interpolation. The larger beta is the less edges/lines will be connected (at 1.0 you get no edge directedness at all). The remaining weight (1.0-alpha-beta) is given to interpolation direction (large directions (away from vertical) cost more). So the more weight you have here the more shorter connections will be favored. Finally, gamma penalizes changes in interpolation direction, the larger gamma is the smoother the interpolation field between two lines (range is [0,inf]. If lines aren't getting connected then increase alpha and maybe decrease beta/gamma. Go the other way if you are getting unwanted artifacts.

* nrad/mdis: nrad sets the radius used for computing neighborhood similarity. Valid range is [0,3]. mdis sets the maximum connection radius. Valid range is [1,40]. If mdis=20, then when interpolating pixel (50,10) (x,y), the farthest connections allowed would be between (30,9)/(70,11) and (70,9)/(30,11). Larger mdis will allow connecting lines of smaller slope, but also increases the chance of artifacts. Larger mdis will be slower. Larger nrad will be slower.

* hp/ucubic/cost3: These are speed vs quality options. hp=True, use half pel steps, hp=False, use full pel steps. Currently only full pel is implemented and this parameter has no effect. ucubic=True, use cubic 4 point interpolation, ucubic=False, use 2 point linear interpolation. cost3=True, use 3 neighborhood cost function to define similarity, cost3=False, use 1 neighborhood cost function.

* vcheck/vthresh0/vthresh1/vthresh2/sclip:
```
      vcheck settings:

          0 - no reliability check
          1 - weak reliability check
          2 - med reliability check
          3 - strong reliability check

      If vcheck is greater than 0, then the resulting interpolation is checked for reliability/consistency. Assume
      we interpolated pixel 'fh' below using dir=4 (i.e. averaging pixels bl and cd).

           aa ab ac ad ae af ag ah ai aj ak al am an ao ap
                                eh          el
           ba bb bc bd be bf bg bh bi bj bk bl bm bn bo bp
                    fd          fh          fl
           ca cb cc cd ce cf cg ch ci cj ck cl cm cn co cp
                    gd          gh
           da db dc dd de df dg dh di dj dk dl dm dn do dp

      When checking pixel 'fh' the following is computed:

            d0 = abs((el+fd)/2 - bh)
            d1 = abs((fl+gd)/2 - ch)

            q2 = abs(bh-fh)+abs(ch-fh)
            q3 = abs(el-bl)+abs(fl-bl)
            q4 = abs(fd-cd)+abs(gd-cd)

            d2 = abs(q2-q3)
            d3 = abs(q2-q4)

            mdiff0 = vcheck == 1 ? min(d0,d1) : vcheck == 2 ? ((d0+d1+1)>>1) : max(d0,d1)
            mdiff1 = vcheck == 1 ? min(d2,d3) : vcheck == 2 ? ((d2+d3+1)>>1) : max(d2,d3)

            a0 = mdiff0/vthresh0;
            a1 = mdiff1/vthresh1;
            a2 = max((vthresh2-abs(dir))/vthresh2,0.0f)

            a = min(max(max(a0,a1),a2),1.0f)

            final_value = (1.0-a)*fh + a*cint


        ** If sclip is supplied, cint is the corresponding value from sclip. If sclip isn't supplied,
           then vertical cubic interpolation is used to create it.
```

* opt: Sets which cpu optimizations to use.
  * 0 = auto detect
  * 1 = use c
  * 2 = use sse2
  * 3 = use sse4.1
  * 4 = use avx
  * 5 = use avx512

---

    eedi3m.EEDI3CL(clip clip, int field[, bint dh=False, int[] planes, float alpha=0.2, float beta=0.25, float gamma=20.0, int nrad=2, int mdis=20, bint hp=False, bint ucubic=True, bint cost3=True, int vcheck=2, float vthresh0=32.0, float vthresh1=64.0, float vthresh2=4.0, clip sclip=None, int opt=0, int device=-1, bint list_device=False, bint info=False])

* device: Sets target OpenCL device. Use `list_device` to get the index of the available devices. By default the default device is selected.

* list_device: Whether the devices list is drawn on the frame.

* info: Whether the OpenCL-related info is drawn on the frame.

* opt: Sets which cpu optimizations to use.
  * 0 = auto detect
  * 1 = use c
  * 2 = use sse2


Compilation
===========

Requires `Boost` unless configured with `--disable-opencl`.

```
./autogen.sh
./configure
make
```
