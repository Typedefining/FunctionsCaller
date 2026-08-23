#include "scanner.h"

int main(int argc, char* argv[])
{
	FCScanner scanner;
	scanner.analysis("def aaa(a:int b:double) a+b*2.0;");

	return 0;
}