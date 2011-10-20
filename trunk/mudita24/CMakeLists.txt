include(FindPkgConfig)

project(mudita24)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.2)
#if (COMMAND cmake_policy)
      #cmake_policy(SET CMP0003 NEW)
      #cmake_policy(SET CMP0011 NEW)
#endif(COMMAND cmake_policy)

IF(NOT DEFINED SHARE_INSTALL_PREFIX)
        SET(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share")
ENDIF(NOT DEFINED SHARE_INSTALL_PREFIX)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

SET(mudita24_INSTALL_NAME  "mudita24-1.1.0")
SET(mudita24_EXEC_NAME     "mudita24")
SET(mudita24_VERSION       "1.1.0")
FIND_PROGRAM(SVNVER svnversion)
if (${SVNVER} STREQUAL "SVNVER-NOTFOUND")
      SET (mudita24_SVNVER 0)
else (${SVNVER} STREQUAL "SVNVER-NOTFOUND")
      EXECUTE_PROCESS( COMMAND svnversion ${PROJECT_SOURCE_DIR}
                       OUTPUT_VARIABLE mudita24_SVNVER 
                       OUTPUT_STRIP_TRAILING_WHITESPACE )
endif (${SVNVER} STREQUAL "SVNVER-NOTFOUND")

## Top documentation dir
IF(NOT DEFINED mudita24_DOC_DIR)
      SET(mudita24_DOC_DIR ${SHARE_INSTALL_PREFIX}/doc/${mudita24_INSTALL_NAME}/)
ENDIF(NOT DEFINED mudita24_DOC_DIR)

#find_package(ALSA 0.9.0 REQUIRED alsa)
# This didn't work. GTK2_INCLUDE_DIRS was blank.
#find_package(GTK2 2.0 REQUIRED gtk)

PKG_CHECK_MODULES(GTK2 REQUIRED gtk+-2.0>=2.20)
# TODO What's the minimum required?
PKG_CHECK_MODULES(ALSA REQUIRED alsa>=1.0.0)

# TODO check for log10
## NPM: version > 1.0.1's logarithmic display requires log10() and therefore
## "-lm". 
#PKG_CHECK_MODULES(M REQUIRED m) 

include_directories(${ALSA_INCLUDE_DIRS})
include_directories(${GTK2_INCLUDE_DIRS})

FIND_PROGRAM(mudita24_ALSACTL alsactl)
# message(" alsactl location: " ${mudita24_ALSACTL} )  
if (${mudita24_ALSACTL} STREQUAL "mudita24_ALSACTL-NOTFOUND")
      SET (mudita24_ALSACTL 0)
endif (${mudita24_ALSACTL} STREQUAL "mudita24_ALSACTL-NOTFOUND")

#
# produce globaldefs.h file
#
configure_file (
      ${PROJECT_SOURCE_DIR}/globaldefs.h.in
      ${PROJECT_BINARY_DIR}/globaldefs.h
      )

include_directories(
      ${CMAKE_CURRENT_BINARY_DIR}
      )

set (mudita24_source_files
      envy24control.c # envy24control.h 
      levelmeters.c 
      midi.c
      mixer.c 
      patchbay.c 
      hardware.c 
      driverevents.c 
      volume.c 
      profiles.c # profiles.h 
      midi.h 
      config.c # config.h
)

add_executable( mudita24 ${mudita24_source_files} )

##
## Compilation flags 
## Oct 18, 2011: Suggested by James Morris along with supplied patch:                                               
##   -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGSEAL_ENABLE 
## Incomplete changes to source for now, causes errors.
#
# set_target_properties( mudita24
#      PROPERTIES COMPILE_FLAGS "-DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGSEAL_ENABLE"
#      )

target_link_libraries(mudita24
      ${ALSA_LIBRARIES}
      ${GTK2_LIBRARIES}
      #${M_LIBRARIES}
      m
      )

install( TARGETS mudita24 
      RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
      )

## Top documentation dir
IF(NOT DEFINED mudita24_DOC_DIR)
      SET(mudita24_DOC_DIR ${SHARE_INSTALL_PREFIX}/doc/${mudita24_INSTALL_NAME}/)
ENDIF(NOT DEFINED mudita24_DOC_DIR)

## Install doc files
file (GLOB doc_files
      README.profiles
      )
INSTALL( FILES ${doc_files} DESTINATION ${mudita24_DOC_DIR})

# produce the manpages
# TODO:
# configure_file (
#      mudita24.1.in
#      ${PROJECT_BINARY_DIR}/man/${mudita24_EXEC_NAME}.1
#      )
#install( FILES 
#      ${PROJECT_BINARY_DIR}/man/${mudita24_EXEC_NAME}.1
#      DESTINATION ${SHARE_INSTALL_PREFIX}/man/man1/
#      )

