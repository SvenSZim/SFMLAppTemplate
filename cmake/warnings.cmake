# Common settings for every target of this project: C++20, warnings and optional sanitizers.

set(ATPL_SANITIZE "" CACHE STRING
  "Sanitizers for the project's own code, for example 'address,undefined' or 'thread' (GCC and Clang only)")

function(atpl_target_defaults target)
  target_compile_features(${target} PUBLIC cxx_std_20)

  if(MSVC)
    # /utf-8: read sources as UTF-8 instead of the system's code page.
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    if(ATPL_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wnon-virtual-dtor
      -Woverloaded-virtual
    )
    if(ATPL_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(ATPL_SANITIZE)
      # A finding stops the program with an error, so a test that hits one fails.
      target_compile_options(${target} PRIVATE
        -fsanitize=${ATPL_SANITIZE}
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
      )
      target_link_options(${target} PUBLIC -fsanitize=${ATPL_SANITIZE})
    endif()
  endif()
endfunction()
