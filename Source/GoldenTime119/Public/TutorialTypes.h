#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"
#include "Engine/DataTable.h"
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

/**
 * 음성 내 개별 자막 세그먼트
 * - 하나의 음성 파일 내에서 무음 구간으로 구분된 각 발화 단위
 * - StartTime에 해당 자막이 표시됨
 */
USTRUCT(BlueprintType)
struct FSubtitleSegment
{
	GENERATED_BODY()

	/** 이 세그먼트가 시작되는 시간 (음성 파일 시작으로부터 초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle")
	float StartTime = 0.f;

	/** 이 세그먼트가 끝나는 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle")
	float EndTime = 0.f;

	/** 이 구간에 표시할 자막 텍스트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle", meta = (MultiLine = "true"))
	FText SubtitleText;

	/**
	 * 이 세그먼트 시작 시 발생시킬 이벤트 태그 (선택사항)
	 * - 예: "ShowTable", "HighlightHose", "FadeInFire" 등
	 * - 비워두면 이벤트 발생 안 함
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle|Event")
	FName EventTag;

	/** 세그먼트 길이 (초) */
	float GetDuration() const { return FMath::Max(0.f, EndTime - StartTime); }
};

/**
 * 튜토리얼 단일 스텝
 * - FTableRowBase 상속으로 DataTable Import 지원
 */
USTRUCT(BlueprintType)
struct FTutorialStep : public FTableRowBase
{
	GENERATED_BODY()

	// ============ 기본 정보 ============

	/** UI 표시용 (예: "Step1", "Step2" 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	FName StepId = NAME_None;

	/**
	 * 나레이션 음성 파일 경로 (DataTable Import용)
	 * - JSON에서 "1_안녕하세요.wav" 형태로 들어옴
	 * - 런타임에 이 경로로 Voice 에셋 로드
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Audio")
	FString VoiceFile;

	/** 나레이션 음성 에셋 (에디터에서 직접 할당하거나, VoiceFile로 로드) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Audio")
	TObjectPtr<USoundBase> Voice = nullptr;

	/**
	 * 이 스텝 시작 시 발생시킬 이벤트 태그 (선택사항)
	 * - AudioSegments의 EventTag와 별개로 스텝 진입 시 즉시 발생
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Event")
	FName StepEventTag;

	// ============ 자막 (3가지 방식 지원 - 우선순위 순) ============

	/**
	 * [방식 1 - 최우선] 음성 타이밍 기반 자막 세그먼트
	 * - 음성 파일의 무음 구간 분석으로 얻은 타이밍 데이터 사용
	 * - 이 배열이 비어있지 않으면 이 방식 사용
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Subtitle", meta = (DisplayName = "Audio Segments (Timing-based)"))
	TArray<FSubtitleSegment> AudioSegments;

	/**
	 * [방식 2] 수동 페이지 자막 (고정 간격)
	 * - AudioSegments가 비어있을 때 사용
	 * - SubtitlePageInterval 간격으로 자동 전환
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Subtitle", meta = (MultiLine = "true"))
	TArray<FText> SubtitlePages;

	/** SubtitlePages 전환 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Subtitle")
	float SubtitlePageInterval = 2.5f;

	/**
	 * [방식 3] 단일 자막
	 * - AudioSegments와 SubtitlePages가 모두 비어있을 때 사용
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Subtitle", meta = (MultiLine = "true"))
	FText Subtitle;

	// ============ 타이밍 ============

	/** 음성 총 길이 (초) - JSON에서 자동 입력됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Timing")
	float TotalDuration = 0.f;

	/** 강제 최소 노출 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Timing")
	float MinHoldSeconds = 0.5f;

	/** 타임아웃으로 강제 진행(원하면) - 0이면 무제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Timing")
	float MaxWaitSeconds = 0.f;

	// ============ 완료 조건 ============

	/** 자동 완료(설명만 하고 넘어갈 때) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Completion")
	bool bAutoComplete = true;

	/** 행동 완료가 필요한 경우 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Completion")
	ETutorialAction RequiredAction = ETutorialAction::None;

	/** 같은 액션이라도 특정 태그/대상으로 구분하고 싶을 때(옵션) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Completion")
	FName RequiredTargetTag = NAME_None;

	/** RequiredAction을 몇 번 충족해야 완료되는지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Completion")
	int32 RequiredCount = 1;

	// ============ 헬퍼 함수 ============

	/** 타이밍 기반 세그먼트 사용 여부 */
	bool UsesAudioSegments() const { return AudioSegments.Num() > 0; }

	/** 고정 간격 페이지 사용 여부 */
	bool UsesSubtitlePages() const { return !UsesAudioSegments() && SubtitlePages.Num() > 0; }

	/** 단일 자막 사용 여부 */
	bool UsesSingleSubtitle() const { return !UsesAudioSegments() && SubtitlePages.Num() == 0; }

	/** 전체 자막 개수 */
	int32 GetSubtitleCount() const
	{
		if (UsesAudioSegments()) return AudioSegments.Num();
		if (UsesSubtitlePages()) return SubtitlePages.Num();
		return Subtitle.IsEmpty() ? 0 : 1;
	}
};