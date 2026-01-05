#include "StageRunner.h"
#include "ObjectiveWorldSubsystem.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "StageRunner"

AStageRunner::AStageRunner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AStageRunner::BeginPlay()
{
	Super::BeginPlay();
	BindSubsystems();
}

void AStageRunner::BindSubsystems()
{
	if (UWorld* World = GetWorld())
	{
		ObjectiveSubsystem = World->GetSubsystem<UObjectiveWorldSubsystem>();
		FireDirector = World->GetSubsystem<UFireDirectorWorldSubsystem>();

	}

}

void AStageRunner::StartExampleStage()
{
	if (RunState == EStageRunState::Running) return;

	RunState = EStageRunState::Running;
	EnterStep(EExampleStageStep::Step1_Intro);
}

void AStageRunner::EnterStep(EExampleStageStep Step)
{
	CurrentStep = Step;

	// BP 훅(무전 재생 등)
	BP_OnEnterStep(Step);

	switch (Step)
	{
	case EExampleStageStep::Step1_Intro:
		SetupStep1();
		break;
	case EExampleStageStep::Step2_IgniteAndEntryAllowed:
		SetupStep2();
		break;
	case EExampleStageStep::Step3_AfterLivingRoomExtinguished:
		SetupStep3();
		break;
	default:
		break;
	}
}

void AStageRunner::SetupStep1()
{
	if (ObjectiveSubsystem)
	{
		FObjectiveState Obj;
		Obj.ObjectiveId = FName(TEXT("OBJ_House_Intro"));
		Obj.Title = LOCTEXT("House_Intro_Title", "상황 확인");
		Obj.Detail = LOCTEXT("House_Intro_Detail", "무전 내용을 확인하고 인질 위치를 파악하십시오.");
		Obj.Status = EObjectiveStatus::Active;
		Obj.SortOrder = 0;

		ObjectiveSubsystem->SetObjective(Obj);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimerHandle);
		World->GetTimerManager().SetTimer(
			StepTimerHandle,
			this,
			&AStageRunner::AdvanceToStep2_ByTimer,
			3.0f,
			false
		);
	}
}

void AStageRunner::AdvanceToStep2_ByTimer()
{
	if (RunState != EStageRunState::Running) return;
	EnterStep(EExampleStageStep::Step2_IgniteAndEntryAllowed);
}



void AStageRunner::SetupStep2()
{
	if (ObjectiveSubsystem)
	{
		FObjectiveState Obj;
		Obj.ObjectiveId = FName(TEXT("OBJ_House_LivingRoomEntry"));
		Obj.Title = FText::FromString(TEXT("Initial Suppression"));
		Obj.Detail = FText::FromString(TEXT("Enter LivingRoom and suppress the fire."));
		Obj.Status = EObjectiveStatus::Active;
		Obj.SortOrder = 10;
		Obj.Meta.Add(FName(TEXT("TargetRoom")), FString(TEXT("LivingRoom")));
		ObjectiveSubsystem->SetObjective(Obj);
	}

	// === Fire: ignite 1 random combustibles in room ===
	if (FireDirector)
	{
		const FName LivingRoomId(TEXT("LivingRoom")); // RoomActor의 이름이 Kitchen 이어야 매칭됩니다.
		FireDirector->OnRoomAllFiresExtinguished.RemoveDynamic(this, &AStageRunner::HandleLivingRoomAllExtinguished);
		FireDirector->OnRoomAllFiresExtinguished.AddDynamic(this, &AStageRunner::HandleLivingRoomAllExtinguished);

		FireDirector->WatchRoomAllFiresExtinguished(LivingRoomId);
		FireDirector->IgniteRandomFiresInRoomById(LivingRoomId, 2, /*bAllowElectric=*/false);
	}
}

void AStageRunner::HandleLivingRoomAllExtinguished(FName RoomId, ARoomActor* Room)
{
	const FName LivingRoomId(TEXT("LivingRoom"));
	if (RoomId != LivingRoomId) return;

	// 다음 스텝으로 진행
	EnterStep(EExampleStageStep::Step3_AfterLivingRoomExtinguished);
}

void AStageRunner::SetupStep3()
{
	if (ObjectiveSubsystem)
	{
		FObjectiveState Obj;
		Obj.ObjectiveId = FName(TEXT("OBJ_House_PostLivingRoom"));
		Obj.Title = FText::FromString(TEXT("Post Event"));
		Obj.Detail = FText::FromString(TEXT("Prepare for next hazard."));
		Obj.Status = EObjectiveStatus::Active;
		Obj.SortOrder = 20;
		ObjectiveSubsystem->SetObjective(Obj);
	}
}
#undef LOCTEXT_NAMESPACE
