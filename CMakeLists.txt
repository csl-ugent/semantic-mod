set(LLVM_LINK_COMPONENTS support)

add_clang_executable(semantic-mod
  SemanticModification.cpp
  SemanticAnalyser.h
  SemanticAnalyser.cpp
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
