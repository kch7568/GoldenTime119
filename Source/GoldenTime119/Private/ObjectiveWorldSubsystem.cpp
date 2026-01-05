#include "ObjectiveWorldSubsystem.h"

void UObjectiveWorldSubsystem::SetObjective(const FObjectiveState& NewState)
{
	CurrentObjective = NewState;
	if (CurrentObjective.Status == EObjectiveStatus::Inactive)
	{
		CurrentObjective.Status = EObjectiveStatus::Active;
	}
	BroadcastChanged();
}

void UObjectiveWorldSubsystem::CompleteObjective(FName ObjectiveId)
{
	if (CurrentObjective.ObjectiveId == ObjectiveId && CurrentObjective.IsValid())
	{
		CurrentObjective.Status = EObjectiveStatus::Completed;
		BroadcastChanged();
	}
}

void UObjectiveWorldSubsystem::FailObjective(FName ObjectiveId)
{
	if (CurrentObjective.ObjectiveId == ObjectiveId && CurrentObjective.IsValid())
	{
		CurrentObjective.Status = EObjectiveStatus::Failed;
		BroadcastChanged();
	}
}

void UObjectiveWorldSubsystem::BroadcastChanged()
{
	OnObjectiveChanged.Broadcast(CurrentObjective);
}
