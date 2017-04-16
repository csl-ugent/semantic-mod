set(LLVM_LINK_COMPONENTS support)

add_clang_executable(semantic-mod
  SemanticModification.cpp
  Semantic.h
  Semantic.cpp
  StructReordering.h
  StructReordering.cpp
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
