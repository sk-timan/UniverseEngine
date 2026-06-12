#include "reflection/Field.h"

#include <utility>

FField::FField(std::string InName)
	: Name_(std::move(InName))
{
}

const std::string& FField::GetName() const
{
	return Name_;
}
