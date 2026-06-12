#pragma once

#include <string>

class FField
{
public:
	explicit FField(std::string InName);
	virtual ~FField() = default;

	const std::string& GetName() const;

private:
	std::string Name_;
};
