# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-src")
  file(MAKE_DIRECTORY "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-src")
endif()
file(MAKE_DIRECTORY
  "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-build"
  "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-subbuild/msgpack-populate-prefix"
  "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-subbuild/msgpack-populate-prefix/tmp"
  "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-subbuild/msgpack-populate-prefix/src/msgpack-populate-stamp"
  "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-subbuild/msgpack-populate-prefix/src"
  "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-subbuild/msgpack-populate-prefix/src/msgpack-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-subbuild/msgpack-populate-prefix/src/msgpack-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/sykim/.openclaw/workspace/vse-platform/build-release/_deps/msgpack-subbuild/msgpack-populate-prefix/src/msgpack-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
