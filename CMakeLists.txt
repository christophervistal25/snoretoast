project( snoretoast )
cmake_minimum_required(VERSION 3.4)


add_definitions(-DUNICODE -D_UNICODE -D__WRL_CLASSIC_COM_STRICT__ -DWIN32_LEAN_AND_MEAN)

set(CMAKE_CXX_STANDARD 17)

#link runtime static
if(MSVC)
    foreach(_bt DEBUG RELEASE RELWITHDEBINFO)
        string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_${_bt} ${CMAKE_CXX_FLAGS_${_bt}})
    endforeach(_bt DEBUG RELEASE RELWITHDEBINFO)
endif(MSVC)

add_subdirectory(data)
add_subdirectory(src)
