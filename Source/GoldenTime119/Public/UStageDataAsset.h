// StageDataAsset.h
// StageDataAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundBase.h"     // USoundBase 때문에 필요
#include "StageTypes.h"          // FStageObjective

#include "UStageDataAsset.generated.h" // 반드시 마지막 include


UENUM(BlueprintType)
enum class EStageCondType : uint8
{
    None,
    FiresExtinguishedInRoom,
    BackdraftTriggeredInRoom,
    TimeElapsed,
    VitalBelow,            // Hp01 <= X 같은 것
};

USTRUCT(BlueprintType)
struct FStageCondition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) EStageCondType Type = EStageCondType::None;

    // Tag로 방/문 등을 지정: "Room.Bedroom"
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TargetTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Value = 0.f; // Time seconds / Vital threshold 등
};

UENUM(BlueprintType)
enum class EStageActionType : uint8
{
    None,
    SetObjective,
    PlayRadio,
    SpawnHostage,
    IgniteRoomRandomFires, // Value=Count
    IgniteRoomAllFires,
    SetRoomRisk,           // Value=0/1
    SetRoomBackdraftReady, // 강제 장전/해제(추가 API 필요)
};

USTRUCT(BlueprintType)
struct FStageAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) EStageActionType Type = EStageActionType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TargetTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Value = 0.f;

    // 목적/음성 등
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FStageObjective Objective;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundBase> RadioSound = nullptr;
};

USTRUCT(BlueprintType)
struct FStageStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FStageAction> OnEnterActions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FStageCondition CompleteCondition;
};

UCLASS()
class UStageDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StageId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FStageStep> Steps;
};
