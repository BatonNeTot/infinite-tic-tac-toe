cmake_minimum_required (VERSION 3.11)
project ("infinite-tic-tac-toe")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_TARGET "infinite-tic-tac-toe")

###############################

add_subdirectory ("dependencies/raylib")

###############################

if(NOT MSVC)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        add_compile_options(-fsanitize=address)
    endif()
endif()

add_subdirectory ("src")

file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS include/*.h)

add_executable(${PROJECT_TARGET} ${PROJECT_SOURCES} ${PROJECT_HEADERS})
set_property(TARGET ${PROJECT_TARGET} PROPERTY C_STANDARD 11)

set_target_properties(${PROJECT_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
    LINKER_LANGUAGE C
)

target_compile_definitions(${PROJECT_TARGET} PRIVATE _AMD64_)

target_link_options(${PROJECT_TARGET} BEFORE PUBLIC -static -mwindows)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_TARGET} BEFORE PUBLIC -fsanitize=address)
endif()

add_dependencies(${PROJECT_TARGET} raylib)

target_include_directories(${PROJECT_TARGET} PUBLIC "include" "dependencies/raylib/src")
target_include_directories(${PROJECT_TARGET} PUBLIC "include" "dependencies/nbnet")
target_include_directories(${PROJECT_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_TARGET} PRIVATE "src")

target_link_directories(${PROJECT_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_TARGET} ${PROJECT_LIBRARIES} raylib ws2_32)
