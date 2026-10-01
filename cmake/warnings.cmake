# Common settings for every target of this project: C++20 and warnings.
function(atpl_target_defaults target)
  target_compile_features(${target} PUBLIC cxx_std_20)

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
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
  endif()
endfunction()
