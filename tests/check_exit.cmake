# Runs PROGRAM and checks its exit code (EXPECTED_EXIT) and that its output
# contains EXPECTED_TEXT.
execute_process(COMMAND ${PROGRAM} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL ${EXPECTED_EXIT})
  message(FATAL_ERROR "exit code ${code}, expected ${EXPECTED_EXIT}\n${out}${err}")
endif()
string(FIND "${out}" "${EXPECTED_TEXT}" found)
if(found EQUAL -1)
  message(FATAL_ERROR "output does not contain '${EXPECTED_TEXT}':\n${out}${err}")
endif()
string(FIND "${out}" "handler.dic:1:" located)
if(located EQUAL -1)
  message(FATAL_ERROR "output does not contain the location of the error:\n${out}")
endif()
