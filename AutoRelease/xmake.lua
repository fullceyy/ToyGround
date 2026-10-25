add_requires("doctest")

target("AutoRelease")
    set_kind("binary")
    add_files("*.cpp")
    set_default(false)
    add_syslinks("stdc++exp")
target_end()

target("AutoReleaseTests")
    set_kind("binary")
    add_files("tests/*.cpp")
    add_includedirs(".")          -- so tests can #include "AutoRelease.hpp"
    add_packages("doctest")
    add_tests("default")          -- registers it with `xmake test`
    set_default(false)
target_end()


-- xmake test                     # builds and runs every registered test
-- xmake run AutoReleaseTests     # or run the test program directly, with doctest's full output
-- xmake run AutoRelease          # your existing example program