file(
    GLOB_RECURSE
    SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.s"
)

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    "${INCLUDE_DIR}"
)
