# CopyRuntimeDlls.cmake — walk the import table of TARGET_FILE via objdump -p
# and copy every DLL resolvable in SEARCH_DIRS into DEST_DIR, recursing into
# copied DLLs until the closure is complete. DLLs only present in system
# directories (kernel32, api-ms-* redirects, ...) are naturally skipped
# because they never appear in SEARCH_DIRS.
#
# Usage (script mode):
#   cmake -DTARGET_FILE=<exe> -DDEST_DIR=<dir> -DSEARCH_DIRS="d1;d2" \
#         -DOBJDUMP=<objdump> [-DSEED_DLLS="a.dll;b.dll"] -P CopyRuntimeDlls.cmake
#
# SEED_DLLS covers libraries loaded at runtime (SDL2_image is dlopen'ed by the
# app DLLs, so the exe's import table never mentions it).
#
# Rationale: a fresh CMake configure does not populate the build dir with the
# MinGW/SDL runtime DLLs, so the exe fails to start (exit 127) unless the
# MSYS bin dirs are on PATH. This keeps build/ self-contained after every
# relink.

set(_queue "${TARGET_FILE}")
set(_seen "${TARGET_FILE}")

# CMake joins list-valued -D arguments with spaces on the generated command
# line; restore them into proper lists here.
string(REPLACE " " ";" SEED_DLLS "${SEED_DLLS}")
string(REPLACE " " ";" SEARCH_DIRS "${SEARCH_DIRS}")

foreach(_seed ${SEED_DLLS})
    foreach(_dir ${SEARCH_DIRS})
        if(EXISTS "${_dir}/${_seed}")
            list(APPEND _queue "${_dir}/${_seed}")
            break()
        endif()
    endforeach()
endforeach()

while(_queue)
    list(POP_FRONT _queue _file)
    execute_process(COMMAND "${OBJDUMP}" -p "${_file}"
                    OUTPUT_VARIABLE _imports
                    ERROR_QUIET RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        continue()
    endif()
    # Import entries look like: "        DLL Name: SDL2.dll"
    string(REGEX MATCHALL "DLL Name: [A-Za-z0-9_.+-]+" _names "${_imports}")
    foreach(_match ${_names})
        string(SUBSTRING "${_match}" 10 -1 _name)   # strip "DLL Name: "
        string(TOLOWER "${_name}" _key)
        if(_key IN_LIST _seen)
            continue()
        endif()
        list(APPEND _seen "${_key}")
        set(_found "")
        foreach(_dir ${SEARCH_DIRS})
            if(EXISTS "${_dir}/${_name}")
                set(_found "${_dir}/${_name}")
                break()
            endif()
        endforeach()
        if(_found AND NOT EXISTS "${DEST_DIR}/${_name}")
            file(COPY "${_found}" DESTINATION "${DEST_DIR}")
            message(STATUS "CopyRuntimeDlls: ${_name}")
            list(APPEND _queue "${_found}")
        endif()
    endforeach()
endwhile()