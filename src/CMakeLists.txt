add_executable(nidl2cpp
	Client.cpp
	Code.cpp
	CodeGenBase.cpp
	Compiler.cpp
	Component.cpp
	Header.cpp
	main.cpp
	Proxy.cpp
	Servant.cpp)

find_package(idlfe CONFIG REQUIRED)
target_link_libraries(nidl2cpp PRIVATE idlfe)

target_compile_features(nidl2cpp PRIVATE cxx_std_20)
