add_executable(nidl2cpp
	Client.cpp
	Code.cpp
	CodeGenBase.cpp
	Compiler.cpp
	Header.cpp
	main.cpp
	Proxy.cpp
	Servant.cpp)

find_package(idlfe CONFIG REQUIRED)
find_path(IDLFE_INCLUDE_DIR NAMES IDL_FrontEnd.h)
target_include_directories(nidl2cpp PRIVATE ${IDLFE_INCLUDE_DIR})
target_link_libraries(nidl2cpp PRIVATE idlfe::idlfe idlfe::decNumber)

target_precompile_headers(nidl2cpp PRIVATE pch.h)
target_compile_features(nidl2cpp PRIVATE cxx_std_20)
