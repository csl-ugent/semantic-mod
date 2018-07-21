#ifndef _SEMANTIC_VISITORS
#define _SEMANTIC_VISITORS

#include "SemanticData.h"

#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"

template <typename TargetType>
class SemanticAnalyser {
    protected:
        clang::ASTContext& astContext; // Used for getting additional AST info.
        const MetaData& metadata;
        Candidates<TargetType>& candidates;

        SemanticAnalyser(clang::ASTContext& Context, const MetaData& metadata, Candidates<TargetType>& candidates)
            : astContext(Context), metadata(metadata), candidates(candidates) { }
};

class SemanticRewriter {
    protected:
        clang::ASTContext& astContext; // Used for getting additional AST info.
        clang::Rewriter& rewriter;

        explicit SemanticRewriter(clang::ASTContext& Context, clang::Rewriter& rewriter)
            : astContext(Context), rewriter(rewriter)
        {
            rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
        }
};

#endif
