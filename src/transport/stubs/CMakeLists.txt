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

set(CMAKE_POSITION_INDEPENDENT_CODE 1)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(SOURCES linux.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  set(SOURCES openbsd.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(SOURCES darwin.cpp)
else ()
  set(SOURCES stubs/dummy.cpp)
endif ()

add_library(tulips_transport_stubs SHARED ${SOURCES})
target_link_libraries(tulips_transport_stubs PRIVATE tulips_stack)

add_library(tulips_transport_stubs_static STATIC ${SOURCES})

install(TARGETS
  tulips_transport_stubs
  tulips_transport_stubs_static
  LIBRARY DESTINATION lib)
