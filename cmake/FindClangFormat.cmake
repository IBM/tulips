# - Find ClangFormat
# Find the clang-format binary.
#
#  ClangFormat_BINARY - Path to the clang-format binary.

find_program(ClangFormat_BINARY NAMES clang-format)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ClangFormat DEFAULT_MSG ClangFormat_BINARY)
mark_as_advanced(ClangFormat_BINARY)
