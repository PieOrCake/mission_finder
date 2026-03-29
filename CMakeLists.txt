cmake_minimum_required(VERSION 3.16)
project(MissionFinder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# ImGui: must match Nexus loader version (1.80) so ImGuiContext ABI is compatible
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.80
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SRC_DIR ${imgui_SOURCE_DIR})

# Embed mission data JSON into the DLL as a C++ raw string literal
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mission_finder_cache.json" MISSION_JSON_CONTENT)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/EmbeddedMissionData.inc.in ${CMAKE_CURRENT_BINARY_DIR}/EmbeddedMissionData.inc @ONLY)

# Addon DLL (module-definition file ensures GetAddonDef is exported for Nexus)
add_library(${PROJECT_NAME} SHARED
  src/dllmain.cpp
  src/Shared.cpp
  src/Config.cpp
  src/ChatMessage.cpp
  src/UI.cpp
  src/MissionData.cpp
  MissionFinder.def
  # ImGui core (Nexus provides context and Win32/DX11 backends at runtime)
  ${IMGUI_SRC_DIR}/imgui.cpp
  ${IMGUI_SRC_DIR}/imgui_draw.cpp
  ${IMGUI_SRC_DIR}/imgui_tables.cpp
  ${IMGUI_SRC_DIR}/imgui_widgets.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${IMGUI_SRC_DIR}
)

# Windows DLL
if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    PREFIX ""
    SUFFIX ".dll"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
  )
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WINDOWS
    UNICODE
    _UNICODE
  )
  set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/MissionFinder.def")
  if(MSVC)
    # MSVC does not use .def from source list; pass explicitly so GetAddonDef is exported.
    target_link_options(${PROJECT_NAME} PRIVATE /DEF:${DEF_FILE})
  elseif(MINGW)
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " ${DEF_FILE}")
    # Strip symbols in Release so the DLL is ~1.7 MB instead of ~14 MB.
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
      target_link_options(${PROJECT_NAME} PRIVATE "LINKER:-s")
    endif()
    # Static-link winpthread so libwinpthread-1.dll is not required at runtime.
    # -l:libwinpthread.a picks the static .a file; --whole-archive forces it in so refs from std libs are satisfied.
    target_link_options(${PROJECT_NAME} PRIVATE
      "LINKER:-Bstatic"
      "LINKER:--whole-archive"
      "LINKER:-l:libwinpthread.a"
      "LINKER:--no-whole-archive"
      "LINKER:-Bdynamic"
    )
  endif()
endif()

