#pragma once

#include <QDir>
#include <QFile>
#include "scanner.h"
#include "ui_calculaterUI.h"

class FCCalculater : public QMainWindow
{
public:
	FCCalculater(FCMyFunctional* f, QMainWindow* parent = nullptr);
	QString* m_regExpr;
private:

	void regFunc();
	void exeExpr();
	void loadDllClicked();
	void loadDll(QString);
	void readConfig(QFile*);
	int findDll(QString*);

	void updateFuncLits();
	FCMarks::FCValue resolveExprValue(::std::unique_ptr<FCExprClass::FCExprAST> expression);
	void compileAndRun(const std::string &sourceFile);

	FCScanner* mp_scanner;
	FCMyFunctional* mp_funcInfo;
	Ui::MainWindow* mp_mw;
};