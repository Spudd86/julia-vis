#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

autoreconf -v --install --symlink || exit 1
cd $ORIGDIR || exit $?

#TODO: find a way to fill build-aux without invoking automake

if test -z "$NOCONFIGURE"; then
    $srcdir/configure "$@"
fi

