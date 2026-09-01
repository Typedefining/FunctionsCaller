#pragma once

#include "token.h"
#include "runtime.h"
#include "semantic.h"
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
		// 语义绑定侧表：正常流程引用 scanner 的 FCSemanticContext（不拷贝）；
		// 手工构造场景（无 scanner）落到本地表。生命周期约定：AST 与绑定源同时存活。
		const FCSemanticContext* semantic = nullptr;
		FCMarks::SemanticBinding localBinding;

		// 绑定符号查询：优先语义上下文侧表，其次本地表；均未绑定返回 nullptr
		const FCMarks::VariableSymbol* boundSymbol(const void* astNode) const
		{
			if (semantic != nullptr)
				return semantic->boundSymbol(astNode);
			return localBinding.find(astNode);
		}

		// 手工绑定（测试/嵌入场景）：node 绑定到 program 中已登记的符号
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
