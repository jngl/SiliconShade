set(BINARY_NAME "CryptCrawler")

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_executable(${BINARY_NAME} ${SOURCES})

target_link_libraries(${BINARY_NAME} PRIVATE Retro3D)

# Copie des assets dans le dossier de build
add_custom_command(TARGET ${BINARY_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${BINARY_NAME}>/assets
)
