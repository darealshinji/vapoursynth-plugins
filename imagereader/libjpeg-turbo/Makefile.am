ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libjpeg.la
libjpeg_la_LDFLAGS = -version-info ${LIBTOOL_CURRENT}:${SO_MINOR_VERSION}:${SO_AGE} -no-undefined
include_HEADERS = jerror.h jmorecfg.h jpeglib.h

if WITH_TURBOJPEG
lib_LTLIBRARIES += libturbojpeg.la
libturbojpeg_la_LDFLAGS = -version-info 0:0 -no-undefined
include_HEADERS += turbojpeg.h
endif

nodist_include_HEADERS = jconfig.h


HDRS = jchuff.h jdct.h jdhuff.h jerror.h jinclude.h jmemsys.h jmorecfg.h \
	jpegint.h jpeglib.h jversion.h jsimd.h jsimddct.h jpegcomp.h

libjpeg_la_SOURCES = $(HDRS) jcapimin.c jcapistd.c jccoefct.c jccolor.c \
	jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c \
	jcomapi.c jcparam.c jcphuff.c jcprepct.c jcsample.c jctrans.c \
	jdapimin.c jdapistd.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c \
	jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c \
	jdmerge.c jdphuff.c jdpostct.c jdsample.c jdtrans.c jerror.c \
	jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c \
	jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c

if WITH_ARITH
libjpeg_la_SOURCES += jaricom.c
endif

if WITH_ARITH_ENC
libjpeg_la_SOURCES += jcarith.c
endif

if WITH_ARITH_DEC
libjpeg_la_SOURCES += jdarith.c
endif


if WITH_TURBOJPEG

libturbojpeg_la_SOURCES = $(libjpeg_la_SOURCES) turbojpeg.c turbojpeg.h \
	transupp.c transupp.h jdatadst-tj.c jdatasrc-tj.c

TJMAPFILE = turbojpeg-mapfile

libturbojpeg_la_SOURCES += $(TJMAPFILE)

if VERSION_SCRIPT
libturbojpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)$(srcdir)/$(TJMAPFILE)
endif

endif


if VERSION_SCRIPT
libjpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)libjpeg.map
endif


SUBDIRS = md5


if WITH_SIMD

SUBDIRS += simd
libjpeg_la_LIBADD = simd/libsimd.la
libturbojpeg_la_LIBADD = simd/libsimd.la

else

libjpeg_la_SOURCES += jsimd_none.c

endif


if WITH_TURBOJPEG

noinst_PROGRAMS = tjbench tjunittest

tjbench_SOURCES = tjbench.c bmp.h bmp.c tjutil.h tjutil.c rdbmp.c rdppm.c \
	wrbmp.c wrppm.c

tjbench_LDADD = libturbojpeg.la libjpeg.la -lm

tjbench_CFLAGS = -DBMP_SUPPORTED -DPPM_SUPPORTED

tjunittest_SOURCES = tjunittest.c tjutil.h tjutil.c

tjunittest_LDADD = libturbojpeg.la

endif


MD5_JPEG_INT = 9a68f56bc76e466aa7e52f415d0f4a5f
MD5_JPEG_FAST = 0e1502e7fa421835e376a314fac2a39f
MD5_JPEG_FAST_100 = 7bf72a8e741d64eecb960c97323af77c
MD5_JPEG_FLOAT = d1623885ffafcd40c684af09e3d65cd5
MD5_JPEG_FLOAT_NOSIMD = fb4884c35f8273f498cb32879de5c455
MD5_JPEG_INT_GRAY = 72b51f894b8f4a10b3ee3066770aa38d
MD5_PPM_INT = d1ed0d11f076b842525271647716aeb8
MD5_PPM_FAST = 048298a2d2410261c0533cb97bcfef23
MD5_PPM_FLOAT = 7f5b446ee36b2630e06785b8d42af15f
MD5_PPM_FLOAT_NOSIMD = 64072f1dbdc5b3a187777788604971a5
MD5_PPM_INT_2_1 = 9f9de8c0612f8d06869b960b05abf9c9
MD5_PPM_INT_15_8 = b6875bc070720b899566cc06459b63b7
MD5_PPM_INT_7_4 = 06a177eae05f164fac57f7a2c346ee87
MD5_PPM_INT_13_8 = bc3452573c8152f6ae552939ee19f82f
MD5_PPM_INT_3_2 = f5a8b88a8a7f96016f04d259cf82ed67
MD5_PPM_INT_11_8 = d8cc73c0aaacd4556569b59437ba00a5
MD5_PPM_INT_5_4 = 32775dd9ad2ab90f4c5b219b53e0c86c
MD5_PPM_INT_9_8 = d25e61bc7eac0002f5b393aa223747b6
MD5_PPM_INT_7_8 = ddb564b7c74a09494016d6cd7502a946
MD5_PPM_INT_3_4 = 8ed8e68808c3fbc4ea764fc9d2968646
MD5_PPM_INT_5_8 = a3363274999da2366a024efae6d16c9b
MD5_PPM_INT_1_2 = e692a315cea26b988c8e8b29a5dbcd81
MD5_PPM_INT_3_8 = 79eca9175652ced755155c90e785a996
MD5_PPM_INT_1_4 = 79cd778f8bf1a117690052cacdd54eca
MD5_PPM_INT_1_8 = 391b3d4aca640c8567d6f8745eb2142f
MD5_PPM_FAST_1_2 = f30bcf6d32ccd44cbdd9aeaacbd9454f
MD5_BMP_256 = 4980185e3776e89bd931736e1cddeee6
MD5_JPEG_ARI = e986fb0a637a8d833d96e8a6d6d84ea1
MD5_PPM_ARI = 72b59a99bcf1de24c5b27d151bde2437
MD5_JPEG_PROG = 1c4afddc05c0a43489ee54438a482d92
MD5_JPEG_CROP = b4197f377e621c4e9b1d20471432610d

test: testclean all
if WITH_TURBOJPEG
	./tjunittest
	./tjunittest -alloc
	./tjunittest -yuv
endif

testclean:
	rm -f testout*
	rm -f *_GRAY_*.bmp
	rm -f *_GRAY_*.png
	rm -f *_GRAY_*.ppm
	rm -f *_GRAY_*.jpg
	rm -f *_GRAY.yuv
	rm -f *_420_*.bmp
	rm -f *_420_*.png
	rm -f *_420_*.ppm
	rm -f *_420_*.jpg
	rm -f *_420.yuv
	rm -f *_422_*.bmp
	rm -f *_422_*.png
	rm -f *_422_*.ppm
	rm -f *_422_*.jpg
	rm -f *_422.yuv
	rm -f *_444_*.bmp
	rm -f *_444_*.png
	rm -f *_444_*.ppm
	rm -f *_444_*.jpg
	rm -f *_444.yuv
	rm -f *_440_*.bmp
	rm -f *_440_*.png
	rm -f *_440_*.ppm
	rm -f *_440_*.jpg
	rm -f *_440.yuv

tjtest:
	sh ./tjbenchtest

