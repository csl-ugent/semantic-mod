#include "SemanticUtil.h"
#include <stdlib.h> // rand
#include <time.h> // time
#include <sstream>
#include <fstream>
#include <string>
#include <math.h>

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Tooling/Tooling.h"

using namespace llvm;


void init_random() {
    /* initialize random seed */
    srand(time(NULL));
}

int random_0_to_n(int n) {

    // return a number between 0 and n-1.
    return rand() % n;
}

unsigned long factorial(unsigned long n)
{
  return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}

double entropyEquiprobable(long m) {
    return log2(m);
}

void writeJSONToFile(std::string outputPath, int version, std::string fileName, Json::Value output) {

    // We construct the full output directory.
    std::string fullPath;
    if (version != -1) {
        std::stringstream s;
        s << outputPath << "v" << version;
        fullPath = s.str();
    } else {
        fullPath = outputPath;
    }

    // We check if this directory exists, if it doesn't we will create it.
    const int dir_err = system(("mkdir -p " + fullPath).c_str());

    // Output stream.
    std::ofstream outputFile;

    // Write changes to the file.
    std::string outputPathFull = fullPath + "/" + fileName;

    llvm::outs() << "Output path: " << outputPathFull << "\n";

    // We open the file.
    outputFile.open(outputPathFull.c_str());

    // Writer used to write JSON to output file.
    Json::StyledWriter styledWriter;
    outputFile << styledWriter.write(output);

    // We close the file.
    outputFile.close();
}
