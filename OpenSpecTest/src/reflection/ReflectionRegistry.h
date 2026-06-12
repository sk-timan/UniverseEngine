#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class FProperty;
class UClass;
class UFunction;

class FReflectionRegistry
{
public:
	static FReflectionRegistry& Get();

	void RegisterClass(const UClass* InClass);
	void AddClassProperty(const UClass* InClass, std::unique_ptr<FProperty> InProperty);
	void AddClassFunction(const UClass* InClass, std::unique_ptr<UFunction> InFunction);

	const UClass* FindClassByName(const std::string& InClassName) const;
	const FProperty* FindPropertyByName(const UClass* InClass, const std::string& InPropertyName) const;
	void ForEachProperty(const UClass* InClass, const std::function<void(const FProperty&)>& InCallback) const;
	const UFunction* FindFunctionByName(const UClass* InClass, const std::string& InFunctionName) const;

private:
	struct FClassReflectionData
	{
		std::vector<std::unique_ptr<FProperty>> Properties;
		std::vector<std::unique_ptr<UFunction>> Functions;
	};

	FReflectionRegistry() = default;

	std::unordered_map<std::string, const UClass*> ClassesByName_;
	std::unordered_map<const UClass*, FClassReflectionData> ClassDataByClass_;
};
