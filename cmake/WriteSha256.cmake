if(NOT DEFINED INPUT_FILE OR NOT EXISTS "${INPUT_FILE}")
    message(FATAL_ERROR "SHA-256 input file does not exist: ${INPUT_FILE}")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE was not specified")
endif()

file(SHA256 "${INPUT_FILE}" EVI_SHA256)
get_filename_component(EVI_FILENAME "${INPUT_FILE}" NAME)
file(WRITE "${OUTPUT_FILE}" "${EVI_SHA256}  ${EVI_FILENAME}\n")
message(STATUS "SHA-256: ${EVI_SHA256}  ${EVI_FILENAME}")
