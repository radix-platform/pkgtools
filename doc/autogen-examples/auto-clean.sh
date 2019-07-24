#!/bin/sh

cd ../..

if [ -f "Makefile" ] ; then
  make distclean
fi

rm -rf autom4te.cache m4

rm  -f Makefile
rm  -f Makefile.in
rm  -f config.h
rm  -f config.h.in
rm  -f config.log
rm  -f config.status
rm  -f compile config.guess config.sub
rm  -f configure
rm  -f install-sh
rm  -f missing
rm  -f stamp-h1
rm  -f aclocal.m4
rm  -f depcomp

rm -rf src/.deps
rm  -f src/Makefile
rm  -f src/Makefile.in
