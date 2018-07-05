set(LLVM_LINK_COMPONENTS support)
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")

add_clang_executable(semantic-mod
  SemanticModification.cpp
  FunctionRewriting.cpp
  StructRewriting.cpp
  SemanticUtil.cpp
  jsoncpp.cpp
  )

target_link_libraries(semantic-mod
  clangTooling
  clangBasic
  clangASTMatchers
  )

# Generate a compilation database
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

install(TARGETS semantic-mod RUNTIME DESTINATION bin)
