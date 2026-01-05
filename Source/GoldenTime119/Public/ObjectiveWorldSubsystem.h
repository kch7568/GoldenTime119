#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ObjectiveTypes.h"
#include "ObjectiveWorldSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveChanged, const FObjectiveState&, NewState);

UCLASS()
class GOLDENTIME119_API UObjectiveWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 현재 목표 설정(언제든 교체 가능)
	UFUNCTION(BlueprintCallable, Category = "Objective")
	void SetObjective(const FObjectiveState& NewState);

	// 목표 완료/실패
	UFUNCTION(BlueprintCallable, Category = "Objective")
	void CompleteObjective(FName ObjectiveId);

	UFUNCTION(BlueprintCallable, Category = "Objective")
	void FailObjective(FName ObjectiveId);

	// 무전기/UX가 읽기
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Objective")
	const FObjectiveState& GetCurrentObjective() const { return CurrentObjective; }

	// 목표 변경 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Objective")
	FOnObjectiveChanged OnObjectiveChanged;

private:
	UPROPERTY()
	FObjectiveState CurrentObjective;

	void BroadcastChanged();
};
