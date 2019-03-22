# cMake version
cmake_minimum_required(VERSION 3.8.2)

# project name
set(PROJECT_NAME crawlserv)
project(${PROJECT_NAME})

# show source directory
message("Source: \"${CMAKE_CURRENT_SOURCE_DIR}/\"")

# find Multi-threading library
find_package(Threads REQUIRED)

if(${Threads_FOUND})
	message("Multi-threading library found: \"${CMAKE_THREAD_LIBS_INIT}\"")
else()
	message(WARNING "Multi-threading library not found.")
endif()

# find Boost.System
find_package(Boost COMPONENTS System QUIET)

if(${Boost_FOUND})
	message("Boost ${Boost_VERSION} library found: \"${Boost_LIBRARIES}\"")
else()
	# find manually
	unset(Boost_LIBRARIES)

	find_library(Boost_LIBRARIES boost_system PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(Boost_LIBRARIES)
		message("Boost.System library manually found: \"${Boost_LIBRARIES}\"")
	else()
		message(WARNING "Boost.System library not found.")
	endif()
endif()

# find cURL
find_package(curl QUIET)

if(${curl_FOUND})
	message("cURL ${curl_VERSION} library found: \"${curl_LIBRARIES}\"")
else()
	# find manually
	unset(curl_LIBRARIES)

	find_library(curl_LIBRARIES curl PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(curl_LIBRARIES)
		message("cURL library manually found: \"${curl_LIBRARIES}\"")
	else()
		message(WARNING "cURL library not found.")
	endif()
endif()

# find tidy-html5
find_package(tidy QUIET)

if(${tidy_FOUND})
	message("tidy-html5 include directory found: \"${tidy_INCLUDE}\"")
	message("tidy-html5 library found: \"${tidy_LIBRARIES}\"")
else()
	# find manually
	unset(tidy_INCLUDE)
	unset(tidy_LIBRARIES)

	find_path(tidy_INCLUDE tidy.h PATH_SUFFIXES tidy-html5 tidy-html5/include tidy tidy/include REQUIRED)
	find_library(tidy_LIBRARIES tidy PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(tidy_INCLUDE)
		message("tidy-html5 include directory manually found: \"${tidy_INCLUDE}/\"")
	else()
		message(WARNING "tidy-html5 include directory not found.")
	endif()

	if(tidy_LIBRARIES)
		message("tidy-html5 library manually found: \"${tidy_LIBRARIES}\"")
	else()
		message(WARNING "tidy-html5 library not found.")
	endif()
endif()

# find uriparser
find_package(uriparser QUIET)

if(${uriparser_FOUND})
	message("uriparser library found: ${uriparser_LIBRARIES}")
else()
	# find manually
	unset(uriparser_LIBRARIES)

	find_library(uriparser_LIBRARIES uriparser PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(uriparser_LIBRARIES)
		message("uriparser library manually found: \"${uriparser_LIBRARIES}\"")
	else()
		message(WARNING "uriparser library not found.")
	endif()
endif()

# find pugixml
find_package(pugixml QUIET)

if(${pugixml_FOUND})
	message("pugixml library found: \"${pugixml_LIBRARIES}\"")
else()
	# find manually
	unset(pugixml_LIBRARIES)

	find_library(pugixml_LIBRARIES pugixml PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(pugixml_LIBRARIES)
		message("pugixml library manually found: \"${pugixml_LIBRARIES}\"")
	else()
		message(WARNING "pugixml library not found.")
	endif()
endif()

# find PCRE
find_package(PCRE2 QUIET)

if(${PCRE2_FOUND})
	message("PCRE2 library found: \"${PCRE2_LIBRARIES}\"")
else()
	# find manually
	unset(PCRE2_LIBRARIES)

	find_library(PCRE2_LIBRARIES NAMES pcre2 pcre2-8 PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(PCRE2_LIBRARIES)
		message("PCRE2 library manually found: \"${PCRE2_LIBRARIES}\"")
	else()
		message(WARNING "PCRE2 library not found.")
	endif()
endif()

# find MySQL C++ Connector
find_package(mysqlcppconn QUIET)

if(${mysqlcppconn_FOUND})
	message("MySQL Connector/C++ include directory found: \"${mysqlcppconn_INCLUDE}/\"")
	message("MySQL Connector/C++ library found: \"${mysqlcppconn_LIBRARIES}\"")
else()
	# find manually
	unset(mysqlcppconn_INCLUDE)
	unset(mysqlcppconn_LIBRARIES)

	find_path(mysqlcppconn_INCLUDE mysql_driver.h PATH_SUFFIXES mysql-cppconn-8/jdbc mysql-cppconn-8 mysqlcppconn/jdbc mysqlcppconn mysql-cppconn/jdbc mysql-cppconn cppcon/jdbc cppconn include REQUIRED)
	find_library(mysqlcppconn_LIBRARIES mysqlcppconn PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(mysqlcppconn_INCLUDE)
		message("MySQL Connector/C++ include directory manually found: \"${mysqlcppconn_INCLUDE}/\"")
	else()
		message(WARNING "MySQL Connector/C++ include directory not found.")
	endif()

	if(mysqlcppconn_LIBRARIES)
		message("MySQL Connector/C++ library manually found: \"${mysqlcppconn_LIBRARIES}\"")
	else()
		message(WARNING "MySQL Connector/C++ library not found.")
	endif()
endif()

# find GNU Aspell
# 	NOTE: Might be removed in the near future.
find_package(ASPELL QUIET)

if(${ASPELL_FOUND})
	message("GNU Aspell library found: \"${ASPELL_LIBRARIES}\"")
else()
	# find manually
	unset(ASPELL_LIBRARIES)

	find_library(ASPELL_LIBRARIES aspell PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(ASPELL_LIBRARIES)
		message("GNU Aspell library manually found: \"${ASPELL_LIBRARIES}\"")
	else()
		message(WARNING "GNU Aspell library not found.")
	endif()
endif()

# source files
add_executable(${PROJECT_NAME}
		src/main.cpp
		src/Main/App.cpp
		src/Main/ConfigFile.cpp
		src/Main/Database.cpp
		src/Main/Server.cpp
		src/Main/WebServer.cpp
		src/Module/Analyzer/Algo/All.cpp
		src/Module/Analyzer/Algo/MarkovText.cpp
		src/Module/Analyzer/Algo/MarkovTweet.cpp
		src/Module/Analyzer/Database.cpp
		src/Module/Analyzer/Thread.cpp
		src/Module/Crawler/Database.cpp
		src/Module/Crawler/Thread.cpp
		src/Module/Parser/Database.cpp
		src/Module/Parser/Thread.cpp
		src/Module/Database.cpp
		src/Module/Thread.cpp
		src/Network/Curl.cpp
		src/Parsing/HTML.cpp
		src/Parsing/URI.cpp
		src/Parsing/XML.cpp
		src/Query/Container.cpp
		src/Query/RegEx.cpp
		src/Query/XPath.cpp
		src/Timer/Simple.cpp
		src/Timer/SimpleHR.cpp
		src/Timer/StartStop.cpp
		src/Timer/StartStopHR.cpp
		src/Wrapper/Curl.cpp
		src/_extern/mongoose/mongoose.c

# NOTE: The following might be removed in the near future.
		src/_extern/rawr/histogram.cpp
		src/_extern/rawr/kgramstats.cpp
		src/_extern/rawr/prefix_search.cpp
)

# C++ 17 (including corresponding standard library) is required
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# set include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${mysqlcppconn_INCLUDE})
target_include_directories(${PROJECT_NAME} PRIVATE ${tidy_INCLUDE})

# set libraries
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PROJECT_NAME} stdc++fs)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${curl_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${tidy_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${uriparser_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${pugixml_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${PCRE2_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${mysqlcppconn_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${ASPELL_LIBRARIES})

# set compiler options
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4)

	if(${CMAKE_BUILD_TYPE} AND ${CMAKE_BUILD_TYPE} STREQUAL Debug)
		message("Using MSVC without optimiziation and with debugging information.")
		target_compile_options(${PROJECT_NAME} PRIVATE /Od)
		target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
	else()
		message("Using MSVC with maximum optimiziation and without debugging information.")
		target_compile_options(${PROJECT_NAME} PRIVATE /Ox)
	endif()
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)

	if(${CMAKE_BUILD_TYPE} AND ${CMAKE_BUILD_TYPE} STREQUAL Debug)
		message("Using \"${CMAKE_CXX_COMPILER}\" with minimum optimization and debugging information.")
		target_compile_options(${PROJECT_NAME} PRIVATE -g3)
		target_compile_options(${PROJECT_NAME} PRIVATE -Og)
	else()
		message("Using \"${CMAKE_CXX_COMPILER}\" with maximum optimization and without debugging information.")
		target_compile_options(${PROJECT_NAME} PRIVATE -g0)
		target_compile_options(${PROJECT_NAME} PRIVATE -O3)
	endif()
endif()

message("Run \"make\" from build directory to build.")

