add_contract(cnyg.token cnyg.token ${CMAKE_CURRENT_SOURCE_DIR}/src/cnyg.token.cpp)

target_include_directories(cnyg.token
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(cnyg.token
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/cnyg.token.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/cnyg.token.contracts.md @ONLY )

target_compile_options( cnyg.token PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
