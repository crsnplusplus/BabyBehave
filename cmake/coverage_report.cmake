# coverage_report.cmake
#
# Runs gcov for one or more translation units and extracts/prints the
# "Lines executed:X% of Y" coverage summary specifically for
# include/BabyBehave/bdd.hpp -- the only file whose coverage matters for
# BabyBehave's coverage-ut / coverage-bbh targets (see root CMakeLists.txt).
#
# This script only relies on plain gcov (no lcov/genhtml/gcovr).
#
# Expected variables (set via -D on the `cmake -P` command line):
#   COV_LABEL   - human-readable label for this report (e.g. "UT", "BBH")
#   COV_OUTFILE - path to write the full raw gcov output to
#   COV_WORKDIR - scratch directory gcov is invoked from (recreated on each
#                 run so stale .gcov files never leak between invocations)
#   COV_GCOV    - path to the gcov executable
#   COV_UNITS   - list of "name|objdir|objfile" triples, one per
#                 translation unit to measure individually. `objfile` is
#                 the compiled .o for that TU and `objdir` is the
#                 directory containing its matching .gcno/.gcda (normally
#                 the same directory the .o lives in).
#   COV_COMBINE - ON/OFF; when ON and COV_UNITS has more than one entry,
#                 also run one merged gcov invocation across all units so
#                 their bdd.hpp counts are combined into a single report.

foreach(_required COV_LABEL COV_OUTFILE COV_WORKDIR COV_GCOV COV_UNITS)
	if(NOT DEFINED ${_required})
		message(FATAL_ERROR "coverage_report.cmake: ${_required} must be set")
	endif()
endforeach()

file(REMOVE_RECURSE "${COV_WORKDIR}")
file(MAKE_DIRECTORY "${COV_WORKDIR}")
file(WRITE "${COV_OUTFILE}" "bdd.hpp coverage report: ${COV_LABEL}\n\n")

# Matches the two-line block gcov prints for bdd.hpp, e.g.:
#   File '/path/to/include/BabyBehave/bdd.hpp'
#   Lines executed:75.76% of 66
set(_bdd_re "File '[^\n]*bdd\\.hpp'\nLines executed:[^\n]*")

set(_all_objfiles "")
set(_summary_lines "")

foreach(_unit IN LISTS COV_UNITS)
	string(REPLACE "|" ";" _parts "${_unit}")
	list(GET _parts 0 _name)
	list(GET _parts 1 _objdir)
	list(GET _parts 2 _objfile)
	list(APPEND _all_objfiles "${_objfile}")

	set(_unit_workdir "${COV_WORKDIR}/${_name}")
	file(MAKE_DIRECTORY "${_unit_workdir}")

	execute_process(
		COMMAND "${COV_GCOV}" "${_objfile}" --object-directory "${_objdir}"
		WORKING_DIRECTORY "${_unit_workdir}"
		OUTPUT_VARIABLE _gcov_stdout
		ERROR_VARIABLE _gcov_stderr
		RESULT_VARIABLE _gcov_result
	)

	file(APPEND "${COV_OUTFILE}" "==== ${COV_LABEL} :: ${_name} ====\n${_gcov_stdout}${_gcov_stderr}\n")

	set(_match "")
	string(REGEX MATCH "${_bdd_re}" _match "${_gcov_stdout}")
	if(_match)
		string(REPLACE "\n" "  " _match_oneline "${_match}")
		list(APPEND _summary_lines "  [${_name}] ${_match_oneline}")
	else()
		list(APPEND _summary_lines "  [${_name}] (no bdd.hpp coverage found in gcov output -- see ${COV_OUTFILE})")
	endif()
endforeach()

list(LENGTH COV_UNITS _n_units)
if(COV_COMBINE AND _n_units GREATER 1)
	# NOTE: gcov's own multi-object invocation (`gcov objfile1 objfile2
	# ...`) does NOT correctly union per-line coverage of a shared
	# header across translation units that each only instantiate a
	# SUBSET of its templates -- bdd.hpp being header-only, each unit
	# above compiled its own private copy of whichever
	# templates/branches IT happens to use, and gcov's combine mode
	# inflates the total line count (observed: "of 1364" for 6 UT
	# units, "of 676" for 2 BBH units) instead of recognizing repeated
	# source lines across units. So instead of asking gcov to combine,
	# union the ALREADY-CORRECT per-unit bdd.hpp.gcov files written by
	# the per-unit loop above ourselves, line by line: a line counts as
	# covered if ANY unit executed it, uncovered only if every unit
	# that considers it executable left it unexecuted, and is skipped
	# entirely if every unit treats it as non-executable ("-").
	set(_covered_lines "")
	set(_uncovered_lines "")

	foreach(_unit IN LISTS COV_UNITS)
		string(REPLACE "|" ";" _parts "${_unit}")
		list(GET _parts 0 _name)
		set(_unit_gcov "${COV_WORKDIR}/${_name}/bdd.hpp.gcov")
		if(NOT EXISTS "${_unit_gcov}")
			continue()
		endif()
		file(STRINGS "${_unit_gcov}" _gcov_lines)
		foreach(_gline IN LISTS _gcov_lines)
			if(_gline MATCHES "^[ \t]*([^:]+):[ \t]*([0-9]+):(.*)$")
				set(_marker "${CMAKE_MATCH_1}")
				set(_lineno "${CMAKE_MATCH_2}")
				set(_srctext "${CMAKE_MATCH_3}")
				if(_lineno STREQUAL "0")
					continue()
				endif()
				list(FIND _covered_lines "${_lineno}" _already_covered)
				if(_already_covered GREATER_EQUAL 0)
					continue() # already known covered from an earlier unit
				endif()
				if(_marker STREQUAL "-")
					continue() # non-executable in this unit
				elseif(_marker STREQUAL "#####" OR _marker STREQUAL "=====")
					# gcov instruments a function's closing brace with its own
					# "reached end of function" counter, separate from the return
					# statement just above it -- a line that is only a brace
					# (optionally followed by ';', e.g. a struct/namespace close)
					# can never independently execute any logic of its own, so
					# treat it like gcov's own "-" non-executable marker instead
					# of a real coverage gap.
					string(STRIP "${_srctext}" _srctext_stripped)
					if(_srctext_stripped STREQUAL "}" OR _srctext_stripped STREQUAL "};")
						continue()
					endif()
					list(FIND _uncovered_lines "${_lineno}" _already_uncovered)
					if(_already_uncovered LESS 0)
						list(APPEND _uncovered_lines "${_lineno}")
					endif()
				else()
					# Numeric execution count, possibly with a trailing
					# '*' marking partial branch coverage on an
					# otherwise-executed line; any nonzero count means
					# covered.
					string(REGEX REPLACE "\\*$" "" _count "${_marker}")
					if(_count GREATER 0)
						list(APPEND _covered_lines "${_lineno}")
						list(REMOVE_ITEM _uncovered_lines "${_lineno}")
					endif()
				endif()
			endif()
		endforeach()
	endforeach()

	list(LENGTH _covered_lines _n_covered)
	list(LENGTH _uncovered_lines _n_uncovered)
	math(EXPR _n_total "${_n_covered} + ${_n_uncovered}")
	if(_n_total GREATER 0)
		# CMake math() is integer-only; compute the percentage to 2
		# decimal places via scaled integer math instead.
		math(EXPR _pct_x100 "(${_n_covered} * 10000) / ${_n_total}")
		math(EXPR _pct_int "${_pct_x100} / 100")
		math(EXPR _pct_frac "${_pct_x100} % 100")
		if(_pct_frac LESS 10)
			set(_pct_frac "0${_pct_frac}")
		endif()
		set(_combined_summary "File 'bdd.hpp' (true per-line union across ${_n_units} units)\nLines executed:${_pct_int}.${_pct_frac}% of ${_n_total}")
		file(APPEND "${COV_OUTFILE}" "==== ${COV_LABEL} :: combined (true per-line union) ====\n${_combined_summary}\n\n")
		if(_n_uncovered GREATER 0)
			list(SORT _uncovered_lines COMPARE NATURAL)
			string(REPLACE ";" ", " _uncovered_oneline "${_uncovered_lines}")
			file(APPEND "${COV_OUTFILE}" "Uncovered bdd.hpp lines (in every unit): ${_uncovered_oneline}\n\n")
		endif()
		list(APPEND _summary_lines "  [combined] ${_combined_summary}")
	endif()
endif()

message(STATUS "---------------------------------------------------------------")
message(STATUS "bdd.hpp coverage (${COV_LABEL}):")
foreach(_line IN LISTS _summary_lines)
	message(STATUS "${_line}")
endforeach()
message(STATUS "Full gcov output written to: ${COV_OUTFILE}")
message(STATUS "---------------------------------------------------------------")
