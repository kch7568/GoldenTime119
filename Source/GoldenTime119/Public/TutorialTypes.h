#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"
#include "TutorialTypes.generated.h"

UENUM(BlueprintType)
enum class ETutorialAction : uint8
{
	None UMETA(DisplayName = "None"),

	// P2: locomotion / UI
	Move UMETA(DisplayName = "Move"),
	Turn UMETA(DisplayName = "Turn"),
	Grab UMETA(DisplayName = "Grab"),
	RadioOpen UMETA(DisplayName = "RadioOpen"),
	WristHUDOpen UMETA(DisplayName = "WristHUDOpen"),

	// P3: equipment / door
	DoorKnobCheck UMETA(DisplayName = "DoorKnobCheck"),
	OpenDoor UMETA(DisplayName = "OpenDoor"),
	UseExtinguisher UMETA(DisplayName = "UseExtinguisher"),
	UseHoseFog UMETA(DisplayName = "UseHoseFog"),
	UseAxe UMETA(DisplayName = "UseAxe"),
	TurnValve UMETA(DisplayName = "TurnValve"),

	// P4/P5: theory -> drill
	IdentifyFuelFire UMETA(DisplayName = "IdentifyFuelFire"),
	IdentifyElectricalFire UMETA(DisplayName = "IdentifyElectricalFire"),
	IdentifyFlashoverSign UMETA(DisplayName = "IdentifyFlashoverSign"),
	RescueCivilian UMETA(DisplayName = "RescueCivilian"),
	Exfil UMETA(DisplayName = "Exfil"),
};

USTRUCT(BlueprintType)
struct FTutorialStep
{
	GENERATED_BODY()

	// UI 표시용 (예: "P2-2 이동")
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName StepId = NAME_None;

	// 자막/지시 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Subtitle;

	// 나레이션(김요한 소방장)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> Voice = nullptr;

	// 음성 길이가 없거나, 강제 최소 노출 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinHoldSeconds = 0.5f;

	// 추가: 여러 페이지 자막
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	TArray<FText> SubtitlePages;

	// 추가: 페이지 자동 넘김 간격(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SubtitlePageInterval = 2.5f;

	// 자동 완료(설명만 하고 넘어갈 때)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAutoComplete = false;

	// 행동 완료가 필요한 경우
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETutorialAction RequiredAction = ETutorialAction::None;

	// 같은 액션이라도 특정 태그/대상으로 구분하고 싶을 때(옵션)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RequiredTargetTag = NAME_None;

	// RequiredAction을 몇 번 충족해야 완료되는지(예: Move 2초 유지, Valve 3회 돌림 등은 외부에서 count 올려줌)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredCount = 1;

	// 타임아웃으로 강제 진행(원하면)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxWaitSeconds = 0.f;


};
