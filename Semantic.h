#ifndef _SEMANTIC
#define _SEMANTIC

#include "SemanticUtil.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"

#include "json.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

class Reordering {
  protected:
    virtual ~Reordering() {}
  public:
    std::string baseDirectory;
    std::string outputPrefix;
    Reordering(const std::string& bd, const std::string& od) : baseDirectory(bd), outputPrefix(od + "version_") {}
};

template <typename ReorderingType, typename AnalyserType>
class AnalysisFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
    class AnalysisFrontendAction : public clang::ASTFrontendAction {
        class AnalysisASTConsumer : public clang::ASTConsumer {
            private:
                ReorderingType& reordering;
            public:
                explicit AnalysisASTConsumer(ReorderingType& reordering)
                    : reordering(reordering) { }

                void HandleTranslationUnit(clang::ASTContext &Context) {
                    AnalyserType visitor = AnalyserType(Context, reordering);
                    visitor.TraverseDecl(Context.getTranslationUnitDecl());
                }
        };

        protected:
        ReorderingType& reordering;
        public:
        explicit AnalysisFrontendAction(ReorderingType& reordering)
            : reordering(reordering) {}

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file) {
            return llvm::make_unique<AnalysisASTConsumer>(reordering);
        }
    };

protected:
    ReorderingType& reordering;

public:
    AnalysisFrontendActionFactory(ReorderingType& reordering)
        : reordering(reordering) {}

    // We create a new instance of the frontend action.
    clang::FrontendAction* create() {
        llvm::outs() << "Phase 1: analysis\n";
        return new AnalysisFrontendAction(reordering);
    }
};

template <typename ReorderingType, typename RewriterType>
class RewritingFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
    class RewritingFrontendAction : public clang::ASTFrontendAction {
        class RewritingASTConsumer : public clang::ASTConsumer {
            private:
                ReorderingType& reordering;
                clang::Rewriter& rewriter;
            public:
                explicit RewritingASTConsumer(ReorderingType& reordering, clang::Rewriter& rewriter)
                    : reordering(reordering), rewriter(rewriter) {}

                void HandleTranslationUnit(clang::ASTContext &Context) {
                    RewriterType visitor = RewriterType(Context, reordering, rewriter);
                    visitor.TraverseDecl(Context.getTranslationUnitDecl());
                }
        };

        private:
        ReorderingType& reordering;
        clang::Rewriter rewriter;
        int version;
        public:
        explicit RewritingFrontendAction(ReorderingType& reordering, int version)
            : reordering(reordering), version(version) {}

        void EndSourceFileAction() {
            // We obtain the filename.
            std::string fileName = std::string(this->getCurrentFile().data());

            // Whenever we are NOT doing analysis we should write out the changes.
            if (rewriter.buffer_begin() != rewriter.buffer_end()) {
                writeChangesToOutput();

                // We need to clear the rewriter's modifications.
                rewriter.undoChanges();
            }
        }

        void writeChangesToOutput() {
            // We construct the full output directory.
            std::stringstream s;
            s << reordering.outputPrefix << "v" << this->version;
            std::string fullPath = s.str();

            // Debug
            llvm::outs() << "Full path: " << fullPath << "\n";

            // We check if this directory exists, if it doesn't we will create it.
            const int dir_err = system(("mkdir -p " + fullPath).c_str());
            if (-1 == dir_err)
            {
                llvm::outs() << "Error creating directory!\n";
                return;
            }
            // Output stream.
            std::ofstream outputFile;

            // We write the results to a new location.
            for (clang::Rewriter::buffer_iterator I = rewriter.buffer_begin(), E = rewriter.buffer_end(); I != E; ++I) {

                llvm::StringRef fileNameRef = rewriter.getSourceMgr().getFileEntryForID(I->first)->getName();
                std::string fileNameStr = std::string(fileNameRef.data());
                llvm::outs() << "Obtained filename: " << fileNameStr << "\n";
                std::string fileName = fileNameStr.substr(fileNameStr.find(reordering.baseDirectory) + reordering.baseDirectory.length()); /* until the end automatically... */

                // Optionally create required subdirectories.
                {
                    size_t subDirectoryPos = fileName.find_last_of("/\\");
                    if (subDirectoryPos != std::string::npos) {
                        llvm::outs() << "Creating subdirectories..." << "\n";
                        std::string subdirectories = fileName.substr(0, fileName.find_last_of("/\\"));
                        llvm::outs() <<  "Extracted subdirectory path: " << subdirectories << "\n";
                        system(("mkdir -p " + fullPath + "/" + subdirectories).c_str());
                    }
                }

                // Write changes to the file.
                std::string outputPath = fullPath + "/" + fileName;

                std::string output = std::string(I->second.begin(), I->second.end());
                outputFile.open(outputPath.c_str());
                llvm::StringRef MB = rewriter.getSourceMgr().getBufferData(I->first);
                std::string content = std::string(MB.data());
                outputFile.write(output.c_str(), output.length());
                outputFile.close();
            }
        }

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file) {
            return llvm::make_unique<RewritingASTConsumer>(reordering, rewriter);
        }
    };

private:
    ReorderingType& reordering;
    int version;

public:
    RewritingFrontendActionFactory(ReorderingType& reordering, int version)
        : reordering(reordering), version(version) {}

    // We create a new instance of the frontend action.
    clang::FrontendAction* create() {
        llvm::outs() << "Phase 2: performing rewrite for version: " << version << " target name: " << reordering.transformation->target.getName() << "\n";
        return new RewritingFrontendAction(reordering, version);
    }
};

// Method used for the function parameter semantic transformation.
template <typename ReorderingType, typename AnalyserType, typename RewriterType, typename TargetUnique, typename Transformation>
void reorder(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings) {
    // We run the analysis phase and get the valid candidates
    ReorderingType reordering(baseDirectory, outputDirectory);
    Tool->run(new AnalysisFrontendActionFactory<ReorderingType, AnalyserType>(reordering));
    std::vector<TargetUnique> candidates;
    for (const auto& it : reordering.candidates) {
        if (it.second.valid)
        {
            llvm::outs() << "Valid candidate: " << it.first.getName() << "\n";
            candidates.push_back(it.first);
        }
    }

    // We need to determine the maximum amount of reorderings that is actually
    // possible with the given input source files (based on the analysis phase).
    std::map<unsigned, unsigned> histogram;
    unsigned long totalReorderings = 0;
    double avgItems = 0;
    for (const auto& candidate : candidates) {
        unsigned nrOfItems = reordering.candidates[candidate].nrOfItems();

        // Keep count of the total possible reorderings and the average number of items
        totalReorderings += factorial(nrOfItems);
        avgItems += nrOfItems;

        // Look if this amount has already occured or not.
        if (histogram.find(nrOfItems) != histogram.end()) {
            histogram[nrOfItems]++;
        } else {
            histogram[nrOfItems] = 1;
        }
    }

    // Create analytics
    Json::Value analytics;
    analytics["number_of_candidates"] = candidates.size();
    analytics["avg_items"] = avgItems / candidates.size();
    analytics["entropy"] = entropyEquiprobable(totalReorderings);

    // Add histogram.
    for (const auto& it : histogram) {
        std::stringstream ss;
        ss << it.first;
        analytics["histogram"][ss.str()] = it.second;
    }

    // Output analytics
    llvm::outs() << "Total number of reorderings possible with " << candidates.size() << " candidates is: " << totalReorderings << "\n";
    llvm::outs() << "Writing analytics output...\n";
    writeJSONToFile(outputDirectory, -1, "analytics.json", analytics);

    unsigned long amountChosen = 0;
    unsigned long amount = std::min(numberOfReorderings, totalReorderings);
    std::vector<Transformation> transformations;
    llvm::outs() << "Amount of reorderings is set to: " << amount << "\n";
    while (amountChosen < amount)
    {
        // We choose a candidate at random.
        const auto& chosen = candidates[random_0_to_n(candidates.size())];

        // Create original ordering
        std::vector<unsigned> ordering(reordering.candidates[chosen].nrOfItems());
        std::iota(ordering.begin(), ordering.end(), 0);

        // Things we should write to an output file.
        Json::Value output;
        output["target_name"] = chosen.getName();
        output["file_name"] = chosen.getFileName();
        output["original"]["items"] = reordering.candidates[chosen].getJSON(ordering);// We output the original order.

        // Make sure the modified ordering isn't the same as the original
        std::vector<unsigned> original_ordering = ordering;
        do {
            std::random_shuffle(ordering.begin(), ordering.end());
        } while (original_ordering == ordering);

        // Check if this transformation isn't duplicate. If it is, we try again
        bool found = false;
        for (const auto& t : transformations) {
            if (t.target == chosen && t.ordering == ordering) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        // Debug information.
        llvm::outs() << "Chosen target: " << chosen.getName() << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        for (auto it : ordering) {
            llvm::outs() << it << " ";
        }
        llvm::outs() << "\n";

        output["modified"]["items"] = reordering.candidates[chosen].getJSON(ordering);// We output the modified order.

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(reordering.outputPrefix, amountChosen+1, "transformations.json", output);

        // Add the transformation
        transformations.emplace_back(chosen, ordering);
        amountChosen++;
    }

    // We perform the rewrite operations.
    for (unsigned long iii = 0; iii < amount; iii++)
    {
        // We select the current transformation
        reordering.transformation = &transformations[iii];

        Tool->run(new RewritingFrontendActionFactory<ReorderingType, RewriterType>(reordering, iii + 1));
    }
}

#endif
