#include "MissionObjective.h"
#include "FireActor.h"
#include "DoorActor.h"
#include "GasTankActor.h"
#include "VitalComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogMissionObjective, Log, All);

UMissionObjective::UMissionObjective()
{
    ObjectiveID = FGuid::NewGuid().ToString();
}

void UMissionObjective::StartObjective(UWorld* World)
{
    if (!World) return;

    if (Status != EMissionObjectiveStatus::NotStarted)
    {
        UE_LOG(LogMissionObjective, Warning, TEXT("[Objective] %s already started"), *ObjectiveTitle.ToString());
        return;
    }

    StartTime = World->GetTimeSeconds();
    Progress01 = 0.f;
    CurrentCount = 0;
    RescuedNPCCount = 0;

    ChangeStatus(EMissionObjectiveStatus::InProgress);

    UE_LOG(LogMissionObjective, Warning, TEXT("[Objective] Started: %s (%s)"),
        *ObjectiveTitle.ToString(), *UEnum::GetValueAsString(ObjectiveType));
}

void UMissionObjective::UpdateProgress(float DeltaSeconds, UWorld* World)
{
    if (!World) return;
    if (Status != EMissionObjectiveStatus::InProgress) return;

    // 제한 시간 체크
    if (bFailOnTimeout && DurationSeconds > 0.f)
    {
        const float Elapsed = GetElapsedTime(World);
        if (Elapsed >= DurationSeconds)
        {
            FailObjective(TEXT("Time limit exceeded"));
            return;
        }
    }

    // 타입별 진행도 체크
    switch (ObjectiveType)
    {
    case EMissionObjectiveType::ExtinguishAllFires:
        CheckExtinguishAllFires(World);
        break;

    case EMissionObjectiveType::ExtinguishFiresInRoom:
        CheckExtinguishFiresInRoom(World);
        break;

    case EMissionObjectiveType::ExtinguishFireCount:
        CheckExtinguishFireCount(World);
        break;

    case EMissionObjectiveType::ClearRoomSmoke:
        CheckClearRoomSmoke(World);
        break;

    case EMissionObjectiveType::StabilizeRoomEnvironment:
        CheckStabilizeRoomEnvironment(World);
        break;

    case EMissionObjectiveType::PreventBackdraft:
        CheckPreventBackdraft(World);
        break;

    case EMissionObjectiveType::SurviveForDuration:
        CheckSurviveForDuration(World);
        break;

    case EMissionObjectiveType::KeepHealthAbove:
        CheckKeepHealthAbove(World);
        break;

    case EMissionObjectiveType::KeepOxygenAbove:
        CheckKeepOxygenAbove(World);
        break;

    case EMissionObjectiveType::PreventGasTankExplosion:
        CheckPreventGasTankExplosion(World);
        break;

    case EMissionObjectiveType::OpenVentHolesInDoor:
        CheckOpenVentHolesInDoor(World);
        break;

    case EMissionObjectiveType::BreachDoor:
        CheckBreachDoor(World);
        break;

    case EMissionObjectiveType::CompleteBeforeTime:
        CheckCompleteBeforeTime(World);
        break;

    case EMissionObjectiveType::RescueNPC:
        CheckRescueNPC(World);
        break;

    case EMissionObjectiveType::EscapeToExitPoint:
        CheckEscapeToExitPoint(World);
        break;

    default:
        break;
    }
}

void UMissionObjective::CompleteObjective()
{
    if (Status == EMissionObjectiveStatus::Completed) return;

    ChangeStatus(EMissionObjectiveStatus::Completed);
    UpdateProgressValue(1.0f, TEXT("Complete!"));

    if (GetWorld())
    {
        CompletionTime = GetWorld()->GetTimeSeconds();
    }

    UE_LOG(LogMissionObjective, Warning, TEXT("[Objective] COMPLETED: %s (Score: %d)"),
        *ObjectiveTitle.ToString(), ScoreReward);
}

void UMissionObjective::FailObjective(const FString& Reason)
{
    if (Status == EMissionObjectiveStatus::Failed) return;
    if (!bCanFail) return;

    ChangeStatus(EMissionObjectiveStatus::Failed);
    UpdateProgressValue(0.f, FString::Printf(TEXT("Failed: %s"), *Reason));

    UE_LOG(LogMissionObjective, Error, TEXT("[Objective] FAILED: %s - Reason: %s"),
        *ObjectiveTitle.ToString(), *Reason);
}

void UMissionObjective::ResetObjective()
{
    Status = EMissionObjectiveStatus::NotStarted;
    Progress01 = 0.f;
    CurrentCount = 0;
    RescuedNPCCount = 0;
    StartTime = 0.f;
    CompletionTime = 0.f;
}

FString UMissionObjective::GetProgressText() const
{
    switch (ObjectiveType)
    {
    case EMissionObjectiveType::ExtinguishFireCount:
        return FormatProgressText(CurrentCount, TargetCount);

    case EMissionObjectiveType::RescueNPC:
        return FormatProgressText(RescuedNPCCount, TargetNPCs.Num());

    case EMissionObjectiveType::SurviveForDuration:
    case EMissionObjectiveType::CompleteBeforeTime:
    case EMissionObjectiveType::PreventBackdraft:
        if (GetWorld())
        {
            const float Remaining = GetRemainingTime(GetWorld());
            return FormatTimeText(Remaining);
        }
        break;

    case EMissionObjectiveType::ClearRoomSmoke:
    case EMissionObjectiveType::StabilizeRoomEnvironment:
    case EMissionObjectiveType::EscapeToExitPoint:
        return FormatPercentText(Progress01);

    default:
        return FormatPercentText(Progress01);
    }

    return TEXT("");
}

float UMissionObjective::GetElapsedTime(UWorld* World) const
{
    if (!World || StartTime <= 0.f) return 0.f;
    return World->GetTimeSeconds() - StartTime;
}

float UMissionObjective::GetRemainingTime(UWorld* World) const
{
    if (DurationSeconds <= 0.f) return 0.f;
    const float Elapsed = GetElapsedTime(World);
    return FMath::Max(0.f, DurationSeconds - Elapsed);
}

// ============================ 외부 이벤트 등록 메서드 ============================

void UMissionObjective::NotifyNPCRescued(AActor* RescuedNPC)
{
    if (!IsValid(RescuedNPC)) return;
    if (!TargetNPCs.Contains(RescuedNPC)) return;
    if (Status != EMissionObjectiveStatus::InProgress) return;

    RescuedNPCCount++;

    UE_LOG(LogMissionObjective, Warning, TEXT("[Objective] NPC Rescued: %d/%d"),
        RescuedNPCCount, TargetNPCs.Num());

    // 즉시 진행도 업데이트
    if (ObjectiveType == EMissionObjectiveType::RescueNPC)
    {
        CheckRescueNPC(GetWorld());
    }
}

void UMissionObjective::NotifyBackdraftOccurred()
{
    if (bFailOnBackdraft && Status == EMissionObjectiveStatus::InProgress)
    {
        FailObjective(TEXT("Backdraft occurred"));
    }
}

void UMissionObjective::NotifyFireExtinguished()
{
    if (Status != EMissionObjectiveStatus::InProgress) return;

    CurrentCount++;

    UE_LOG(LogMissionObjective, Log, TEXT("[Objective] Fire Extinguished: %d"), CurrentCount);

    // 즉시 진행도 업데이트
    if (ObjectiveType == EMissionObjectiveType::ExtinguishFireCount)
    {
        CheckExtinguishFireCount(GetWorld());
    }
}

void UMissionObjective::NotifyGasTankExplosion()
{
    if (bFailOnGasTankExplosion && Status == EMissionObjectiveStatus::InProgress)
    {
        FailObjective(TEXT("Gas tank exploded"));
    }
}

void UMissionObjective::NotifyPlayerDeath()
{
    if (bFailOnPlayerDeath && Status == EMissionObjectiveStatus::InProgress)
    {
        FailObjective(TEXT("Player died"));
    }
}

// ============================ 타입별 체크 함수들 ============================

void UMissionObjective::CheckExtinguishAllFires(UWorld* World)
{
    TArray<ARoomActor*> AllRooms;
    FindAllRooms(World, AllRooms);

    int32 TotalFires = 0;
    for (ARoomActor* Room : AllRooms)
    {
        if (IsValid(Room))
        {
            TotalFires += Room->GetActiveFireCount();
        }
    }

    if (TotalFires == 0)
    {
        CompleteObjective();
    }
    else
    {
        // 진행도 = 1 - (남은 불 / 초기 불 개수)
        // 초기 개수는 TargetCount에 저장했다고 가정
        if (TargetCount > 0)
        {
            const float Prog = 1.f - (float)TotalFires / (float)TargetCount;
            UpdateProgressValue(FMath::Clamp(Prog, 0.f, 1.f),
                FString::Printf(TEXT("Fires remaining: %d"), TotalFires));
        }
    }
}

void UMissionObjective::CheckExtinguishFiresInRoom(UWorld* World)
{
    int32 TotalFires = 0;

    for (ARoomActor* Room : TargetRooms)
    {
        if (IsValid(Room))
        {
            TotalFires += Room->GetActiveFireCount();
        }
    }

    if (TotalFires == 0)
    {
        CompleteObjective();
    }
    else
    {
        if (TargetCount > 0)
        {
            const float Prog = 1.f - (float)TotalFires / (float)TargetCount;
            UpdateProgressValue(FMath::Clamp(Prog, 0.f, 1.f),
                FString::Printf(TEXT("Fires remaining: %d"), TotalFires));
        }
    }
}

void UMissionObjective::CheckExtinguishFireCount(UWorld* World)
{
    // CurrentCount는 NotifyFireExtinguished()에서 증가

    if (CurrentCount >= TargetCount)
    {
        CompleteObjective();
    }
    else
    {
        const float Prog = (float)CurrentCount / (float)FMath::Max(1, TargetCount);
        UpdateProgressValue(Prog, FormatProgressText(CurrentCount, TargetCount));
    }
}

void UMissionObjective::CheckClearRoomSmoke(UWorld* World)
{
    bool bAllClear = true;
    float AvgSmoke = 0.f;
    int32 ValidRooms = 0;

    for (ARoomActor* Room : TargetRooms)
    {
        if (!IsValid(Room)) continue;

        ValidRooms++;
        const float Smoke = Room->Smoke;
        AvgSmoke += Smoke;

        if (Smoke > ThresholdValue)
        {
            bAllClear = false;
        }
    }

    if (ValidRooms > 0)
    {
        AvgSmoke /= ValidRooms;

        if (bAllClear)
        {
            CompleteObjective();
        }
        else
        {
            // 진행도 = 1 - (평균 연기 / 임계값)
            const float Prog = FMath::Clamp(1.f - (AvgSmoke / FMath::Max(0.01f, ThresholdValue)), 0.f, 1.f);
            UpdateProgressValue(Prog, FString::Printf(TEXT("Smoke: %.1f%%"), AvgSmoke * 100.f));
        }
    }
}

void UMissionObjective::CheckStabilizeRoomEnvironment(UWorld* World)
{
    bool bAllStable = true;
    float AvgStability = 0.f;
    int32 ValidRooms = 0;

    for (ARoomActor* Room : TargetRooms)
    {
        if (!IsValid(Room)) continue;

        ValidRooms++;

        // 안정 조건: 연기 < 30%, O2 > 20%, 온도 < 100도
        const bool bSmokeOK = Room->Smoke < 0.3f;
        const bool bO2OK = Room->Oxygen > 0.2f;
        const bool bHeatOK = Room->Heat < 100.f;

        const float Stability = (bSmokeOK ? 0.33f : 0.f) + (bO2OK ? 0.33f : 0.f) + (bHeatOK ? 0.34f : 0.f);
        AvgStability += Stability;

        if (!bSmokeOK || !bO2OK || !bHeatOK)
        {
            bAllStable = false;
        }
    }

    if (ValidRooms > 0)
    {
        AvgStability /= ValidRooms;

        if (bAllStable)
        {
            CompleteObjective();
        }
        else
        {
            UpdateProgressValue(AvgStability, FString::Printf(TEXT("Stability: %.1f%%"), AvgStability * 100.f));
        }
    }
}

void UMissionObjective::CheckPreventBackdraft(UWorld* World)
{
    // 백드래프트 발생 시 NotifyBackdraftOccurred() 호출로 실패 처리
    // 시간이 지나면 성공
    const float Elapsed = GetElapsedTime(World);

    if (Elapsed >= DurationSeconds)
    {
        CompleteObjective();
    }
    else
    {
        const float Prog = Elapsed / FMath::Max(0.01f, DurationSeconds);
        UpdateProgressValue(Prog, FormatTimeText(DurationSeconds - Elapsed));
    }
}

void UMissionObjective::CheckSurviveForDuration(UWorld* World)
{
    const float Elapsed = GetElapsedTime(World);

    if (Elapsed >= DurationSeconds)
    {
        CompleteObjective();
    }
    else
    {
        const float Prog = Elapsed / FMath::Max(0.01f, DurationSeconds);
        UpdateProgressValue(Prog, FormatTimeText(DurationSeconds - Elapsed));
    }
}

void UMissionObjective::CheckKeepHealthAbove(UWorld* World)
{
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return;

    APawn* PlayerPawn = PC->GetPawn();
    if (!IsValid(PlayerPawn)) return;

    UVitalComponent* Vital = PlayerPawn->FindComponentByClass<UVitalComponent>();
    if (!IsValid(Vital)) return;

    const float HP = Vital->GetHp01();

    if (HP < ThresholdValue)
    {
        FailObjective(FString::Printf(TEXT("HP %.1f%% 미만"), ThresholdValue * 100.f));
    }
    else
    {
        // 목표 달성 조건: 지정 시간 동안 유지
        const float Elapsed = GetElapsedTime(World);
        if (Elapsed >= DurationSeconds)
        {
            CompleteObjective();
        }
        else
        {
            const float Prog = Elapsed / FMath::Max(0.01f, DurationSeconds);
            UpdateProgressValue(Prog, FString::Printf(TEXT("HP: %.1f%% / %s"),
                HP * 100.f, *FormatTimeText(DurationSeconds - Elapsed)));
        }
    }
}

void UMissionObjective::CheckKeepOxygenAbove(UWorld* World)
{
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return;

    APawn* PlayerPawn = PC->GetPawn();
    if (!IsValid(PlayerPawn)) return;

    UVitalComponent* Vital = PlayerPawn->FindComponentByClass<UVitalComponent>();
    if (!IsValid(Vital)) return;

    const float O2 = Vital->GetO201();

    if (O2 < ThresholdValue)
    {
        FailObjective(FString::Printf(TEXT("O2 %.1f%% 미만"), ThresholdValue * 100.f));
    }
    else
    {
        const float Elapsed = GetElapsedTime(World);
        if (Elapsed >= DurationSeconds)
        {
            CompleteObjective();
        }
        else
        {
            const float Prog = Elapsed / FMath::Max(0.01f, DurationSeconds);
            UpdateProgressValue(Prog, FString::Printf(TEXT("O2: %.1f%% / %s"),
                O2 * 100.f, *FormatTimeText(DurationSeconds - Elapsed)));
        }
    }
}

void UMissionObjective::CheckPreventGasTankExplosion(UWorld* World)
{
    // 가스탱크 BLEVE 발생 시 NotifyGasTankExplosion() 호출로 실패 처리
    // 모든 타겟 가스탱크가 안전하면 성공

    bool bAllSafe = true;
    int32 DangerCount = 0;

    for (AActor* Actor : TargetActors)
    {
        AGasTankActor* Tank = Cast<AGasTankActor>(Actor);
        if (IsValid(Tank))
        {
            if (Tank->IsInDanger())
            {
                bAllSafe = false;
                DangerCount++;
            }
        }
    }

    const float Elapsed = GetElapsedTime(World);
    if (Elapsed >= DurationSeconds && bAllSafe)
    {
        CompleteObjective();
    }
    else
    {
        const float Prog = Elapsed / FMath::Max(0.01f, DurationSeconds);
        UpdateProgressValue(Prog, FString::Printf(TEXT("Danger tanks: %d / %s"),
            DangerCount, *FormatTimeText(DurationSeconds - Elapsed)));
    }
}

void UMissionObjective::CheckOpenVentHolesInDoor(UWorld* World)
{
    int32 TotalHoles = 0;

    for (AActor* Actor : TargetActors)
    {
        ADoorActor* Door = Cast<ADoorActor>(Actor);
        if (IsValid(Door))
        {
            TotalHoles += Door->GetVentHoleCount();
        }
    }

    if (TotalHoles >= TargetCount)
    {
        CompleteObjective();
    }
    else
    {
        const float Prog = (float)TotalHoles / (float)FMath::Max(1, TargetCount);
        UpdateProgressValue(Prog, FormatProgressText(TotalHoles, TargetCount));
    }
}

void UMissionObjective::CheckBreachDoor(UWorld* World)
{
    int32 BreachedCount = 0;

    for (AActor* Actor : TargetActors)
    {
        ADoorActor* Door = Cast<ADoorActor>(Actor);
        if (IsValid(Door) && Door->DoorState == EDoorState::Breached)
        {
            BreachedCount++;
        }
    }

    if (BreachedCount >= TargetCount)
    {
        CompleteObjective();
    }
    else
    {
        const float Prog = (float)BreachedCount / (float)FMath::Max(1, TargetCount);
        UpdateProgressValue(Prog, FormatProgressText(BreachedCount, TargetCount));
    }
}

void UMissionObjective::CheckCompleteBeforeTime(UWorld* World)
{
    // 다른 목표들과 조합해서 사용
    // 단독으로는 시간만 체크
    const float Remaining = GetRemainingTime(World);

    if (Remaining <= 0.f)
    {
        FailObjective(TEXT("Time limit exceeded"));
    }
}

void UMissionObjective::CheckRescueNPC(UWorld* World)
{
    // RescuedNPCCount는 NotifyNPCRescued()에서 증가
    const int32 TotalNPCs = TargetNPCs.Num();

    if (TotalNPCs == 0)
    {
        UE_LOG(LogMissionObjective, Warning, TEXT("[Objective] No target NPCs set for rescue objective"));
        return;
    }

    if (RescuedNPCCount >= TotalNPCs)
    {
        CompleteObjective();
    }
    else
    {
        const float Prog = (float)RescuedNPCCount / (float)TotalNPCs;
        UpdateProgressValue(Prog, FormatProgressText(RescuedNPCCount, TotalNPCs));
    }
}

void UMissionObjective::CheckEscapeToExitPoint(UWorld* World)
{
    if (!IsValid(ExitPoint))
    {
        UE_LOG(LogMissionObjective, Warning, TEXT("[Objective] No exit point set for escape objective"));
        return;
    }

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return;

    APawn* PlayerPawn = PC->GetPawn();
    if (!IsValid(PlayerPawn)) return;

    // 플레이어와 탈출구 거리 체크
    const float Distance = FVector::Dist(PlayerPawn->GetActorLocation(), ExitPoint->GetActorLocation());

    if (Distance <= ExitReachDistance)
    {
        CompleteObjective();
    }
    else
    {
        // 거리 기반 진행도 (가까워질수록 증가)
        // 최대 거리에서 0%, 탈출 지점에 가까워질수록 100%로 증가
        const float Prog = FMath::Clamp(1.f - (Distance / MaxDistanceForProgress), 0.f, 0.95f);
        UpdateProgressValue(Prog, FString::Printf(TEXT("%.1fm to exit"), Distance / 100.f));
    }
}

// ============================ 헬퍼 함수들 ============================

void UMissionObjective::UpdateProgressValue(float NewProgress, const FString& ProgressText)
{
    const float OldProgress = Progress01;
    Progress01 = FMath::Clamp(NewProgress, 0.f, 1.f);

    if (!FMath::IsNearlyEqual(OldProgress, Progress01, 0.01f))
    {
        OnProgressChanged.Broadcast(this, Progress01, ProgressText);
    }
}

void UMissionObjective::ChangeStatus(EMissionObjectiveStatus NewStatus)
{
    if (Status == NewStatus) return;

    const EMissionObjectiveStatus OldStatus = Status;
    Status = NewStatus;

    OnStatusChanged.Broadcast(this, NewStatus);

    UE_LOG(LogMissionObjective, Log, TEXT("[Objective] %s: %s -> %s"),
        *ObjectiveTitle.ToString(),
        *UEnum::GetValueAsString(OldStatus),
        *UEnum::GetValueAsString(NewStatus));
}

void UMissionObjective::FindAllRooms(UWorld* World, TArray<ARoomActor*>& OutRooms)
{
    OutRooms.Empty();

    if (!World) return;

    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* Room = *It;
        if (IsValid(Room))
        {
            OutRooms.Add(Room);
        }
    }
}

FString UMissionObjective::FormatProgressText(int32 Current, int32 Target) const
{
    return FString::Printf(TEXT("%d / %d"), Current, Target);
}

FString UMissionObjective::FormatPercentText(float Percent01) const
{
    return FString::Printf(TEXT("%.1f%%"), Percent01 * 100.f);
}

FString UMissionObjective::FormatTimeText(float Seconds) const
{
    const int32 Minutes = FMath::FloorToInt(Seconds / 60.f);
    const int32 Secs = FMath::FloorToInt(Seconds) % 60;
    return FString::Printf(TEXT("%02d:%02d"), Minutes, Secs);
}