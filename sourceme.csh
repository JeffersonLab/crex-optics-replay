#!/bin/tcsh
source /u/apps/root/6.18.04/setroot_CUE.csh
setenv ANALYZER /w/halla-scifs17exp/parity/disk1/crex_optics/analyzer-1.6.6
setenv DB_DIR /w/halla-scifs17exp/parity/disk1/crex_optics/replay/CREX_DB/DB_CREX

setenv LD_LIBRARY_PATH ${ANALYZER}:${ROOTSYS}/lib:${LD_LIBRARY_PATH}
setenv PATH ${ROOTSYS}/bin:${ANALYZER}:${PATH}
