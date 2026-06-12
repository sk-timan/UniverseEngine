#pragma once

#include <QWidget>
#include <functional>
#include <string>

class QObject;
class QVBoxLayout;
class UObject;

class PropertyDetailsBuilder
{
public:
	using FPropertyChangedCallback = std::function<void()>;

	static QWidget* BuildPropertiesWidget(
		UObject* InObject,
		QWidget* InParent,
		const std::string& InSearchText,
		const FPropertyChangedCallback& InOnPropertyChanged);
};
