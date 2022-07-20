cmake_minimum_required(VERSION 3.15)

set(TARGET sqlite2yandexdisk)

#To include cYandexDisk to your project use:
#set(SQLITE2YANDEXDISK_SOURCE_DIR ${CMAKE_SOURCE_DIR}/path/to/sqlite2yandexdisk)
#add_subdirectory(${SQLITE2YANDEXDISK_SOURCE_DIR})

if (NOT DEFINED SQLITE2YANDEXDISK_SOURCE_DIR)
  set(SQLITE2YANDEXDISK_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
	${TARGET} VERSION 1.0 
	DESCRIPTION "C API for sync SQLite3 database to YandexDisk"
	HOMEPAGE_URL ""
	LANGUAGES C CXX 
)

add_subdirectory(SQLiteConnect)
add_subdirectory(cYandexDisk)

add_library(${TARGET} STATIC ${TARGET}.c)
target_link_libraries(${TARGET} 
	cYandexDisk 
	SQLiteConnect
)

if(${SQLITE2YANDEXDISK_BUILD_TEST})
	add_executable(${TARGET}_test test.c)
	target_link_libraries(${TARGET}_test ${TARGET})
endif()
