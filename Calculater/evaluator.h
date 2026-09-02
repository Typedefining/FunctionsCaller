#pragma once

#include "token.h"
#include "runtime.h"
#include "semantic.h"

namespace FCExprClass
{
	class FCFunctionRegistry
	{
	public:
		bool registerFunction(FCFunctionAST* function);
		FCFunctionAST* findFunction(const std::string& name) const;
		bool index(FCExprAST* root);
		void clear();

	private:
		std::unordered_map<std::string, FCFunctionAST*> m_functions;
	};

	struct FCEvaluationContext
	{
		FCFunctionRegistry functions;
		Frame globalFrame{"<global>", {}};
		std::vector<Frame> callStack;
		CompiledProgram compiledProgram;
		FCMarks::SemanticBinding localBinding;

		std::shared_ptr<VariableSymbol> boundSymbol(const void* astNode) const
		{
			if (compiledProgram.m_binding->size() != 0)
				return compiledProgram.m_binding->find(astNode);
			return localBinding.find(astNode);
		}

		bool bindNode(const void* astNode, const std::string& name)
		{
			auto shared = compiledProgram.allSymbols.lookupShared(name);
			if (shared == nullptr)
				return false;
			return localBinding.bind(astNode, std::move(shared));
		}

		FCEvaluationContext(const CompiledProgram& program);

		void pushFrame(const std::string& functionName);
		void popFrame();
		Frame& currentFrame();
	};

	FCValue evaluate(FCExprAST* expression, struct FCEvaluationContext& context);
	FCValue evaluate(const FCExprAST* expression, struct FCEvaluationContext& context);
}
