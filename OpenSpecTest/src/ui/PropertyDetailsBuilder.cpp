#include "ui/PropertyDetailsBuilder.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <cctype>
#include <unordered_map>

#include "core/UObject.h"
#include "reflection/Property.h"
#include "reflection/PropertyFlags.h"
#include "reflection/ScriptStruct.h"
#include "ui/DraggableDoubleSpinBox.h"

namespace
{
bool MatchesSearch(const FProperty& InProperty, const std::string& InSearchText)
{
	if (InSearchText.empty())
	{
		return true;
	}

	const auto ContainsInsensitive = [&](const std::string& InHaystack)
	{
		if (InHaystack.size() < InSearchText.size())
		{
			return false;
		}
		for (std::size_t Index = 0; Index <= InHaystack.size() - InSearchText.size(); ++Index)
		{
			bool bMatches = true;
			for (std::size_t CharIndex = 0; CharIndex < InSearchText.size(); ++CharIndex)
			{
				const char Left = InHaystack[Index + CharIndex];
				const char Right = InSearchText[CharIndex];
				if (std::tolower(static_cast<unsigned char>(Left)) != std::tolower(static_cast<unsigned char>(Right)))
				{
					bMatches = false;
					break;
				}
			}
			if (bMatches)
			{
				return true;
			}
		}
		return false;
	};

	if (ContainsInsensitive(InProperty.GetName()))
	{
		return true;
	}
	if (!InProperty.GetMetadata().DisplayName.empty() && ContainsInsensitive(InProperty.GetMetadata().DisplayName))
	{
		return true;
	}
	return false;
}

QString BuildPropertyLabel(const FProperty& InProperty)
{
	if (!InProperty.GetMetadata().DisplayName.empty())
	{
		return QString::fromStdString(InProperty.GetMetadata().DisplayName);
	}
	return QString::fromStdString(InProperty.GetName());
}

void AddStructFields(
	QFormLayout* InLayout,
	void* InStructMemory,
	const UScriptStruct& InStructType,
	const PropertyDetailsBuilder::FPropertyChangedCallback& InOnPropertyChanged,
	bool bCanEdit)
{
	if (InLayout == nullptr || InStructMemory == nullptr)
	{
		return;
	}

	InStructType.ForEachProperty([&](const FProperty& SubProperty)
	{
		if (SubProperty.GetPropertyType() != EPropertyType::Float)
		{
			return;
		}

		const FFloatProperty* FloatProperty = static_cast<const FFloatProperty*>(&SubProperty);
		float Value = 0.0f;
		FloatProperty->GetValue(InStructMemory, &Value);

		auto* Spin = new DraggableDoubleSpinBox(InLayout->parentWidget());
		Spin->setRange(-100000.0, 100000.0);
		Spin->setDecimals(3);
		Spin->setValue(Value);
		Spin->setSingleStep(0.1);
		Spin->SetDragSensitivity(1.0);
		Spin->setEnabled(bCanEdit);
		InLayout->addRow(BuildPropertyLabel(SubProperty), Spin);

		QObject::connect(Spin, &DraggableDoubleSpinBox::valueChanged, InLayout->parentWidget(),
			[InStructMemory, FloatProperty, InOnPropertyChanged](double InNewValue)
			{
				if (FloatProperty != nullptr && InStructMemory != nullptr)
				{
					FloatProperty->SetValue(InStructMemory, static_cast<float>(InNewValue));
				}
				if (InOnPropertyChanged)
				{
					InOnPropertyChanged();
				}
			});
	});
}
} // namespace

QWidget* PropertyDetailsBuilder::BuildPropertiesWidget(
	UObject* InObject,
	QWidget* InParent,
	const std::string& InSearchText,
	const FPropertyChangedCallback& InOnPropertyChanged)
{
	auto* Container = new QWidget(InParent);
	auto* ContainerLayout = new QVBoxLayout(Container);
	ContainerLayout->setContentsMargins(0, 0, 0, 0);

	if (InObject == nullptr)
	{
		return Container;
	}

	std::unordered_map<std::string, QFormLayout*> CategoryLayouts;
	const UClass& Class = InObject->GetClass();
	Class.ForEachProperty([&](const FProperty& Property)
	{
		const bool bCanEdit = HasAnyPropertyFlags(Property.GetFlags(), EPropertyFlags::EditAnywhere);
		const bool bCanView = bCanEdit || HasAnyPropertyFlags(Property.GetFlags(), EPropertyFlags::VisibleAnywhere);
		if (!bCanView || !MatchesSearch(Property, InSearchText))
		{
			return;
		}

		const std::string CategoryName = Property.GetMetadata().Category.empty() ? "General" : Property.GetMetadata().Category;
		QFormLayout*& FormLayout = CategoryLayouts[CategoryName];
		if (FormLayout == nullptr)
		{
			auto* GroupBox = new QGroupBox(QString::fromStdString(CategoryName), Container);
			auto* GroupLayout = new QVBoxLayout(GroupBox);
			FormLayout = new QFormLayout();
			GroupLayout->addLayout(FormLayout);
			ContainerLayout->addWidget(GroupBox);
		}

		switch (Property.GetPropertyType())
		{
		case EPropertyType::Bool:
		{
			const auto* BoolProperty = static_cast<const FBoolProperty*>(&Property);
			bool Value = false;
			BoolProperty->GetValue(InObject, &Value);
			auto* CheckBox = new QCheckBox(FormLayout->parentWidget());
			CheckBox->setChecked(Value);
			CheckBox->setEnabled(bCanEdit);
			FormLayout->addRow(BuildPropertyLabel(Property), CheckBox);
			QObject::connect(CheckBox, &QCheckBox::toggled, Container,
				[InObject, BoolProperty, InOnPropertyChanged](bool bChecked)
				{
					if (BoolProperty != nullptr && InObject != nullptr)
					{
						BoolProperty->SetValue(InObject, bChecked);
					}
					if (InOnPropertyChanged)
					{
						InOnPropertyChanged();
					}
				});
			break;
		}
		case EPropertyType::Float:
		{
			const auto* FloatProperty = static_cast<const FFloatProperty*>(&Property);
			float Value = 0.0f;
			FloatProperty->GetValue(InObject, &Value);
			auto* Spin = new DraggableDoubleSpinBox(FormLayout->parentWidget());
			Spin->setRange(-100000.0, 100000.0);
			Spin->setDecimals(3);
			Spin->setValue(Value);
			Spin->setEnabled(bCanEdit);
			FormLayout->addRow(BuildPropertyLabel(Property), Spin);
			QObject::connect(Spin, &DraggableDoubleSpinBox::valueChanged, Container,
				[InObject, FloatProperty, InOnPropertyChanged](double InNewValue)
				{
					if (FloatProperty != nullptr && InObject != nullptr)
					{
						FloatProperty->SetValue(InObject, static_cast<float>(InNewValue));
					}
					if (InOnPropertyChanged)
					{
						InOnPropertyChanged();
					}
				});
			break;
		}
		case EPropertyType::Struct:
		{
			const auto* StructProperty = static_cast<const FStructProperty*>(&Property);
			if (const UScriptStruct* StructType = StructProperty->GetStructType())
			{
				AddStructFields(
					FormLayout,
					StructProperty->ContainerPtrToValuePtr(InObject),
					*StructType,
					InOnPropertyChanged,
					bCanEdit);
			}
			break;
		}
		default:
			FormLayout->addRow(
				BuildPropertyLabel(Property),
				new QLabel(QStringLiteral("(unsupported type)"), FormLayout->parentWidget()));
			break;
		}
	});

	return Container;
}
