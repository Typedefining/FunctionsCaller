#pragma once

#include <map>

#include "token.h"

// 头文件只包含自身直接使用的 LLVM 组件；
// BasicBlock/Constants/DerivedTypes/Verifier 由 codegen.cc 按需包含
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "semantic.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

namespace FCExprClass
{
	llvm::Value* codegen(FCExprAST* expression, struct FCCodegenContext& context);
	llvm::Value* codegen(const FCExprAST* expression, struct FCCodegenContext& context);

	// 程序级入口：函数/原型/程序包装按原有语义生成，其余顶层表达式
	// 统一包进 __fc_main，使每个程序都有入口函数。
	llvm::Value* codegenTopLevel(const FCExprAST* expression, struct FCCodegenContext& context);

	struct FCCodegenContext
	{
		llvm::LLVMContext llvmContext;
		llvm::IRBuilder<> builder;
		std::unique_ptr<llvm::Module> module;
		std::map<std::shared_ptr<VariableSymbol>, llvm::AllocaInst*> namedValues;
		std::map<std::shared_ptr<VariableSymbol>, llvm::GlobalVariable*> globalValues;
		std::unordered_map<std::string, FCFunctionAST*> definitions;
		llvm::Function* currentFunction = nullptr;
		CompiledProgram compiledProgram;
		const FCSemanticContext* semantic = nullptr;
		FCMarks::SemanticBinding localBinding;

		std::shared_ptr<VariableSymbol> boundSymbol(const void* astNode) const
		{
			if (semantic != nullptr)
				return semantic->boundSymbol(astNode);
			return localBinding.find(astNode);
		}

		// 本地侧表访问（手工绑定场景）
		FCMarks::SemanticBinding& semanticBinding() { return localBinding; }

		explicit FCCodegenContext(const std::string& moduleName, const CompiledProgram& program);
		llvm::Type* getType(const std::string& typeName);
		llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function,
			const std::string& name, llvm::Type* type);
	};
}
