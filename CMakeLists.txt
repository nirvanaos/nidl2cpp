cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_SCAN_FOR_MODULES 0)
cmake_policy(SET CMP0010 NEW)
cmake_policy(SET CMP0053 NEW)

set(CMAKE_CONFIGURATION_TYPES Debug Release)

project(nidl2cpp CXX)

add_subdirectory(src)
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}_targets)

# Generate and install *-targets.cmake 
install(EXPORT ${PROJECT_NAME}_targets
	FILE ${PROJECT_NAME}-targets.cmake
	DESTINATION share/${PROJECT_NAME}
)

# Generate the config file in the current binary dir (this ensures it's not placed directly in source)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
	"include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}-targets.cmake\")\n"
)

# Install the generated config file
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
	DESTINATION share/${PROJECT_NAME}
)
