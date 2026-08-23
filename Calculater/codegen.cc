#include "codegen.h"
#include <unordered_set>
#include "llvm/Support/raw_ostream.h"

using namespace FCExprClass;
using namespace FCMarks;

namespace {

llvm::Type* integerType(FCCodegenContext& context)
{
	return llvm::Type::getInt32Ty(context.llvmContext);
}

llvm::Type* stringType(FCCodegenContext& context)
{
	return llvm::PointerType::getUnqual(context.llvmContext);
}

llvm::Value* logCodegenError(const char* message)
{
	fprintf(stderr, "Codegen error: %s\n", message);
	return nullptr;
}

llvm::Type* inferExprType(const FCExprAST* expression,
	FCCodegenContext& context,
	std::unordered_set<const FCFunctionAST*>& visitingFunctions);

llvm::Type* inferFunctionReturnType(const FCFunctionAST* function,
	FCCodegenContext& context,
	std::unordered_set<const FCFunctionAST*>& visitingFunctions)
{
	if (function == nullptr || function->getBody() == nullptr)
		return integerType(context);

	if (!visitingFunctions.insert(function).second)
		return integerType(context);

	auto* result = inferExprType(function->getBody(), context, visitingFunctions);
	visitingFunctions.erase(function);
	return result == nullptr ? integerType(context) : result;
}

llvm::Type* inferExprType(const FCExprAST* expression,
	FCCodegenContext& context,
	std::unordered_set<const FCFunctionAST*>& visitingFunctions)
{
	if (expression == nullptr)
		return integerType(context);

	if (auto* number = dynamic_cast<const FCNumberExprAST*>(expression))
	{
		return number->m_exprVal.type == FCValueCategory::Floating
			? llvm::Type::getDoubleTy(context.llvmContext)
			: integerType(context);
	}

	if (dynamic_cast<const FCStringExprAST*>(expression) != nullptr)
		return stringType(context);

	if (auto* variable = dynamic_cast<const FCVariableExprAST*>(expression))
		return variable->decl == nullptr
			? integerType(context)
			: context.getType(variable->decl->typeName);

	if (auto* binary = dynamic_cast<const FCBinaryExprAST*>(expression))
	{
		if (binary->getOperator() == '=')
			return inferExprType(binary->getRHS(), context, visitingFunctions);

		auto* lhsType = inferExprType(binary->getLHS(), context, visitingFunctions);
		auto* rhsType = inferExprType(binary->getRHS(), context, visitingFunctions);
		if (binary->getOperator() == '<')
			return integerType(context);
		if (lhsType->isDoubleTy() || rhsType->isDoubleTy())
			return llvm::Type::getDoubleTy(context.llvmContext);
		return lhsType;
	}

	if (auto* call = dynamic_cast<const FCCallExprAST*>(expression))
	{
		auto definition = context.definitions.find(call->getName());
		if (definition != context.definitions.end())
			return inferFunctionReturnType(definition->second, context, visitingFunctions);

		if (auto* function = context.module->getFunction(call->getName()))
			return function->getReturnType();

		return integerType(context);
	}

	if (auto* conditional = dynamic_cast<const FCIfExprAST*>(expression))
	{
		auto* thenType = inferExprType(conditional->getThen(), context, visitingFunctions);
		auto* elseType = inferExprType(conditional->getElse(), context, visitingFunctions);
		if (thenType->isDoubleTy() || elseType->isDoubleTy())
			return llvm::Type::getDoubleTy(context.llvmContext);
		return thenType;
	}

	if (auto* loop = dynamic_cast<const FCForExprAST*>(expression))
		return inferExprType(loop->getBody(), context, visitingFunctions);

	if (auto* sequence = dynamic_cast<const FCSeqExprAST*>(expression))
	{
		if (sequence->getExpressions().empty())
			return integerType(context);
		return inferExprType(sequence->getExpressions().back().get(), context, visitingFunctions);
	}

	if (auto* declaration = dynamic_cast<const FCVarDeclExprAST*>(expression))
		return declaration->decl == nullptr
			? inferExprType(declaration->initExpr.get(), context, visitingFunctions)
			: context.getType(declaration->decl->typeName);

	if (auto* function = dynamic_cast<const FCFunctionAST*>(expression))
		return inferFunctionReturnType(function, context, visitingFunctions);

	return integerType(context);
}

llvm::Type* inferFunctionReturnType(const FCFunctionAST* function,
	FCCodegenContext& context)
{
	std::unordered_set<const FCFunctionAST*> visitingFunctions;
	return inferFunctionReturnType(function, context, visitingFunctions);
}

llvm::Value* castValue(FCCodegenContext& context, llvm::Value* value,
	llvm::Type* targetType)
{
	if (value == nullptr || targetType == nullptr)
		return nullptr;
	if (value->getType() == targetType)
		return value;

	if (value->getType()->isIntegerTy() && targetType->isDoubleTy())
		return context.builder.CreateSIToFP(value, targetType, "inttodouble");
	if (value->getType()->isDoubleTy() && targetType->isIntegerTy())
		return context.builder.CreateFPToSI(value, targetType, "doubletoint");
	if (value->getType()->isIntegerTy(1) && targetType->isIntegerTy())
		return context.builder.CreateZExt(value, targetType, "booltoint");

	return nullptr;
}

llvm::Value* createCondition(FCCodegenContext& context, llvm::Value* value)
{
	if (value == nullptr)
		return nullptr;
	if (value->getType()->isIntegerTy(1))
		return value;
	if (value->getType()->isIntegerTy())
		return context.builder.CreateICmpNE(
			value,
			llvm::ConstantInt::get(value->getType(), 0),
			"ifcond");
	if (value->getType()->isFloatingPointTy())
		return context.builder.CreateFCmpONE(
			value,
			llvm::ConstantFP::get(value->getType(), 0.0),
			"ifcond");

	return nullptr;
}

llvm::Function* createFunctionDeclaration(FCCodegenContext& context,
	const std::string& name,
	const std::vector<FCVariableExprAST>& arguments,
	llvm::Type* returnType)
{
	if (auto* existing = context.module->getFunction(name))
		return existing;

	std::vector<llvm::Type*> argumentTypes;
	argumentTypes.reserve(arguments.size());
	for (const auto& argument : arguments)
	{
		argumentTypes.push_back(argument.decl == nullptr
			? integerType(context)
			: context.getType(argument.decl->typeName));
	}

	auto* functionType = llvm::FunctionType::get(returnType, argumentTypes, false);
	auto* function = llvm::Function::Create(
		functionType,
		llvm::Function::ExternalLinkage,
		name,
		context.module.get());

	unsigned index = 0;
	for (auto& argument : function->args())
	{
		if (index < arguments.size() && arguments[index].decl != nullptr)
			argument.setName(arguments[index].decl->name);
		++index;
	}
	return function;
}

llvm::Function* declareFunction(FCFunctionAST* function,
	FCCodegenContext& context)
{
	if (function == nullptr)
		return nullptr;

	return createFunctionDeclaration(
		context,
		function->getProtoName(),
		function->getProto()->getArgs(),
		inferFunctionReturnType(function, context));
}

} // namespace

FCCodegenContext::FCCodegenContext(const std::string& moduleName)
	: builder(llvmContext),
	  module(std::make_unique<llvm::Module>(moduleName, llvmContext))
{
}

llvm::Type* FCCodegenContext::getType(const std::string& typeName)
{
	if (typeName == "double")
		return llvm::Type::getDoubleTy(llvmContext);
	if (typeName == "string")
		return llvm::PointerType::getUnqual(llvmContext);
	return llvm::Type::getInt32Ty(llvmContext);
}

llvm::AllocaInst* FCCodegenContext::createEntryBlockAlloca(
	llvm::Function* function, const std::string& name, llvm::Type* type)
{
	llvm::IRBuilder<> entryBuilder(
		&function->getEntryBlock(), function->getEntryBlock().begin());
	return entryBuilder.CreateAlloca(type, nullptr, name);
}

llvm::Value* FCNumberExprAST::codegen(FCCodegenContext& context)
{
	if (m_exprVal.type == FCValueCategory::Floating)
		return llvm::ConstantFP::get(
			llvm::Type::getDoubleTy(context.llvmContext), m_doubleVal);
	return llvm::ConstantInt::get(
		llvm::Type::getInt32Ty(context.llvmContext), m_intVal, true);
}

llvm::Value* FCStringExprAST::codegen(FCCodegenContext& context)
{
	return context.builder.CreateGlobalStringPtr(m_stringVal, "str");
}

llvm::Value* FCVariableExprAST::codegen(FCCodegenContext& context)
{
	if (decl == nullptr)
		return logCodegenError("variable declaration is missing");

	auto it = context.namedValues.find(decl.get());
	if (it == context.namedValues.end())
		return logCodegenError("variable is not allocated in the current function");

	return context.builder.CreateLoad(
		it->second->getAllocatedType(), it->second, decl->name);
}

llvm::Value* FCBinaryExprAST::codegen(FCCodegenContext& context)
{
	if (m_Op == '=')
	{
		auto* variable = dynamic_cast<FCVariableExprAST*>(mup_LHS.get());
		if (variable == nullptr || variable->decl == nullptr)
			return logCodegenError("left side of assignment must be a variable");

		auto address = context.namedValues.find(variable->decl.get());
		if (address == context.namedValues.end())
			return logCodegenError("assigned variable is not allocated");

		auto* rhs = mup_RHS->codegen(context);
		if (rhs == nullptr)
			return nullptr;
		rhs = castValue(context, rhs, address->second->getAllocatedType());
		if (rhs == nullptr)
			return logCodegenError("assignment type mismatch");

		context.builder.CreateStore(rhs, address->second);
		return rhs;
	}

	auto* lhs = mup_LHS->codegen(context);
	auto* rhs = mup_RHS->codegen(context);
	if (lhs == nullptr || rhs == nullptr)
		return nullptr;

	if (lhs->getType()->isPointerTy() || rhs->getType()->isPointerTy())
	{
		if (m_Op != '+' || !lhs->getType()->isPointerTy() ||
			!rhs->getType()->isPointerTy())
			return logCodegenError("only string concatenation is supported");

		auto* i8Pointer = stringType(context);
		auto* sizeType = llvm::Type::getInt64Ty(context.llvmContext);
		auto* strlenType = llvm::FunctionType::get(sizeType, { i8Pointer }, false);
		auto strlen = context.module->getOrInsertFunction("strlen", strlenType);
		auto* lhsLength = context.builder.CreateCall(strlen, { lhs }, "lhslen");
		auto* rhsLength = context.builder.CreateCall(strlen, { rhs }, "rhslen");
		auto* totalLength = context.builder.CreateAdd(lhsLength, rhsLength, "strlen");
		totalLength = context.builder.CreateAdd(
			totalLength, llvm::ConstantInt::get(sizeType, 1), "strlenwithnull");

		auto* mallocType = llvm::FunctionType::get(i8Pointer, { sizeType }, false);
		auto mallocFunction = context.module->getOrInsertFunction("malloc", mallocType);
		auto* buffer = context.builder.CreateCall(mallocFunction, { totalLength }, "strbuf");

		auto* copyType = llvm::FunctionType::get(
			i8Pointer, { i8Pointer, i8Pointer }, false);
		auto strcpyFunction = context.module->getOrInsertFunction("strcpy", copyType);
		auto strcatFunction = context.module->getOrInsertFunction("strcat", copyType);
	context.builder.CreateCall(strcpyFunction, { buffer, lhs });
	context.builder.CreateCall(strcatFunction, { buffer, rhs });
	return buffer;
	}

	if (lhs->getType()->isDoubleTy() || rhs->getType()->isDoubleTy())
	{
		lhs = castValue(context, lhs, llvm::Type::getDoubleTy(context.llvmContext));
		rhs = castValue(context, rhs, llvm::Type::getDoubleTy(context.llvmContext));
		if (lhs == nullptr || rhs == nullptr)
			return logCodegenError("numeric type conversion failed");

		switch (m_Op)
		{
		case '+': return context.builder.CreateFAdd(lhs, rhs, "addtmp");
		case '-': return context.builder.CreateFSub(lhs, rhs, "subtmp");
		case '*': return context.builder.CreateFMul(lhs, rhs, "multmp");
		case '/': return context.builder.CreateFDiv(lhs, rhs, "divtmp");
		case '<':
		{
			auto* comparison = context.builder.CreateFCmpULT(lhs, rhs, "cmptmp");
			return context.builder.CreateZExt(comparison,
				integerType(context), "booltmp");
		}
		default: break;
		}
	}
	else if (lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy())
	{
		if (lhs->getType() != rhs->getType())
		{
			lhs = castValue(context, lhs, integerType(context));
			rhs = castValue(context, rhs, integerType(context));
		}

		switch (m_Op)
		{
		case '+': return context.builder.CreateAdd(lhs, rhs, "addtmp");
		case '-': return context.builder.CreateSub(lhs, rhs, "subtmp");
		case '*': return context.builder.CreateMul(lhs, rhs, "multmp");
		case '/': return context.builder.CreateSDiv(lhs, rhs, "divtmp");
		case '<':
		{
			auto* comparison = context.builder.CreateICmpSLT(lhs, rhs, "cmptmp");
			return context.builder.CreateZExt(comparison,
				integerType(context), "booltmp");
		}
		default: break;
		}
	}

	return logCodegenError("unknown binary operator");
}

llvm::Value* FCCallExprAST::codegen(FCCodegenContext& context)
{
	auto* function = context.module->getFunction(m_callee);
	if (function == nullptr)
	{
		fprintf(stderr, "Codegen error: function not found: %s\n", m_callee.c_str());
		return nullptr;
	}
	if (m_args.size() != function->arg_size())
	{
		fprintf(stderr, "Codegen error: argument count mismatch in %s\n", m_callee.c_str());
		return nullptr;
	}

	std::vector<llvm::Value*> arguments;
	arguments.reserve(m_args.size());
	unsigned index = 0;
	for (const auto& argument : m_args)
	{
		auto* value = argument->codegen(context);
		if (value == nullptr)
			return nullptr;
		value = castValue(context, value, function->getArg(index)->getType());
		if (value == nullptr)
			return logCodegenError("call argument type mismatch");
		arguments.push_back(value);
		++index;
	}

	return context.builder.CreateCall(function, arguments, "calltmp");
}

llvm::Value* FCPrototypeAST::codegen(FCCodegenContext& context)
{
	return createFunctionDeclaration(
		context, m_funcName, m_funcArgsVar, integerType(context));
}

llvm::Value* FCFunctionAST::codegen(FCCodegenContext& context)
{
	auto* function = declareFunction(this, context);
	if (function == nullptr)
		return nullptr;
	if (!function->empty())
		return function;

	auto oldInsertPoint = context.builder.saveIP();
	auto oldFunction = context.currentFunction;
	auto oldNamedValues = std::move(context.namedValues);
	context.namedValues.clear();
	context.currentFunction = function;

	auto* entry = llvm::BasicBlock::Create(context.llvmContext, "entry", function);
	context.builder.SetInsertPoint(entry);

	unsigned index = 0;
	for (auto& argument : function->args())
	{
		auto& parameter = getProto()->getArgs()[index];
		if (parameter.decl == nullptr)
		{
			context.namedValues = std::move(oldNamedValues);
			context.currentFunction = oldFunction;
			context.builder.restoreIP(oldInsertPoint);
			return logCodegenError("function parameter declaration is missing");
		}

		auto* alloca = context.createEntryBlockAlloca(
			function, parameter.decl->name, argument.getType());
		context.builder.CreateStore(&argument, alloca);
		context.namedValues.emplace(parameter.decl.get(), alloca);
		++index;
	}

	auto* body = mup_funcBody == nullptr ? nullptr : mup_funcBody->codegen(context);
	if (body == nullptr)
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return nullptr;
	}

	auto* returnValue = castValue(context, body, function->getReturnType());
	if (returnValue == nullptr)
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return logCodegenError("function return type mismatch");
	}
	context.builder.CreateRet(returnValue);

	if (llvm::verifyFunction(*function, &llvm::errs()))
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return nullptr;
	}

	context.namedValues = std::move(oldNamedValues);
	context.currentFunction = oldFunction;
	context.builder.restoreIP(oldInsertPoint);
	return function;
}

llvm::Value* FCIfExprAST::codegen(FCCodegenContext& context)
{
	if (context.currentFunction == nullptr)
		return logCodegenError("if expression is outside a function");

	auto* condition = createCondition(context, Cond->codegen(context));
	if (condition == nullptr)
		return logCodegenError("if condition must be numeric");

	auto* function = context.currentFunction;
	auto* thenBlock = llvm::BasicBlock::Create(context.llvmContext, "then", function);
	auto* elseBlock = llvm::BasicBlock::Create(context.llvmContext, "else");
	auto* mergeBlock = llvm::BasicBlock::Create(context.llvmContext, "ifcont");
	context.builder.CreateCondBr(condition, thenBlock, elseBlock);

	context.builder.SetInsertPoint(thenBlock);
	auto* thenValue = Then->codegen(context);
	if (thenValue == nullptr)
		return nullptr;
	if (context.builder.GetInsertBlock()->getTerminator() == nullptr)
		context.builder.CreateBr(mergeBlock);
	thenBlock = context.builder.GetInsertBlock();

	function->insert(function->end(), elseBlock);
	context.builder.SetInsertPoint(elseBlock);
	auto* elseValue = Else->codegen(context);
	if (elseValue == nullptr)
		return nullptr;
	if (context.builder.GetInsertBlock()->getTerminator() == nullptr)
		context.builder.CreateBr(mergeBlock);
	elseBlock = context.builder.GetInsertBlock();

	function->insert(function->end(), mergeBlock);
	context.builder.SetInsertPoint(mergeBlock);

	std::unordered_set<const FCFunctionAST*> noFunctions;
	// The branch type is inferred independently from the two branches.
	auto* thenType = inferExprType(Then.get(), context, noFunctions);
	auto* elseType = inferExprType(Else.get(), context, noFunctions);
	auto* phiType = thenType->isDoubleTy() || elseType->isDoubleTy()
		? llvm::Type::getDoubleTy(context.llvmContext) : thenType;
	thenValue = castValue(context, thenValue, phiType);
	elseValue = castValue(context, elseValue, phiType);
	if (thenValue == nullptr || elseValue == nullptr)
		return logCodegenError("if branch types do not match");

	auto* phi = context.builder.CreatePHI(phiType, 2, "iftmp");
	phi->addIncoming(thenValue, thenBlock);
	phi->addIncoming(elseValue, elseBlock);
	return phi;
}

llvm::Value* FCForExprAST::codegen(FCCodegenContext& context)
{
	if (context.currentFunction == nullptr || decl == nullptr)
		return logCodegenError("for expression is outside a function");

	auto oldDeclaration = context.namedValues.find(decl.get());
	llvm::AllocaInst* oldAddress = oldDeclaration == context.namedValues.end()
		? nullptr : oldDeclaration->second;
	auto* variable = context.createEntryBlockAlloca(
		context.currentFunction, decl->name, context.getType(decl->typeName));
	context.namedValues[decl.get()] = variable;

	auto* start = Start->codegen(context);
	start = castValue(context, start, variable->getAllocatedType());
	if (start == nullptr)
		return logCodegenError("for start value type mismatch");
	context.builder.CreateStore(start, variable);

	auto* function = context.currentFunction;
	auto* loopBlock = llvm::BasicBlock::Create(context.llvmContext, "loop", function);
	auto* afterBlock = llvm::BasicBlock::Create(context.llvmContext, "afterloop");
	context.builder.CreateBr(loopBlock);
	context.builder.SetInsertPoint(loopBlock);

	auto* end = End->codegen(context);
	auto* condition = createCondition(context, end);
	if (condition == nullptr)
		return logCodegenError("for end condition must be numeric");
	auto* bodyBlock = llvm::BasicBlock::Create(context.llvmContext, "loopbody");
	context.builder.CreateCondBr(condition, bodyBlock, afterBlock);

	function->insert(function->end(), bodyBlock);
	context.builder.SetInsertPoint(bodyBlock);
	if (Body->codegen(context) == nullptr)
		return nullptr;

	if (context.builder.GetInsertBlock()->getTerminator() == nullptr)
	{
		auto* step = Step == nullptr
			? llvm::ConstantInt::get(integerType(context), 1)
			: Step->codegen(context);
		step = castValue(context, step, variable->getAllocatedType());
		if (step == nullptr)
			return logCodegenError("for step value type mismatch");

		auto* current = context.builder.CreateLoad(
			variable->getAllocatedType(), variable, decl->name);
		auto* next = variable->getAllocatedType()->isFloatingPointTy()
			? context.builder.CreateFAdd(current, step, "nextvar")
			: context.builder.CreateAdd(current, step, "nextvar");
		context.builder.CreateStore(next, variable);
		context.builder.CreateBr(loopBlock);
	}

	function->insert(function->end(), afterBlock);
	context.builder.SetInsertPoint(afterBlock);

	if (oldDeclaration == context.namedValues.end())
		context.namedValues.erase(decl.get());
	else
		context.namedValues[decl.get()] = oldAddress;
	return llvm::ConstantInt::get(integerType(context), 0);
}

llvm::Value* FCSeqExprAST::codegen(FCCodegenContext& context)
{
	llvm::Value* last = nullptr;
	for (const auto& expression : exprs)
	{
		last = expression->codegen(context);
		if (last == nullptr)
			return nullptr;
	}
	return last;
}

llvm::Value* FCVarDeclExprAST::codegen(FCCodegenContext& context)
{
	if (context.currentFunction == nullptr || decl == nullptr)
		return logCodegenError("variable declaration is outside a function");

	auto* variable = context.createEntryBlockAlloca(
		context.currentFunction, decl->name, context.getType(decl->typeName));
	context.namedValues[decl.get()] = variable;

	llvm::Value* initialValue = initExpr == nullptr
		? llvm::Constant::getNullValue(variable->getAllocatedType())
		: initExpr->codegen(context);
	initialValue = castValue(context, initialValue, variable->getAllocatedType());
	if (initialValue == nullptr)
		return logCodegenError("variable initializer type mismatch");

	context.builder.CreateStore(initialValue, variable);
	return initialValue;
}

llvm::Value* FCProgramAST::codegen(FCCodegenContext& context)
{
	for (const auto& statement : m_statements)
	{
		if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
			context.definitions[function->getProtoName()] = function;
	}

	for (const auto& statement : m_statements)
	{
		if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
		{
			if (declareFunction(function, context) == nullptr)
				return nullptr;
		}
	}

	for (const auto& statement : m_statements)
	{
		if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
		{
			if (function->codegen(context) == nullptr)
				return nullptr;
		}
	}

	std::vector<const FCExprAST*> runtimeStatements;
	for (const auto& statement : m_statements)
	{
		if (dynamic_cast<FCFunctionAST*>(statement.get()) == nullptr)
			runtimeStatements.push_back(statement.get());
	}
	if (runtimeStatements.empty())
		return m_statements.empty() ? nullptr : m_statements.back()->codegen(context);

	auto* mainFunction = context.module->getFunction("__fc_main");
	if (mainFunction == nullptr)
	{
		auto* mainType = llvm::FunctionType::get(integerType(context), {}, false);
		mainFunction = llvm::Function::Create(
			mainType,
			llvm::Function::InternalLinkage,
			"__fc_main",
			context.module.get());
	}
	if (!mainFunction->empty())
		return mainFunction;

	auto oldInsertPoint = context.builder.saveIP();
	auto oldFunction = context.currentFunction;
	auto oldNamedValues = std::move(context.namedValues);
	context.namedValues.clear();
	context.currentFunction = mainFunction;
	context.builder.SetInsertPoint(
		llvm::BasicBlock::Create(context.llvmContext, "entry", mainFunction));

	llvm::Value* last = nullptr;
	for (const auto* statement : runtimeStatements)
	{
		last = const_cast<FCExprAST*>(statement)->codegen(context);
		if (last == nullptr)
		{
			context.namedValues = std::move(oldNamedValues);
			context.currentFunction = oldFunction;
			context.builder.restoreIP(oldInsertPoint);
			return nullptr;
		}
	}

	last = castValue(context, last, integerType(context));
	if (last == nullptr)
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return logCodegenError("top-level expression must produce an integer");
	}
	context.builder.CreateRet(last);

	const bool invalid = llvm::verifyFunction(*mainFunction, &llvm::errs());
	context.namedValues = std::move(oldNamedValues);
	context.currentFunction = oldFunction;
	context.builder.restoreIP(oldInsertPoint);
	return invalid ? nullptr : mainFunction;
}

