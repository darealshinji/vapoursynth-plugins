A collection of [VapourSynth](https://github.com/vapoursynth/vapoursynth) plugins
===================================

The main reason for this collection is to make it easier to build packages for Ubuntu.
So don't expect this build system to work on anything else than Linux on x86 architectures.

**Build dependencies**:<br>
```
git nasm yasm libtool pkg-config
libfftw3-dev libpng-dev libsndfile1-dev libxvidcore-dev zlib1g-dev
libopencv-dev ocl-icd-libopencl1 opencl-headers
```


**Installation**:
```
git clone https://github.com/darealshinji/vapoursynth-plugins.git
cd vapoursynth-plugins
./autogen.sh
./configure
make
make install
```

Run `make install INSTALL_MODEL_WEIGHTS=0` to skip the installation of the model weights files.

Ubuntu users can also use my PPA:
```
sudo add-apt-repository ppa:djcj/vapoursynth
sudo apt-get update
sudo apt-get install vapoursynth-extra-plugins
```


**Plugins**:<br>
[addgrain r5](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-AddGrain)<br>
[awarpsharp2 1](https://github.com/dubhater/vapoursynth-awarpsharp2)<br>
[bifrost 2.2](https://github.com/dubhater/vapoursynth-bifrost)<br>
[bilateral r3](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-Bilateral)<br>
[bm3d r5](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-BM3D)<br>
[combmask 0.0.1](https://github.com/chikuzen/CombMask)<br>
[convo2d 0.2.0](https://github.com/chikuzen/convo2d)<br>
[ctmf r2](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-CTMF)<br>
[d2vsource 16+git20160202](https://github.com/dwbuiten/d2vsource)<br>
[damb r3](https://github.com/dubhater/vapoursynth-damb)<br>
[dctfilter r2](https://bitbucket.org/mystery_keeper/vapoursynth-dctfilter)<br>
[deblock r4](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-Deblock)<br>
[delogo 0.4](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-DeLogo)<br>
[dfttest r3](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-DFTTest)<br>
[eedi2 r6](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-EEDI2)<br>
[ffms2 2.22+git20160222](https://github.com/FFMS/ffms2)<br>
[ff3dfilter git20150227](https://github.com/VFR-maniac/VapourSynth-FFT3DFilter)<br>
[fieldhint 3](https://github.com/dubhater/vapoursynth-fieldhint)<br>
[fillborders 1.0](https://github.com/dubhater/vapoursynth-fillborders)<br>
[flash3kyuu_deband 2.0.0](https://github.com/SAPikachu/flash3kyuu_deband)<br>
[fluxsmooth 1.0+git20141002](https://github.com/dubhater/vapoursynth-fluxsmooth)<br>
[fmtconv r17+git20150726](https://github.com/EleonoreMizo/fmtconv)<br>
[genericfilters 1.0.0+git20150505](https://github.com/myrsloik/GenericFilters)<br>
[histogram 1.0+git20141212](https://github.com/dubhater/vapoursynth-histogram)<br>
[imagereader 0.2.1](https://github.com/chikuzen/vsimagereader)<br>
[it 1.2](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-IT)<br>
[knlmeanscl 0.7.4](https://github.com/Khanattila/KNLMeansCL)<br>
[lsmashsource git20160131](https://github.com/VFR-maniac/L-SMASH-Works)<br>
[msmoosh 1.1](https://github.com/dubhater/vapoursynth-msmoosh)<br>
[mvtools 11](https://github.com/dubhater/vapoursynth-mvtools)<br>
[nnedi3 8](https://github.com/dubhater/vapoursynth-nnedi3)<br>
[rawsource git20130731](https://github.com/chikuzen/vsrawsource)<br>
[reduceflicker git20150225](https://github.com/VFR-maniac/VapourSynth-ReduceFlicker)<br>
[retinex r3](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-Retinex)<br>
[sangnommod 0.1+git20150109](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-SangNomMod)<br>
[scenechange 0.2.0](http://forum.doom9.org/showthread.php?t=166769)<br>
[scrawl 1.0+git20131016](https://github.com/dubhater/vapoursynth-scrawl)<br>
[scxvid 1](https://github.com/dubhater/vapoursynth-scxvid)<br>
[ssiq 1.0](https://github.com/dubhater/vapoursynth-ssiq)<br>
[tc2cfr 0.0.1+git20131117](https://github.com/gnaggnoyil/tc2cfr)<br>
[tcanny r5](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-TCanny)<br>
[tcomb 1.0](https://github.com/dubhater/vapoursynth-tcomb)<br>
[tdeintmod r6+git20160110](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-TDeintMod)<br>
[templinearapproximate r3+git20160228](https://bitbucket.org/mystery_keeper/templinearapproximate-vapoursynth)<br>
[temporalsoften 1.0](https://github.com/dubhater/vapoursynth-temporalsoften)<br>
[temporalsoften2 0.1.1](http://forum.doom9.org/showthread.php?t=166769)<br>
[tnlmeans git20150225](https://github.com/VFR-maniac/VapourSynth-TNLMeans)<br>
[vaguedenoiser r2](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-VagueDenoiser)<br>
[vautodeint git20130608](https://github.com/gnaggnoyil/VAutoDeint)<br>
[videoscope 1.0](https://github.com/dubhater/vapoursynth-videoscope)<br>
[waifu2x-opt r1+20160218](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-waifu2x-opt)<br>
[waifu2x-w2xc r6](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-Waifu2x-w2xc)<br>
[wwxd 1.0](https://github.com/dubhater/vapoursynth-wwxd)<br>
[yadifmod r9](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-Yadifmod)<br>
[zimg git20141102](https://github.com/dubhater/zimg)

