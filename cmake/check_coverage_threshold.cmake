# check_coverage_threshold.cmake
#
# Parses a coverage report file written by coverage_report.cmake (see
# COV_OUTFILE there, e.g. build/coverage-ut.txt / build/coverage-bbh.txt)
# and fails the build (non-zero exit via message(FATAL_ERROR)) if the
# bdd.hpp "Lines executed" percentage found in it is below COV_MIN_PERCENT.
#
# Expected variables (set via -D on the `cmake -P` command line):
#   COV_FILE        - path to a coverage-ut.txt / coverage-bbh.txt report
#   COV_MIN_PERCENT - minimum required "Lines executed" percentage (e.g. 90)
#   COV_LABEL       - human-readable label for messages (e.g. "UT", "BBH")

foreach(_required COV_FILE COV_MIN_PERCENT COV_LABEL)
	if(NOT DEFINED ${_required})
		message(FATAL_ERROR "check_coverage_threshold.cmake: ${_required} must be set")
	endif()
endforeach()

if(NOT EXISTS "${COV_FILE}")
	message(FATAL_ERROR "check_coverage_threshold.cmake: ${COV_FILE} does not exist "
		"(did the matching coverage-ut/coverage-bbh target run first?)")
endif()

file(READ "${COV_FILE}" _contents)

# coverage_report.cmake emits a "File 'bdd.hpp' (true per-line union across N
# units)\nLines executed:X% of Y" block whenever it measured more than one
# translation unit (COV_COMBINE) - which is the true, deduplicated bdd.hpp
# percentage across the whole suite, and is preferred whenever present.
string(REGEX MATCH "File 'bdd\\.hpp' \\(true per-line union[^\n]*\\)\nLines executed:([0-9]+)\\.([0-9]+)% of ([0-9]+)"
	_combined_match "${_contents}")

if(_combined_match)
	set(_int_part "${CMAKE_MATCH_1}")
	set(_frac_part "${CMAKE_MATCH_2}")
	set(_total "${CMAKE_MATCH_3}")
else()
	# Fall back to the last single-unit "File '.../bdd.hpp'" block (e.g. a
	# report with only one measured unit, where coverage_report.cmake never
	# emits a combined section). Matches the same file-path pattern
	# coverage_report.cmake itself uses to find bdd.hpp's own summary among
	# gcov's raw per-file output.
	string(REGEX MATCHALL "File '[^\n']*bdd\\.hpp'\nLines executed:[0-9]+\\.[0-9]+% of [0-9]+"
		_bdd_blocks "${_contents}")
	list(LENGTH _bdd_blocks _n_bdd_blocks)
	if(_n_bdd_blocks EQUAL 0)
		message(FATAL_ERROR "check_coverage_threshold.cmake: no bdd.hpp 'Lines executed:' "
			"summary found in ${COV_FILE}")
	endif()
	list(GET _bdd_blocks -1 _last_block)
	string(REGEX MATCH "Lines executed:([0-9]+)\\.([0-9]+)% of ([0-9]+)" _unused "${_last_block}")
	set(_int_part "${CMAKE_MATCH_1}")
	set(_frac_part "${CMAKE_MATCH_2}")
	set(_total "${CMAKE_MATCH_3}")
endif()

# Compare as scaled integers (percent * 100) to avoid floating point in
# CMake's integer-only math() - e.g. 89.99% must fail a 90% threshold.
math(EXPR _pct_x100 "${_int_part} * 100 + ${_frac_part}")
math(EXPR _min_x100 "${COV_MIN_PERCENT} * 100")

message(STATUS "bdd.hpp coverage (${COV_LABEL}): ${_int_part}.${_frac_part}% of ${_total} lines "
	"(minimum required: ${COV_MIN_PERCENT}%)")

if(_pct_x100 LESS _min_x100)
	message(FATAL_ERROR "bdd.hpp coverage (${COV_LABEL}) is ${_int_part}.${_frac_part}%, "
		"below the required ${COV_MIN_PERCENT}% threshold")
endif()
