find_path(IBVerbs_INCLUDE_DIR infiniband/verbs.h
  HINTS $ENV{OFED_ROOT}/include
  PATHS /usr/include /usr/local/include /opt/local/include)

find_library(IBVerbs_LIBRARY NAMES ibverbs
  HINTS $ENV{OFED_ROOT}/lib
  PATHS /usr/lib /usr/local/lib /opt/local/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IBVerbs DEFAULT_MSG IBVerbs_INCLUDE_DIR IBVerbs_LIBRARY)
mark_as_advanced(IBVerbs_INCLUDE_DIR IBVerbs_LIBRARY)
