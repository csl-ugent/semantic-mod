#ifndef _SEMANTICUTIL
#define _SEMANTICUTIL

#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"

#include "json.h"

#include <string>

// Method used to initialize the random seed.
void init_random(unsigned seed);

// Method used to choose a number between 0 and n - 1 ([0, n-1]).
int random_0_to_n(int n);

// General utility functions.
std::string location2str(const clang::SourceRange& range, const clang::ASTContext& astContext);

// Method used to calculate the factorial of some given number.
unsigned long factorial(unsigned long n);

// Method used to calculate the entropy of M equiprobable choices.
double entropyEquiprobable(long m);

// Method used to write JSON to a give file.
void writeJSONToFile(std::string outputPath, int version, std::string fileName, Json::Value output);

#endif
