cmake_minimum_required(VERSION 3.21)
project(shannon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure plugin modules retain DT_NEEDED entries for all linked libraries,
# even if no symbols are directly referenced at link time (e.g. ROOT
# dictionary-only dependencies like SHiPDataModel's collection proxies).
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-as-needed")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-as-needed")

# Dependencies
find_package(SHiPDataModel REQUIRED)
find_path(Random123_INCLUDE_DIR Random123/philox.h REQUIRED)

# Phlex's cmake config does not call find_dependency for its transitive deps
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW) # use BoostConfig.cmake, not the removed FindBoost
endif()
find_package(Boost REQUIRED COMPONENTS json)
find_package(TBB REQUIRED)
find_package(spdlog REQUIRED)
find_package(phlex REQUIRED)
find_package(
    ROOT
    REQUIRED
    COMPONENTS Core RIO Hist MathCore ROOTNTuple
)

# Sim file reader plugin
add_library(read_sim_file MODULE src/read_sim_file.cpp)
target_link_libraries(
    read_sim_file
    PRIVATE phlex::module SHiP::SHiPDataModel ROOT::RIO ROOT::ROOTNTuple
)

# Digitisation plugin
add_library(digitise_hits MODULE src/digitise_hits.cpp)

target_include_directories(
    digitise_hits
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_include_directories(
    digitise_hits
    SYSTEM
    PRIVATE "${Random123_INCLUDE_DIR}"
)
target_link_libraries(digitise_hits PRIVATE phlex::module SHiP::SHiPDataModel)

# Digitised output module plugin
add_library(digitised_output_module MODULE src/digitised_output_module.cpp)
target_link_libraries(
    digitised_output_module
    PRIVATE
        phlex::module
        SHiP::SHiPDataModel
        ROOT::Hist
        ROOT::RIO
        ROOT::ROOTHistUtil
        ROOT::ROOTNTuple
        spdlog::spdlog
)

# Smoke-test input generator (build-only, not installed)
add_executable(make_test_input tools/make_test_input.cpp)
target_include_directories(
    make_test_input
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_include_directories(
    make_test_input
    SYSTEM
    PRIVATE "${Random123_INCLUDE_DIR}"
)
target_link_libraries(
    make_test_input
    PRIVATE SHiP::SHiPDataModel ROOT::RIO ROOT::ROOTNTuple
)

# Install
include(GNUInstallDirs)

set(PLUGIN_TARGETS read_sim_file digitise_hits digitised_output_module)

install(TARGETS ${PLUGIN_TARGETS} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY workflows/ DESTINATION share/shannon/workflows)
