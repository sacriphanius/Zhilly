

cmake_minimum_required(VERSION 3.16)

add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "2.2.3")
project(xiaozhi)