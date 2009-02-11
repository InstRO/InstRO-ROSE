#!/bin/bash

# Stub file to include settings for roseFreshTest-common

export PATH=/Users/dquinlan/local/libtool-install/bin:/Users/dquinlan/local/fontconfig-install/bin/:/Users/dquinlan/local/ghostscript-install/bin/:/Users/dquinlan/local/latex2html-install/bin/:/Users/dquinlan/local/texlive-install/2007/bin/i386-darwin/:/Users/dquinlan/local/graphviz-install/bin:/Users/dquinlan/local/doxygen-install/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin
export DYLD_LIBRARY_PATH=/Users/dquinlan/local/boost-1.35-install/lib

ROSE_TOP="/Users/dquinlan/ROSE/svn-test-rose/$$/"
export BOOST_ROOTX="/Users/dquinlan/local/boost-1.35-install/"

CONFIGURE_FLAGS=( --disable-dq-developer-tests --with-ROSE_LONG_MAKE_CHECK_RULE=yes --with-CXX_DEBUG=-g --with-C_DEBUG=-g --with-CXX_WARNINGS='-Wall' --with-C_WARNINGS='-Wall' --with-CXXFLAGS=-fPIC --with-CFLAGS=-fPIC --prefix=${ROSE_TOP}/install --with-boost=${BOOST_ROOTX} "$@")

ROSE_SVNROOT=svn+ssh://tux270.llnl.gov/usr/casc/overture/ROSE/svn/ROSE/trunk/ROSE
MAILADDRS="liaoch@llnl.gov panas2@llnl.gov dquinlan@llnl.gov matzke1@llnl.gov"
ENABLE_BUILD_BINARY_EDG=1

# the right version of subversion is essential!!
SVN=/usr/bin/svn

MAKEFLAGS="-j16 -w"

set -e
set -o pipefail
