# 
# Copyright (c) 2020, International Business Machines
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
# 

NPROCS   ?= 4
BUILDDIR	= build
DEBFLAGS	= -DCMAKE_BUILD_TYPE=DEBUG -DTULIPS_TESTS=ON
RELFLAGS	= -DCMAKE_BUILD_TYPE=RELEASE -DTULIPS_HAS_HW_CHECKSUM=ON -DTULIPS_DISABLE_CHECKSUM_CHECK=ON -DTULIPS_DEBUG=ON -DTULIPS_IGNORE_INCOMPATIBLE_HW=OFF
EXTFLAGS ?=

.PHONY: build

default: release

build:
	@[ -e $(BUILDDIR) ] && make -s -C $(BUILDDIR) -j $(NPROCS)

test:
	@[ -e $(BUILDDIR) ] && make -s -C $(BUILDDIR) test

debug:
	@mkdir -p $(BUILDDIR);																																\
	 cd $(BUILDDIR);																																			\
	 rm -rf *;																																						\
	 GTEST_ROOT=$(HOME)/.local TCLAP_ROOT=$(HOME)/.local cmake $(DEBFLAGS) $(EXTFLAGS) .. \
	 cd ..

release:
	@mkdir -p $(BUILDDIR);																																\
	 cd $(BUILDDIR);																																			\
	 rm -rf *;																																						\
	 GTEST_ROOT=$(HOME)/.local TCLAP_ROOT=$(HOME)/.local cmake $(RELFLAGS) $(EXTFLAGS) ..	\
	 cd ..

release-arp:
	@mkdir -p $(BUILDDIR);																																												\
	 cd $(BUILDDIR);																																															\
	 rm -rf *;																																																		\
	 GTEST_ROOT=$(HOME)/.local TCLAP_ROOT=$(HOME)/.local cmake $(RELFLAGS) $(EXTFLAGS) -DTULIPS_ENABLE_ARP=ON ..	\
	 cd ..

release-raw:
	@mkdir -p $(BUILDDIR);																																												\
	 cd $(BUILDDIR);																																															\
	 rm -rf *;																																																		\
	 GTEST_ROOT=$(HOME)/.local TCLAP_ROOT=$(HOME)/.local cmake $(RELFLAGS) $(EXTFLAGS) -DTULIPS_ENABLE_RAW=ON ..	\
	 cd ..

clean:
	@rm -rf $(BUILDDIR)

