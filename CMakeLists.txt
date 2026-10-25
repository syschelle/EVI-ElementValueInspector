cmake_minimum_required(VERSION 3.25)

project(ElementValueInspector VERSION 0.2.19 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # One portable EXE: use the static Microsoft C/C++ runtime (/MT in Release).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

include(FetchContent)

# ------------------------------ wxWidgets ------------------------------
set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
set(wxBUILD_MONOLITHIC OFF CACHE BOOL "" FORCE)
set(wxBUILD_PRECOMP OFF CACHE BOOL "" FORCE)
set(wxBUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(wxBUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(wxBUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    wxWidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.12/wxWidgets-3.2.12.zip
    URL_HASH SHA1=c2ffa5941f23162782c009d838ddb71d344070e1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(wxWidgets)

# -------------------------------- DCMTK --------------------------------
# Only modules required for reading DICOM metadata and charset conversion.
set(DCMTK_MODULES "ofstd;oflog;oficonv;dcmdata" CACHE STRING "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_TIFF OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_PNG OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_XML OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_OPENSSL OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_SNDFILE OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_OPENJPEG OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_DOXYGEN OFF CACHE BOOL "" FORCE)
set(DCMTK_ENABLE_MANPAGES OFF CACHE BOOL "" FORCE)
set(DCMTK_ENABLE_PRIVATE_TAGS OFF CACHE BOOL "" FORCE)
set(DCMTK_DEFAULT_DICT builtin CACHE STRING "" FORCE)
set(DCMTK_USE_DCMDICTPATH OFF CACHE BOOL "" FORCE)
set(DCMTK_ENABLE_CHARSET_CONVERSION oficonv CACHE STRING "" FORCE)
set(DCMTK_ENABLE_BUILTIN_OFICONV_DATA ON CACHE BOOL "" FORCE)
set(DCMTK_ENABLE_STL ON CACHE BOOL "" FORCE)
set(DCMTK_WIDE_CHAR_FILE_IO_FUNCTIONS ON CACHE BOOL "" FORCE)
set(DCMTK_OVERWRITE_WIN32_COMPILER_FLAGS ON CACHE BOOL "" FORCE)
set(DCMTK_COMPILE_WIN32_MULTITHREADED_DLL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    dcmtk
    URL https://github.com/DCMTK/dcmtk/archive/refs/tags/DCMTK-3.7.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(dcmtk)

add_executable(evi WIN32
    src/main.cpp
    src/AppIcon.cpp
    src/AppIcon.h
    src/MainFrame.cpp
    src/MainFrame.h
    src/TagDialog.cpp
    src/TagDialog.h
    src/HeaderFrame.cpp
    src/HeaderFrame.h
    src/DicomScanner.cpp
    src/DicomScanner.h
    src/DicomUtils.cpp
    src/DicomUtils.h
    src/Models.h
    src/UiLanguage.h
    src/UiTheme.h
    src/evi.rc
    resources/evi.ico
)

# DCMTK targets use plain library names when included in-tree.
target_link_libraries(evi PRIVATE
    wx::core
    wx::base
    dcmdata
    oficonv
    oflog
    ofstd
)

if(WIN32)
    target_link_libraries(evi PRIVATE dwmapi uxtheme)
endif()

target_include_directories(evi PRIVATE
    src
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

target_compile_definitions(evi PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(evi PRIVATE /W4 /permissive- /utf-8)
    target_link_options(evi PRIVATE
        /SUBSYSTEM:WINDOWS
        /MANIFEST:EMBED
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/evi.manifest"
    )
endif()

set_target_properties(evi PROPERTIES
    OUTPUT_NAME "evi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# --------------------------- Release artifacts ---------------------------
# Keep the repository clean while still producing ready-to-upload artifacts
# for GitHub Releases after every successful Release build.
add_custom_command(TARGET evi POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/dist"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:evi>"
            "${CMAKE_SOURCE_DIR}/dist/evi.exe"
    COMMAND "${CMAKE_COMMAND}"
            "-DINPUT_FILE=${CMAKE_SOURCE_DIR}/dist/evi.exe"
            "-DOUTPUT_FILE=${CMAKE_SOURCE_DIR}/dist/evi.exe.sha256"
            -P "${CMAKE_SOURCE_DIR}/cmake/WriteSha256.cmake"
    COMMENT "Creating portable evi.exe and SHA-256 release checksum"
    VERBATIM
)
