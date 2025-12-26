#include "TutorialSequenceActor.h"
#include "Kismet/GameplayStatics.h"
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

	if (bEnableDebugHotkeys)
	{
		BindDebugInput();
	}
	AWorldSubtitleAnchorActor* Anchor = Cast<AWorldSubtitleAnchorActor>(
		UGameplayStatics::GetActorOfClass(this, AWorldSubtitleAnchorActor::StaticClass())
	);

	if (Anchor)
	{
		OnSubtitle.AddDynamic(Anchor, &ASubtitleBaseActor::HandleTutorialSubtitle);
		DebugPrint(TEXT("[Tutorial] Bound to WorldSubtitleAnchorActor OK"));
	}
	else
	{
		DebugPrint(TEXT("[Tutorial] WorldSubtitleAnchorActor NOT FOUND in level"));
	}
	/*AFollowSubtitleActor* Follow = Cast<AFollowSubtitleActor>(
		UGameplayStatics::GetActorOfClass(this, AFollowSubtitleActor::StaticClass())
	);
	if (Follow)
	{
		OnSubtitle.AddDynamic(Follow, &ASubtitleBaseActor::HandleTutorialSubtitle);
	}*/
}

void ATutorialSequenceActor::BindDebugInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
		return;

	// Actor가 입력을 받도록 활성화
	EnableInput(PC);

	if (!InputComponent)
		return;

	// F2: 다음 스텝 강제 진행
	InputComponent->BindKey(EKeys::End, IE_Pressed, this, &ATutorialSequenceActor::DebugAdvanceStep);

	DebugPrint(TEXT("[Tutorial] Debug hotkeys enabled: END = Advance Step"));
}

void ATutorialSequenceActor::DebugAdvanceStep()
{
	// 튜토리얼이 아직 시작 전이면 시작부터
	if (CurrentIndex == INDEX_NONE)
	{
		DebugPrint(TEXT("[Tutorial][END] StartTutorial()"));
		StartTutorial();
		return;
	}

	// 이미 끝난 상태면 무시
	if (!Steps.IsValidIndex(CurrentIndex))
	{
		DebugPrint(TEXT("[Tutorial][F2] No valid step. (Already finished?)"));
		return;
	}

	const FName StepId = Steps[CurrentIndex].StepId;
	DebugPrint(FString::Printf(TEXT("[Tutorial][F2] Force advance from Step %d (%s)"),
		CurrentIndex, *StepId.ToString()));

	// 타이머 무시하고 즉시 다음 Step로
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
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Yellow,
			Msg
		);
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
}

FName ATutorialSequenceActor::GetCurrentStepId() const
{
	if (Steps.IsValidIndex(CurrentIndex))
	{
		return Steps[CurrentIndex].StepId;
	}
	return NAME_None;
}

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
	bMinHoldSatisfied = true;

	OnFinished.Broadcast(false);
	DebugPrint(TEXT("[Tutorial] Skipped. Finished(false)"));
}

void ATutorialSequenceActor::RestartTutorial()
{
	ClearTimers();
	CurrentIndex = INDEX_NONE;
	CurrentCount = 0;
	bMinHoldSatisfied = false;

	DebugPrint(TEXT("[Tutorial] Restart"));
	StartTutorial();
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
	bMinHoldSatisfied = false;

	const FTutorialStep& Step = Steps[CurrentIndex];

	OnStepChanged.Broadcast(Step.StepId, CurrentIndex);
	StartSubtitlePages(Step);

	DebugPrint(FString::Printf(TEXT("[Tutorial] Enter Step %d (%s)"),
		CurrentIndex, *Step.StepId.ToString()));

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

	// Min hold
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
					CompleteStepIfReady();
				}),
			Step.MinHoldSeconds,
			false
		);
	}

	// Auto / MaxWait
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

	DebugPrint(FString::Printf(TEXT("[Tutorial] MaxWait timeout -> advance from %d"), CurrentIndex));
	EnterStep(CurrentIndex + 1);
}

void ATutorialSequenceActor::ClearTimers()
{
	GetWorldTimerManager().ClearTimer(MinHoldTimer);
	GetWorldTimerManager().ClearTimer(MaxWaitTimer);
}

void ATutorialSequenceActor::StopSubtitlePages()
{
	GetWorldTimerManager().ClearTimer(SubtitlePageTimer);
	CurrentPageIndex = 0;
}

void ATutorialSequenceActor::BroadcastCurrentSubtitlePage(const FTutorialStep& Step)
{
	// 페이지가 있으면 페이지 우선, 없으면 단일 Subtitle 사용
	if (Step.SubtitlePages.Num() > 0)
	{
		const int32 SafeIdx = FMath::Clamp(CurrentPageIndex, 0, Step.SubtitlePages.Num() - 1);
		OnSubtitle.Broadcast(Step.SubtitlePages[SafeIdx], Step.MinHoldSeconds);
	}
	else
	{
		OnSubtitle.Broadcast(Step.Subtitle, Step.MinHoldSeconds);
	}
}

void ATutorialSequenceActor::StartSubtitlePages(const FTutorialStep& Step)
{
	StopSubtitlePages();

	// 첫 페이지 송출
	CurrentPageIndex = 0;
	BroadcastCurrentSubtitlePage(Step);

	// 페이지가 2개 이상이면 자동 넘김
	if (Step.SubtitlePages.Num() >= 2 && Step.SubtitlePageInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			SubtitlePageTimer,
			FTimerDelegate::CreateLambda([this, Step]()
				{
					// Step가 바뀌었으면 중단
					if (!Steps.IsValidIndex(CurrentIndex) || Steps[CurrentIndex].StepId != Step.StepId)
					{
						StopSubtitlePages();
						return;
					}

					CurrentPageIndex++;
					if (CurrentPageIndex >= Step.SubtitlePages.Num())
					{
						// 마지막 페이지 도달 후 더 이상 넘기지 않음
						StopSubtitlePages();
						return;
					}

					BroadcastCurrentSubtitlePage(Step);
				}),
			Step.SubtitlePageInterval,
			true
		);
	}
}

