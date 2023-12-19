if [ "$1" != "" ]; then
config="$1"
else
config="Debug"
fi

if [[ "$OSTYPE" == "msys" ]]; then
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_HOST_TRIPLET=x64-mingw-dynamic
else
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
fi
cmake --build build --config "$config"