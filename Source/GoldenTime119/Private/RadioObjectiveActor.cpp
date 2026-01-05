#include "RadioObjectiveActor.h"
#include "ObjectiveWorldSubsystem.h"

ARadioObjectiveActor::ARadioObjectiveActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARadioObjectiveActor::BeginPlay()
{
	Super::BeginPlay();
	BindSubsystem();

	// 시작 시 현재 목표가 이미 존재할 수 있으니 동기화
	if (ObjectiveSubsystem)
	{
		HandleObjectiveChanged(ObjectiveSubsystem->GetCurrentObjective());
	}
}

void ARadioObjectiveActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSubsystem();
	Super::EndPlay(EndPlayReason);
}

void ARadioObjectiveActor::BindSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		ObjectiveSubsystem = World->GetSubsystem<UObjectiveWorldSubsystem>();
		if (ObjectiveSubsystem)
		{
			ObjectiveSubsystem->OnObjectiveChanged.AddDynamic(this, &ARadioObjectiveActor::HandleObjectiveChanged);
		}
	}
}

void ARadioObjectiveActor::UnbindSubsystem()
{
	if (ObjectiveSubsystem)
	{
		ObjectiveSubsystem->OnObjectiveChanged.RemoveDynamic(this, &ARadioObjectiveActor::HandleObjectiveChanged);
		ObjectiveSubsystem = nullptr;
	}
}

void ARadioObjectiveActor::HandleObjectiveChanged(const FObjectiveState& NewState)
{
	// BP로 넘겨서 무전기 UI/텍스트/사운드 갱신
	BP_OnObjectiveChanged(NewState);
}

FObjectiveState ARadioObjectiveActor::GetCurrentObjectiveSnapshot() const
{
	if (ObjectiveSubsystem)
	{
		return ObjectiveSubsystem->GetCurrentObjective();
	}

	return FObjectiveState{};
}
