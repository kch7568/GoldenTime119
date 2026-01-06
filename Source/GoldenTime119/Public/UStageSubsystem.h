// StageSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "StageTypes.h"       // FStageObjective, (선택) 기타 타입
#include "UStageDataAsset.h"   // UStageDataAsset, FStageStep 등

#include "UStageSubsystem.generated.h" // 반드시 마지막 include

class ARoomActor;
class UVitalComponent;
class AFireActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveChanged, const FStageObjective&, NewObjective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageFinished, bool, bSuccess, float, FinalScore);

UCLASS()
class GOLDENTIME119_API UStageSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable) FOnObjectiveChanged OnObjectiveChanged;
    UPROPERTY(BlueprintAssignable) FOnStageFinished OnStageFinished;

    UFUNCTION(BlueprintCallable) void StartStage(UStageDataAsset* InStage);
    UFUNCTION(BlueprintCallable) void AbortStage(bool bFail);

    UFUNCTION(BlueprintCallable) FStageObjective GetCurrentObjective() const { return CurrentObjective; }
    UFUNCTION(BlueprintCallable) int32 GetCurrentStepIndex() const { return StepIndex; }

private:
    UPROPERTY() TObjectPtr<UStageDataAsset> Stage = nullptr;
    int32 StepIndex = -1;

    FStageObjective CurrentObjective;

    TWeakObjectPtr<UVitalComponent> Vital;
    TMap<FName, TWeakObjectPtr<ARoomActor>> RoomsByTag;

    float StepTimeAcc = 0.f;
    FTimerHandle TickHandle;

    UFUNCTION()
    void TickStage();
private:
    void BuildAutoBindings(UWorld* World);
    void BindRoomSignals(ARoomActor* Room);
    void BindVitalSignals(UVitalComponent* InVital);

    void EnterStep(int32 NewIndex);
    void ExecuteAction(const FStageAction& A);

    bool IsStepComplete(float DeltaSeconds);
    void AdvanceStep();

    UFUNCTION() void HandleVitalsChanged(float Hp01, float Temp01, float O201);
    UFUNCTION() void HandleBackdraftTriggered();
    UFUNCTION() void HandleFireExtinguished(AFireActor* Fire);
};
