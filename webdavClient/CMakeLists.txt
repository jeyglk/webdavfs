# Project setup
PROJECT(httpClient)
SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "1")
SET(VERSION_PATCH "0")
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0 FATAL_ERROR) 


# Compiler setup
SET(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
SET(CMAKE_C_FLAGS_RELEASE "-O2")

# Options
OPTION(XML_PARSER_VERBOSE "Enable to be told everything the xml parser does" OFF)

IF(XML_PARSER_VERBOSE)
	ADD_DEFINITIONS(-DXML_PARSER_VERBOSE)
ENDIF(XML_PARSER_VERBOSE)


# Sources
SET(SOURCE_DIRECTORY src)


# Build library
ADD_LIBRARY(xml STATIC
	${SOURCE_DIRECTORY}/xml.c
)

# Building httpClient
ADD_EXECUTABLE(webdavClient
	${SOURCE_DIRECTORY}/webdavClient.c
)
TARGET_LINK_LIBRARIES(webdavClient xml)

