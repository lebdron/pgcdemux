project(cpgcdemux CXX)
cmake_minimum_required(VERSION 2.6)

find_package(spdlog REQUIRED)

add_executable(cpgcd main.cpp PgcDemux.cpp util.cpp)
target_link_libraries(cpgcd PRIVATE spdlog::spdlog)

if(NOT SHARE_INSTALL_PREFIX)
    set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share")
endif(NOT SHARE_INSTALL_PREFIX)

install(TARGETS cpgcd RUNTIME DESTINATION bin)
install(FILES README.txt ReadmePgcDemux.txt DESTINATION "${SHARE_INSTALL_PREFIX}/doc/cpgcdemux")
