cmake_minimum_required(VERSION 3.10.0)
project(grand-regent VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
pkg_check_modules(SDL3_MIXER REQUIRED sdl3-mixer)

add_executable(grand-regent
    src/main.c

    # core: window/render loop, camera, texture cache, audio
    src/core/game.c
    src/core/camera.c
    src/core/texture.c
    src/core/audio.c

    # world: tile map data, TMJ (Tiled JSON) loading + rendering
    src/world/map.c
    src/world/tmj_loader.c

    # entities: animated, simulated actors
    src/entities/animation.c
    src/entities/physics.c
    src/entities/player.c
    src/entities/enemy.c
    src/entities/boss.c

    # combat: collision resolution, attack hitboxes, player skills
    src/combat/collision.c
    src/combat/hitbox.c
    src/combat/skill.c

    # vendored single-file JSON parser (https://github.com/DaveGamble/cJSON)
    src/third_party/cJSON.c
)

add_custom_command(
    TARGET grand-regent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:grand-regent>/assets
)

target_include_directories(grand-regent PRIVATE
    ${SDL3_INCLUDE_DIRS}
    ${SDL3_IMAGE_INCLUDE_DIRS}
    ${SDL3_MIXER_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/third_party
)

target_link_directories(grand-regent PRIVATE
    ${SDL3_LIBRARY_DIRS}
    ${SDL3_IMAGE_LIBRARY_DIRS}
    ${SDL3_MIXER_LIBRARY_DIRS}
)


target_link_libraries(grand-regent PRIVATE
    ${SDL3_LIBRARIES}
    ${SDL3_IMAGE_LIBRARIES}
    ${SDL3_MIXER_LIBRARIES}
    m
)
