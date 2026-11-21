# x50_add_test(name sources...): headless Qt Test linked against all project libs.
function(x50_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE x50ui x50midi x50core Qt6::Test Qt6::Widgets)
  x50_target_options(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

x50_add_test(test_scaffold test_scaffold.cpp)
