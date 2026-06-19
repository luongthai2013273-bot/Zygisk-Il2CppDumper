cmake_minimum_required(VERSION 3.18.1)

# Đặt tên module mặc định nếu không truyền vào
if (NOT DEFINED MODULE_NAME)
    set(MODULE_NAME "menu")
endif ()
project(${MODULE_NAME})

message("Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 17)  # C++17 là đủ
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cờ tối ưu và ẩn symbol
set(LINKER_FLAGS "-Wl,--hash-style=both -Wl,-exclude-libs,ALL -Wl,--gc-sections -Wl,--strip-all")
set(C_FLAGS "-fdata-sections -ffunction-sections")
set(CXX_FLAGS "-fno-exceptions -fno-rtti -fno-stack-protector")

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(C_FLAGS "${C_FLAGS} -O3 -fvisibility=hidden -fvisibility-inlines-hidden")
else ()
    set(C_FLAGS "${C_FLAGS} -O0 -g -DDEBUG")
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS} -Wno-format")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS} ${CXX_FLAGS} -Wno-format")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")

# CHỈ GIỮ LẠI main.cpp (đã gộp logic)
add_library(${MODULE_NAME} SHARED
    main.cpp
    ultis.cpp
)

# Link với các thư viện cần thiết
target_link_libraries(${MODULE_NAME}
    log
    android
    dl
    # Nếu dùng Native Window / OpenGL / EGL
    EGL
    GLESv2
    # Nếu dùng xdl để ẩn thư viện (không bắt buộc)
    # xdl
)

# Strip sau khi build (nếu không phải debug)
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-all --remove-section=.comment "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${MODULE_NAME}.so"
        COMMENT "Stripping symbols..."
    )
endif ()

# In ra thông báo
message(STATUS "Build for: ${CMAKE_ANDROID_ARCH_ABI}")
message(STATUS "Output: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${MODULE_NAME}.so")
