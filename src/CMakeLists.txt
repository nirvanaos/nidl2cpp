add_executable(nidl2cpp
	Client.cpp
	Code.cpp
	CodeGenBase.cpp
	Compiler.cpp
	Header.cpp
	main.cpp
	Proxy.cpp
	Servant.cpp)

FIND_PACKAGE(nirvanaos.idlfe REQUIRED)

##[===[
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()
#]===]

target_precompile_headers(nidl2cpp PRIVATE pch.h)
target_compile_features(nidl2cpp PUBLIC cxx_std_20)
target_include_directories(nidl2cpp PRIVATE ${nirvanaos.idlfe_INCLUDE_DIR})
target_link_libraries(nidl2cpp ${nirvanaos.idlfe_LIBRARIES_TARGETS})