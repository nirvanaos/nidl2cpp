vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

find_program(GIT git)

set(GIT_URL "https://github.com/nirvanaos/idlfe.git")
set(BRANCH "develop")
set(COMMIT "b819f00b7baf89befaba2881a4161fd8f4f60829")

set(SOURCE_PATH ${CURRENT_BUILDTREES_DIR}/src/${PORT})

if(NOT EXISTS "${SOURCE_PATH}/.git")
	message(STATUS "Cloning and fetching submodules")
	vcpkg_execute_required_process(
	  COMMAND ${GIT} clone --recurse-submodules ${GIT_URL} ${SOURCE_PATH}
	  WORKING_DIRECTORY ${CURRENT_BUILDTREES_DIR}
	  LOGNAME clone
	)
endif()

message(STATUS "Pull")
vcpkg_execute_required_process(
  COMMAND ${GIT} pull origin ${BRANCH}
  WORKING_DIRECTORY ${SOURCE_PATH}
  LOGNAME pull
)

message(STATUS "Checkout")
vcpkg_execute_required_process(
  COMMAND ${GIT} checkout ${COMMIT}
  WORKING_DIRECTORY ${SOURCE_PATH}
  LOGNAME checkout
)

#vcpkg_from_github(
#    OUT_SOURCE_PATH SOURCE_PATH
#    REPO nirvanaos/idlfe
#    HEAD_REF master
#)

vcpkg_find_acquire_program(FLEX)
get_filename_component(FLEX_EXE_PATH "${FLEX}" DIRECTORY)
vcpkg_add_to_path("${FLEX_EXE_PATH}")

vcpkg_find_acquire_program(BISON)
get_filename_component(BISON_EXE_PATH "${BISON}" DIRECTORY)
vcpkg_add_to_path("${BISON_EXE_PATH}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME "idlfe")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/license-lgpl.md" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)