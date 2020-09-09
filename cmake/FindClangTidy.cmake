# - Find ClangTidy
# Find the clang-tidy binary.
#
#  ClangTidy_BINARY - Path to the clang-tidy binary.

find_program(ClangTidy_BINARY NAMES clang-tidy)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ClangTidy DEFAULT_MSG ClangTidy_BINARY)
mark_as_advanced(ClangTidy_BINARY)
