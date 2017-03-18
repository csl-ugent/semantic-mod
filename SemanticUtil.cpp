#include "SemanticUtil.h"
#include <stdlib.h> // rand
#include <time.h> // time
#include <sstream>
#include <fstream>
#include <string>

int random_0_to_n(int n) {

    /* initialize random seed */
    srand (time(NULL));

    // return a number between 0 and n-1.
    return rand() % n;
}

int factorial(int n)
{
  return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}

void writeJSONToFile(std::string outputPath, int version, std::string fileName, Json::Value output) {

    // We construct the full output directory.
    std::stringstream s;
    s << outputPath << "v" << version;
    std::string fullPath = s.str();

    // We check if this directory exists, if it doesn't we will create it.
    const int dir_err = system(("mkdir -p " + fullPath).c_str());

    // Output stream.
    std::ofstream outputFile;

    // Write changes to the file.
    std::string outputPathFull = fullPath + "/" + fileName;

    // We open the file.
    outputFile.open(outputPathFull.c_str());

    // Writer used to write JSON to output file.
    Json::StyledWriter styledWriter;
    outputFile << styledWriter.write(output);

    // We close the file.
    outputFile.close();
}
