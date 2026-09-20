"""Shared compile/link flags mirroring ocs2_core/cmake/ocs2_cxx_flags.cmake.

Every migrated OCS2 package's CMakeLists.txt either includes that file
directly (only ocs2_core does) or picks up its flags transitively through
`target_compile_options(ocs2_core PUBLIC ...)` propagating to consumers that
link it. Bazel's `copts`/`linkopts` don't propagate to dependents the way
CMake's PUBLIC compile options do, so this list is applied explicitly on
every migrated cc_library/cc_binary/cc_test instead.

DEVIATION FROM THE PLAN on the C++ standard: `-std=c++14` (matching
`set(CMAKE_CXX_STANDARD 14)` in ocs2_cxx_flags.cmake) is applied here, in
OCS2_COPTS, to OCS2's own cc_library/cc_binary targets -- deliberately NOT
as a repo-wide `--cxxopt` in .bazelrc. A repo-wide C++14 default would also
force every external dependency down to C++14, and the RCR/BCR-resolved
graph only offers googletest >=1.17.0.bcr.2, which hard `#error`s below
C++17 ("C++ versions less than C++17 are not supported."). cc_test targets
that pull in gtest use OCS2_TEST_COPTS instead, which asks for C++17
explicitly. Mixing C++14-compiled production code with a C++17-compiled
test binary that links it is safe on the GCC/libstdc++ toolchain this was
verified against (no ABI-relevant differences for the types this
codebase's headers expose across that boundary between the two standards).
"""

# "-Wl,--no-as-needed" is a linker flag, not a compiler flag; kept out of
# OCS2_COPTS and applied only via OCS2_LINKOPTS.
OCS2_COPTS = [
    "-pthread",
    "-Wfatal-errors",
    "-fopenmp",
    "-std=c++14",
]

OCS2_TEST_COPTS = OCS2_COPTS[:-1] + ["-std=c++17"]

OCS2_LINKOPTS = [
    "-pthread",
    "-Wl,--no-as-needed",
    "-fopenmp",
]

# DEVIATION FROM THE PLAN: ocs2_cxx_flags.cmake sets `-DBOOST_ALL_DYN_LINK`
# because the original CMake build links against a prebuilt *dynamic*
# system libboost_log.so. Here, @boost.log (from BCR) is compiled from
# source as a plain cc_library (a static archive by default, built without
# that define). Boost.Log's headers pick one of two ABI-versioned inline
# namespaces depending on this macro (`v2_mt_posix` with the define,
# `v2s_mt_posix` -- note the "s" -- without it); defining it on our side
# while @boost.log itself was compiled without it produces exactly the
# "undefined reference to boost::log::v2_mt_posix::core::..." link errors
# this was verified against (the compiled archive only has the
# `v2s_mt_posix`-mangled symbols). So this define is dropped rather than
# carried over -- it must match whatever @boost.log itself was compiled
# with, not the original CMake build's linking strategy.
OCS2_DEFINES = []
