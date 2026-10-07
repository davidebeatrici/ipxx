# Part of ipxx. See LICENSE.

cmake_minimum_required(VERSION 3.16)

project(ipxx
	VERSION "0.1.0"
	DESCRIPTION "IP address/endpoint header-only C++ library."
	LANGUAGES "CXX"
)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ipxx INTERFACE)
add_library(ipxx::ipxx ALIAS ipxx)

target_include_directories(ipxx
	INTERFACE
		"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/ipxx"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(TARGETS ipxx
	EXPORT ipxx-targets
)

install(EXPORT ipxx-targets
	FILE ipxx-targets.cmake
	NAMESPACE ipxx::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ipxx"
)

configure_package_config_file(
	"${PROJECT_SOURCE_DIR}/cmake/ipxx-config.cmake.in"
	"${PROJECT_BINARY_DIR}/ipxx-config.cmake"
	INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ipxx"
)

write_basic_package_version_file(
	"${PROJECT_BINARY_DIR}/ipxx-config-version.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)

install(FILES
	"${PROJECT_BINARY_DIR}/ipxx-config.cmake"
	"${PROJECT_BINARY_DIR}/ipxx-config-version.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ipxx"
)
