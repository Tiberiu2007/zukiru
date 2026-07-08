# engine/ — the engine proper: a collection of layered library modules.
#
# No application entry point lives here (see agents/PROJECT_STRUCTURE.md §4).
# Each subdirectory of modules/ that has a CMakeLists.txt is an independent
# `zuki::<module>` target; they are auto-discovered so adding a module is
# just creating its folder + CMakeLists.txt (no edit here needed).

file(GLOB _module_lists CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/modules/*/CMakeLists.txt")

if(NOT _module_lists)
  message(STATUS "Zuki: no engine modules yet (Milestone 1 will add them)")
endif()

foreach(_list ${_module_lists})
  get_filename_component(_dir "${_list}" DIRECTORY)
  add_subdirectory("${_dir}")
endforeach()
