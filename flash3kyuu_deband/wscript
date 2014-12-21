import os

from waflib import Utils, Logs

APPNAME = "f3kdb"
VERSION = "2.0pre"

top = "."


def options(opt):
    opt.load("compiler_cxx")

    conf_opt = opt.get_option_group("configure options")

    conf_opt.add_option("--libdir", action="store", default="${PREFIX}/lib",
                        help="library installation directory")
    conf_opt.add_option("--includedir", action="store",
                        default="${PREFIX}/include/f3kdb",
                        help="header installation directory")

    conf_opt.add_option("--mode", action="store", default="release",
                        help="the mode to compile in (debug/release)")
    conf_opt.add_option("--shared", action="store_true", default=True,
                        help="build shared libraries (default)")
    conf_opt.add_option("--no-shared", action="store_false", dest="shared",
                        help="do not build shared libraries")
    conf_opt.add_option("--static", action="store_true", default=False,
                        help="build static libraries")
    conf_opt.add_option("--no-static", action="store_false", dest="static",
                        help="do not build static libraries (default)")

    conf_opt.add_option("--enable-vs", "--enable-vapoursynth",
                        action="store_true", dest="vapoursynth", default=True,
                        help="enable Vapoursynth support (default)")
    conf_opt.add_option("--disable-vs", "--disable-vapoursynth",
                        action="store_false", dest="vapoursynth",
                        help="disable Vapoursynth support")

    conf_opt.add_option("--enable-avs", "--enable-avisynth",
                        action="store_true", dest="avisynth", default=True,
                        help="enable Avisynth support (default, Windows only)")
    conf_opt.add_option("--disable-avs", "--disable-avisynth",
                        action="store_false", dest="avisynth",
                        help="disable Avisynth support")

    inst_opt = opt.get_option_group("install/uninstall options")
    inst_opt.add_option("--no-ldconfig", action="store_false",
                        dest="ldconfig", default=True,
                        help="don't run ldconfig after install "
                             "(default: run, non-Windows only)")

    opt.recurse("test")


def _check_cxx(conf, feature, fragment, mandatory=False):
    conf.check_cxx(
        msg=" - " + feature,
        define_name="HAVE_" + feature.replace(" ", "_").upper(),
        fragment=fragment,
        mandatory=mandatory,
    )

def _check_optional(conf, **kwargs):
    args = {'mandatory': False, 'uselib_store': 'OPTIONAL'}
    args.update(kwargs)
    conf.check_cxx(**args)

def configure_gcc(conf):
    # clang is also configured here, since their configurations are the same
    def add_options(flags, options):
        for flag in flags:
            conf.env.append_unique(flag, options)

    add_options(["CFLAGS", "CXXFLAGS"],
                ["-fPIC", "-Wall", "-Wextra", "-Wno-unused-parameter",
                 "-fvisibility=hidden", "-fvisibility-inlines-hidden",
                 "-Werror", "-std=c++11"])

    _check_optional(conf, linkflags='-Wl,-Bsymbolic')
    _check_optional(conf, linkflags='-Wl,-z,noexecstack')

    add_options(["LINKFLAGS_cshlib",
                 "LINKFLAGS_cprogram",
                 "LINKFLAGS_cxxshlib",
                 "LINKFLAGS_cxxprogram"],
                 conf.env.LINKFLAGS_OPTIONAL)

    if conf.options.mode == "debug":
        add_options(["CFLAGS", "CXXFLAGS"],
                    ["-g", "-ggdb", "-ftrapv"])
    elif conf.options.mode == "release":
        add_options(["CFLAGS", "CXXFLAGS"],
                    ["-O3"])
    else:
        conf.fatal("--mode must be either debug or release.")

    _check_cxx(
        conf,
        "alignas",
        "int main() { alignas(8) int x = 0; return x; }",
        mandatory=True,
    )

    for dir in ["libdir", "includedir"]:
        u = dir.upper()

        conf.env[u] = Utils.subst_vars(conf.options.__dict__[dir], conf.env)
        conf.msg("Setting {0} to".format(u), conf.env[u])

    conf.env.VENDORLIBS = conf.path.find_node("lib").abspath()
    conf.env.append_value("INCLUDES", conf.path.find_node("include").abspath())

    conf.find_program("python3", var="PYTHON3")


def configure(conf):
    conf.msg_feature = lambda feature, v: conf.msg(feature, "yes" if v else "no")
    for x in ["shared", "static"]:
        val = conf.options.__dict__[x]
        conf.env[x.upper()] = val
        conf.msg_feature(x.title() + " library", val)

    if (conf.env.SHARED, conf.env.STATIC) == (False, False):
        conf.fatal("Either shared or static library need to be selected.")

    if "MSVC_VERSIONS" not in conf.env:
        conf.env["MSVC_VERSIONS"] = ["msvc 11.0"]

    conf.load("compiler_cxx")
    dest_os = (
        "windows" if conf.env.DEST_OS in ["win32", "cygwin", "msys", "uwin"]
        else conf.env.DEST_OS
    )

    conf.env.USE_MSBUILD = dest_os == "windows" and conf.env.CXX_NAME != "gcc"
    if conf.env.USE_MSBUILD:
        conf.recurse("msvc")
    else:
        configure_gcc(conf)

    conf.env.ENABLE_VAPOURSYNTH = conf.options.vapoursynth
    conf.msg_feature("Vapoursynth support", conf.env.ENABLE_VAPOURSYNTH)

    if not conf.env.USE_MSBUILD:
        conf.recurse("test")

    conf.msg("Build mode", conf.options.mode)

    conf.env.APPNAME = APPNAME


def post_install(ctx):
    if not ctx.options.ldconfig:
        return

    if not os.path.isfile("/sbin/ldconfig"):
        return

    Logs.info("- ldconfig")
    ctx.exec_command(Utils.subst_vars("/sbin/ldconfig ${LIBDIR}", ctx.env))


def build(bld):
    if bld.env.USE_MSBUILD:
        bld.recurse("msvc")
        return

    bld.recurse("src")
    for var, feature in [("SHARED", "cxxshlib"), ("STATIC", "cxxstlib")]:
        if bld.env[var]:
            bld(features="cxx " + feature,
                use=[
                    "f3kdb-objs",
                    "f3kdb-impl-sse2",
                    "f3kdb-impl-ssse3",
                    "f3kdb-impl-sse4",
                ],
                target="f3kdb",
                install_path="${LIBDIR}")

    bld.install_files("${INCLUDEDIR}", bld.path.ant_glob(["include/*.h"]))

    if bld.cmd == "install":
        bld.add_post_fun(post_install)

    bld.recurse("test")
