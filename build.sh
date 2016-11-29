#!/bin/sh

gen_svn_header_file()
{
     echo $1
     cd $1
     svn_version_c_head="const char *$2 = \""
     svn_version_now=$(svnversion)
     svn_version_c_tail="\";"
     svn_version_c_src="$svn_version_c_head$svn_version_now$svn_version_c_tail"
     cd -
     echo $svn_version_c_src >> "$SVN_FILE"
}

SVN_FILE="svn_ver.h"
if [ -f $SVN_FILE ]; then
    rm $SVN_FILE
fi
gen_svn_header_file ./ g_str_libver

make -C tinyxml clean || exit 1
rm -f tinyxml/libtinyxml.a || exit 1
make -C mbedtls/library clean || exit 1
make -C libkdm clean || exit 1
make clean || exit 1

if [ $1 = "clean" ]; then
    echo "just clean"
    exit 0
fi

make -C tinyxml || exit 1
make -C mbedtls/library || exit 1
make -C libkdm || exit 1
make || exit 1
