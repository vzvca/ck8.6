#! /bin/bash[5~
for f in *.n ; do roffit $f > ../html/$(basename $f).html; done
