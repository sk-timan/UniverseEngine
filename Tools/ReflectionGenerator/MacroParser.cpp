#include "MacroParser.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
std::string Trim(std::string InValue)
{
	while (!InValue.empty() && std::isspace(static_cast<unsigned char>(InValue.front())))
	{
		InValue.erase(InValue.begin());
	}

	while (!InValue.empty() && std::isspace(static_cast<unsigned char>(InValue.back())))
	{
		InValue.pop_back();
	}

	return InValue;
}

bool StartsWithMacro(const std::string& InLine, const char* InMacroName, std::string* OutArguments)
{
	const std::size_t MacroLength = std::strlen(InMacroName);
	if (InLine.size() < MacroLength + 1 || InLine.compare(0, MacroLength, InMacroName) != 0 || InLine[MacroLength] != '(')
	{
		return false;
	}

	if (OutArguments != nullptr)
	{
		*OutArguments = InLine.substr(MacroLength + 1);
		const std::size_t CloseIndex = OutArguments->rfind(')');
		if (CloseIndex != std::string::npos)
		{
			OutArguments->resize(CloseIndex);
		}
		*OutArguments = Trim(*OutArguments);
	}

	return true;
}

bool TryDetectReflectionMacro(const std::string& InLine, EReflectionMacroType* OutMacroType, std::string* OutArguments)
{
	if (OutMacroType == nullptr)
	{
		return false;
	}

	if (StartsWithMacro(InLine, "UCLASS", OutArguments))
	{
		*OutMacroType = EReflectionMacroType::UClass;
		return true;
	}
	if (StartsWithMacro(InLine, "USTRUCT", OutArguments))
	{
		*OutMacroType = EReflectionMacroType::UStruct;
		return true;
	}
	if (StartsWithMacro(InLine, "UENUM", OutArguments))
	{
		*OutMacroType = EReflectionMacroType::UEnum;
		return true;
	}
	if (StartsWithMacro(InLine, "UPROPERTY", OutArguments))
	{
		*OutMacroType = EReflectionMacroType::UProperty;
		return true;
	}
	if (StartsWithMacro(InLine, "UFUNCTION", OutArguments))
	{
		*OutMacroType = EReflectionMacroType::UFunction;
		return true;
	}

	return false;
}

std::vector<std::string> SplitCommaSeparatedFlags(const std::string& InArguments)
{
	std::vector<std::string> Flags;
	std::string Current;
	int ParenthesisDepth = 0;
	bool bInString = false;

	for (char Character : InArguments)
	{
		if (Character == '"')
		{
			bInString = !bInString;
			Current.push_back(Character);
			continue;
		}

		if (!bInString && Character == '(')
		{
			++ParenthesisDepth;
			Current.push_back(Character);
			continue;
		}

		if (!bInString && Character == ')')
		{
			--ParenthesisDepth;
			Current.push_back(Character);
			continue;
		}

		if (!bInString && ParenthesisDepth == 0 && Character == ',')
		{
			const std::string Trimmed = Trim(Current);
			if (!Trimmed.empty())
			{
				Flags.push_back(Trimmed);
			}
			Current.clear();
			continue;
		}

		Current.push_back(Character);
	}

	const std::string Trimmed = Trim(Current);
	if (!Trimmed.empty())
	{
		Flags.push_back(Trimmed);
	}

	return Flags;
}

void ParseMacroArguments(FReflectionMacroRecord* InOutRecord)
{
	if (InOutRecord == nullptr)
	{
		return;
	}

	InOutRecord->Flags = SplitCommaSeparatedFlags(InOutRecord->RawArguments);
	for (const std::string& Flag : InOutRecord->Flags)
	{
		const std::string CategoryPrefix = "Category=";
		if (Flag.rfind(CategoryPrefix, 0) == 0)
		{
			InOutRecord->Category = Flag.substr(CategoryPrefix.size());
			if (InOutRecord->Category.size() >= 2 && InOutRecord->Category.front() == '"' && InOutRecord->Category.back() == '"')
			{
				InOutRecord->Category = InOutRecord->Category.substr(1, InOutRecord->Category.size() - 2);
			}
		}
	}
}

} // namespace

const FReflectionMacroRecord* FindMacroOnOrAboveLine(
	const std::vector<FReflectionMacroRecord>& InMacroRecords,
	int32_t InTargetLine,
	EReflectionMacroType InMacroType)
{
	const FReflectionMacroRecord* BestMatch = nullptr;
	for (const FReflectionMacroRecord& MacroRecord : InMacroRecords)
	{
		if (MacroRecord.MacroType != InMacroType)
		{
			continue;
		}

		if (MacroRecord.Line > InTargetLine)
		{
			continue;
		}

		if (BestMatch == nullptr || MacroRecord.Line > BestMatch->Line)
		{
			BestMatch = &MacroRecord;
		}
	}

	return BestMatch;
}

std::vector<FReflectionMacroRecord> TokenizeAndParseReflectionMacros(const std::filesystem::path& InHeaderPath, std::string* OutErrorMessage)
{
	std::vector<FReflectionMacroRecord> MacroRecords;
	std::ifstream Stream(InHeaderPath);
	if (!Stream.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open header for macro parsing: " + InHeaderPath.string();
		}
		return MacroRecords;
	}

	std::string Line;
	int32_t LineNumber = 0;
	while (std::getline(Stream, Line))
	{
		++LineNumber;
		const std::string TrimmedLine = Trim(Line);
		if (TrimmedLine.empty())
		{
			continue;
		}

		std::string RawArguments;
		EReflectionMacroType MacroType = EReflectionMacroType::UProperty;
		if (!TryDetectReflectionMacro(TrimmedLine, &MacroType, &RawArguments))
		{
			continue;
		}

		int ParenthesisDepth = 0;
		for (char Character : TrimmedLine)
		{
			if (Character == '(')
			{
				++ParenthesisDepth;
			}
			else if (Character == ')')
			{
				--ParenthesisDepth;
			}
		}

		if (ParenthesisDepth != 0)
		{
			if (OutErrorMessage != nullptr)
			{
				std::ostringstream Builder;
				Builder << InHeaderPath.string() << ":" << LineNumber << ": unbalanced parentheses in reflection macro.";
				*OutErrorMessage = Builder.str();
			}
			return {};
		}

		FReflectionMacroRecord MacroRecord{};
		MacroRecord.MacroType = MacroType;
		MacroRecord.Line = LineNumber;
		MacroRecord.RawArguments = RawArguments;
		ParseMacroArguments(&MacroRecord);
		MacroRecords.push_back(std::move(MacroRecord));
	}

	return MacroRecords;
}

void AssociateReflectionMacros(FReflectionClassRecord* InOutClassRecord, const std::vector<FReflectionMacroRecord>& InMacroRecords)
{
	if (InOutClassRecord == nullptr)
	{
		return;
	}

	const EReflectionMacroType ClassMacroType = InOutClassRecord->bIsStruct ? EReflectionMacroType::UStruct : EReflectionMacroType::UClass;
	if (const FReflectionMacroRecord* ClassMacro = FindMacroOnOrAboveLine(InMacroRecords, InOutClassRecord->ClassLine, ClassMacroType))
	{
		InOutClassRecord->ClassFlags = ClassMacro->Flags;
	}

	int32_t PreviousMemberLine = InOutClassRecord->ClassLine;
	for (FReflectionFieldRecord& FieldRecord : InOutClassRecord->Fields)
	{
		const FReflectionMacroRecord* PropertyMacro = FindMacroOnOrAboveLine(InMacroRecords, FieldRecord.Line, EReflectionMacroType::UProperty);
		if (PropertyMacro != nullptr && PropertyMacro->Line > PreviousMemberLine)
		{
			FieldRecord.PropertyFlags = PropertyMacro->Flags;
			FieldRecord.Category = PropertyMacro->Category;
			FieldRecord.bIsReflected = true;
		}
		PreviousMemberLine = FieldRecord.Line;
	}

	for (FReflectionMethodRecord& MethodRecord : InOutClassRecord->Methods)
	{
		if (const FReflectionMacroRecord* FunctionMacro = FindMacroOnOrAboveLine(InMacroRecords, MethodRecord.Line, EReflectionMacroType::UFunction))
		{
			MethodRecord.FunctionFlags = FunctionMacro->Flags;
		}
	}
}
