f3kdb (a.k.a. flash3kyuu_deband)
================================

[![Build Status](https://travis-ci.org/SAPikachu/flash3kyuu_deband.png)](https://travis-ci.org/SAPikachu/flash3kyuu_deband)

[Documentation](https://f3kdb.readthedocs.org/)

How to build (Linux)
--------------------

### Before compiling

* Ensure Python 3 is installed
* GCC 4.8.1+ or Clang 3.2+ is required, please install either one
* Run `git submodule update --init --recursive` to initialize dependencies (Only needed for building tests)

### Configuration

To configure for compiling using default compiler in your system, run:

    ./waf configure

If you need to use a non-default compiler, use this:

    CC={your c compiler} CXX={your c++ compiler} ./waf configure

To build test cases, append `--enable-test` after the command line.

### Compiling

Simply run:

    ./waf build
    
Optionally, you can install f3kdb into your system:

    sudo ./waf install
    
### Run test cases

To run all test cases (may need 20 ~ 30 minutes):

    build/test/f3kdb-test
    
If you want to run only a subset of tests, check `.travis.yml` for command line examples.
