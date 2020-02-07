CMAKE_MINIMUM_REQUIRED(VERSION 3.14)
PROJECT(dtrmgr LANGUAGES C)

# Install to standard GNU directories:
INCLUDE(GNUInstallDirs)

# SQLite3 is required:
FIND_PACKAGE(SQLite3 REQUIRED)

# Generate the config.h file:
CONFIGURE_FILE(config.h.in config.h)

#
# Target: dtrmgr
#
ADD_EXECUTABLE(dtrmgr STimeRange.c SSchedule.c dtrmgr.c)
TARGET_INCLUDE_DIRECTORIES(dtrmgr PUBLIC ${SQLite3_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
TARGET_LINK_LIBRARIES(dtrmgr ${SQLite3_LIBRARIES})
INSTALL(TARGETS dtrmgr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
