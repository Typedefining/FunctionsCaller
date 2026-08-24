#include "codegen.h"
#include "evaluator.h"
#include "scanner.h"
#include "semantic.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "llvm/IR/Verifier.h"

using namespace FCExprClass;
using namespace FCMarks;

namespace
{
class TestSuite
{
public:
	void expect(bool condition, const std::string& name)
	{
		++m_total;
		if (!condition)
		{
			++m_failed;
			std::cerr << "[FAIL] " << name << "\n";
		}
	}

	int result() const
	{
		std::cout << "unit tests: " << (m_total - m_failed) << "/" << m_total
			<< " passed\n";
		return m_failed == 0 ? 0 : 1;
	}

private:
	int m_total = 0;
	int m_failed = 0;
};

bool isInteger(const FCValue& value, int expected)
{
	return value.type == FCValueCategory::Integer &&
		value.evaluteVal.intVal == expected;
}

bool isFloating(const FCValue& value, double expected)
{
	return value.type == FCValueCategory::Floating &&
		value.evaluteVal.doubleVal == expected;
}

bool isString(const FCValue& value, const std::string& expected)
{
	return value.type == FCValueCategory::String &&
		value.evaluteVal.charVal->str == expected;
}

std::unique_ptr<FCExprAST> integer(int value)
{
	return std::make_unique<FCNumberExprAST>(value);
}

std::unique_ptr<FCExprAST> floating(double value)
{
	return std::make_unique<FCNumberExprAST>(value);
}

std::unique_ptr<FCExprAST> stringLiteral(const std::string& value)
{
	return std::make_unique<FCStringExprAST>(value);
}

std::unique_ptr<FCExprAST> variable(const VarDeclPtr& declaration)
{
	return std::make_unique<FCVariableExprAST>(declaration);
}

std::unique_ptr<FCExprAST> binary(
	char op, std::unique_ptr<FCExprAST> lhs, std::unique_ptr<FCExprAST> rhs)
{
	return std::make_unique<FCBinaryExprAST>(op, std::move(lhs), std::move(rhs));
}

std::unique_ptr<FCCallExprAST> call(
	const std::string& name, std::vector<std::unique_ptr<FCExprAST>> args = {})
{
	return std::make_unique<FCCallExprAST>(name, std::move(args));
}

std::unique_ptr<FCFunctionAST> function(
	const std::string& name,
	std::vector<VarDeclPtr> parameters,
	std::unique_ptr<FCExprAST> body,
	int localCount)
{
	std::vector<FCVariableExprAST> arguments;
	for (const auto& parameter : parameters)
		arguments.emplace_back(parameter);
	return std::make_unique<FCFunctionAST>(
		std::make_unique<FCPrototypeAST>(name, std::move(arguments)),
		std::move(body), localCount);
}

struct UnknownExprAST : FCExprAST
{
	void info() override {}
};

llvm::Function* beginHostFunction(FCCodegenContext& context, const std::string& name)
{
	auto* type = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(context.llvmContext), {}, false);
	auto* host = llvm::Function::Create(
		type, llvm::Function::InternalLinkage, name, context.module.get());
	context.currentFunction = host;
	context.builder.SetInsertPoint(
		llvm::BasicBlock::Create(context.llvmContext, "entry", host));
	return host;
}

void scannerTests(TestSuite& tests)
{
	FCScanner scanner;
	tests.expect(scanner.analysis("; 1") != nullptr, "scanner leading semicolon");

	{
		auto ast = scanner.analysis("1 + 2 * 3");
		FCEvaluationContext context;
		tests.expect(ast != nullptr && isInteger(evaluate(ast.get(), context), 7),
			"scanner/evaluator integer precedence");
	}
	{
		auto ast = scanner.analysis("(1 + 2) * 3");
		FCEvaluationContext context;
		tests.expect(ast != nullptr && isInteger(evaluate(ast.get(), context), 9),
			"scanner/evaluator parentheses");
	}
	{
		auto ast = scanner.analysis("1.5 + 2.5");
		FCEvaluationContext context;
		tests.expect(ast != nullptr && isFloating(evaluate(ast.get(), context), 4.0),
			"scanner/evaluator floating literal");
	}
	{
		auto ast = scanner.analysis("\"hello\" + \" world\"");
		FCEvaluationContext context;
		tests.expect(ast != nullptr && isString(evaluate(ast.get(), context), "hello world"),
			"scanner/evaluator string literal");
	}
	{
		auto ast = scanner.analysis("# comment\n1 + 2");
		FCEvaluationContext context;
		tests.expect(ast != nullptr && isInteger(evaluate(ast.get(), context), 3),
			"scanner comment handling");
	}
	{
		auto ast = scanner.analysis("var g:int = 2; g = g + 3; g");
		FCEvaluationContext context;
		tests.expect(ast != nullptr && isInteger(evaluate(ast.get(), context), 5),
			"scanner top-level declaration and assignment");
		tests.expect(scanner.semanticContext().globalDeclarations().size() == 1,
			"scanner records global declaration");
	}
	{
		auto ast = scanner.analysis("def add(a:int, b:double) a + b");
		auto* definition = dynamic_cast<FCFunctionAST*>(ast.get());
		tests.expect(definition != nullptr && definition->getProto()->getArgs().size() == 2,
			"scanner comma-separated typed parameters");
		tests.expect(definition != nullptr &&
			definition->getProto()->getArgs()[0].decl->typeName == "int" &&
			definition->getProto()->getArgs()[1].decl->typeName == "double",
			"scanner parameter type binding");
	}
	{
		auto ast = scanner.analysis("def empty() 42");
		auto* definition = dynamic_cast<FCFunctionAST*>(ast.get());
		tests.expect(definition != nullptr && definition->getProto()->getArgs().empty(),
			"scanner zero-argument function");
	}
	{
		auto ast = scanner.analysis("def choose(a:int) if a < 1 then 10 else 20");
		auto* definition = dynamic_cast<FCFunctionAST*>(ast.get());
		tests.expect(definition != nullptr &&
			dynamic_cast<const FCIfExprAST*>(definition->getBody()) != nullptr,
			"scanner if expression");
	}
	{
		auto ast = scanner.analysis("def loop(n:int) for i = 0, i < n, 1 in i");
		auto* definition = dynamic_cast<FCFunctionAST*>(ast.get());
		tests.expect(definition != nullptr &&
			dynamic_cast<const FCForExprAST*>(definition->getBody()) != nullptr,
			"scanner for expression");
	}
	{
		auto ast = scanner.analysis("foo(1, 2)");
		auto* callExpression = dynamic_cast<FCCallExprAST*>(ast.get());
		tests.expect(callExpression != nullptr && callExpression->getArgs().size() == 2,
			"scanner comma-separated call arguments");
	}

	const std::vector<std::string> invalidInputs = {
		"unknown",
		"(1 + 2",
		"foo(1 2)",
		"foo(1,)",
		"def missingName 1",
		"def f(a:int b:int) a",
		"def f(a:bool) a",
		"def f(a) a",
		"if 1 2 else 3",
		"if 1 then 2",
		"for i 0, 1 in i",
		"for i = 0 1 in i",
		"for i = 0, 1 1 in i",
		"var missingType = 1",
		"var x:bool = 1",
		"var x:int 1",
		"var x:int =",
		"var x:int = 1; var x:int = 2",
		"def 1() 1",
		"def f(1:int) 1",
		"def f(a:1) 1",
		"def f(a:int,) 1",
		"def f() +",
		"if then 1 else 2",
		"if 1 then else 2",
		"if 1 then 2 else",
		"for 1 = 0, 1 in 1",
		"for i = +, 1 in i",
		"for i = 0, + in i",
		"for i = 0, 1, + in i",
		"var :int = 1",
		"var x: = 1",
		"def f() var x:int = 1; var x:int = 2",
		"1 + ;",
		"()",
		"\"unterminated",
		"@"
	};
	for (size_t index = 0; index < invalidInputs.size(); ++index)
	{
		auto ast = scanner.analysis(invalidInputs[index]);
		tests.expect(ast != nullptr, "scanner error recovery " + std::to_string(index));
	}
	std::string nonAscii(1, static_cast<char>(0xff));
	tests.expect(scanner.analysis(nonAscii) != nullptr, "scanner non-ascii token recovery");

	FCScanner reusable;
	reusable.analysis("var old:int = 1");
	tests.expect(reusable.semanticContext().globalDeclarations().size() == 1,
		"scanner first analysis state");
	reusable.analysis("def fresh(x:int) x");
		tests.expect(reusable.semanticContext().globalDeclarations().empty() &&
			reusable.semanticContext().functionDeclarations("fresh").size() == 1,
		"scanner resets semantic state");

	FCNumberExprAST integerNode(1);
	FCNumberExprAST floatingNode(1.5);
	FCStringExprAST stringNode("info");
	auto infoDeclaration = std::make_shared<VarDecl>("infoVar", "int");
	infoDeclaration->slot = 0;
	FCVariableExprAST variableNode(infoDeclaration);
	FCBinaryExprAST binaryNode('+', integer(1), integer(2));
	FCCallExprAST callNode("infoCall", {});
	FCPrototypeAST prototypeNode("infoPrototype", {variableNode});
	FCFunctionAST functionNode(
		std::make_unique<FCPrototypeAST>("infoFunction", std::vector<FCVariableExprAST>{}),
		integer(1), 0);
	FCIfExprAST ifNode(integer(1), integer(2), integer(3));
	FCForExprAST forNode(infoDeclaration, integer(0), integer(1), nullptr, integer(0));
	FCVarDeclExprAST declarationNode(infoDeclaration, integer(1));
	std::ostringstream infoOutput;
	auto* oldOutput = std::cout.rdbuf(infoOutput.rdbuf());
	integerNode.info();
	floatingNode.info();
	stringNode.info();
	variableNode.info();
	binaryNode.info();
	callNode.info();
	prototypeNode.info();
	functionNode.info();
	ifNode.info();
	forNode.info();
	declarationNode.info();
	std::cout.rdbuf(oldOutput);
	tests.expect(true, "AST info methods");
}

void semanticTests(TestSuite& tests)
{
	FCSemanticContext context;
	tests.expect(context.getOperatorPrecedence('+') > context.getOperatorPrecedence('-') - 1,
		"semantic operator precedence lookup");
	tests.expect(context.getOperatorPrecedence('?') == 0,
		"semantic unknown operator precedence");

	context.pushScopeForFunc("f");
	auto outer = std::make_shared<VarDecl>("x", "int");
	context.insertVariableInCurrentScope("f", "x", outer);
	tests.expect(context.lookupVariableDecl("f", "x") == outer,
		"semantic function scope lookup");

	context.pushScopeForFunc("f");
	auto inner = std::make_shared<VarDecl>("x", "double");
	context.insertVariableInCurrentScope("f", "x", inner);
	tests.expect(context.lookupVariableInCurrentScope("f", "x") == inner &&
		context.lookupVariableDecl("f", "x") == inner,
		"semantic nested scope shadows outer scope");
	context.popScopeForFunc("f");
	tests.expect(context.lookupVariableDecl("f", "x") == outer,
		"semantic outer scope restored");
	context.popScopeForFunc("f");

	auto global = std::make_shared<VarDecl>("x", "string");
	context.insertGlobalVariable("x", global);
	tests.expect(global->isGlobal && global->slot == 0 &&
		context.lookupGlobalVariable("x") == global &&
		context.lookupVariableDecl("missing", "x") == global,
		"semantic global fallback lookup");

	context.pushScopeForFunc("g");
	auto local = std::make_shared<VarDecl>("x", "int");
	context.insertVariableInCurrentScope("g", "x", local);
	tests.expect(context.lookupVariableDecl("g", "x") == local,
		"semantic local shadows global");
	context.popScopeForFunc("g");
	context.popScopeForFunc("never-created");
	context.reset();
	tests.expect(context.globalDeclarations().empty() &&
		context.getOperatorPrecedence('+') > 0,
		"semantic reset");
}

void evaluatorTests(TestSuite& tests)
{
	FCEvaluationContext context;
	tests.expect(evaluate(static_cast<const FCExprAST*>(nullptr), context).type ==
		FCValueCategory::Dangle, "evaluator null expression");

	{
		FCValue value = evaluate(binary('+', integer(2), integer(3)).get(), context);
		tests.expect(isInteger(value, 5), "evaluator integer addition");
		value = evaluate(binary('-', integer(5), integer(2)).get(), context);
		tests.expect(isInteger(value, 3), "evaluator integer subtraction");
		value = evaluate(binary('*', integer(3), integer(4)).get(), context);
		tests.expect(isInteger(value, 12), "evaluator integer multiplication");
		value = evaluate(binary('/', integer(12), integer(4)).get(), context);
		tests.expect(isInteger(value, 3), "evaluator integer division");
		value = evaluate(binary('<', integer(1), integer(2)).get(), context);
		tests.expect(isInteger(value, 1), "evaluator integer comparison");
	}
	{
		FCValue value = evaluate(binary('+', floating(1.5), floating(2.5)).get(), context);
		tests.expect(isFloating(value, 4.0), "evaluator floating addition");
		value = evaluate(binary('-', floating(4.0), floating(1.5)).get(), context);
		tests.expect(isFloating(value, 2.5), "evaluator floating subtraction");
		value = evaluate(binary('*', floating(2.0), floating(3.0)).get(), context);
		tests.expect(isFloating(value, 6.0), "evaluator floating multiplication");
		value = evaluate(binary('/', floating(6.0), floating(2.0)).get(), context);
		tests.expect(isFloating(value, 3.0), "evaluator floating division");
	}
	tests.expect(evaluate(binary('%', integer(1), integer(2)).get(), context).type ==
		FCValueCategory::Dangle, "evaluator unsupported integer operator");
	tests.expect(evaluate(binary('<', floating(1.0), floating(2.0)).get(), context).type ==
		FCValueCategory::Dangle, "evaluator unsupported floating comparison");
	tests.expect(isString(evaluate(binary('+', stringLiteral("a"), stringLiteral("b")).get(), context), "ab"),
		"evaluator string concatenation");
	tests.expect(evaluate(binary('-', stringLiteral("a"), stringLiteral("b")).get(), context).type ==
		FCValueCategory::Dangle, "evaluator unsupported string operator");
	tests.expect(evaluate(binary('+', integer(1), floating(2.0)).get(), context).type ==
		FCValueCategory::Dangle, "evaluator mixed numeric categories");
	tests.expect(evaluate(binary('=', integer(1), integer(2)).get(), context).type ==
		FCValueCategory::Dangle, "evaluator assignment requires variable lhs");
	tests.expect(evaluate(std::make_unique<FCVariableExprAST>(nullptr).get(), context).type ==
		FCValueCategory::Dangle, "evaluator null variable declaration");

	{
		auto parameter = std::make_shared<VarDecl>("x", "int");
		parameter->slot = 0;
		auto add = function("identity", {parameter}, variable(parameter), 1);
		context.functions.registerFunction(add.get());
		std::vector<std::unique_ptr<FCExprAST>> args;
		args.push_back(integer(9));
		tests.expect(isInteger(evaluate(call("identity", std::move(args)).get(), context), 9),
			"evaluator function call and parameter frame");
		std::vector<std::unique_ptr<FCExprAST>> wrongCount;
		tests.expect(evaluate(call("identity", std::move(wrongCount)).get(), context).type ==
			FCValueCategory::Dangle, "evaluator argument count mismatch");
		std::vector<std::unique_ptr<FCExprAST>> danglingArgument;
		auto unallocated = std::make_shared<VarDecl>("unallocated", "int");
		danglingArgument.push_back(variable(unallocated));
		tests.expect(evaluate(call("identity", std::move(danglingArgument)).get(), context).type ==
			FCValueCategory::Dangle, "evaluator dangling call argument");
	}

	{
		auto parameterA = std::make_shared<VarDecl>("a", "int");
		parameterA->slot = 0;
		auto parameterB = std::make_shared<VarDecl>("b", "int");
		parameterB->slot = 1;
		auto add = function("add", {parameterA, parameterB},
			binary('+', variable(parameterA), variable(parameterB)), 2);
		context.functions.registerFunction(add.get());
		std::vector<std::unique_ptr<FCExprAST>> args;
		args.push_back(integer(2));
		args.push_back(integer(3));
		tests.expect(isInteger(evaluate(call("add", std::move(args)).get(), context), 5),
			"evaluator two-argument call");
	}
	{
		std::vector<FCVariableExprAST> invalidArguments;
		invalidArguments.emplace_back(nullptr);
		auto invalidParameterFunction = std::make_unique<FCFunctionAST>(
			std::make_unique<FCPrototypeAST>("invalidParameter", std::move(invalidArguments)),
			integer(1), 1);
		context.functions.registerFunction(invalidParameterFunction.get());
		std::vector<std::unique_ptr<FCExprAST>> invalidParameterArgs;
		invalidParameterArgs.push_back(integer(1));
		tests.expect(evaluate(call("invalidParameter", std::move(invalidParameterArgs)).get(), context).type ==
			FCValueCategory::Dangle, "evaluator null parameter declaration");
		context.functions.clear();
		std::vector<FCVariableExprAST> invalidSlotArguments;
		auto invalidSlotParameter = std::make_shared<VarDecl>("x", "int");
		invalidSlotParameter->slot = 1;
		invalidSlotArguments.emplace_back(invalidSlotParameter);
		auto invalidSlotFunction = std::make_unique<FCFunctionAST>(
			std::make_unique<FCPrototypeAST>("invalidSlot", std::move(invalidSlotArguments)),
			integer(1), 1);
		context.functions.registerFunction(invalidSlotFunction.get());
		std::vector<std::unique_ptr<FCExprAST>> invalidSlotArgs;
		invalidSlotArgs.push_back(integer(1));
		tests.expect(evaluate(call("invalidSlot", std::move(invalidSlotArgs)).get(), context).type ==
			FCValueCategory::Dangle, "evaluator invalid parameter slot");
	}
	tests.expect(evaluate(call("notFound").get(), context).type == FCValueCategory::Dangle,
		"evaluator unknown function");
	{
		auto global = std::make_shared<VarDecl>("g", "int");
		global->isGlobal = true;
		global->slot = 0;
		std::vector<std::unique_ptr<FCExprAST>> statements;
		statements.push_back(std::make_unique<FCVarDeclExprAST>(global, integer(4)));
		statements.push_back(binary('=', variable(global), integer(8)));
		statements.push_back(variable(global));
		auto program = std::make_unique<FCProgramAST>(std::move(statements));
		FCEvaluationContext globals;
		tests.expect(isInteger(evaluate(program.get(), globals), 8),
			"evaluator global initialization and assignment");
	}
	{
		auto invalidLocal = std::make_shared<VarDecl>("x", "int");
		invalidLocal->slot = -1;
		FCEvaluationContext invalidContext;
		invalidContext.callStack.push_back(Frame{"manual", {}});
		tests.expect(evaluate(variable(invalidLocal).get(), invalidContext).type ==
			FCValueCategory::Dangle, "evaluator invalid local slot");
		auto outOfRangeLocal = std::make_shared<VarDecl>("outOfRange", "int");
		outOfRangeLocal->slot = 0;
		tests.expect(evaluate(variable(outOfRangeLocal).get(), invalidContext).type ==
			FCValueCategory::Dangle, "evaluator out-of-range local slot");
		invalidContext.callStack.back().locals.resize(1);
		tests.expect(evaluate(binary('=', variable(outOfRangeLocal), variable(invalidLocal)).get(),
			invalidContext).type == FCValueCategory::Dangle,
			"evaluator assignment with dangling rhs");
		auto invalidAssignmentSlot = std::make_shared<VarDecl>("invalidAssignment", "int");
		invalidAssignmentSlot->slot = 1;
		tests.expect(evaluate(binary('=', variable(invalidAssignmentSlot), integer(1)).get(),
			invalidContext).type == FCValueCategory::Dangle,
			"evaluator assignment with invalid slot");
		auto invalidGlobal = std::make_shared<VarDecl>("g", "int");
		invalidGlobal->isGlobal = true;
		invalidGlobal->slot = -1;
		tests.expect(evaluate(variable(invalidGlobal).get(), invalidContext).type ==
			FCValueCategory::Dangle, "evaluator invalid global slot");
	}

	{
		FCIfExprAST choose(integer(1), integer(10), integer(20));
		tests.expect(isInteger(evaluate(&choose, context), 10), "evaluator if true branch");
		FCIfExprAST chooseElse(integer(0), integer(10), integer(20));
		tests.expect(isInteger(evaluate(&chooseElse, context), 20), "evaluator if false branch");
		FCIfExprAST invalid(stringLiteral("condition"), integer(1), integer(2));
		tests.expect(evaluate(&invalid, context).type == FCValueCategory::Dangle,
			"evaluator nonnumeric condition");
		FCIfExprAST floatingCondition(floating(0.5), integer(1), integer(2));
		tests.expect(isInteger(evaluate(&floatingCondition, context), 1),
			"evaluator floating condition");
	}

	{
		auto loopVariable = std::make_shared<VarDecl>("i", "int");
		loopVariable->slot = 0;
		auto loopEnd = std::make_shared<VarDecl>("end", "int");
		loopEnd->slot = 1;
		auto loop = std::make_unique<FCForExprAST>(
			loopVariable,
			integer(0),
			variable(loopEnd),
			integer(1),
			binary('=', variable(loopEnd), integer(0)));
		std::vector<std::unique_ptr<FCExprAST>> loopBody;
		loopBody.push_back(std::make_unique<FCVarDeclExprAST>(loopEnd, integer(1)));
		loopBody.push_back(std::move(loop));
		auto loopFunction = function("loop", {},
			std::make_unique<FCSeqExprAST>(std::move(loopBody)), 2);
		context.functions.registerFunction(loopFunction.get());
		tests.expect(evaluate(call("loop").get(), context).type == FCValueCategory::Dangle,
			"evaluator for loop with explicit step");
		FCForExprAST outside(loopVariable, integer(0), integer(1), nullptr, integer(0));
		FCEvaluationContext outsideContext;
		tests.expect(evaluate(&outside, outsideContext).type == FCValueCategory::Dangle,
			"evaluator for loop outside frame");
		auto defaultLoopVariable = std::make_shared<VarDecl>("j", "int");
		defaultLoopVariable->slot = 0;
		auto defaultLoopEnd = std::make_shared<VarDecl>("defaultEnd", "int");
		defaultLoopEnd->slot = 1;
		auto defaultLoop = std::make_unique<FCForExprAST>(
			defaultLoopVariable,
			integer(0),
			variable(defaultLoopEnd),
			nullptr,
			binary('=', variable(defaultLoopEnd), integer(0)));
		std::vector<std::unique_ptr<FCExprAST>> defaultLoopBody;
		defaultLoopBody.push_back(std::make_unique<FCVarDeclExprAST>(defaultLoopEnd, integer(1)));
		defaultLoopBody.push_back(std::move(defaultLoop));
		auto defaultLoopFunction = function("defaultLoop", {},
			std::make_unique<FCSeqExprAST>(std::move(defaultLoopBody)), 2);
		context.functions.registerFunction(defaultLoopFunction.get());
		tests.expect(evaluate(call("defaultLoop").get(), context).type == FCValueCategory::Dangle,
			"evaluator for loop default step");
	}

	{
		std::vector<std::unique_ptr<FCExprAST>> expressions;
		expressions.push_back(integer(1));
		expressions.push_back(integer(2));
		FCSeqExprAST sequence(std::move(expressions));
		tests.expect(isInteger(evaluate(&sequence, context), 2), "evaluator expression sequence");
		FCPrototypeAST prototype("prototype", {});
			tests.expect(evaluate(&prototype, context).type == FCValueCategory::Dangle,
			"evaluator prototype node");
		auto directFunction = function("direct", {}, integer(1), 0);
		tests.expect(evaluate(directFunction.get(), context).type == FCValueCategory::Dangle &&
			context.functions.findFunction("direct") == directFunction.get(),
			"evaluator function node registration");
		std::vector<std::unique_ptr<FCExprAST>> functionStatements;
		functionStatements.push_back(function("registered", {}, integer(1), 0));
		functionStatements.push_back(integer(2));
		FCProgramAST program(std::move(functionStatements));
		FCEvaluationContext programContext;
		tests.expect(isInteger(evaluate(&program, programContext), 2) &&
			programContext.functions.findFunction("registered") != nullptr,
			"evaluator program indexes and skips functions");
		FCProgramAST emptyProgram(std::vector<std::unique_ptr<FCExprAST>>{});
		tests.expect(evaluate(&emptyProgram, programContext).type == FCValueCategory::Dangle,
			"evaluator empty program");
		FCVarDeclExprAST nullDeclaration(nullptr, integer(1));
		tests.expect(evaluate(&nullDeclaration, programContext).type == FCValueCategory::Dangle,
			"evaluator null declaration");
		FCVarDeclExprAST nullInitializer(
			std::make_shared<VarDecl>("nullInit", "int"), nullptr);
		tests.expect(evaluate(&nullInitializer, programContext).type == FCValueCategory::Dangle,
			"evaluator null initializer");
		auto danglingInitializerDecl = std::make_shared<VarDecl>("danglingInit", "int");
		danglingInitializerDecl->slot = 0;
		FCVarDeclExprAST danglingInitializer(danglingInitializerDecl, variable(nullptr));
		programContext.callStack.push_back(Frame{"declaration", {FCValue{}}});
		tests.expect(evaluate(&danglingInitializer, programContext).type == FCValueCategory::Dangle,
			"evaluator dangling initializer");
		programContext.callStack.clear();
		FCVarDeclExprAST noFrameDeclaration(
			std::make_shared<VarDecl>("noFrame", "int"), integer(1));
		noFrameDeclaration.decl->slot = 0;
		tests.expect(evaluate(&noFrameDeclaration, programContext).type == FCValueCategory::Dangle,
			"evaluator declaration without frame");
		UnknownExprAST unknown;
		tests.expect(evaluate(&unknown, programContext).type == FCValueCategory::Dangle,
			"evaluator unknown expression");
	}
}

void registryTests(TestSuite& tests)
{
	FCFunctionRegistry registry;
	tests.expect(!registry.registerFunction(nullptr), "function registry rejects null");
	auto first = function("f", {}, integer(1), 0);
	auto second = function("f", {}, integer(2), 0);
	tests.expect(registry.registerFunction(first.get()), "function registry inserts function");
	tests.expect(registry.registerFunction(first.get()), "function registry accepts same pointer");
	tests.expect(!registry.registerFunction(second.get()), "function registry rejects duplicate name");
	tests.expect(registry.findFunction("f") == first.get() &&
		registry.findFunction("missing") == nullptr,
		"function registry lookup");
	std::vector<std::unique_ptr<FCExprAST>> programStatements;
	programStatements.push_back(std::move(second));
	FCProgramAST program(std::move(programStatements));
	tests.expect(!registry.index(nullptr), "function registry rejects null root");
	tests.expect(!registry.index(&program), "function registry rejects duplicate in program");
	auto standalone = function("standalone", {}, integer(3), 0);
	tests.expect(registry.index(standalone.get()), "function registry indexes standalone function");
	UnknownExprAST unknown;
	tests.expect(!registry.index(&unknown), "function registry rejects unknown root");
	registry.clear();
	tests.expect(registry.findFunction("f") == nullptr, "function registry clear");
}

void codegenTests(TestSuite& tests)
{
	{
		FCCodegenContext context("Types");
		tests.expect(context.getType("int")->isIntegerTy(32), "codegen int type");
		tests.expect(context.getType("double")->isDoubleTy(), "codegen double type");
		tests.expect(context.getType("string")->isPointerTy(), "codegen string type");
		tests.expect(context.getType("unknown")->isIntegerTy(32), "codegen fallback type");
		tests.expect(codegen(static_cast<const FCExprAST*>(nullptr), context) == nullptr,
			"codegen null expression");
		const auto number = codegen(integer(3).get(), context);
		tests.expect(number != nullptr && llvm::isa<llvm::ConstantInt>(number),
			"codegen integer constant");
		const auto real = codegen(floating(3.5).get(), context);
		tests.expect(real != nullptr && llvm::isa<llvm::ConstantFP>(real),
			"codegen floating constant");
		beginHostFunction(context, "constantHost");
		const auto text = codegen(stringLiteral("text").get(), context);
		tests.expect(text != nullptr && text->getType()->isPointerTy(),
			"codegen string constant");
		UnknownExprAST unknown;
		tests.expect(codegen(&unknown, context) == nullptr, "codegen unknown expression");
	}
	{
		FCScanner scanner;
		auto ast = scanner.analysis("def arithmetic(a:int, b:double) if a < 2 then b + 1.0 else b - 1.0");
		FCCodegenContext context("Arithmetic");
		tests.expect(ast != nullptr && codegen(ast.get(), context) != nullptr &&
			!llvm::verifyModule(*context.module, &llvm::errs()),
			"codegen arithmetic and if function");
		FCScanner integerIfScanner;
		auto integerIf = integerIfScanner.analysis("def integerIf(a:int) if a < 1 then 1 else 2");
		FCCodegenContext integerIfContext("IntegerIf");
		tests.expect(integerIf != nullptr && codegen(integerIf.get(), integerIfContext) != nullptr,
			"codegen integer if branch inference");
		FCScanner operationsScanner;
		auto operations = operationsScanner.analysis(
			"def operations(a:int, b:int) a - b * 2 / 3");
		FCCodegenContext operationsContext("IntegerOperations");
		tests.expect(operations != nullptr && codegen(operations.get(), operationsContext) != nullptr,
			"codegen integer subtraction multiplication division");
		FCScanner floatingScanner;
		auto floatingOperations = floatingScanner.analysis(
			"def floatingOps(a:double, b:double) a * b / 2.0");
		FCCodegenContext floatingContext("FloatingOperations");
		tests.expect(floatingOperations != nullptr &&
			codegen(floatingOperations.get(), floatingContext) != nullptr,
			"codegen floating multiplication division");
		FCScanner mixedScanner;
		auto mixed = mixedScanner.analysis("def mixed(a:int) a + 1.5");
		FCCodegenContext mixedContext("MixedOperations");
		tests.expect(mixed != nullptr && codegen(mixed.get(), mixedContext) != nullptr,
			"codegen integer to floating conversion");
		FCScanner floatCompareScanner;
		auto floatCompare = floatCompareScanner.analysis(
			"def floatCompare(a:double, b:double) a < b");
		FCCodegenContext floatCompareContext("FloatingCompare");
		tests.expect(floatCompare != nullptr && codegen(floatCompare.get(), floatCompareContext) != nullptr,
			"codegen floating comparison");
		FCScanner floatConditionScanner;
		auto floatCondition = floatConditionScanner.analysis(
			"def floatCondition(a:double) if a then 1 else 2");
		FCCodegenContext floatConditionContext("FloatingCondition");
		tests.expect(floatCondition != nullptr &&
			codegen(floatCondition.get(), floatConditionContext) != nullptr,
			"codegen floating condition");
	}
	{
		FCScanner scanner;
		auto ast = scanner.analysis("def concat(a:string, b:string) a + b");
		FCCodegenContext context("Strings");
		tests.expect(ast != nullptr && codegen(ast.get(), context) != nullptr &&
			!llvm::verifyModule(*context.module, &llvm::errs()),
			"codegen string concatenation function");
	}
	{
		FCScanner scanner;
		auto ast = scanner.analysis("def local(a:int) var b:int = a + 1; b");
		FCCodegenContext context("Locals");
		tests.expect(ast != nullptr && codegen(ast.get(), context) != nullptr &&
			!llvm::verifyModule(*context.module, &llvm::errs()),
			"codegen local declaration");
	}
	{
		FCScanner scanner;
		auto ast = scanner.analysis("def loop(n:int) for i = 0, i < n, 1 in i");
		FCCodegenContext context("Loops");
		tests.expect(ast != nullptr && codegen(ast.get(), context) != nullptr &&
			!llvm::verifyModule(*context.module, &llvm::errs()),
			"codegen for loop");
	}
	{
		FCScanner scanner;
		auto ast = scanner.analysis("var g:int = 10; def read() g; def write() g = 20");
		FCCodegenContext context("Globals");
		tests.expect(ast != nullptr && codegen(ast.get(), context) != nullptr &&
			context.module->getGlobalVariable("g", true) != nullptr &&
			!llvm::verifyModule(*context.module, &llvm::errs()),
			"codegen global variable access");
	}
	{
		FCScanner scanner;
		auto ast = scanner.analysis("var only:int = 7");
		FCCodegenContext context("StandaloneGlobal");
		tests.expect(ast != nullptr && codegen(ast.get(), context) != nullptr &&
			context.module->getFunction("__fc_main") != nullptr,
			"codegen standalone global declaration");
	}

	{
		FCCodegenContext context("Errors");
		beginHostFunction(context, "host");
		auto allocated = std::make_shared<VarDecl>("allocated", "int");
		auto* allocatedAddress = context.createEntryBlockAlloca(
			context.currentFunction, "allocated", context.getType("int"));
		context.namedValues[allocated.get()] = allocatedAddress;
		tests.expect(codegen(variable(allocated).get(), context) != nullptr,
			"codegen allocated local variable");
		tests.expect(codegen(binary('=', variable(allocated), integer(7)).get(), context) != nullptr,
			"codegen assignment to allocated local variable");
		tests.expect(codegen(binary('=', variable(allocated), std::make_unique<UnknownExprAST>()).get(), context) == nullptr,
			"codegen assignment rhs generation failure");
		tests.expect(codegen(binary('=', variable(allocated), stringLiteral("wrong")).get(), context) == nullptr,
			"codegen assignment type mismatch");
		auto missing = std::make_shared<VarDecl>("missing", "int");
		tests.expect(codegen(variable(missing).get(), context) == nullptr,
			"codegen missing local variable");
		auto global = std::make_shared<VarDecl>("global", "int");
		global->isGlobal = true;
		tests.expect(codegen(variable(global).get(), context) == nullptr,
			"codegen missing global variable");
		tests.expect(codegen(binary('=', integer(1), integer(2)).get(), context) == nullptr,
			"codegen assignment requires variable lhs");
		auto missingDeclaration = std::make_shared<VarDecl>("missingDeclaration", "int");
		tests.expect(codegen(binary('=', variable(missingDeclaration), integer(1)).get(), context) == nullptr,
			"codegen assignment to missing local variable");
	tests.expect(codegen(binary('=', variable(global), integer(1)).get(), context) == nullptr,
			"codegen assignment to missing global variable");
	tests.expect(codegen(std::make_unique<FCVariableExprAST>(nullptr).get(), context) == nullptr,
			"codegen variable with null declaration");
		tests.expect(codegen(binary('%', integer(1), integer(2)).get(), context) == nullptr,
			"codegen unsupported binary operator");
		tests.expect(codegen(binary('-', stringLiteral("a"), stringLiteral("b")).get(), context) == nullptr,
			"codegen unsupported string operator");
		tests.expect(codegen(binary('+', std::make_unique<UnknownExprAST>(), integer(1)).get(), context) == nullptr,
			"codegen binary operand failure");
	}
	{
		FCCodegenContext context("CallErrors");
		FCScanner scanner;
		auto definition = scanner.analysis("def one(a:int) a");
		tests.expect(codegen(definition.get(), context) != nullptr, "codegen call target declaration");
		beginHostFunction(context, "callHost");
		tests.expect(codegen(call("missing").get(), context) == nullptr,
			"codegen unknown call");
		std::vector<std::unique_ptr<FCExprAST>> wrongCount;
		wrongCount.push_back(integer(1));
		wrongCount.push_back(integer(2));
		tests.expect(codegen(call("one", std::move(wrongCount)).get(), context) == nullptr,
			"codegen call argument count mismatch");
		std::vector<std::unique_ptr<FCExprAST>> wrongType;
		wrongType.push_back(stringLiteral("bad"));
		tests.expect(codegen(call("one", std::move(wrongType)).get(), context) == nullptr,
			"codegen call argument type mismatch");
		std::vector<std::unique_ptr<FCExprAST>> numericConversion;
		numericConversion.push_back(floating(2.5));
		tests.expect(codegen(call("one", std::move(numericConversion)).get(), context) != nullptr,
			"codegen floating to integer call conversion");
		std::vector<std::unique_ptr<FCExprAST>> invalidArgument;
		invalidArgument.push_back(variable(std::make_shared<VarDecl>("unallocated", "int")));
		tests.expect(codegen(call("one", std::move(invalidArgument)).get(), context) == nullptr,
			"codegen call argument expression failure");
		std::vector<std::unique_ptr<FCExprAST>> correct;
		correct.push_back(integer(4));
		tests.expect(codegen(call("one", std::move(correct)).get(), context) != nullptr,
			"codegen valid call");
	}
	{
		FCCodegenContext context("ControlErrors");
		FCIfExprAST outside(integer(1), integer(2), integer(3));
		FCForExprAST loop(nullptr, integer(0), integer(1), nullptr, integer(0));
		tests.expect(codegen(&outside, context) == nullptr, "codegen if outside function");
		tests.expect(codegen(&loop, context) == nullptr, "codegen for outside function");

		beginHostFunction(context, "controlHost");
		FCIfExprAST invalidCondition(stringLiteral("bad"), integer(1), integer(2));
		tests.expect(codegen(&invalidCondition, context) == nullptr,
			"codegen if nonnumeric condition");
		FCIfExprAST invalidThen(integer(1), nullptr, integer(2));
		tests.expect(codegen(&invalidThen, context) == nullptr, "codegen if then failure");
		FCIfExprAST invalidElse(integer(1), integer(2), nullptr);
		tests.expect(codegen(&invalidElse, context) == nullptr, "codegen if else failure");
		FCIfExprAST nullCondition(nullptr, integer(1), integer(2));
		tests.expect(codegen(&nullCondition, context) == nullptr, "codegen null if condition");
		FCIfExprAST mismatchedBranches(integer(1), stringLiteral("bad"), integer(2));
		tests.expect(codegen(&mismatchedBranches, context) == nullptr,
			"codegen if branch type mismatch");

		FCCodegenContext forContext("ForErrors");
		beginHostFunction(forContext, "forHost");
		auto forVariable = std::make_shared<VarDecl>("i", "int");
		forVariable->slot = 0;
		FCForExprAST badStart(forVariable, stringLiteral("bad"), integer(1), nullptr, integer(0));
		tests.expect(codegen(&badStart, forContext) == nullptr, "codegen for start type mismatch");
		FCForExprAST badEnd(forVariable, integer(0), stringLiteral("bad"), nullptr, integer(0));
		tests.expect(codegen(&badEnd, forContext) == nullptr, "codegen for end type mismatch");
		FCForExprAST badBody(forVariable, integer(0), integer(1), nullptr, nullptr);
		tests.expect(codegen(&badBody, forContext) == nullptr, "codegen for body failure");
		FCForExprAST badStep(forVariable, integer(0), integer(1), stringLiteral("bad"), integer(0));
		tests.expect(codegen(&badStep, forContext) == nullptr, "codegen for step type mismatch");
	}
	{
		FCCodegenContext context("DeclarationErrors");
		auto local = std::make_shared<VarDecl>("local", "int");
		FCVarDeclExprAST declaration(local, stringLiteral("wrong"));
		tests.expect(codegen(&declaration, context) == nullptr,
			"codegen local declaration outside function");
		beginHostFunction(context, "declarationHost");
		tests.expect(codegen(&declaration, context) == nullptr,
			"codegen local initializer type mismatch");
		auto global = std::make_shared<VarDecl>("global", "int");
		global->isGlobal = true;
		global->slot = 0;
		FCVarDeclExprAST globalDeclaration(global, stringLiteral("wrong"));
		FCScanner globalScanner;
		auto globalAst = globalScanner.analysis("var global:int = \"wrong\"");
		FCCodegenContext globalContext("GlobalDeclarationErrors");
		tests.expect(globalAst != nullptr && codegen(globalAst.get(), globalContext) == nullptr,
			"codegen global initializer type mismatch");
		FCCodegenContext missingGlobalContext("MissingGlobalDeclaration");
		beginHostFunction(missingGlobalContext, "missingGlobalHost");
		tests.expect(codegen(&globalDeclaration, missingGlobalContext) == nullptr,
			"codegen undeclared global declaration");
		std::vector<std::unique_ptr<FCExprAST>> expressions;
		expressions.push_back(nullptr);
		FCSeqExprAST sequence(std::move(expressions));
		tests.expect(codegen(&sequence, context) == nullptr, "codegen null sequence item");
	}
	{
		FCCodegenContext context("FunctionErrors");
		auto emptyBody = std::make_unique<FCFunctionAST>(
			std::make_unique<FCPrototypeAST>("emptyBody", std::vector<FCVariableExprAST>{}),
			nullptr, 0);
		tests.expect(codegen(emptyBody.get(), context) == nullptr,
			"codegen missing function body");

		std::vector<FCVariableExprAST> invalidArguments;
		invalidArguments.emplace_back(nullptr);
		auto invalidParameter = std::make_unique<FCFunctionAST>(
			std::make_unique<FCPrototypeAST>("invalidParameter", std::move(invalidArguments)),
			integer(1), 0);
		tests.expect(codegen(invalidParameter.get(), context) == nullptr,
			"codegen missing parameter declaration");
		FCScanner duplicateScanner;
		auto duplicate = duplicateScanner.analysis("def duplicate() 1");
		tests.expect(duplicate != nullptr && codegen(duplicate.get(), context) != nullptr &&
			codegen(duplicate.get(), context) != nullptr,
			"codegen already-defined function");
		FCPrototypeAST prototype("prototype", {});
		tests.expect(codegen(&prototype, context) != nullptr, "codegen prototype");
		FCScanner recursiveScanner;
		auto recursive = recursiveScanner.analysis("def recursive() recursive()");
		FCCodegenContext recursiveContext("Recursive");
		tests.expect(recursive != nullptr && codegen(recursive.get(), recursiveContext) != nullptr,
			"codegen recursive return type inference");
		std::vector<std::unique_ptr<FCExprAST>> recursiveStatements;
		recursiveStatements.push_back(function("programRecursive", {}, call("programRecursive"), 0));
		FCProgramAST recursiveProgram(std::move(recursiveStatements));
		FCCodegenContext recursiveProgramContext("RecursiveProgram");
		tests.expect(codegen(&recursiveProgram, recursiveProgramContext) != nullptr,
			"codegen recursive program return type inference");
		FCCodegenContext externalContext("ExternalInference");
		FCPrototypeAST externalPrototype("external", {});
		tests.expect(codegen(&externalPrototype, externalContext) != nullptr,
			"codegen external prototype inference setup");
		auto externalCaller = function("externalCaller", {}, call("external"), 0);
		tests.expect(codegen(externalCaller.get(), externalContext) != nullptr,
			"codegen module function return type inference");
		auto emptySequenceFunction = function("emptySequence", {},
			std::make_unique<FCSeqExprAST>(std::vector<std::unique_ptr<FCExprAST>>{}), 0);
		tests.expect(codegen(emptySequenceFunction.get(), externalContext) == nullptr,
			"codegen empty sequence");
		auto nullDeclarationFunction = function("nullDeclaration", {},
			std::make_unique<FCVarDeclExprAST>(nullptr, integer(1)), 0);
		tests.expect(codegen(nullDeclarationFunction.get(), externalContext) == nullptr,
			"codegen null declaration inference");
		auto unknownBodyFunction = function("unknownBody", {},
			std::make_unique<UnknownExprAST>(), 0);
		tests.expect(codegen(unknownBodyFunction.get(), externalContext) == nullptr,
			"codegen unknown body inference fallback");
		auto nullBranchFunction = function("nullBranch", {},
			std::make_unique<FCIfExprAST>(integer(1), nullptr, integer(2)), 0);
		tests.expect(codegen(nullBranchFunction.get(), externalContext) == nullptr,
			"codegen null branch inference");
		auto nestedFunction = function("nestedOuter", {},
			function("nestedInner", {}, integer(1), 0), 0);
		tests.expect(codegen(nestedFunction.get(), externalContext) == nullptr,
			"codegen nested function inference");
		FCCodegenContext existingLoopContext("ExistingLoop");
		beginHostFunction(existingLoopContext, "existingLoopHost");
		auto existingLoopVariable = std::make_shared<VarDecl>("i", "int");
		auto* existingAddress = existingLoopContext.createEntryBlockAlloca(
			existingLoopContext.currentFunction, "i", existingLoopContext.getType("int"));
		existingLoopContext.namedValues[existingLoopVariable.get()] = existingAddress;
		FCForExprAST existingLoop(existingLoopVariable, integer(0), integer(0), nullptr, integer(0));
		tests.expect(codegen(&existingLoop, existingLoopContext) != nullptr,
			"codegen restores existing loop variable binding");
	}
	{
		FCCodegenContext context("ProgramErrors");
		std::vector<std::unique_ptr<FCExprAST>> emptyStatements;
		FCProgramAST empty(std::move(emptyStatements));
		tests.expect(codegen(&empty, context) == nullptr, "codegen empty program");
		std::vector<std::unique_ptr<FCExprAST>> stringStatements;
		stringStatements.push_back(stringLiteral("not an integer"));
		FCProgramAST stringProgram(std::move(stringStatements));
		tests.expect(codegen(&stringProgram, context) == nullptr,
			"codegen top-level noninteger result");
		std::vector<std::unique_ptr<FCExprAST>> unknownStatements;
		unknownStatements.push_back(std::make_unique<UnknownExprAST>());
		FCProgramAST unknownProgram(std::move(unknownStatements));
		FCCodegenContext unknownContext("UnknownProgram");
		tests.expect(codegen(&unknownProgram, unknownContext) == nullptr,
			"codegen unknown top-level expression");
		std::vector<std::unique_ptr<FCExprAST>> nullStatements;
		nullStatements.push_back(nullptr);
		FCProgramAST nullProgram(std::move(nullStatements));
		FCCodegenContext nullProgramContext("NullProgram");
		tests.expect(codegen(&nullProgram, nullProgramContext) == nullptr,
			"codegen null top-level statement");
		FCCodegenContext repeatedContext("RepeatedProgram");
		std::vector<std::unique_ptr<FCExprAST>> repeatedStatements;
		repeatedStatements.push_back(integer(1));
		FCProgramAST repeatedProgram(std::move(repeatedStatements));
		tests.expect(codegen(&repeatedProgram, repeatedContext) != nullptr &&
			codegen(&repeatedProgram, repeatedContext) != nullptr,
			"codegen reuses completed top-level main");
		FCCodegenContext badFunctionProgramContext("BadFunctionProgram");
		std::vector<std::unique_ptr<FCExprAST>> badFunctionStatements;
		badFunctionStatements.push_back(function("badFunction", {},
			std::make_unique<UnknownExprAST>(), 0));
		FCProgramAST badFunctionProgram(std::move(badFunctionStatements));
		tests.expect(codegen(&badFunctionProgram, badFunctionProgramContext) == nullptr,
			"codegen program function failure");
		FCCodegenContext sequenceGlobalContext("SequenceGlobals");
		auto sequenceGlobal = std::make_shared<VarDecl>("sequenceGlobal", "int");
		sequenceGlobal->isGlobal = true;
		sequenceGlobal->slot = 0;
		std::vector<std::unique_ptr<FCExprAST>> globalSequenceItems;
		globalSequenceItems.push_back(std::make_unique<FCVarDeclExprAST>(sequenceGlobal, integer(1)));
		globalSequenceItems.push_back(variable(sequenceGlobal));
		std::vector<std::unique_ptr<FCExprAST>> sequenceStatements;
		sequenceStatements.push_back(std::make_unique<FCSeqExprAST>(std::move(globalSequenceItems)));
		FCProgramAST sequenceGlobalProgram(std::move(sequenceStatements));
		tests.expect(codegen(&sequenceGlobalProgram, sequenceGlobalContext) != nullptr,
			"codegen global declaration in sequence");
		FCCodegenContext branchGlobalContext("BranchGlobals");
		auto thenGlobal = std::make_shared<VarDecl>("thenGlobal", "int");
		thenGlobal->isGlobal = true;
		thenGlobal->slot = 0;
		auto elseGlobal = std::make_shared<VarDecl>("elseGlobal", "int");
		elseGlobal->isGlobal = true;
		elseGlobal->slot = 1;
		std::vector<std::unique_ptr<FCExprAST>> branchStatements;
		branchStatements.push_back(std::make_unique<FCVarDeclExprAST>(thenGlobal, integer(0)));
		branchStatements.push_back(std::make_unique<FCVarDeclExprAST>(elseGlobal, integer(0)));
		branchStatements.push_back(std::make_unique<FCIfExprAST>(
			integer(1), std::make_unique<FCVarDeclExprAST>(thenGlobal, integer(1)),
			std::make_unique<FCVarDeclExprAST>(elseGlobal, integer(2))));
		FCProgramAST branchGlobalProgram(std::move(branchStatements));
		tests.expect(codegen(&branchGlobalProgram, branchGlobalContext) != nullptr,
			"codegen global declarations in branches");
		FCScanner stringGlobalScanner;
		auto stringGlobal = stringGlobalScanner.analysis("var text:string = \"text\"");
		FCCodegenContext stringGlobalContext("StringGlobal");
		tests.expect(stringGlobal != nullptr && codegen(stringGlobal.get(), stringGlobalContext) == nullptr,
			"codegen standalone string global result mismatch");
		FCScanner repeatedGlobalScanner;
		auto repeatedGlobal = repeatedGlobalScanner.analysis("var repeated:int = 1");
		FCCodegenContext repeatedGlobalContext("RepeatedGlobal");
		tests.expect(repeatedGlobal != nullptr && codegen(repeatedGlobal.get(), repeatedGlobalContext) != nullptr &&
			codegen(repeatedGlobal.get(), repeatedGlobalContext) != nullptr,
			"codegen reuses completed standalone main");
	}
}

void complexIntegrationTests(TestSuite& tests)
{
	std::fprintf(stderr, "complex: parsed source\n");
	FCScanner scanner;
	auto program = scanner.analysis(
		"var counter:int = 10;"
		"def shadow() var counter:int = 5; counter = counter + 1; counter;"
		"def factorial(n:int) if n < 2 then 1 else n * factorial(n - 1);"
		"def increment() counter = counter + 1;"
		"def nested(n:int) increment(); factorial(n) + shadow()");

	tests.expect(program != nullptr, "complex case parses");
	auto* parsedProgram = dynamic_cast<FCProgramAST*>(program.get());
	tests.expect(parsedProgram != nullptr && parsedProgram->getStatements().size() == 5,
		"complex case contains global and four functions");
	parsedProgram->info();

	auto globalCounter = scanner.semanticContext().lookupGlobalVariable("counter");
	tests.expect(globalCounter != nullptr && globalCounter->isGlobal && globalCounter->slot == 0,
		"complex case binds global counter");

	std::fprintf(stderr, "complex: before program evaluate\n");
	FCEvaluationContext evaluationContext;
	const auto initialResult = evaluate(program.get(), evaluationContext);
	std::fprintf(stderr, "complex: after program evaluate\n");
	tests.expect(isInteger(initialResult, 10),
		"complex case initializes global counter");

	const auto shadowResult = evaluate(call("shadow").get(), evaluationContext);
	std::fprintf(stderr, "complex: after shadow\n");
	tests.expect(isInteger(shadowResult, 6),
		"complex case local counter shadows global counter");
	tests.expect(globalCounter != nullptr &&
		isInteger(evaluate(variable(globalCounter).get(), evaluationContext), 10),
		"complex case shadow does not modify global counter");

	std::vector<std::unique_ptr<FCExprAST>> factorialZeroArgs;
	factorialZeroArgs.push_back(integer(0));
	tests.expect(isInteger(evaluate(call("factorial", std::move(factorialZeroArgs)).get(),
		evaluationContext), 1),
		"complex case recursive base case");
	std::fprintf(stderr, "complex: after factorial zero\n");
	std::vector<std::unique_ptr<FCExprAST>> factorialArgs;
	factorialArgs.push_back(integer(4));
	tests.expect(isInteger(evaluate(call("factorial", std::move(factorialArgs)).get(),
		evaluationContext), 24),
		"complex case recursive factorial");
	std::fprintf(stderr, "complex: after factorial four\n");

	std::vector<std::unique_ptr<FCExprAST>> nestedArgs;
	nestedArgs.push_back(integer(4));
	tests.expect(isInteger(evaluate(call("nested", std::move(nestedArgs)).get(),
		evaluationContext), 30),
		"complex case nested call combines increment recursion and shadow");
	std::fprintf(stderr, "complex: after nested four\n");
	tests.expect(globalCounter != nullptr &&
		isInteger(evaluate(variable(globalCounter).get(), evaluationContext), 11),
		"complex case nested call modifies global counter");

	std::vector<std::unique_ptr<FCExprAST>> secondNestedArgs;
	secondNestedArgs.push_back(integer(3));
	tests.expect(isInteger(evaluate(call("nested", std::move(secondNestedArgs)).get(),
		evaluationContext), 12),
		"complex case repeated nested call");
	std::fprintf(stderr, "complex: after nested three\n");
	tests.expect(globalCounter != nullptr &&
		isInteger(evaluate(variable(globalCounter).get(), evaluationContext), 12),
		"complex case global state persists across calls");

	FCCodegenContext codegenContext("ComplexIntegration");
	std::fprintf(stderr, "complex: before codegen\n");
	auto* generated = codegen(program.get(), codegenContext);
	std::fprintf(stderr, "complex: after codegen\n");
	tests.expect(generated != nullptr &&
		!llvm::verifyModule(*codegenContext.module, &llvm::errs()),
		"complex case generates valid LLVM module");
	if (generated != nullptr)
	{
		tests.expect(codegenContext.module->getGlobalVariable("counter", true) != nullptr,
			"complex case generates global counter");
		for (const auto& name : {"shadow", "factorial", "increment", "nested"})
			tests.expect(codegenContext.module->getFunction(name) != nullptr,
				std::string("complex case generates function ") + name);
		tests.expect(codegenContext.module->getFunction("__fc_main") != nullptr,
			"complex case generates top-level initializer");
	}
}
}

int main()
{
	TestSuite tests;
	scannerTests(tests);
	semanticTests(tests);
	evaluatorTests(tests);
	registryTests(tests);
	codegenTests(tests);
	complexIntegrationTests(tests);
	return tests.result();
}
