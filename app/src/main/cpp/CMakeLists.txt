cmake_minimum_required(VERSION 3.22)
project(opsu LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

set(JNI_DIR ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})
add_library(bass    SHARED IMPORTED)
add_library(bass_fx SHARED IMPORTED)
set_target_properties(bass    PROPERTIES IMPORTED_LOCATION ${JNI_DIR}/libbass.so)
set_target_properties(bass_fx PROPERTIES IMPORTED_LOCATION ${JNI_DIR}/libbass_fx.so)

# NDK's native_app_glue = toolchain smoke test; swap for AGDK GameActivity later
add_library(app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

# Pure C++ game logic (no Android/Vulkan/BASS/SQLite). Tests are skipped on Android.
add_subdirectory(core)

# Android-only backends: any new .cpp under platform/ is picked up automatically.
file(GLOB_RECURSE PLATFORM_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp)

# SQLite (T5): the amalgamation is C, compiled as its own static library
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION SQLITE_DEFAULT_MEMSTATUS=0)
target_compile_options(sqlite3 PRIVATE -O2)

add_library(opsu SHARED main.cpp ${PLATFORM_SRC})
target_compile_definitions(opsu PRIVATE OPSU_VERSION="${OPSU_VERSION}")
# lets platform/*.cpp write #include "platform/..." (the cpp folder is the include root)
target_include_directories(opsu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(opsu PRIVATE -Wl,-z,max-page-size=16384 -u ANativeActivity_onCreate)
target_link_libraries(opsu sqlite3 opsu_core app_glue bass bass_fx android log EGL GLESv3)
