cmake_minimum_required(VERSION 3.21)

project(openhexagon VERSION 0.1.0 LANGUAGES C CXX)

# Open Hexagon for VectorPie. The game core is Vittorio Romeo's own code
# (src/oh, from reference/SSVOpenHexagon), the level packs run unchanged on
# Lua 5.1; drawing, sound, controls and the menus are the VectorPie side
# (src/vp). Builds as the SDK's own programs do, see BUILD.md.
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(OH_PI_BUILD)
    # cmake/toolchain-pi-clang.cmake: the cabinet's own SDL2, by its file.
    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0"
        IMPORTED_SONAME "libSDL2-2.0.so.0"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/usr/include/SDL2"
        INTERFACE_COMPILE_DEFINITIONS "_REENTRANT")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/VectorPie/sdk vectorpie)

# --- Lua 5.1.5 (the original runs LuaJIT, which is Lua 5.1) ---------------
file(GLOB LUA_SOURCES vendor/lua-5.1.5/src/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua-5.1.5/src/lua.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua-5.1.5/src/luac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua-5.1.5/src/print.c)
add_library(lua51 STATIC ${LUA_SOURCES})
target_include_directories(lua51 PUBLIC vendor/lua-5.1.5/src)
if(OH_PI_BUILD OR UNIX)
    target_compile_definitions(lua51 PRIVATE LUA_USE_POSIX)
endif()
set_target_properties(lua51 PROPERTIES LINKER_LANGUAGE C)

# --- The Open Hexagon core -------------------------------------------------
file(GLOB_RECURSE OH_SOURCES CONFIGURE_DEPENDS src/oh/src/*.cpp)
add_library(ohcore STATIC ${OH_SOURCES})
target_include_directories(ohcore PUBLIC
    src/oh/include
    src/compat
    vendor/SSVUtils/include
    vendor/vrm_pp/include
    src/vp)
target_link_libraries(ohcore PUBLIC lua51 VectorPie::vectorpie)
target_compile_definitions(ohcore PUBLIC SSVOH_VECTORPIE=1 SSVOH_ANDROID=1)
target_compile_options(ohcore PRIVATE -Wno-deprecated-declarations)

# --- The VectorPie game ----------------------------------------------------
file(GLOB VP_SOURCES CONFIGURE_DEPENDS src/vp/*.cpp)
add_executable(openhexagon ${VP_SOURCES} src/vp/vpbind.c)
target_link_libraries(openhexagon PRIVATE ohcore)
vp_program(openhexagon)
# The soundtrack synthesizer (src/vp/synth.cpp) renders a few million
# samples when a level starts: let the compiler loose on it.
set_source_files_properties(src/vp/synth.cpp PROPERTIES COMPILE_OPTIONS "-O3;-ffast-math")

add_custom_command(TARGET openhexagon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/package/game.ini
            $<TARGET_FILE_DIR:openhexagon>/game.ini
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:openhexagon>/assets)
