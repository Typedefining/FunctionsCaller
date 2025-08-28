#include "sub.h"

::std::vector<::std::string> FuncNames{
	{"testSub"}
};

FCMarks::FCValue DLL testSub(void* any)
{
	using real_type = ::std::vector<::std::unique_ptr<FCExprClass::FCExprAST>>;
	//转换为真实类型
	auto args = static_cast<real_type*>(any);
	auto& argVec = *args;
	//测试操作数是否全为数字
	bool numbersicFlag = true;
	//记录所有浮点参数和

	auto ff = 1.0;
	auto f2 = 1.0f;

	double sumDoubleRes = 0.0;
	//记录所有整型参数和
	int sumIntRes = 0;
	//判断是否具有浮点值
	bool hasDouble = false;
	//返回值
	FCMarks::FCValue res;
	res.evaluteVal.danglingVal = nullptr;
	res.type = FCMarks::FCValueCategory::Dangle;
	//对所有参数求值
	//并确定参数合法性
	FCMarks::FCValue firstArgValue = {};
	for (size_t i = 0; i < argVec.size(); i++)
	{
		auto tmp = argVec[i]->evaluate();
		if (tmp.type == FCMarks::FCValueCategory::String || tmp.type == FCMarks::FCValueCategory::Dangle)
			return res;//非数字不求值
		if (i == 0)
			firstArgValue = tmp;//保存第一个参数值

		//分别计算浮点数和、整型数和
		if (tmp.type == FCMarks::FCValueCategory::Floating)
		{
			sumDoubleRes += tmp.evaluteVal.doubleVal;
		}
		else {
			sumIntRes += tmp.evaluteVal.intVal;
		}
	}
	//由于sumDoubles + sumIntRes为所有参数的和
	//sub的值计算为首个参数素减剩余参数和
	//则 "(sumDoubles + sumIntRes) - 首个参数" 为剩余参数和
	//
	//即 首个参数*2 - sumDoubles - sumIntRes为sub值
	if (firstArgValue.type == FCMarks::FCValueCategory::Floating)
	{//此时必然存在浮点数，求值结果也为浮点数
		//记录首个参数的值的二倍
		double tmp = firstArgValue.evaluteVal.doubleVal * 2;
		//获取sub值
		res.evaluteVal.doubleVal = tmp - sumDoubleRes - sumIntRes;
		//返回值为浮点类型
		res.type = FCMarks::FCValueCategory::Floating;
		return res;
	}
	//记录首个参数的值的二倍
	int tmp = firstArgValue.evaluteVal.intVal * 2;//首个参数必为整型，但其他未知
	if (hasDouble)
	{
		res.type = FCMarks::FCValueCategory::Floating;
		res.evaluteVal.doubleVal = tmp - sumDoubleRes - sumIntRes;
	}
	else
	{
		res.type = FCMarks::FCValueCategory::Integer;
		res.evaluteVal.intVal = tmp - sumIntRes;
	}
	return res;
}

DLL void giveName(void* names)
{
	using real_type = ::std::vector<::std::string>;
	auto realObj = static_cast<real_type*>(names);

	for (auto& i : FuncNames)
		realObj->push_back(i);
	return;
}