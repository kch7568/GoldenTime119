// StageSubsystem.cpp (핵심 흐름만)
#include "UStageSubsystem.h"
#include "UStageDataAsset.h"
#include "StageTypes.h"

#include "RoomActor.h"
#include "FireActor.h"
#include "VitalComponent.h"

#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogStageSubsystem, Log, All);

static FORCEINLINE bool HasTagExact(const AActor* A, const FName& Tag)
{
    return IsValid(A) && A->Tags.Contains(Tag);
}

void UStageSubsystem::StartStage(UStageDataAsset* InStage)
{
    if (!InStage || InStage->Steps.Num() == 0)
    {
        UE_LOG(LogStageSubsystem, Error, TEXT("[Stage] StartStage failed: invalid stage or no steps"));
        return;
    }

    Stage = InStage;
    StepIndex = -1;
    StepTimeAcc = 0.f;
    CurrentObjective = FStageObjective{};

    UWorld* World = GetWorld();
    if (!World)
        return;

    BuildAutoBindings(World);

    // 첫 스텝 진입
    EnterStep(0);

    // 타이머 루프 (간단/안전)
    World->GetTimerManager().ClearTimer(TickHandle);
    World->GetTimerManager().SetTimer(
        TickHandle,
        this,
        &UStageSubsystem::TickStage,
        0.05f,   // 20Hz면 충분
        true
    );

    UE_LOG(LogStageSubsystem, Warning, TEXT("[Stage] Start: %s Steps=%d"), *GetNameSafe(Stage), Stage->Steps.Num());
}

void UStageSubsystem::AbortStage(bool bFail)
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(TickHandle);
    }

    // 점수: 예시(HP+O2+(1-Temp))*100
    float FinalScore = 0.f;
    if (Vital.IsValid())
    {
        // Vital은 0..1
        const float Hp = Vital->GetHp01();
        const float O2 = Vital->GetO201();
        const float Temp = Vital->GetTemp01();
        FinalScore = (Hp + O2 + (1.f - Temp)) * 100.f;
    }

    const bool bSuccess = !bFail;
    OnStageFinished.Broadcast(bSuccess, FinalScore);

    UE_LOG(LogStageSubsystem, Warning, TEXT("[Stage] Finish bSuccess=%d Score=%.1f"), bSuccess, FinalScore);

    Stage = nullptr;
    StepIndex = -1;
}

void UStageSubsystem::BuildAutoBindings(UWorld* World)
{
    RoomsByTag.Empty();
    Vital.Reset();

    // 1) RoomsByTag: RoomActor Tags 수집
    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* Room = *It;
        if (!IsValid(Room)) continue;

        for (const FName& Tag : Room->Tags)
        {
            // "Room.Bedroom" 같은 키를 그대로 저장
            RoomsByTag.Add(Tag, Room);
        }

        BindRoomSignals(Room);
    }

    // 2) Vital 찾기 (월드에 있는 첫 VitalComponent)
    //    - 가장 간단한 방식: 모든 Actor에서 VitalComponent 검색
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (!IsValid(A)) continue;

        if (UVitalComponent* VC = A->FindComponentByClass<UVitalComponent>())
        {
            Vital = VC;
            BindVitalSignals(VC);
            break;
        }
    }

    UE_LOG(LogStageSubsystem, Warning, TEXT("[Stage] AutoBindings Rooms=%d Vital=%s"),
        RoomsByTag.Num(), *GetNameSafe(Vital.Get()));
}

void UStageSubsystem::BindRoomSignals(ARoomActor* Room)
{
    if (!IsValid(Room)) return;

    // 중복 바인딩 방지
    Room->OnBackdraft.RemoveDynamic(this, &UStageSubsystem::HandleBackdraftTriggered);
    Room->OnFireExtinguished.RemoveDynamic(this, &UStageSubsystem::HandleFireExtinguished);

    Room->OnBackdraft.AddDynamic(this, &UStageSubsystem::HandleBackdraftTriggered);
    Room->OnFireExtinguished.AddDynamic(this, &UStageSubsystem::HandleFireExtinguished);
}

void UStageSubsystem::BindVitalSignals(UVitalComponent* InVital)
{
    if (!InVital) return;

    InVital->OnVitals01Changed.RemoveDynamic(this, &UStageSubsystem::HandleVitalsChanged);
    InVital->OnVitals01Changed.AddDynamic(this, &UStageSubsystem::HandleVitalsChanged);
}

void UStageSubsystem::EnterStep(int32 NewIndex)
{
    if (!Stage || !Stage->Steps.IsValidIndex(NewIndex))
    {
        AbortStage(true);
        return;
    }

    StepIndex = NewIndex;
    StepTimeAcc = 0.f;

    const FStageStep& Step = Stage->Steps[StepIndex];

    // OnEnterActions 실행
    for (const FStageAction& A : Step.OnEnterActions)
    {
        ExecuteAction(A);
    }

    UE_LOG(LogStageSubsystem, Warning, TEXT("[Stage] EnterStep %d"), StepIndex);
}

void UStageSubsystem::ExecuteAction(const FStageAction& A)
{
    if (!Stage) return;

    switch (A.Type)
    {
    case EStageActionType::SetObjective:
        CurrentObjective = A.Objective;
        OnObjectiveChanged.Broadcast(CurrentObjective);
        UE_LOG(LogStageSubsystem, Log, TEXT("[Stage] Objective updated"));
        break;

    case EStageActionType::PlayRadio:
        if (A.RadioSound)
        {
            // 2D로 단순 재생 (VR에서도 안정적)
            UGameplayStatics::PlaySound2D(GetWorld(), A.RadioSound);
        }
        break;

    case EStageActionType::IgniteRoomRandomFires:
    {
        ARoomActor* Room = RoomsByTag.Contains(A.TargetTag) ? RoomsByTag[A.TargetTag].Get() : nullptr;
        if (!IsValid(Room)) { UE_LOG(LogStageSubsystem, Warning, TEXT("[Stage] Room not found: %s"), *A.TargetTag.ToString()); break; }

        const int32 Count = FMath::Max(0, (int32)A.Value);
        for (int32 i = 0; i < Count; ++i)
        {
            Room->IgniteRandomCombustibleInRoom(/*bAllowElectric=*/true);
        }
        break;
    }

    case EStageActionType::IgniteRoomAllFires:
    {
        ARoomActor* Room = RoomsByTag.Contains(A.TargetTag) ? RoomsByTag[A.TargetTag].Get() : nullptr;
        if (!IsValid(Room)) { UE_LOG(LogStageSubsystem, Warning, TEXT("[Stage] Room not found: %s"), *A.TargetTag.ToString()); break; }

        Room->IgniteAllCombustiblesInRoom(/*bAllowElectric=*/true);
        break;
    }

    // SpawnHostage / SetRoomRisk / SetRoomBackdraftReady 같은 건
    // "Room / Pawn API가 준비되는 순간" 쉽게 확장 가능
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
    case EStageCondType::None:
        return true;

    case EStageCondType::TimeElapsed:
        return StepTimeAcc >= C.Value;

    case EStageCondType::FiresExtinguishedInRoom:
    {
        ARoomActor* Room = RoomsByTag.Contains(C.TargetTag) ? RoomsByTag[C.TargetTag].Get() : nullptr;
        if (!IsValid(Room)) return false;
        return Room->GetActiveFireCount() <= 0;
    }

    case EStageCondType::VitalBelow:
    {
        // Value 의미: Hp 임계 예시 (필요하면 타입 확장)
        if (!Vital.IsValid()) return false;
        return Vital->GetHp01() <= C.Value;
    }

    // BackdraftTriggeredInRoom는 "이벤트 기반"이 더 안전합니다.
    // 여기서는 간단히 StepRuntimeFlags로 처리하는 걸 권장 (아래 HandleBackdraftTriggered 참고)
    default:
        return false;
    }
}

void UStageSubsystem::AdvanceStep()
{
    if (!Stage) return;

    const int32 Next = StepIndex + 1;
    if (!Stage->Steps.IsValidIndex(Next))
    {
        // 끝
        AbortStage(false);
        return;
    }

    EnterStep(Next);
}

void UStageSubsystem::TickStage()
{
    if (!Stage || StepIndex < 0) return;

    constexpr float Dt = 0.05f;
    if (IsStepComplete(Dt))
    {
        AdvanceStep();
    }
}

// ===== event handlers =====
void UStageSubsystem::HandleVitalsChanged(float Hp01, float Temp01, float O201)
{
    // 게임오버 기준: Hp 0
    if (Hp01 <= 0.001f)
    {
        UE_LOG(LogStageSubsystem, Warning, TEXT("[Stage] GameOver by HP"));
        AbortStage(true);
    }
}

void UStageSubsystem::HandleBackdraftTriggered()
{
    // 여기서 “현재 스텝 조건이 BackdraftTriggeredInRoom”이면 통과시키는 식으로 확장 가능
    UE_LOG(LogStageSubsystem, Log, TEXT("[Stage] BackdraftTriggered event"));
}

void UStageSubsystem::HandleFireExtinguished(AFireActor* Fire)
{
    // FiresExtinguishedInRoom 조건은 polling(카운트 0)만으로도 충분해서
    // 여기서는 로그/확장 훅으로만 둠
    UE_LOG(LogStageSubsystem, VeryVerbose, TEXT("[Stage] FireExtinguished %s"), *GetNameSafe(Fire));
}