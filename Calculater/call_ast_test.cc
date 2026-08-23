#include "scanner.h"
#include "evaluator.h"
#include "codegen.h"

#include <iostream>
#include "llvm/IR/Verifier.h"

int main()
{
	FCScanner scanner;
	auto program = scanner.analysis(
		"def add(a:int, b:int) a+b\n"
		"add(2,3);");
	if (!program)
	{
		std::cerr << "failed to parse function definition and call\n";
		return 1;
	}

	FCExprClass::FCEvaluationContext context;
	const auto result = FCExprClass::evaluate(program.get(), context);
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
	if (FCExprClass::codegen(program.get(), codegenContext) == nullptr)
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
		"def concat(a:string, b:string) a+b");
	if (!stringFunction)
	{
		std::cerr << "failed to parse string function\n";
		return 7;
	}
	FCExprClass::FCCodegenContext stringCodegen("StringCodegenTest");
	if (FCExprClass::codegen(stringFunction.get(), stringCodegen) == nullptr ||
		llvm::verifyModule(*stringCodegen.module, &llvm::errs()) ||
		stringCodegen.module->getFunction("concat") == nullptr)
	{
		std::cerr << "failed to generate string concatenation IR\n";
		return 8;
	}
	stringCodegen.module->print(llvm::outs(), nullptr);

	FCScanner standaloneGlobalScanner;
	auto standaloneGlobal = standaloneGlobalScanner.analysis("var only:int = 7");
	FCExprClass::FCEvaluationContext standaloneContext;
	const auto standaloneResult = standaloneGlobal == nullptr
		? FCMarks::FCValue{}
		: FCExprClass::evaluate(standaloneGlobal.get(), standaloneContext);
	FCExprClass::FCCodegenContext standaloneCodegen("StandaloneGlobalTest");
	if (standaloneGlobal == nullptr ||
		standaloneResult.type != FCMarks::FCValueCategory::Integer ||
		standaloneResult.evaluteVal.intVal != 7 ||
		FCExprClass::codegen(standaloneGlobal.get(), standaloneCodegen) == nullptr ||
		llvm::verifyModule(*standaloneCodegen.module, &llvm::errs()) ||
		standaloneCodegen.module->getGlobalVariable("only", true) == nullptr ||
		standaloneCodegen.module->getFunction("__fc_main") == nullptr)
	{
		std::cerr << "standalone top-level variable failed\n";
		return 13;
	}

	FCScanner globalScanner;
	auto globalProgram = globalScanner.analysis(
		"var counter:int = 10;"
		"def read() counter;"
		"def increment() counter = counter + 1");
	if (!globalProgram || globalScanner.semanticContext().globalDeclarations().size() != 1)
	{
		std::cerr << "failed to parse top-level variable\n";
		return 14;
	}
	FCExprClass::FCEvaluationContext globalContext;
	FCExprClass::evaluate(globalProgram.get(), globalContext);
	FCExprClass::FCCallExprAST incrementCall("increment", {});
	FCExprClass::evaluate(&incrementCall, globalContext);
	FCExprClass::FCCallExprAST readCall("read", {});
	const auto globalResult = FCExprClass::evaluate(&readCall, globalContext);
	if (globalResult.type != FCMarks::FCValueCategory::Integer ||
		globalResult.evaluteVal.intVal != 11)
	{
		std::cerr << "top-level variable evaluation failed\n";
		return 15;
	}
	FCExprClass::FCCodegenContext globalCodegen("GlobalVariableTest");
	const auto globalCodegenResult = FCExprClass::codegen(globalProgram.get(), globalCodegen);
	const auto globalModuleInvalid = llvm::verifyModule(*globalCodegen.module, &llvm::errs());
	if (globalCodegenResult == nullptr || globalModuleInvalid ||
		globalCodegen.module->getGlobalVariable("counter", true) == nullptr ||
		globalCodegen.module->getFunction("read") == nullptr ||
		globalCodegen.module->getFunction("increment") == nullptr ||
		globalCodegen.module->getFunction("__fc_main") == nullptr)
	{
		std::cerr << "top-level variable LLVM codegen failed\n";
		return 16;
	}

	FCScanner shadowScanner;
	auto shadowProgram = shadowScanner.analysis(
		"var value:int = 10;"
		"def local() var value:int = 3; value");
	FCExprClass::FCEvaluationContext shadowContext;
	if (shadowProgram != nullptr)
		FCExprClass::evaluate(shadowProgram.get(), shadowContext);
	FCExprClass::FCCallExprAST localCall("local", {});
	const auto shadowResult = shadowProgram == nullptr
		? FCMarks::FCValue{}
		: FCExprClass::evaluate(&localCall, shadowContext);
	if (shadowProgram == nullptr || shadowResult.type != FCMarks::FCValueCategory::Integer ||
		shadowResult.evaluteVal.intVal != 3)
	{
		std::cerr << "local variable did not shadow global variable\n";
		return 17;
	}

	FCScanner reusableScanner;
	if (!reusableScanner.analysis("def first(a:int) a"))
	{
		std::cerr << "failed to parse first reusable scanner input\n";
		return 9;
	}
	if (reusableScanner.semanticContext().functionDeclarations("first").empty())
	{
		std::cerr << "semantic context did not record declarations\n";
		return 10;
	}
	if (!reusableScanner.analysis("def second(b:int) b"))
	{
		std::cerr << "failed to parse second reusable scanner input\n";
		return 11;
	}
	if (!reusableScanner.semanticContext().functionDeclarations("first").empty() ||
		reusableScanner.semanticContext().functionDeclarations("second").empty())
	{
		std::cerr << "semantic context was not reset between inputs\n";
		return 12;
	}

	return 0;
}
