#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectiveTypes.h"
#include "RadioObjectiveActor.generated.h"

class UObjectiveWorldSubsystem;

UCLASS()
class GOLDENTIME119_API ARadioObjectiveActor : public AActor
{
	GENERATED_BODY()

public:
	ARadioObjectiveActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 목표 갱신 시 BP에서 UI/텍스트/사운드 업데이트
	UFUNCTION(BlueprintImplementableEvent, Category = "Radio")
	void BP_OnObjectiveChanged(const FObjectiveState& NewObjective);

	// 현재 목표 즉시 동기화(상호작용 버튼 등에서 사용 가능)
	UFUNCTION(BlueprintCallable, Category = "Radio")
	FObjectiveState GetCurrentObjectiveSnapshot() const;

private:
	UPROPERTY()
	UObjectiveWorldSubsystem* ObjectiveSubsystem = nullptr;

	UFUNCTION()
	void HandleObjectiveChanged(const FObjectiveState& NewState);

	void BindSubsystem();
	void UnbindSubsystem();
};
