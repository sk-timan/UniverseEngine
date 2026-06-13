#include "ReflectionGeneratorApp.h"

#include <iostream>

int main(int InArgc, char* InArgv[])
{
	FReflectionGeneratorOptions Options{};
	std::string ErrorMessage;
	if (!ParseReflectionGeneratorOptions(InArgc, InArgv, &Options, &ErrorMessage))
	{
		std::cerr << ErrorMessage << std::endl;
		return 1;
	}

	return RunReflectionGenerator(Options);
}
