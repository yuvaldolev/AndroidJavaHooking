cmake_minimum_required(VERSION 3.22.1)

project("app" C CXX)

add_library(app
        SHARED

        app.cpp
        hooker.cpp
        hooker.hpp
        log.hpp
        math.hpp
        payload_dex_data.hpp
        trampoline_factory.cpp
        trampoline_factory.hpp
)

set_property(TARGET app PROPERTY CXX_STANDARD 20)

find_library(log-lib
        log
)

target_link_libraries(app
        ${log-lib}
)
