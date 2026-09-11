target_include_directories("${target}" ${public} [==[$<BUILD_INTERFACE:C:/Git/2D Engine v1/Source/Engine/Game/vcpkg_installed/vcpkg/blds/simdjson/src/v4.2.2-e234549bba.clean/include>]==] ${private} [==[$<BUILD_INTERFACE:C:/Git/2D Engine v1/Source/Engine/Game/vcpkg_installed/vcpkg/blds/simdjson/src/v4.2.2-e234549bba.clean/src>]==])
target_compile_features("${target}" ${public} [==[cxx_std_11]==])
target_link_libraries("${target}" ${public} [==[Threads::Threads]==])
target_compile_definitions("${target}" ${public} [==[SIMDJSON_THREADS_ENABLED=1]==])
