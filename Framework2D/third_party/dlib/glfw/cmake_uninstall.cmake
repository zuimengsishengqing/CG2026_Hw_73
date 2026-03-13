
if (NOT EXISTS "F:/CG2026/homework in winter/USTC_CG_26/Framework2D/third_party/dlib/glfw/install_manifest.txt")
    message(FATAL_ERROR "Cannot find install manifest: \"F:/CG2026/homework in winter/USTC_CG_26/Framework2D/third_party/dlib/glfw/install_manifest.txt\"")
endif()

file(READ "F:/CG2026/homework in winter/USTC_CG_26/Framework2D/third_party/dlib/glfw/install_manifest.txt" files)
string(REGEX REPLACE "\n" ";" files "${files}")

foreach (file ${files})
  message(STATUS "Uninstalling \"$ENV{DESTDIR}${file}\"")
  if (EXISTS "$ENV{DESTDIR}${file}")
    exec_program("F:/CG2026/CMake/cmake-3.31.5-windows-x86_64/bin/cmake.exe" ARGS "-E remove \"$ENV{DESTDIR}${file}\""
                 OUTPUT_VARIABLE rm_out
                 RETURN_VALUE rm_retval)
    if (NOT "${rm_retval}" STREQUAL 0)
      MESSAGE(FATAL_ERROR "Problem when removing \"$ENV{DESTDIR}${file}\"")
    endif()
  elseif (IS_SYMLINK "$ENV{DESTDIR}${file}")
    EXEC_PROGRAM("F:/CG2026/CMake/cmake-3.31.5-windows-x86_64/bin/cmake.exe" ARGS "-E remove \"$ENV{DESTDIR}${file}\""
                 OUTPUT_VARIABLE rm_out
                 RETURN_VALUE rm_retval)
    if (NOT "${rm_retval}" STREQUAL 0)
      message(FATAL_ERROR "Problem when removing symlink \"$ENV{DESTDIR}${file}\"")
    endif()
  else()
    message(STATUS "File \"$ENV{DESTDIR}${file}\" does not exist.")
  endif()
endforeach()

