#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectiveTypes.h"
#include "FireDirectorWorldSubsystem.h"
#include "StageRunner.generated.h"

class UObjectiveWorldSubsystem;

UENUM(BlueprintType)
enum class EStageRunState : uint8
{
	NotStarted,
	Running,
	Completed,
	Failed
};

UENUM(BlueprintType)
enum class EExampleStageStep : uint8
{
	None,
	Step1_Intro,
	Step2_IgniteAndEntryAllowed,
	Step3_AfterLivingRoomExtinguished,
	// Step4... 이후 확장
};

UCLASS()
class GOLDENTIME119_API AStageRunner : public AActor
{
	GENERATED_BODY()

public:
	AStageRunner();

	UFUNCTION(BlueprintCallable, Category = "Stage")
	void StartExampleStage();

	UFUNCTION(BlueprintCallable, Category = "Stage")

	UPROPERTY()
	UFireDirectorWorldSubsystem* FireDirector = nullptr;

	UFUNCTION()
	void HandleLivingRoomAllExtinguished(FName RoomId, ARoomActor* Room);
	EExampleStageStep GetCurrentStep() const { return CurrentStep; }


protected:
	virtual void BeginPlay() override;

	// Step 진입 훅: BP로 넘겨서 무전 음성 재생 등 처리 가능
	UFUNCTION(BlueprintImplementableEvent, Category = "Stage")
	void BP_OnEnterStep(EExampleStageStep Step);

private:
	UPROPERTY()
	UObjectiveWorldSubsystem* ObjectiveSubsystem = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Stage")
	EStageRunState RunState = EStageRunState::NotStarted;

	UPROPERTY(VisibleAnywhere, Category = "Stage")
	EExampleStageStep CurrentStep = EExampleStageStep::None;

	FTimerHandle StepTimerHandle;

	void BindSubsystems();

	void EnterStep(EExampleStageStep Step);
	void SetupStep1();
	void SetupStep2();

	void AdvanceToStep2_ByTimer();
};
