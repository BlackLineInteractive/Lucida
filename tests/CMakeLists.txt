# Tests that need neither a window nor a GPU, so CI can run them anywhere.

add_executable(lucida_tests EditorTests.cpp)
target_link_libraries(lucida_tests PRIVATE lucida::framework lucida::core)

add_test(NAME editor COMMAND lucida_tests)
