#include "scanner.h"

#include <iostream>
#include "llvm/IR/Verifier.h"

int main()
{
	FCScanner scanner;
	auto program = scanner.analysis(
		"def add(a:int b:int) a+b\n"
		"add(2,3);");
	if (!program)
	{
		std::cerr << "failed to parse function definition and call\n";
		return 1;
	}

	FCExprClass::FCEvaluationContext context;
	const auto result = program->evaluate(context);
	if (result.type != FCMarks::FCValueCategory::Integer || result.evaluteVal.intVal != 5)
	{
		std::cerr << "unexpected result from function call\n";
		return 2;
	}

	if (context.functions.findFunction("add") == nullptr)
	{
		std::cerr << "function was not indexed\n";
		return 3;
	}

	FCExprClass::FCCodegenContext codegenContext("FunctionCallASTTest");
	if (program->codegen(codegenContext) == nullptr)
	{
		std::cerr << "failed to generate LLVM IR\n";
		return 4;
	}
	if (llvm::verifyModule(*codegenContext.module, &llvm::errs()))
	{
		std::cerr << "generated LLVM module is invalid\n";
		return 5;
	}
	if (codegenContext.module->getFunction("add") == nullptr ||
		codegenContext.module->getFunction("__fc_main") == nullptr)
	{
		std::cerr << "expected functions are missing from LLVM module\n";
		return 6;
	}

	codegenContext.module->print(llvm::outs(), nullptr);

	FCScanner stringScanner;
	auto stringFunction = stringScanner.analysis(
		"def concat(a:string b:string) a+b");
	if (!stringFunction)
	{
		std::cerr << "failed to parse string function\n";
		return 7;
	}
	FCExprClass::FCCodegenContext stringCodegen("StringCodegenTest");
	if (stringFunction->codegen(stringCodegen) == nullptr ||
		llvm::verifyModule(*stringCodegen.module, &llvm::errs()) ||
		stringCodegen.module->getFunction("concat") == nullptr)
	{
		std::cerr << "failed to generate string concatenation IR\n";
		return 8;
	}
	stringCodegen.module->print(llvm::outs(), nullptr);

	return 0;
}
