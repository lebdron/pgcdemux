project(cpgcdemux CXX)
cmake_minimum_required(VERSION 2.6)

find_package(spdlog REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DVDREAD REQUIRED dvdread)

add_executable(cpgcd main.cpp PgcDemux.cpp util.cpp)
target_link_libraries(cpgcd PRIVATE spdlog::spdlog ${DVDREAD_LIBRARIES})
target_include_directories(cpgcd PRIVATE ${DVDREAD_INCLUDE_DIRS})
target_compile_options(cpgcd PRIVATE ${DVDREAD_CFLAGS_OTHER})

if(NOT SHARE_INSTALL_PREFIX)
    set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share")
endif(NOT SHARE_INSTALL_PREFIX)

install(TARGETS cpgcd RUNTIME DESTINATION bin)
install(FILES README.txt ReadmePgcDemux.txt DESTINATION "${SHARE_INSTALL_PREFIX}/doc/cpgcdemux")
