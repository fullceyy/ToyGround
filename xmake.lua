set_project("TG")
set_toolchains("mingw")
set_languages("c++23")
add_rules("mode.debug", "mode.release")
add_cxxflags("-Wpedantic", "-Wall", {tools = {"gcc", "clang"}})
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})
-- set_policy("generator.compile_commands", true)

includes("*/xmake.lua")


-- To share code between the programs, 
-- put it in a folder like common/ with a set_kind("static") target, 
-- then add add_deps("common") to each target that uses it.