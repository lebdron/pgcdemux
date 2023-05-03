project(cpgcdemux CXX)
cmake_minimum_required(VERSION 2.6)

add_executable(cpgcd main.cpp PgcDemux.cpp util.cpp)

if(NOT SHARE_INSTALL_PREFIX)
    set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share")
endif(NOT SHARE_INSTALL_PREFIX)

install(TARGETS cpgcd RUNTIME DESTINATION bin)
install(FILES README.txt ReadmePgcDemux.txt DESTINATION "${SHARE_INSTALL_PREFIX}/doc/cpgcdemux")
