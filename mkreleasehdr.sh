#!/bin/sh

# TODO: Check if $# is 2.

srcdir=$1

cd $srcdir

GIT_SHA1=`(git show-ref --head --hash=8 2> /dev/null || echo 00000000) | head -n1`
GIT_DIRTY=`git diff --no-ext-diff 2> /dev/null | wc -l`
BUILD_ID=`uname -n`"-"`date +%s`
if [ -n "$SOURCE_DATE_EPOCH" ]; then
  BUILD_ID=$(date -u -d "@$SOURCE_DATE_EPOCH" +%s 2>/dev/null || date -u -r "$SOURCE_DATE_EPOCH" +%s 2>/dev/null || date -u %s)
fi

cd -

test -f release.hpp || touch release.hpp
(cat release.hpp | grep SHA1 | grep $GIT_SHA1) && \
(cat release.hpp | grep DIRTY | grep $GIT_DIRTY) && exit 0 # Already up-to-date
echo "#define GIT_SHA1 \"$GIT_SHA1\"" > release.hpp
echo "#define GIT_DIRTY \"$GIT_DIRTY\"" >> release.hpp
echo "#define BUILD_ID \"$BUILD_ID\"" >> release.hpp
touch release.cpp # Force recompile of release.cpp

