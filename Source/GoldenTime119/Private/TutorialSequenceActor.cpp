#include "TutorialSequenceActor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "FollowSubtitleActor.h"
#include "WorldSubtitleAnchorActor.h"
#include "InputCoreTypes.h"

ATutorialSequenceActor::ATutorialSequenceActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATutorialSequenceActor::BeginPlay()
{
	Super::BeginPlay();

	// DataTable에서 자동 로드
	if (bAutoLoadFromDataTable && StepsDataTable)
	{
		LoadStepsFromDataTable(StepsDataTable);
		LoadVoiceAssets();
		DebugPrint(TEXT("[Tutorial] Auto-loaded from DataTable"));
	}

	if (bEnableDebugHotkeys)
	{
		BindDebugInput();
	}

	// FollowSubtitleActor 바인딩
	AFollowSubtitleActor* Anchor = Cast<AFollowSubtitleActor>(
		UGameplayStatics::GetActorOfClass(this, AFollowSubtitleActor::StaticClass())
	);

	if (Anchor)
	{
		OnSubtitle.AddDynamic(Anchor, &AFollowSubtitleActor::HandleTutorialSubtitle);
		DebugPrint(TEXT("[Tutorial] Bound to FollowSubtitleActor OK"));
	}
	else
	{
		DebugPrint(TEXT("[Tutorial] FollowSubtitleActor NOT FOUND in level"));
	}
}

// ============================================================================
// 디버그
// ============================================================================

void ATutorialSequenceActor::BindDebugInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
		return;

	EnableInput(PC);

	if (!InputComponent)
		return;

	InputComponent->BindKey(EKeys::End, IE_Pressed, this, &ATutorialSequenceActor::DebugAdvanceStep);

	DebugPrint(TEXT("[Tutorial] Debug hotkeys enabled: END = Advance Step"));
}

void ATutorialSequenceActor::DebugAdvanceStep()
{
	if (CurrentIndex == INDEX_NONE)
	{
		DebugPrint(TEXT("[Tutorial][END] StartTutorial()"));
		StartTutorial();
		return;
	}

	if (!Steps.IsValidIndex(CurrentIndex))
	{
		DebugPrint(TEXT("[Tutorial][END] No valid step. (Already finished?)"));
		return;
	}

	const FName StepId = Steps[CurrentIndex].StepId;
	DebugPrint(FString::Printf(TEXT("[Tutorial][END] Force advance from Step %d (%s)"),
		CurrentIndex, *StepId.ToString()));

	ClearTimers();
	bMinHoldSatisfied = true;
	EnterStep(CurrentIndex + 1);
}

void ATutorialSequenceActor::DebugPrint(const FString& Msg) const
{
	if (!bPrintDebugOnScreen)
		return;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, Msg);
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
}

// ============================================================================
// 상태 조회
// ============================================================================

FName ATutorialSequenceActor::GetCurrentStepId() const
{
	if (Steps.IsValidIndex(CurrentIndex))
	{
		return Steps[CurrentIndex].StepId;
	}
	return NAME_None;
}

// ============================================================================
// 튜토리얼 제어
// ============================================================================

void ATutorialSequenceActor::StartTutorial()
{
	if (Steps.Num() <= 0)
	{
		OnFinished.Broadcast(false);
		DebugPrint(TEXT("[Tutorial] No steps. Finished(false)"));
		return;
	}

	EnterStep(0);
}

void ATutorialSequenceActor::SkipTutorial()
{
	ClearTimers();
	CurrentIndex = INDEX_NONE;
	CurrentCount = 0;
	CurrentSegmentIndex = 0;
	bMinHoldSatisfied = true;

	OnFinished.Broadcast(false);
	DebugPrint(TEXT("[Tutorial] Skipped. Finished(false)"));
}

void ATutorialSequenceActor::RestartTutorial()
{
	ClearTimers();
	CurrentIndex = INDEX_NONE;
	CurrentCount = 0;
	CurrentSegmentIndex = 0;
	bMinHoldSatisfied = false;

	DebugPrint(TEXT("[Tutorial] Restart"));
	StartTutorial();
}

bool ATutorialSequenceActor::LoadStepsFromJSON(const FString& FilePath)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		DebugPrint(FString::Printf(TEXT("[Tutorial] Failed to load JSON: %s"), *FilePath));
		return false;
	}

	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
	{
		DebugPrint(TEXT("[Tutorial] Failed to parse JSON"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* StepsArray;
	if (!JsonValue->TryGetArray(StepsArray))
	{
		DebugPrint(TEXT("[Tutorial] JSON root is not an array"));
		return false;
	}

	Steps.Empty();

	for (const TSharedPtr<FJsonValue>& StepValue : *StepsArray)
	{
		const TSharedPtr<FJsonObject>* StepObject;
		if (!StepValue->TryGetObject(StepObject))
			continue;

		FTutorialStep Step;

		// 기본 정보
		FString StepIdStr;
		if ((*StepObject)->TryGetStringField(TEXT("StepId"), StepIdStr))
			Step.StepId = FName(*StepIdStr);

		(*StepObject)->TryGetStringField(TEXT("VoiceFile"), Step.VoiceFile);
		(*StepObject)->TryGetNumberField(TEXT("TotalDuration"), Step.TotalDuration);
		(*StepObject)->TryGetNumberField(TEXT("MinHoldSeconds"), Step.MinHoldSeconds);
		(*StepObject)->TryGetNumberField(TEXT("MaxWaitSeconds"), Step.MaxWaitSeconds);
		(*StepObject)->TryGetBoolField(TEXT("bAutoComplete"), Step.bAutoComplete);
		(*StepObject)->TryGetNumberField(TEXT("RequiredCount"), Step.RequiredCount);

		FString StepEventTagStr;
		if ((*StepObject)->TryGetStringField(TEXT("StepEventTag"), StepEventTagStr) && !StepEventTagStr.IsEmpty())
			Step.StepEventTag = FName(*StepEventTagStr);

		FString RequiredActionStr;
		if ((*StepObject)->TryGetStringField(TEXT("RequiredAction"), RequiredActionStr))
		{
			// 문자열을 ETutorialAction으로 변환
			if (RequiredActionStr == TEXT("Move")) Step.RequiredAction = ETutorialAction::Move;
			else if (RequiredActionStr == TEXT("Turn")) Step.RequiredAction = ETutorialAction::Turn;
			else if (RequiredActionStr == TEXT("Grab")) Step.RequiredAction = ETutorialAction::Grab;
			else if (RequiredActionStr == TEXT("UseHoseFog")) Step.RequiredAction = ETutorialAction::UseHoseFog;
			else if (RequiredActionStr == TEXT("UseAxe")) Step.RequiredAction = ETutorialAction::UseAxe;
			else if (RequiredActionStr == TEXT("TurnValve")) Step.RequiredAction = ETutorialAction::TurnValve;
			// 필요한 다른 액션들 추가...
			else Step.RequiredAction = ETutorialAction::None;
		}

		FString RequiredTargetTagStr;
		if ((*StepObject)->TryGetStringField(TEXT("RequiredTargetTag"), RequiredTargetTagStr) && !RequiredTargetTagStr.IsEmpty())
			Step.RequiredTargetTag = FName(*RequiredTargetTagStr);

		// AudioSegments 파싱
		const TArray<TSharedPtr<FJsonValue>>* SegmentsArray;
		if ((*StepObject)->TryGetArrayField(TEXT("AudioSegments"), SegmentsArray))
		{
			for (const TSharedPtr<FJsonValue>& SegValue : *SegmentsArray)
			{
				const TSharedPtr<FJsonObject>* SegObject;
				if (!SegValue->TryGetObject(SegObject))
					continue;

				FSubtitleSegment Segment;
				(*SegObject)->TryGetNumberField(TEXT("StartTime"), Segment.StartTime);
				(*SegObject)->TryGetNumberField(TEXT("EndTime"), Segment.EndTime);

				FString SubtitleStr;
				if ((*SegObject)->TryGetStringField(TEXT("SubtitleText"), SubtitleStr))
					Segment.SubtitleText = FText::FromString(SubtitleStr);

				FString EventTagStr;
				if ((*SegObject)->TryGetStringField(TEXT("EventTag"), EventTagStr) && !EventTagStr.IsEmpty())
					Segment.EventTag = FName(*EventTagStr);

				Step.AudioSegments.Add(Segment);
			}
		}

		Steps.Add(Step);
	}

	DebugPrint(FString::Printf(TEXT("[Tutorial] Loaded %d steps from JSON"), Steps.Num()));
	return true;
}

bool ATutorialSequenceActor::LoadStepsFromDataTable(UDataTable* DataTable)
{
	if (!DataTable)
	{
		DebugPrint(TEXT("[Tutorial] DataTable is null"));
		return false;
	}

	Steps.Empty();

	TArray<FTutorialStep*> AllRows;
	DataTable->GetAllRows<FTutorialStep>(TEXT("LoadStepsFromDataTable"), AllRows);

	for (FTutorialStep* Row : AllRows)
	{
		if (Row)
		{
			Steps.Add(*Row);
		}
	}

	DebugPrint(FString::Printf(TEXT("[Tutorial] Loaded %d steps from DataTable"), Steps.Num()));
	return true;
}

void ATutorialSequenceActor::LoadVoiceAssets()
{
	for (FTutorialStep& Step : Steps)
	{
		if (Step.Voice != nullptr || Step.VoiceFile.IsEmpty())
			continue;

		// 확장자 제거하고 에셋 경로 생성
		FString AssetName = FPaths::GetBaseFilename(Step.VoiceFile);
		FString FullPath = AudioFolderPath + AssetName + TEXT(".") + AssetName;

		USoundBase* LoadedSound = LoadObject<USoundBase>(nullptr, *FullPath);
		if (LoadedSound)
		{
			Step.Voice = LoadedSound;
			DebugPrint(FString::Printf(TEXT("[Tutorial] Loaded voice: %s"), *AssetName));
		}
		else
		{
			DebugPrint(FString::Printf(TEXT("[Tutorial] Failed to load voice: %s"), *FullPath));
		}
	}
}

void ATutorialSequenceActor::ReportAction(ETutorialAction Action, FName TargetTag)
{
	if (!Steps.IsValidIndex(CurrentIndex))
		return;

	const FTutorialStep& Step = Steps[CurrentIndex];

	if (Step.bAutoComplete)
		return;

	if (!DoesActionMatchStep(Step, Action, TargetTag))
		return;

	CurrentCount++;

	DebugPrint(FString::Printf(TEXT("[Tutorial] Action reported: %d/%d"),
		CurrentCount, Step.RequiredCount));

	CompleteStepIfReady();
}

bool ATutorialSequenceActor::DoesActionMatchStep(const FTutorialStep& Step, ETutorialAction Action, FName TargetTag) const
{
	if (Step.RequiredAction == ETutorialAction::None)
		return false;

	if (Step.RequiredAction != Action)
		return false;

	if (Step.RequiredTargetTag != NAME_None && Step.RequiredTargetTag != TargetTag)
		return false;

	return true;
}

// ============================================================================
// 스텝 진행
// ============================================================================

void ATutorialSequenceActor::EnterStep(int32 NewIndex)
{
	ClearTimers();

	if (!Steps.IsValidIndex(NewIndex))
	{
		CurrentIndex = INDEX_NONE;
		OnFinished.Broadcast(true);
		DebugPrint(TEXT("[Tutorial] Finished(true)"));
		return;
	}

	CurrentIndex = NewIndex;
	CurrentCount = 0;
	CurrentSegmentIndex = 0;
	bMinHoldSatisfied = false;

	const FTutorialStep& Step = Steps[CurrentIndex];

	// 스텝 변경 이벤트
	OnStepChanged.Broadcast(Step.StepId, CurrentIndex);

	// ★ 스텝 레벨 이벤트 태그 트리거
	if (Step.StepEventTag != NAME_None)
	{
		OnEventTriggered.Broadcast(Step.StepEventTag);
		DebugPrint(FString::Printf(TEXT("[Tutorial] Step Event: %s"), *Step.StepEventTag.ToString()));
	}

	DebugPrint(FString::Printf(TEXT("[Tutorial] Enter Step %d (%s) - Subtitles: %d"),
		CurrentIndex, *Step.StepId.ToString(), Step.GetSubtitleCount()));

	// 음성 재생
	if (Step.Voice)
	{
		if (bUse2DAudio)
		{
			UGameplayStatics::PlaySound2D(this, Step.Voice);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(this, Step.Voice, GetActorLocation());
		}
	}

	// 자막 시작
	StartSubtitles(Step);

	// MinHold 타이머
	if (Step.MinHoldSeconds <= 0.f)
	{
		bMinHoldSatisfied = true;
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			MinHoldTimer,
			FTimerDelegate::CreateLambda([this]()
				{
					bMinHoldSatisfied = true;
					DebugPrint(TEXT("[Tutorial] MinHold satisfied"));
					CompleteStepIfReady();
				}),
			Step.MinHoldSeconds,
			false
		);
	}

	// 자동 완료 또는 MaxWait 타이머
	if (Step.bAutoComplete)
	{
		CompleteStepIfReady();
	}
	else if (Step.MaxWaitSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			MaxWaitTimer,
			this,
			&ATutorialSequenceActor::ForceCompleteStep,
			Step.MaxWaitSeconds,
			false
		);
	}
}

void ATutorialSequenceActor::CompleteStepIfReady()
{
	if (!Steps.IsValidIndex(CurrentIndex))
		return;

	const FTutorialStep& Step = Steps[CurrentIndex];

	if (!bMinHoldSatisfied)
		return;

	if (Step.bAutoComplete)
	{
		EnterStep(CurrentIndex + 1);
		return;
	}

	if (Step.RequiredAction == ETutorialAction::None)
		return;

	if (CurrentCount >= FMath::Max(1, Step.RequiredCount))
	{
		EnterStep(CurrentIndex + 1);
	}
}

void ATutorialSequenceActor::ForceCompleteStep()
{
	if (!Steps.IsValidIndex(CurrentIndex))
		return;

	DebugPrint(FString::Printf(TEXT("[Tutorial] MaxWait timeout -> advance from Step %d"), CurrentIndex));
	EnterStep(CurrentIndex + 1);
}

void ATutorialSequenceActor::ClearTimers()
{
	GetWorldTimerManager().ClearTimer(MinHoldTimer);
	GetWorldTimerManager().ClearTimer(MaxWaitTimer);
	StopSubtitles();
}

// ============================================================================
// 자막 시스템
// ============================================================================

void ATutorialSequenceActor::StartSubtitles(const FTutorialStep& Step)
{
	StopSubtitles();

	if (Step.UsesAudioSegments())
	{
		// [모드 1] 음성 타이밍 기반 세그먼트
		StartAudioSegmentSubtitles(Step);
		DebugPrint(FString::Printf(TEXT("[Tutorial] Using AudioSegments mode (%d segments)"),
			Step.AudioSegments.Num()));
	}
	else if (Step.UsesSubtitlePages())
	{
		// [모드 2] 고정 간격 페이지
		StartPagedSubtitles(Step);
		DebugPrint(FString::Printf(TEXT("[Tutorial] Using SubtitlePages mode (%d pages, %.1fs interval)"),
			Step.SubtitlePages.Num(), Step.SubtitlePageInterval));
	}
	else
	{
		// [모드 3] 단일 자막
		ShowSingleSubtitle(Step);
		DebugPrint(TEXT("[Tutorial] Using single subtitle mode"));
	}
}

void ATutorialSequenceActor::StopSubtitles()
{
	// 페이지 모드 타이머
	GetWorldTimerManager().ClearTimer(SubtitlePageTimer);

	// 세그먼트 모드 타이머들
	for (FTimerHandle& Handle : SegmentTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(Handle);
	}
	SegmentTimerHandles.Empty();

	CurrentSegmentIndex = 0;
}

void ATutorialSequenceActor::StartAudioSegmentSubtitles(const FTutorialStep& Step)
{
	if (Step.AudioSegments.Num() == 0)
		return;

	const FName CurrentStepId = Step.StepId;

	// 각 세그먼트마다 해당 시간에 자막을 표시하는 타이머 설정
	for (int32 i = 0; i < Step.AudioSegments.Num(); i++)
	{
		const FSubtitleSegment& Segment = Step.AudioSegments[i];

		FTimerHandle TimerHandle;

		// 캡처할 데이터
		const int32 SegmentIndex = i;
		const FText SubtitleText = Segment.SubtitleText;
		const float Duration = Segment.GetDuration();
		const FName EventTag = Segment.EventTag;  // ★ 이벤트 태그도 캡처

		if (Segment.StartTime <= 0.f)
		{
			// 즉시 표시 (첫 번째 세그먼트가 0초에 시작하는 경우)
			BroadcastSegment(SegmentIndex, SubtitleText, Duration, EventTag);
		}
		else
		{
			// StartTime 후에 표시
			GetWorldTimerManager().SetTimer(
				TimerHandle,
				FTimerDelegate::CreateLambda([this, CurrentStepId, SegmentIndex, SubtitleText, Duration, EventTag]()
					{
						// 스텝이 바뀌었으면 무시
						if (GetCurrentStepId() != CurrentStepId)
							return;

						BroadcastSegment(SegmentIndex, SubtitleText, Duration, EventTag);
					}),
				Segment.StartTime,
				false
			);

			SegmentTimerHandles.Add(TimerHandle);
		}
	}
}

void ATutorialSequenceActor::StartPagedSubtitles(const FTutorialStep& Step)
{
	if (Step.SubtitlePages.Num() == 0)
		return;

	const FName CurrentStepId = Step.StepId;

	// 첫 페이지 즉시 표시
	CurrentSegmentIndex = 0;
	BroadcastSegment(0, Step.SubtitlePages[0], Step.SubtitlePageInterval);

	// 2개 이상일 때 자동 전환
	if (Step.SubtitlePages.Num() >= 2 && Step.SubtitlePageInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			SubtitlePageTimer,
			FTimerDelegate::CreateLambda([this, CurrentStepId, Step]()
				{
					// 스텝이 바뀌었으면 중단
					if (GetCurrentStepId() != CurrentStepId)
					{
						StopSubtitles();
						return;
					}

					CurrentSegmentIndex++;

					if (CurrentSegmentIndex >= Step.SubtitlePages.Num())
					{
						StopSubtitles();
						return;
					}

					BroadcastSegment(CurrentSegmentIndex, Step.SubtitlePages[CurrentSegmentIndex], Step.SubtitlePageInterval);
				}),
			Step.SubtitlePageInterval,
			true // 반복
		);
	}
}

void ATutorialSequenceActor::ShowSingleSubtitle(const FTutorialStep& Step)
{
	if (Step.Subtitle.IsEmpty())
		return;

	// MinHoldSeconds를 표시 시간으로 사용, 없으면 기본 5초
	float Duration = Step.MinHoldSeconds > 0.f ? Step.MinHoldSeconds : 5.0f;

	BroadcastSegment(0, Step.Subtitle, Duration);
}

void ATutorialSequenceActor::BroadcastSegment(int32 SegmentIndex, const FText& Text, float Duration, FName EventTag)
{
	CurrentSegmentIndex = SegmentIndex;

	// 메인 자막 이벤트
	OnSubtitle.Broadcast(Text, Duration);

	// 세그먼트 변경 이벤트 (선택적 사용)
	OnSegmentChanged.Broadcast(SegmentIndex, Text, Duration);

	// ★ 이벤트 태그가 있으면 브로드캐스트
	if (EventTag != NAME_None)
	{
		OnEventTriggered.Broadcast(EventTag);
		DebugPrint(FString::Printf(TEXT("[Tutorial] Event Triggered: %s"), *EventTag.ToString()));
	}

	DebugPrint(FString::Printf(TEXT("[Tutorial] Subtitle[%d]: %s (%.1fs)"),
		SegmentIndex, *Text.ToString(), Duration));
}