#pragma once

#include "token.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

namespace FCExprClass
{
	struct FCCodegenContext
	{
		llvm::LLVMContext llvmContext;
		llvm::IRBuilder<> builder;
		std::unique_ptr<llvm::Module> module;
		std::map<const VarDecl*, llvm::AllocaInst*> namedValues;
		std::unordered_map<std::string, FCFunctionAST*> definitions;
		llvm::Function* currentFunction = nullptr;

		explicit FCCodegenContext(const std::string& moduleName);
		llvm::Type* getType(const std::string& typeName);
		llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function,
			const std::string& name, llvm::Type* type);
	};
}
