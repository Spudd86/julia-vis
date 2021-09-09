#! /bin/sh

libdir() {
        echo $(cd $1/$(gcc -print-multi-os-directory); pwd)
}

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`
cd  "$srcdir"

AUTORECONF=`which autoreconf`
if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please install it ***"
        exit 1
fi

autoreconf --verbose --force --install --symlink || exit 1
cd "$olddir" || exit $?

args="--prefix=/usr \
--sysconfdir=/etc \
--libdir=$(libdir /usr/lib)"

if test -n "$NOCONFIGURE"; then
        "$srcdir/configure" "--libdir=$(libdir /usr/lib)" "$@"
else
        echo
        echo "----------------------------------------------------------------"
        echo "Initialized build system. For a common configuration please run:"
        echo "----------------------------------------------------------------"
        echo
        echo "./configure CFLAGS='-g -O2' $args"
        echo
fi
