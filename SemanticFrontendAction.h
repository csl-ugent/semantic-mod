#ifndef _SEMANTIC_FRONTENDACTION
#define _SEMANTIC_FRONTENDACTION

#include "SemanticData.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"

#include <fstream>
#include <sstream>
#include <string>

template <typename VersionType, typename AnalyserType>
class AnalysisFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
    class AnalysisFrontendAction : public clang::ASTFrontendAction {
        class AnalysisASTConsumer : public clang::ASTConsumer {
            private:
                VersionType& version;
            public:
                explicit AnalysisASTConsumer(VersionType& version)
                    : version(version) { }

                void HandleTranslationUnit(clang::ASTContext &Context) {
                    AnalyserType visitor = AnalyserType(Context, version);
                    visitor.TraverseDecl(Context.getTranslationUnitDecl());
                }
        };

        protected:
        VersionType& version;
        public:
        explicit AnalysisFrontendAction(VersionType& version)
            : version(version) {}

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file) {
            return llvm::make_unique<AnalysisASTConsumer>(version);
        }
    };

protected:
    VersionType& version;

public:
    AnalysisFrontendActionFactory(VersionType& version)
        : version(version) {}

    // We create a new instance of the frontend action.
    clang::FrontendAction* create() {
        llvm::outs() << "Phase 1: analysis\n";
        return new AnalysisFrontendAction(version);
    }
};

template <typename VersionType, typename RewriterType>
class RewritingFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
    class RewritingFrontendAction : public clang::ASTFrontendAction {
        class RewritingASTConsumer : public clang::ASTConsumer {
            private:
                const Transformation& transformation;
                clang::Rewriter& rewriter;
            public:
                explicit RewritingASTConsumer(const Transformation& transformation, clang::Rewriter& rewriter)
                    : transformation(transformation), rewriter(rewriter) {}

                void HandleTranslationUnit(clang::ASTContext &Context) {
                    RewriterType visitor = RewriterType(Context, transformation, rewriter);
                    visitor.TraverseDecl(Context.getTranslationUnitDecl());
                }
        };

        private:
        VersionType& version;
        const Transformation& transformation;
        clang::Rewriter rewriter;
        int id;
        public:
        explicit RewritingFrontendAction(VersionType& version, const Transformation& transformation, int id)
            : version(version), transformation(transformation), id(id) {}

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
            s << version.outputPrefix << "v" << this->id;
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
                std::string fileName = fileNameStr.substr(fileNameStr.find(version.baseDirectory) + version.baseDirectory.length()); /* until the end automatically... */

                // Optionally create required subdirectories.
                {
                    size_t subDirectoryPos = fileName.find_last_of("/\\");
                    if (subDirectoryPos != std::string::npos) {
                        llvm::outs() << "Creating subdirectories..." << "\n";
                        std::string subdirectories = fileName.substr(0, fileName.find_last_of("/\\"));
                        llvm::outs() <<  "Extracted subdirectory path: " << subdirectories << "\n";
                        const int dir_err = system(("mkdir -p " + fullPath + "/" + subdirectories).c_str());
                        if (-1 == dir_err)
                        {
                            llvm::outs() << "Error creating directory!\n";
                            return;
                        }
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
            return llvm::make_unique<RewritingASTConsumer>(transformation, rewriter);
        }
    };

private:
    VersionType& version;
    const Transformation& transformation;
    int id;

public:
    RewritingFrontendActionFactory(VersionType& version, const Transformation& transformation, int id)
        : version(version), transformation(transformation), id(id) {}

    // We create a new instance of the frontend action.
    clang::FrontendAction* create() {
        llvm::outs() << "Phase 2: performing rewrite for version: " << id << " target name: " << transformation.target.getName() << "\n";
        return new RewritingFrontendAction(version, transformation, id);
    }
};

#endif
