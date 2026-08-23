#pragma once

#include "token.h"
#include "runtime.h"

namespace FCExprClass
{
	FCValue evaluate(FCExprAST* expression, struct FCEvaluationContext& context);
	FCValue evaluate(const FCExprAST* expression, struct FCEvaluationContext& context);

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
		std::vector<Frame> callStack;

		void pushFrame(const std::string& functionName);
		void popFrame();
		Frame& currentFrame();
	};
}
