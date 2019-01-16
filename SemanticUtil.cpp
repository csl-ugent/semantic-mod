#include "SemanticUtil.h"

#include "clang/Lex/Lexer.h"
#include "llvm/Support/raw_ostream.h"

#include <cmath>
#include <cstdlib> // rand
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>

// Obtain source information corresponding to a statement.
std::string location2str(const clang::SourceRange& range, const clang::ASTContext& astContext) {
    const clang::SourceManager& sm = astContext.getSourceManager();
    clang::SourceLocation end = clang::Lexer::getLocForEndOfToken(range.getEnd(), 0, sm, astContext.getLangOpts());
    const char* Start = sm.getCharacterData(range.getBegin());
    const char* End = sm.getCharacterData(end);
    return std::string(Start, End - Start);
}

static std::mt19937 generator;

void init_random(unsigned seed) {
    generator.seed(seed);
}

unsigned random_0_to_n(const unsigned n) {
    std::uniform_int_distribution<unsigned> distribution(0, n);

    return distribution(generator);
}

const std::vector<unsigned> generate_random_ordering(unsigned nrOfElements)
{
    // Create original ordering
    std::vector<unsigned> ordering(nrOfElements);
    std::iota(ordering.begin(), ordering.end(), 0);

    // Make sure the modified ordering isn't the same as the original
    std::vector<unsigned> original_ordering = ordering;
    do {
        std::shuffle(ordering.begin(), ordering.end(), generator);
    } while (original_ordering == ordering);

    return ordering;
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
