#This sets the default cache values for the "simi-qt" project.

MACRO(SQ_SET_CACHE varname value type string_description)
	SET(${varname} ${value} CACHE ${type} ${string_description})

	IF(DO_SUPER_BUILD)
		#If doing superbuild build cache value list
		SET(SQ_CMAKE_CACHE_SB 
		${SQ_CMAKE_CACHE_SB} -D${varname}:${type}=${value})
	ENDIF()
ENDMACRO(SQ_SET_CACHE)

SQ_SET_CACHE(SQ_BUILD_TYPE "Debug" STRING "Choose the type of build.")
# Set the possible values of build type for cmake-gui
set_property(CACHE SQ_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
