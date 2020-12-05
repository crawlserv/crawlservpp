# cMake version
cmake_minimum_required(VERSION 3.8.2)

# project name
set(PROJECT_NAME crawlserv)
project(${PROJECT_NAME})

# show source directory
message("Source: \"${CMAKE_CURRENT_SOURCE_DIR}/\"")

# find Multi-threading library
find_package(Threads REQUIRED)

if(${Threads_FOUND} AND NOT CMAKE_THREAD_LIBS_INIT STREQUAL "")
	message("Multi-threading library found: \"${CMAKE_THREAD_LIBS_INIT}\"")
else()
	message(WARNING "Multi-threading library not found.")
endif()

# find Boost.System and Boost.Iostreams
find_package(Boost REQUIRED COMPONENTS system iostreams QUIET)

if(${Boost_FOUND} AND Boost_LIBRARIES)
	message("Boost v${Boost_VERSION} libraries found: \"${Boost_LIBRARIES}\"")
else()
	# find manually
	unset(Boost_LIBRARIES)

	find_library(Boost_LIBRARIES1 boost_system PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(Boost_LIBRARIES1)
		message("Boost.System library manually found: \"${Boost_LIBRARIES1}\"")
	else()
		message(WARNING "Boost.System library not found.")
	endif()
	
	find_library(Boost_LIBRARIES2 boost_iostreams PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)

	if(Boost_LIBRARIES2)
		message("Boost.Iostreams library manually found: \"${Boost_LIBRARIES2}\"")
	else()
		message(WARNING "Boost.Iostreams library not found.")
	endif()
	
	set(Boost_LIBRARIES ${Boost_LIBRARIES1};${Boost_LIBRARIES2})
	
	message("Using Boost libraries: ${Boost_LIBRARIES}") 
endif()

# find cURL
find_package(curl QUIET)

if(${curl_FOUND} AND curl_LIBRARIES)
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

if(${tidy_FOUND} AND tidy_INCLUDE AND tidy_LIBRARIES)
	message("tidy-html5 include directory found: \"${tidy_INCLUDE}\"")
	message("tidy-html5 library found: \"${tidy_LIBRARIES}\"")
else()
	# find manually
	unset(tidy_INCLUDE)
	unset(tidy_LIBRARIES)

	find_path(tidy_INCLUDE tidybuffio.h PATH_SUFFIXES tidy-html5 tidy-html5/include tidy tidy/include REQUIRED)
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

if(${uriparser_FOUND} AND uriparser_LIBRARIES)
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

if(${pugixml_FOUND} AND pugixml_LIBRARIES)
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

if(${PCRE2_FOUND} AND PCRE2_LIBRARIES)
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

if(${mysqlcppconn_FOUND} AND mysqlcppconn_INCLUDE AND mysqlcppconn_LIBRARIES)
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

# find zlib
find_package(ZLIB QUIET)

if(${ZLIB_FOUND} AND ZLIB_INCLUDE_DIRS AND ZLIB_LIBRARIES)
	message("zlib v${ZLIB_VERSION_STRING} include directory found: \"${ZLIB_INCLUDE_DIRS}\"")
	message("zlib v${ZLIB_VERSION_STRING} library found: \"${ZLIB_LIBRARIES}\"")
else()
	# find manually
	unset(ZLIB_INCLUDE_DIRS)
	unset(ZLIB_LIBRARIES)
	
	find_path(ZLIB_INCLUDE_DIRS zlib.h PATH_SUFFIXES zlib/include include REQUIRED)
	find_library(ZLIB_LIBRARIES z PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} lib64 lib REQUIRED)
	
	if(ZLIB_INCLUDE_DIRS)
		message("zlib include directory manually found: \"${ZLIB_INCLUDE_DIRS}/\"")
	else()
		message(WARNING "zlib include directory not found.")
	endif()

	if(ZLIB_LIBRARIES)
		message("zlib library manually found: \"${ZLIB_LIBRARIES}\"")
	else()
		message(WARNING "zlib library not found.")
	endif()
endif()

# find GNU Aspell
# 	NOTE: Might be removed in the near future.
find_package(ASPELL QUIET)

if(${ASPELL_FOUND} AND ASPELL_LIBRARIES)
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

# set include directory for asio to sub-directory of source code
set(asio_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src/_extern/asio/asio/include")

message("asio include directory set to: \"${asio_INCLUDE}\".")

# set include directory for jsoncons to sub-directory of source code
set(jsoncons_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src/_extern/jsoncons/include")

message("jsoncons include directory set to: \"${jsoncons_INCLUDE}\".")

# set include directory for porter2_stemmer to sub-directory of source code
set(porter2_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src/_extern/porter2_stemmer")

message("porter2_stemmer include directory set to: \"${porter2_INCLUDE}\".")

# source files
add_executable(${PROJECT_NAME}
		src/main.cpp
		src/Main/App.cpp
		src/Main/Database.cpp
		src/Main/Server.cpp
		src/Main/WebServer.cpp
		src/Module/Analyzer/Algo/All.cpp
		src/Module/Analyzer/Algo/AssocOverTime.cpp
		src/Module/Analyzer/Algo/CorpusGenerator.cpp
		src/Module/Analyzer/Algo/MarkovText.cpp
		src/Module/Analyzer/Algo/MarkovTweet.cpp
		src/Module/Analyzer/Algo/TokensOverTime.cpp
		src/Module/Analyzer/Database.cpp
		src/Module/Analyzer/Thread.cpp
		src/Module/Crawler/Database.cpp
		src/Module/Crawler/Thread.cpp
		src/Module/Extractor/Database.cpp
		src/Module/Extractor/Thread.cpp
		src/Module/Parser/Database.cpp
		src/Module/Parser/Thread.cpp
		src/Module/Database.cpp
		src/Module/Thread.cpp
		src/Wrapper/Curl.cpp
		src/_extern/mongoose/mongoose.c
		src/_extern/porter2_stemmer/porter2_stemmer.cpp
		src/_extern/wapiti/wapiti.cpp

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
target_include_directories(${PROJECT_NAME} PRIVATE ${asio_INCLUDE})
target_include_directories(${PROJECT_NAME} PRIVATE ${jsoncons_INCLUDE})
target_include_directories(${PROJECT_NAME} PRIVATE ${porter2_INCLUDE})
target_include_directories(${PROJECT_NAME} PRIVATE ${mysqlcppconn_INCLUDE})
target_include_directories(${PROJECT_NAME} PRIVATE ${tidy_INCLUDE})
target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIRS})

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
target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES})

# NOTE: The following might be removed in the near future.
target_link_libraries(${PROJECT_NAME} ${ASPELL_LIBRARIES})

# set required library options
target_compile_definitions(${PROJECT_NAME} PRIVATE MG_ENABLE_HTTP_STREAMING_MULTIPART)
target_compile_definitions(${PROJECT_NAME} PRIVATE PCRE2_CODE_UNIT_WIDTH=8)
target_compile_definitions(${PROJECT_NAME} PRIVATE RAPIDJSON_NO_SIZETYPEDEFINE)
target_compile_definitions(${PROJECT_NAME} PRIVATE RAPIDJSON_HAS_STDSTRING)
target_compile_definitions(${PROJECT_NAME} PRIVATE ZLIB_CONST)

# set optional library option
target_compile_definitions(${PROJECT_NAME} PRIVATE JSONCONS_NO_DEPRECATED)

# set temporary library option (see https://github.com/HowardHinnant/date/issues/435) 
target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_UNCAUGHT_EXCEPTIONS)

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

    if(NOT CMAKE_BUILD_TYPE)
    	SET(CMAKE_BUILT_TYPE Release ... FORCE)
    endif()

	if(${CMAKE_BUILD_TYPE} MATCHES Debug)
		message("Using \"${CMAKE_CXX_COMPILER}\" with minimum optimization and debugging information.")
		target_compile_options(${PROJECT_NAME} PRIVATE -g3)
		target_compile_options(${PROJECT_NAME} PRIVATE -O0)
	else()
		message("Using \"${CMAKE_CXX_COMPILER}\" with maximum optimization and without debugging information.")
		target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
		target_compile_options(${PROJECT_NAME} PRIVATE -g0)
		target_compile_options(${PROJECT_NAME} PRIVATE -O3)
	endif()
endif()

# setup clang-tidy
#find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable" )
#
#if(CLANG_TIDY_EXE)
#	message("clang-tidy found: ${CLANG_TIDY_EXE}")
#	
#	string(
#			CONCAT CLANG_TIDY_TESTS "*,"
#			"-clang-analyzer-core.NullDereference,"
#			"-clang-analyzer-optin.cplusplus.UninitializedObject,"
#			"-clang-diagnostic-c++17-extensions,"
#			"-cppcoreguidelines-non-private-member-variables-in-classes,"
#			"-cppcoreguidelines-pro-type-reinterpret-cast,"
#			"-cppcoreguidelines-pro-type-union-access,"
#			"-fuchsia-default-arguments-calls,"
#			"-fuchsia-multiple-inheritance,"
#			"-fuchsia-overloaded-operator,"
#			"-google-readability-todo,"	
#			"-google-runtime-references,"
#			"-llvm-header-guard,"
#			"-misc-non-private-member-variables-in-classes,"
#			"-modernize-pass-by-value,"
#			"-modernize-use-trailing-return-type,"
#	)
#	
#	set(
#			CLANG_TIDY_HEADER_FILTER [=[[A-Z][a-zA-Z]+\.hpp$]=]
#	)
#	
#	set(
#			DO_CLANG_TIDY
#			"${CLANG_TIDY_EXE}"
#			"-header-filter=${CLANG_TIDY_HEADER_FILTER}"
#			"-checks=${CLANG_TIDY_TESTS}"
#	)
#	
#	set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
#else()
#	message(WARNING "clang-tidy not found.")
#endif()

message("Run \"make\" from build directory to build.")

