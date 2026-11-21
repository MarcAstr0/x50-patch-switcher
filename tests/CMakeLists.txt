# x50_add_test(name sources...): headless Qt Test linked against all project libs.
function(x50_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE x50ui x50midi x50core Qt6::Test Qt6::Widgets)
  x50_target_options(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

x50_add_test(test_scaffold test_scaffold.cpp)
x50_add_test(test_factory_table test_factory_table.cpp)
target_compile_definitions(test_factory_table PRIVATE
  X50_FACTORY_TSV="${CMAKE_SOURCE_DIR}/data/x50-factory.tsv")
x50_add_test(test_sound_address test_sound_address.cpp)
target_compile_definitions(test_sound_address PRIVATE
  X50_FACTORY_TSV="${CMAKE_SOURCE_DIR}/data/x50-factory.tsv")
x50_add_test(test_catalogue test_catalogue.cpp)
target_compile_definitions(test_catalogue PRIVATE
  X50_FACTORY_TSV="${CMAKE_SOURCE_DIR}/data/x50-factory.tsv")
x50_add_test(test_search_query test_search_query.cpp)
target_compile_definitions(test_search_query PRIVATE
  X50_FACTORY_TSV="${CMAKE_SOURCE_DIR}/data/x50-factory.tsv")
