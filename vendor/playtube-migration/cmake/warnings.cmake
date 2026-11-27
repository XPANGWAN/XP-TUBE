# Strict warnings for Playtube's own targets (third-party code is built without them).
function(playtube_set_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
endfunction()
