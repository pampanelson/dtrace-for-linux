#!/usr/bin/env bash
#
# Painless DTrace install for Ubuntu 8.04
#
# Author: philippe.hanrigou@gmail.com
# Date November 2008

DISTRIBUTION=dtrace-20081028

download()
{
   sudo apt-get install curl
   if [ ! -e ${DISTRIBUTION} ]; then
       echo "\n==== Downloading DTrace for Linux ====\n"
       curl -O
ftp://crisp.dynalias.com/pub/release/website/dtrace/${DISTRIBUTION}.tar.bz2
       tar jxvf ${DISTRIBUTION}.tar.bz2
   fi
}

install_dependencies()
{
   sudo apt-get install zlib1g-dev flex bison \
   		elfutils libelf-dev libc6-dev linux-libc-dev
}

build()
{
   make -C ${DISTRIBUTION} clean all
}

download
install_dependencies
build

