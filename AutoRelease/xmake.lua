target("AutoRelease")
    set_kind("binary")
    add_files("*.cpp")
    set_default(false)
    add_syslinks("stdc++exp", {public = true})

target_end()