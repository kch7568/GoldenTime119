// StageSubsystem.cpp (핵심 흐름만)
#include "UStageSubsystem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RoomActor.h"
#include "VitalComponent.h"

void UStageSubsystem::StartStage(UStageDataAsset* InStage)
{
    Stage = InStage;
    StepIndex = -1;
    StepTimeAcc = 0.f;

    UWorld* World = GetWorld();
    if (!World || !Stage)
        return;

    BuildAutoBindings(World);

    EnterStep(0);
}

void UStageSubsystem::AbortStage(bool bFail)
{
    const float Score = (Vital.IsValid())
        ? (Vital->GetHp01() + Vital->GetTemp01() + Vital->GetO201()) // 예시: 실제로는 Temp는 감점일 수도 있음
        : 0.f;

    OnStageFinished.Broadcast(!bFail, Score);
}

void UStageSubsystem::BuildAutoBindings(UWorld* World)
{
    RoomsByTag.Empty();
    Vital.Reset();

    // 1) Rooms
    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* R = *It;
        if (!IsValid(R)) continue;

        for (const FName& Tag : R->Tags)
        {
            RoomsByTag.FindOrAdd(Tag) = R;
        }

        BindRoomSignals(R);
    }

    // 2) Vital (플레이어 폰/캐릭터에서 검색)
    APawn* P = UGameplayStatics::GetPlayerPawn(World, 0);
    if (IsValid(P))
    {
        if (UVitalComponent* V = P->FindComponentByClass<UVitalComponent>())
        {
            Vital = V;
            BindVitalSignals(V);
        }
    }
}

void UStageSubsystem::BindRoomSignals(ARoomActor* Room)
{
    // 스테이지 로직은 “불 꺼짐”, “백드래프트” 정도만 구독해도 충분합니다.
    Room->OnFireExtinguished.AddDynamic(this, &UStageSubsystem::HandleFireExtinguished);
    Room->OnBackdraft.AddDynamic(this, &UStageSubsystem::HandleBackdraftTriggered);
}

void UStageSubsystem::BindVitalSignals(UVitalComponent* InVital)
{
    InVital->OnVitals01Changed.AddDynamic(this, &UStageSubsystem::HandleVitalsChanged);
}

void UStageSubsystem::EnterStep(int32 NewIndex)
{
    if (!Stage || !Stage->Steps.IsValidIndex(NewIndex))
    {
        AbortStage(/*bFail=*/false);
        return;
    }

    StepIndex = NewIndex;
    StepTimeAcc = 0.f;

    const FStageStep& Step = Stage->Steps[StepIndex];
    for (const FStageAction& A : Step.OnEnterActions)
        ExecuteAction(A);
}

void UStageSubsystem::ExecuteAction(const FStageAction& A)
{
    switch (A.Type)
    {
    case EStageActionType::SetObjective:
        CurrentObjective = A.Objective;
        OnObjectiveChanged.Broadcast(CurrentObjective);
        break;

    case EStageActionType::PlayRadio:
        if (A.RadioSound)
            UGameplayStatics::PlaySound2D(GetWorld(), A.RadioSound);
        break;

    case EStageActionType::IgniteRoomRandomFires:
    {
        if (TWeakObjectPtr<ARoomActor>* Found = RoomsByTag.Find(A.TargetTag))
        {
            if (ARoomActor* R = Found->Get())
            {
                const int32 Count = FMath::Max(1, (int32)A.Value);
                for (int32 i = 0; i < Count; ++i)
                    R->IgniteRandomCombustibleInRoom(/*bAllowElectric=*/true);
            }
        }
        break;
    }

    case EStageActionType::IgniteRoomAllFires:
    {
        if (TWeakObjectPtr<ARoomActor>* Found = RoomsByTag.Find(A.TargetTag))
        {
            if (ARoomActor* R = Found->Get())
                R->IgniteAllCombustiblesInRoom(/*bAllowElectric=*/true);
        }
        break;
    }

    default:
        break;
    }
}

bool UStageSubsystem::IsStepComplete(float DeltaSeconds)
{
    if (!Stage || !Stage->Steps.IsValidIndex(StepIndex))
        return false;

    StepTimeAcc += DeltaSeconds;

    const FStageCondition& C = Stage->Steps[StepIndex].CompleteCondition;
    switch (C.Type)
    {
    case EStageCondType::TimeElapsed:
        return StepTimeAcc >= C.Value;

    case EStageCondType::FiresExtinguishedInRoom:
    {
        if (TWeakObjectPtr<ARoomActor>* Found = RoomsByTag.Find(C.TargetTag))
        {
            if (ARoomActor* R = Found->Get())
                return (R->GetActiveFireCount() <= 0); // 이 함수는 RoomActor에 간단히 추가 추천
        }
        return false;
    }

    case EStageCondType::VitalBelow:
        return Vital.IsValid() && (Vital->GetHp01() <= C.Value); // getter 추가 추천

    default:
        return false;
    }
}

void UStageSubsystem::AdvanceStep()
{
    EnterStep(StepIndex + 1);
}

// ===== Vital events =====
void UStageSubsystem::HandleVitalsChanged(float Hp01, float Temp01, float O201)
{
    // 게임오버 예시
    if (Hp01 <= 0.0001f)
    {
        AbortStage(/*bFail=*/true);
    }
}

// ===== Room events (옵션: 조건용 플래그로만 써도 됨) =====
void UStageSubsystem::HandleBackdraftTriggered()
{
    // “백드래프트 발생”을 조건으로 쓰고 싶으면 여기서 플래그 세팅
}

void UStageSubsystem::HandleFireExtinguished(AFireActor* /*Fire*/)
{
    // “불 꺼짐”은 조건 체크를 다시 돌리는 트리거로 활용 가능
}
