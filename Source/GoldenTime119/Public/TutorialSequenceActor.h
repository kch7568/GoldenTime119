#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialTypes.h"
#include "TutorialSequenceActor.generated.h"

// ============ 델리게이트 선언 ============

/** 자막 변경 이벤트 - 자막 텍스트와 표시 시간 전달 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTutorialSubtitleEvent, const FText&, Subtitle, float, Duration);

/** 스텝 변경 이벤트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTutorialStepChangedEvent, FName, StepId, int32, StepIndex);

/** 튜토리얼 완료 이벤트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTutorialFinishedEvent, bool, bCompleted);

/** 세그먼트 변경 이벤트 (더 상세한 정보 필요시) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTutorialSegmentChangedEvent, int32, SegmentIndex, const FText&, Subtitle, float, Duration);

/** ★ 이벤트 태그 트리거 - 연출용 (페이드인, VFX 등) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTutorialEventTriggered, FName, EventTag);

/**
 * 튜토리얼 시퀀스 관리 액터
 * - 스텝 기반 튜토리얼 진행
 * - 음성 파일 내 무음 구간 기반 자막 타이밍 지원
 * - EventTag로 연출 이벤트 트리거 지원
 */
UCLASS()
class ATutorialSequenceActor : public AActor
{
	GENERATED_BODY()

public:
	ATutorialSequenceActor();

	// ============ 튜토리얼 스텝 데이터 ============

	/** 스텝 데이터 배열 (직접 입력 또는 DataTable에서 로드) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Steps")
	TArray<FTutorialStep> Steps;

	/** DataTable에서 자동 로드할 경우 여기에 할당 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Data")
	UDataTable* StepsDataTable = nullptr;

	/** 음성 파일 폴더 경로 (예: /Game/Audio/Tutorial/) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Data")
	FString AudioFolderPath = TEXT("/Game/Audio/Tutorial/");

	/** BeginPlay에서 DataTable 자동 로드 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Data")
	bool bAutoLoadFromDataTable = true;

	// ============ 오디오 설정 ============

	/** true면 2D 오디오로 재생, false면 액터 위치에서 3D 재생 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Audio")
	bool bUse2DAudio = true;

	// ============ 디버그 설정 ============

	/** 디버그 단축키 활성화 (END: 다음 스텝) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Debug")
	bool bEnableDebugHotkeys = true;

	/** 화면에 디버그 메시지 출력 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Debug")
	bool bPrintDebugOnScreen = true;

	// ============ 이벤트 ============

	/** 자막이 변경될 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialSubtitleEvent OnSubtitle;

	/** 스텝이 변경될 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialStepChangedEvent OnStepChanged;

	/** 튜토리얼이 끝났을 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialFinishedEvent OnFinished;

	/** 세그먼트가 변경될 때 발생 (선택적 사용) */
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialSegmentChangedEvent OnSegmentChanged;

	/** ★ 이벤트 태그가 트리거될 때 발생 - 연출용 */
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FTutorialEventTriggered OnEventTriggered;

	// ============ 제어 함수 ============

	/** 튜토리얼 시작 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartTutorial();

	/** 튜토리얼 스킵 (미완료 상태로 종료) */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SkipTutorial();

	/** 튜토리얼 재시작 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void RestartTutorial();

	/** JSON 파일에서 Steps 로드 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool LoadStepsFromJSON(const FString& FilePath);

	/** DataTable에서 Steps 로드 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool LoadStepsFromDataTable(UDataTable* DataTable);

	/** VoiceFile 경로로 Voice 에셋 로드 (AudioFolderPath 사용) */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void LoadVoiceAssets();

	/** 액션 보고 - 플레이어가 특정 액션 수행 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ReportAction(ETutorialAction Action, FName TargetTag = NAME_None);

	// ============ 상태 조회 ============

	/** 현재 스텝 인덱스 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	int32 GetCurrentStepIndex() const { return CurrentIndex; }

	/** 현재 스텝 ID */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	FName GetCurrentStepId() const;

	/** 현재 세그먼트 인덱스 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	int32 GetCurrentSegmentIndex() const { return CurrentSegmentIndex; }

	/** 튜토리얼 진행 중 여부 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool IsInProgress() const { return Steps.IsValidIndex(CurrentIndex); }

protected:
	virtual void BeginPlay() override;

private:
	// ============ 상태 변수 ============

	UPROPERTY(Transient)
	int32 CurrentIndex = INDEX_NONE;

	UPROPERTY(Transient)
	int32 CurrentCount = 0;

	UPROPERTY(Transient)
	int32 CurrentSegmentIndex = 0;

	bool bMinHoldSatisfied = false;

	// ============ 타이머 핸들 ============

	FTimerHandle MinHoldTimer;
	FTimerHandle MaxWaitTimer;
	FTimerHandle SubtitlePageTimer;

	/** AudioSegments용 - 각 세그먼트마다 개별 타이머 */
	TArray<FTimerHandle> SegmentTimerHandles;

	// ============ 스텝 진행 ============

	void EnterStep(int32 NewIndex);
	void CompleteStepIfReady();
	void ForceCompleteStep();
	bool DoesActionMatchStep(const FTutorialStep& Step, ETutorialAction Action, FName TargetTag) const;
	void ClearTimers();

	// ============ 자막 시스템 ============

	/** 자막 시작 - 스텝의 자막 모드에 따라 적절한 방식 선택 */
	void StartSubtitles(const FTutorialStep& Step);

	/** 모든 자막 타이머 정리 */
	void StopSubtitles();

	/** [모드 1] 타이밍 기반 세그먼트 자막 시작 */
	void StartAudioSegmentSubtitles(const FTutorialStep& Step);

	/** [모드 2] 고정 간격 페이지 자막 시작 */
	void StartPagedSubtitles(const FTutorialStep& Step);

	/** [모드 3] 단일 자막 표시 */
	void ShowSingleSubtitle(const FTutorialStep& Step);

	/** 특정 세그먼트 자막 브로드캐스트 */
	void BroadcastSegment(int32 SegmentIndex, const FText& Text, float Duration, FName EventTag = NAME_None);

	// ============ 디버그 ============

	void BindDebugInput();
	void DebugAdvanceStep();
	void DebugPrint(const FString& Msg) const;
};