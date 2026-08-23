#pragma once

#include "token.h"

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
		std::vector<Frame> callStack;

		void pushFrame(const std::string& functionName);
		void popFrame();
		Frame& currentFrame();
	};
}
