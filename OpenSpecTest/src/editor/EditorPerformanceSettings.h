#pragma once

#include "editor/EditorTypes.h"

struct FEditorPerformanceSettings
{
	EPickTriangleBvhSplitMethod TriangleBvhSplitMethod = EPickTriangleBvhSplitMethod::Median;
};
