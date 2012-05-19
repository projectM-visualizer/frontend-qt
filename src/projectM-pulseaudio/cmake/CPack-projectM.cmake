#CPACK projectM config

SET(CPACK_GENERATORS "TGZ")
SET(CPACK_SOURCE_GENERATORS "TGZ")
SET(CPACK_STRIP_FILES ON)
#SET(CPACK_IGNORE_FILES 
 # "*~;CMakeCache.txt;CMakeFiles/;.svn;.*.bak;*.Z;_*/;.*.gz;.*.tgz;.*.tar;.*.bz2;.*.zip;.*.rpm;.*.deb;.*.o")
SET(CPACK_SOURCE_IGNORE_FILES 
  "Makefile;CMakeCache.txt;CMakeFiles/;.*.gz;.*.bz2;.*.Z;.*.zip;.*svn")

#SET(CPACK_BINARY_RPM OFF)
# #SET(CPACK_BINARY_Z OFF)
#SET(CPACK_SOURCE_TGZ ON)
SET(CPACK_SOURCE_Z OFF)
SET(CPACK_SOURCE_TZ OFF)
SET(CPACK_SOURCE_TBZ2 ON)
#SET(CPACK_BINARY_TGZ ON)
#SET(CPACK_SOURCE_Z OFF)
SET(CPACK_PACKAGE_VERSION_MAJOR "2")
SET(CPACK_PACKAGE_VERSION_MINOR "1")
SET(CPACK_PACKAGE_VERSION_PATCH "0")

INCLUDE(CPack)

