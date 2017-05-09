#ifndef _SEMANTICUTIL
#define _SEMANTICUTIL

#include "json.h"

// Method used to initialize the random seed.
void init_random();

// Method used to choose a number between 0 and n - 1 ([0, n-1]).
int random_0_to_n(int n);

// Method used to calculate the factorial of some given number.
int factorial(int n);

// Method used to calculate the entropy of M equiprobable choices.
double entropyEquiprobable(int m);

// Method used to write JSON to a give file.
void writeJSONToFile(std::string outputPath, int version, std::string fileName, Json::Value output);

#endif
