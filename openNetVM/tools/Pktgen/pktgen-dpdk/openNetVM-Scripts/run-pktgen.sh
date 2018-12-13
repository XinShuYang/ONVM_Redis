#!/bin/sh

#                    openNetVM
#      https://github.com/sdnfv/openNetVM
#
# BSD LICENSE
#
# Copyright(c)
#          2015-2016 George Washington University
#          2015-2016 University of California Riverside
#          2010-2014 Intel Corporation.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
# The name of the author may not be used to endorse or promote
# products derived from this software without specific prior
# written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# These are the interfaces that you do not want to use for Pktgen-DPDK
BLACK_LIST="-b 0000:05:00.0 -b 0000:05:00.1"

# Path variable for pktgen
PKTGENBUILD="./app/app/x86_64-native-linuxapp-gcc/app/pktgen"

# Path for pktgen config
PKTGENCONFIG="./openNetVM-Scripts/pktgen-config.lua"

if [[ $1 -eq 0 ]] ; then
	echo "pass an argument for port count"
	echo "example usage: sudo bash run-pktgen.sh 1"
	exit 0
fi

echo "Start pktgen"

if [ $1 = 2 ]; then

(cd ../ && sudo $PKTGENBUILD -c ffff -n 3 $BLACK_LIST -- -p 0x1 -P -m "[1:2].0, [3:4].1" -f $PKTGENCONFIG)

else

(cd ../ && sudo $PKTGENBUILD -c ffff -n 3 $BLACK_LIST -- -p 0x3 -P -m "[4:8].0" -f $PKTGENCONFIG)

fi

echo "Pktgen done"
