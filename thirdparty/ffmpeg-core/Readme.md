## FFmpeg builtins

This repository contains the FFmpeg builtins for MarathonRecomp and the CI to build them using vcpkg and GitHub actions.

The Windows version is built using clang-cl, this is done so that inline assembly optimisations (which are not supported by MSVC) can be enabled.
