#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialTypes.h"
#include "TutorialSequenceActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTutorialSubtitleEvent, const FText&, Subtitle, float, MinHoldSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTutorialStepChangedEvent, FName, StepId, int32, StepIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTutorialFinishedEvent, bool, bCompleted);

UCLASS()
class ATutorialSequenceActor : public AActor
{
	GENERATED_BODY()

public:
	ATutorialSequenceActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Steps")
	TArray<FTutorialStep> Steps;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Audio")
	bool bUse2DAudio = true;

	// ====== Debug ======
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Debug")
	bool bEnableDebugHotkeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Debug")
	bool bPrintDebugOnScreen = true;

	// ====== Events ======
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialSubtitleEvent OnSubtitle;

	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialStepChangedEvent OnStepChanged;

	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialFinishedEvent OnFinished;

	// ====== Control ======
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartTutorial();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SkipTutorial();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void RestartTutorial();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ReportAction(ETutorialAction Action, FName TargetTag);

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	int32 GetCurrentStepIndex() const { return CurrentIndex; }

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	FName GetCurrentStepId() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	int32 CurrentIndex = INDEX_NONE;

	UPROPERTY(Transient)
	int32 CurrentCount = 0;

	FTimerHandle MinHoldTimer;
	FTimerHandle MaxWaitTimer;

	bool bMinHoldSatisfied = false;

	FTimerHandle SubtitlePageTimer;
	int32 CurrentPageIndex = 0;

	void StartSubtitlePages(const FTutorialStep& Step);
	void StopSubtitlePages();
	void BroadcastCurrentSubtitlePage(const FTutorialStep& Step);

	void EnterStep(int32 NewIndex);
	void CompleteStepIfReady();
	void ForceCompleteStep();

	bool DoesActionMatchStep(const FTutorialStep& Step, ETutorialAction Action, FName TargetTag) const;
	void ClearTimers();

	// ===== Debug Hotkeys =====
	void BindDebugInput();
	void DebugAdvanceStep(); // F2
	void DebugPrint(const FString& Msg) const;
};
