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

add_subdirectory(arp)
add_subdirectory(ethernet)
add_subdirectory(icmpv4)
add_subdirectory(ipv4)
add_subdirectory(tcpv4)

set(CMAKE_POSITION_INDEPENDENT_CODE 1)
file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.h)

add_library(tulips_stack SHARED ${SOURCES}
  $<TARGET_OBJECTS:tulips_stack_arp>
  $<TARGET_OBJECTS:tulips_stack_ethernet>
  $<TARGET_OBJECTS:tulips_stack_ipv4>
  $<TARGET_OBJECTS:tulips_stack_icmpv4>
  $<TARGET_OBJECTS:tulips_stack_tcpv4>)
target_link_libraries(tulips_stack PUBLIC tulips_system)

add_library(tulips_stack_static STATIC ${SOURCES}
  $<TARGET_OBJECTS:tulips_stack_arp>
  $<TARGET_OBJECTS:tulips_stack_ethernet>
  $<TARGET_OBJECTS:tulips_stack_ipv4>
  $<TARGET_OBJECTS:tulips_stack_icmpv4>
  $<TARGET_OBJECTS:tulips_stack_tcpv4>)

install(TARGETS
  tulips_stack
  tulips_stack_static
  LIBRARY DESTINATION lib)
