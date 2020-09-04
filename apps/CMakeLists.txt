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

include_directories(${TCLAP_INCLUDE_DIR})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif ()

if (IBVerbs_FOUND)
  add_executable(lat_ofed lat_ofed.cpp)
  target_link_libraries(lat_ofed
    PRIVATE
    tulips_apps_static
    tulips_stack_static
    tulips_transport_ofed_static
    tulips_transport_stubs_static
    tulips_fifo_static
    PUBLIC
    ${IBVerbs_LIBRARY})
endif (IBVerbs_FOUND)

if (IBVerbs_FOUND AND (TULIPS_ENABLE_RAW OR TULIPS_ENABLE_ARP))
  add_executable(raw_ofed raw_ofed.cpp)
  target_link_libraries(raw_ofed
    PRIVATE
    tulips_api_static
    tulips_transport_ofed_static
    tulips_transport_pcap_static
    tulips_transport_stubs_static
    tulips_fifo_static
    tulips_stack_static
    tulips_system_static
    PUBLIC
    ${IBVerbs_LIBRARY}
    ${PCAP_LIBRARY})
endif (IBVerbs_FOUND AND (TULIPS_ENABLE_RAW OR TULIPS_ENABLE_ARP))

if (NOT TULIPS_HAS_HW_CHECKSUM OR TULIPS_DISABLE_CHECKSUM_CHECK)
  if (TULIPS_ENABLE_ARP)
    add_executable(lat_pipe lat_pipe.cpp)
    target_link_libraries(lat_pipe PRIVATE
      tulips_apps_static
      tulips_transport_npipe)
  endif (TULIPS_ENABLE_ARP)
  add_executable(trc_fifo trc_fifo.cpp)
  target_link_libraries(trc_fifo PRIVATE
    tulips_api
    tulips_transport_shm)
  add_executable(ssl_fifo ssl_fifo.cpp)
  target_link_libraries(ssl_fifo PRIVATE
    tulips_api
    tulips_ssl
    tulips_transport_pcap
    tulips_transport_shm)
endif (NOT TULIPS_HAS_HW_CHECKSUM OR TULIPS_DISABLE_CHECKSUM_CHECK)
