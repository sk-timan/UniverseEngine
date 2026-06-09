#pragma once

#include <string>

#include "world/ActorTransform.h"

class AActor;
class USceneComponent;

class FEditorActorTransform
{
public:
	static AActor* GetAttachParentActor(const AActor* InActor);
	static bool IsAttachedToActor(const AActor* InActor);
	static FTransform GetActorWorldTransform(const AActor* InActor);
	static FActorTransform GetEditableTransform(const AActor* InActor);
	static bool SetEditableTransform(AActor* InActor, const FActorTransform& InTransform);
	static bool SetActorWorldTransform(AActor* InActor, const FTransform& InWorldTransform);
	static bool WouldCreateAttachmentCycle(const AActor* InChildActor, const AActor* InNewParentActor);
	static bool ReparentActor(AActor* InChildActor, AActor* InNewParentActor, std::string* OutErrorMessage);
};
