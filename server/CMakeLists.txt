project(upnp_server CXX)

# Find required modules
# PKG_CHECK_MODULES: used to check for given modules
# PKG_SEARCH_MODULE: used to check for given modules and uses the first working one
INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(rpkgs-dlog REQUIRED dlog)
PKG_CHECK_MODULES(rpkgs-capi REQUIRED capi-network-bluetooth capi-system-info)
PKG_CHECK_MODULES(rpkgs-glib REQUIRED glib-2.0)

# Add CFLAGS used for required modules
ADD_DEFINITIONS(${rpkgs-dlog_CFLAGS})
ADD_DEFINITIONS(${rpkgs-capi_CFLAGS})
ADD_DEFINITIONS(${rpkgs-glib_CFLAGS})

# Set include directories
INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../upnp/include
	${rpkgs-dlog_INCLUDE_DIRS}
	${rpkgs-capi_INCLUDE_DIRS}
	)

# Set sources
ADD_EXECUTABLE(${PROJECT_NAME}
	server_main.cpp
	tizen_ctrl.cpp
	sample_util.cpp
)


set(UPNP_LIBRARIES "-lixml -lupnp -lthreadutil")
set(UPNP_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../upnp/lib)
set(UPNP_LDFLAGS -L${UPNP_LIBRARY_DIR} ${UPNP_LIBRARIES})

#
# Set LDFLAGS
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${rpkgs-dlog_LDFLAGS} ${rpkgs-capi_LDFLAGS} ${rpkgs-glib_LDFLAGS} "-ldl -lrt" ${UPNP_LDFLAGS})

# Set CFLAGS
ADD_DEFINITIONS(-Wall -O3) 
SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

# Move result binary to the target position(/usr/bin)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)
