# Create blank config.h
FILE(WRITE ladspa/config.h "")

# Precompiler PIC Macro
IF(LMMS_BUILD_WIN32)
	SET(PIC_FLAGS "")
ELSE()
	SET(PIC_FLAGS "-fPIC -DPIC")
ENDIF()

# Additional compile flags
if(MSVC)
	set(COMPILE_FLAGS "${COMPILE_FLAGS} /wd4244 /wd4273 /wd4305")
else()
	set(COMPILE_FLAGS "${COMPILE_FLAGS} -O3 -Wall")
	set(COMPILE_FLAGS "${COMPILE_FLAGS} -fomit-frame-pointer -funroll-loops -ffast-math -c -fno-strict-aliasing")
	set(COMPILE_FLAGS "${COMPILE_FLAGS} ${PIC_FLAGS}")
endif()

# Loop over every XML file
FILE(GLOB XML_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ladspa/*.xml")
LIST(SORT XML_SOURCES)

FOREACH(_item ${XML_SOURCES})
	# Get library name and (soon to be) C file
	GET_FILENAME_COMPONENT(_plugin "${_item}" NAME_WE)
	SET(_out_file "${CMAKE_CURRENT_SOURCE_DIR}/ladspa/${_plugin}.c")

	# Coerce XML source file to C
	ADD_CUSTOM_COMMAND(
		OUTPUT "${_out_file}"
		COMMAND "${PERL_EXECUTABLE}" ./makestub.pl "${_item}" > "${_out_file}"
		DEPENDS "${_item}"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ladspa"
		VERBATIM
	)

	# Add a library target for this C file, which depends on success of makestup.pl
	ADD_LIBRARY("${_plugin}" MODULE "${_out_file}")

	if(MSVC)
		target_link_options("${_plugin}" PRIVATE "/EXPORT:ladspa_descriptor")
	endif()

	# Vocoder does not use fftw
	IF(NOT ("${_plugin}" STREQUAL "vocoder_1337"))
		TARGET_LINK_LIBRARIES("${_plugin}" ${FFTW3F_LIBRARIES})
	ENDIF()
	IF(NOT MSVC)
		TARGET_LINK_LIBRARIES("${_plugin}" m)
	ENDIF()

	if(LMMS_BUILD_WIN32)
		add_custom_command(
			TARGET "${_plugin}"
			POST_BUILD
			COMMAND "${STRIP_COMMAND}" "$<TARGET_FILE:${_plugin}>"
			VERBATIM
			COMMAND_EXPAND_LISTS
		)
	endif(LMMS_BUILD_WIN32)

	SET_TARGET_PROPERTIES("${_plugin}" PROPERTIES PREFIX "")
	SET_TARGET_PROPERTIES("${_plugin}" PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS}")
	SET_TARGET_PROPERTIES("${_plugin}" PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
	INSTALL(TARGETS "${_plugin}" LIBRARY DESTINATION "${PLUGIN_DIR}/ladspa")
ENDFOREACH()

ADD_DEFINITIONS(-DFFTW3)
INCLUDE_DIRECTORIES(
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/ladspa"
	"${FFTW3F_INCLUDE_DIRS}"
	"${CMAKE_BINARY_DIR}"
)

ADD_LIBRARY(iir STATIC ladspa/util/iir.c)
SET_TARGET_PROPERTIES(iir PROPERTIES COMPILE_FLAGS "${PIC_FLAGS}")
TARGET_LINK_LIBRARIES(bandpass_a_iir_1893 iir)
TARGET_LINK_LIBRARIES(bandpass_iir_1892 iir)
TARGET_LINK_LIBRARIES(butterworth_1902 iir)
TARGET_LINK_LIBRARIES(highpass_iir_1890 iir)
TARGET_LINK_LIBRARIES(lowpass_iir_1891 iir)
TARGET_LINK_LIBRARIES(notch_iir_1894 iir)

FILE(GLOB GSM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ladspa/gsm/*.c")
LIST(SORT GSM_SOURCES)
ADD_LIBRARY(gsm STATIC ${GSM_SOURCES})
SET_TARGET_PROPERTIES(gsm PROPERTIES COMPILE_FLAGS "${PIC_FLAGS}")
TARGET_LINK_LIBRARIES(gsm_1215 gsm)

ADD_LIBRARY(gverb STATIC ladspa/gverb/gverb.c ladspa/gverb/gverbdsp.c)
SET_TARGET_PROPERTIES(gverb PROPERTIES COMPILE_FLAGS "${PIC_FLAGS}")
TARGET_LINK_LIBRARIES(gverb_1216 gverb)

ADD_LIBRARY(blo STATIC ladspa/util/blo.c)
SET_TARGET_PROPERTIES(blo PROPERTIES COMPILE_FLAGS "${PIC_FLAGS}")
TARGET_LINK_LIBRARIES(analogue_osc_1416 blo)
TARGET_LINK_LIBRARIES(fm_osc_1415 blo)
TARGET_LINK_LIBRARIES(hermes_filter_1200 blo)

ADD_LIBRARY(rms STATIC ladspa/util/rms.c)
ADD_LIBRARY(db STATIC ladspa/util/db.c)
SET_TARGET_PROPERTIES(rms PROPERTIES COMPILE_FLAGS "${PIC_FLAGS}")
SET_TARGET_PROPERTIES(db PROPERTIES COMPILE_FLAGS "${PIC_FLAGS}")
TARGET_LINK_LIBRARIES(sc1_1425 rms db)
TARGET_LINK_LIBRARIES(sc2_1426 rms db)
TARGET_LINK_LIBRARIES(sc3_1427 rms db)
TARGET_LINK_LIBRARIES(sc4_1882 rms db)
TARGET_LINK_LIBRARIES(sc4m_1916 rms db)
TARGET_LINK_LIBRARIES(se4_1883 rms db)

ADD_LIBRARY(pitchscale STATIC ladspa/util/pitchscale.c)
SET_TARGET_PROPERTIES(pitchscale PROPERTIES COMPILE_FLAGS "${PIC_FLAGS}")
TARGET_LINK_LIBRARIES(pitchscale -lfftw3f)
TARGET_LINK_LIBRARIES(pitch_scale_1193 pitchscale)
TARGET_LINK_LIBRARIES(pitch_scale_1194 pitchscale)
