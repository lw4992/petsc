#!/bin/sh
# $Id: solid.make,v 1.42 2001/04/13 20:50:42 balay Exp balay $ 

# Defaults
hme="/home/petsc/petsc-2.0.29"
src_dir=""
action="lib"

# process the command line arguments
for arg in "$@" ; do
#    echo procs sing arg $arg
    case "$arg" in 
        -echo)
        set -x
        ;;

        -help | -h)
        echo "Description: "
        echo " This program is used to build petsc.solid libraries on the variety"
        echo " of platforms on which it is built."
        echo " "
        echo "Options:"
        echo "  PETSC_DIR=petsc_dir : the current installation of petsc"
        echo "  SRC_DIR=src_dir     : the petsc src dir where make should be invoked"
        echo "  ACTION=action       : defaults to \"lib\" "
        echo " "
        echo "Example Usage:"
        echo "  - To update the libraries with changes in src/sles/interface"
        echo "  solid.make PETSC_DIR=/home/petsc/petsc-2.0.29 SRC_DIR=src/sles/interface ACTION=lib"
        echo "  - To rebuild a new version of PETSC on all the machines"
        echo "  solid.make PETSC_DIR=/home/petsc/petsc-2.0.29 SRC_DIR=\"\" ACTION=\"all\" "
        echo " "
        echo "Defaults:"
        echo "  PETSC_DIR=$hme SRC_DIR=$src_dir ACTION=$action"
        echo " "
        echo "Notes:"
        echo " To avoid problems with file permissions, this script is restricted"
        echo " to be run by the user petsc"
        exit 1
        ;;

        PETSC_DIR=*)
        hme=`echo $arg|sed 's/PETSC_DIR=//g'`
        ;;

        SRC_DIR=*)
        src_dir=`echo $arg|sed 's/SRC_DIR=//g'`
        ;;

        ACTION=*)
        action=`echo $arg|sed 's/ACTION=//g'`
        ;;

        *) 
        echo " ignoring option $arg"
        ;;
    esac
done

user=`whoami`
if [ ${user} != petsc ]; then
    echo 'Run this script as user petsc'
    exit
fi

set -x

arch=linux
make="make PETSC_ARCH=$arch PETSC_DIR=$hme $action shared"
rsh -n terra "cd $hme/$src_dir; $make BOPT=g"
rsh -n terra "cd $hme/$src_dir; $make BOPT=O"
rsh -n terra "cd $hme/$src_dir; $make BOPT=g_c++"
rsh -n terra "cd $hme/$src_dir; $make BOPT=O_c++"
rsh -n terra "cd $hme/$src_dir; $make BOPT=g_complex"

arch=IRIX64
make="make PETSC_ARCH=$arch PETSC_DIR=$hme $action shared"
rsh -n denali "cd $hme/$src_dir; $make BOPT=g"
rsh -n denali "cd $hme/$src_dir; $make BOPT=O"
rsh -n denali "cd $hme/$src_dir; $make BOPT=g_c++"
rsh -n denali "cd $hme/$src_dir; $make BOPT=O_c++"
rsh -n denali "cd $hme/$src_dir; $make BOPT=g_complex"
rsh -n denali "cd $hme/$src_dir; $make BOPT=O_complex"

# rs6000_sp
arch=rs6000_sp
make="make PETSC_ARCH=$arch PETSC_DIR=$hme $action shared"
rsh -n ico09 "cd $hme/$src_dir; $make BOPT=g"
rsh -n ico09 "cd $hme/$src_dir; $make BOPT=O"
rsh -n ico09 "cd $hme/$src_dir; $make BOPT=g_c++"
rsh -n ico09 "cd $hme/$src_dir; $make BOPT=O_c++"
rsh -n ico09 "cd $hme/$src_dir; $make BOPT=g_complex"
rsh -n ico09 "cd $hme/$src_dir; $make BOPT=O_complex"


arch=rs6000
make="make PETSC_ARCH=$arch PETSC_DIR=$hme $action shared"
rsh -n  ico09 "cd $hme/$src_dir; $make BOPT=g"
rsh -n  ico09 "cd $hme/$src_dir; $make BOPT=O"

# solaris
arch=solaris
make="make PETSC_ARCH=$arch PETSC_DIR=$hme $action shared"
rsh -n lava "cd $hme/$src_dir; $make BOPT=g"
rsh -n lava "cd $hme/$src_dir; $make BOPT=g_c++"
rsh -n lava "cd $hme/$src_dir; $make BOPT=O_c++"
rsh -n lava "cd $hme/$src_dir; $make BOPT=g_complex"

# solaris_uni
arch=solaris_uni
make="make PETSC_ARCH=$arch PETSC_DIR=$hme $action shared"
#rsh -n maple "cd $hme/$src_dir; $make BOPT=g"
