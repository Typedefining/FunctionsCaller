#pragma once

#include <map>

#include "token.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

namespace FCExprClass
{
	llvm::Value* codegen(FCExprAST* expression, struct FCCodegenContext& context);
	llvm::Value* codegen(const FCExprAST* expression, struct FCCodegenContext& context);

	struct FCCodegenContext
	{
		llvm::LLVMContext llvmContext;
		llvm::IRBuilder<> builder;
		std::unique_ptr<llvm::Module> module;
		// 符号地址表：key 为语义期绑定的 VariableSymbol（共享持有，指针稳定）
		std::map<const VariableSymbol*, llvm::AllocaInst*> namedValues;
		std::map<const VariableSymbol*, llvm::GlobalVariable*> globalValues;
		std::unordered_map<std::string, FCFunctionAST*> definitions;
		llvm::Function* currentFunction = nullptr;
		CompiledProgram compiledProgram;

		explicit FCCodegenContext(const std::string& moduleName, const CompiledProgram& program);
		llvm::Type* getType(const std::string& typeName);
		llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function,
			const std::string& name, llvm::Type* type);
	};
}
