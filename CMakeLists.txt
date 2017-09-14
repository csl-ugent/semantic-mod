set(LLVM_LINK_COMPONENTS support)
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")

add_clang_executable(semantic-mod
  SemanticModification.cpp
  Semantic.h
  Semantic.cpp
  FPReordering.h
  FPReordering.cpp
  StructReordering.h
  StructReordering.cpp
  SWCReordering.h
  SWCReordering.cpp
  SemanticData.h
  SemanticData.cpp
  SemanticUtil.h
  SemanticUtil.cpp
  jsoncpp.cpp
  json.h
  json-forwards.h
  )

target_link_libraries(semantic-mod
  clangTooling
  clangBasic
  clangASTMatchers
  )

install(TARGETS semantic-mod RUNTIME DESTINATION bin)
