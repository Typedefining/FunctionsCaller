#include "ast_dump.h"

#include "ast.h"

#include <ostream>
#include <sstream>
#include <string>

namespace FCMarks {
using namespace FCExprClass;

namespace {

void indentTo(std::ostream &out, int depth) {
  out << std::string(static_cast<size_t>(depth) * 2, ' ');
}

const char *opSymbol(FCBinaryOp op) {
  switch (op) {
  case FCBinaryOp::Assign:
    return "=";
  case FCBinaryOp::Less:
    return "<";
  case FCBinaryOp::Add:
    return "+";
  case FCBinaryOp::Sub:
    return "-";
  case FCBinaryOp::Mul:
    return "*";
  case FCBinaryOp::Div:
    return "/";
  default:
    return "?";
  }
}

void dumpNode(const FCExprAST &node, std::ostream &out, int depth);

void dumpChildren(const std::vector<std::unique_ptr<FCExprAST>> &children,
                  std::ostream &out, int depth) {
  for (const auto &child : children) {
    if (child)
      dumpNode(*child, out, depth + 1);
  }
}

void dumpParams(const FCPrototypeAST &proto, std::ostream &out) {
  out << "(";
  const auto &args = proto.getArgs();
  for (size_t i = 0; i < args.size(); ++i) {
    if (i != 0)
      out << ", ";
    out << args[i]->name << ":" << args[i]->typeName;
  }
  out << ")";
}

void dumpNode(const FCExprAST &node, std::ostream &out, int depth) {
  if (const auto *program = dynamic_cast<const FCProgramAST *>(&node)) {
    indentTo(out, depth);
    out << "Program (" << program->getStatements().size() << " statements)\n";
    dumpChildren(program->getStatements(), out, depth);
    return;
  }
  if (const auto *block = dynamic_cast<const FCBlockExprAST *>(&node)) {
    indentTo(out, depth);
    out << "Block (" << block->getExpressions().size() << " expressions)\n";
    dumpChildren(block->getExpressions(), out, depth);
    return;
  }
  if (const auto *seq = dynamic_cast<const FCSeqExprAST *>(&node)) {
    indentTo(out, depth);
    out << "Seq (" << seq->getExpressions().size() << " expressions)\n";
    dumpChildren(seq->getExpressions(), out, depth);
    return;
  }
  if (const auto *function = dynamic_cast<const FCFunctionAST *>(&node)) {
    indentTo(out, depth);
    out << "Function " << function->getProtoName();
    if (const auto &proto = function->getProto())
      dumpParams(*proto, out);
    out << "\n";
    if (const auto *body = function->getBody())
      dumpNode(*body, out, depth + 1);
    return;
  }
  if (const auto *proto = dynamic_cast<const FCPrototypeAST *>(&node)) {
    indentTo(out, depth);
    out << "Prototype " << proto->getProtoName();
    dumpParams(*proto, out);
    out << "\n";
    return;
  }
  if (const auto *decl = dynamic_cast<const FCVarDeclExprAST *>(&node)) {
    indentTo(out, depth);
    out << "VarDecl " << decl->decl->name << ":" << decl->decl->typeName << "\n";
    if (decl->initExpr)
      dumpNode(*decl->initExpr, out, depth + 1);
    return;
  }
  if (const auto *loop = dynamic_cast<const FCForExprAST *>(&node)) {
    indentTo(out, depth);
    out << "For " << loop->getDecl()->name << ":" << loop->getDecl()->typeName
        << "\n";
    if (const auto *start = loop->getStart())
      dumpNode(*start, out, depth + 1);
    if (const auto *end = loop->getEnd())
      dumpNode(*end, out, depth + 1);
    if (const auto *step = loop->getStep())
      dumpNode(*step, out, depth + 1);
    if (const auto *body = loop->getBody())
      dumpNode(*body, out, depth + 1);
    return;
  }
  if (const auto *branch = dynamic_cast<const FCIfExprAST *>(&node)) {
    indentTo(out, depth);
    out << "If\n";
    dumpNode(*branch->getCondition(), out, depth + 1);
    dumpNode(*branch->getThen(), out, depth + 1);
    dumpNode(*branch->getElse(), out, depth + 1);
    return;
  }
  if (const auto *call = dynamic_cast<const FCCallExprAST *>(&node)) {
    indentTo(out, depth);
    out << "Call " << call->getName() << "\n";
    dumpChildren(call->getArgs(), out, depth);
    return;
  }
  if (const auto *binary = dynamic_cast<const FCBinaryExprAST *>(&node)) {
    indentTo(out, depth);
    out << "Binary " << opSymbol(binary->getOperator()) << "\n";
    dumpNode(*binary->getLHS(), out, depth + 1);
    dumpNode(*binary->getRHS(), out, depth + 1);
    return;
  }
  if (const auto *variable = dynamic_cast<const FCVariableExprAST *>(&node)) {
    indentTo(out, depth);
    out << "Variable " << variable->name << "\n";
    return;
  }
  if (const auto *text = dynamic_cast<const FCStringExprAST *>(&node)) {
    indentTo(out, depth);
    out << "String \"" << text->m_stringVal << "\"\n";
    return;
  }
  if (const auto *number = dynamic_cast<const FCNumberExprAST *>(&node)) {
    indentTo(out, depth);
    out << "Number ";
    if (number->isFloating())
      out << number->m_doubleVal;
    else
      out << number->m_intVal;
    out << "\n";
    return;
  }
  indentTo(out, depth);
  out << "Expr\n";
}

} // namespace

std::string describeAst(const FCExprAST &node) {
  std::ostringstream out;
  dumpNode(node, out, 0);
  return out.str();
}

} // namespace FCMarks
