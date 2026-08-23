#pragma once

#include <map>

#include "token.h"

namespace FCMarks
{
class FCSemanticContext
{
public:
	FCSemanticContext();

	void reset();
	int getOperatorPrecedence(char op) const;

	void pushScopeForFunc(const std::string& functionName);
	void popScopeForFunc(const std::string& functionName);
	VarDeclPtr lookupVariableInCurrentScope(const std::string& functionName,
		const std::string& name) const;
	VarDeclPtr lookupVariableDecl(const std::string& functionName,
		const std::string& name) const;
	void insertVariableInCurrentScope(const std::string& functionName,
		const std::string& name, VarDeclPtr declaration);
	VarDeclPtr lookupGlobalVariable(const std::string& name) const;
	void insertGlobalVariable(const std::string& name, VarDeclPtr declaration);

	std::vector<VarDeclPtr>& functionDeclarations(const std::string& functionName);
	const std::vector<VarDeclPtr>& functionDeclarations(const std::string& functionName) const;
	const std::vector<VarDeclPtr>& globalDeclarations() const;

private:
	std::map<char, int> m_binopPrecedence;
	std::unordered_map<std::string, ScopeStack> m_varTableInFunc;
	std::unordered_map<std::string, std::vector<VarDeclPtr>> m_funcDeclList;
	Scope m_globalScope;
	std::vector<VarDeclPtr> m_globalDeclList;
};
}
