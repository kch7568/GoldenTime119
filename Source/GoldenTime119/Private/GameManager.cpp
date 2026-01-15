// ============================ GameManager.cpp ============================
#include "GameManager.h"
#include "MissionObjective.h"
#include "FireActor.h"
#include "RoomActor.h"
#include "GasTankActor.h"
#include "PressureVesselComponent.h"
#include "DoorActor.h"
#include "CombustibleComponent.h"
#include "VitalComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameManager, Log, All);

AGameManager::AGameManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AGameManager::BeginPlay()
{
    Super::BeginPlay();

    // 챕터 초기화
    InitializeChapters();

    // 저장된 진행 상황 로드
    LoadGameProgress();

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Initialized"));
}

void AGameManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentGameState == EGameState::Gameplay)
    {
        ChapterElapsedTime += DeltaTime;
        CurrentSessionStats.TotalPlayTime += DeltaTime;

        // 화재 시나리오 업데이트
        UpdateFireScenario(DeltaTime);

        // NPC 위험도 업데이트
        UpdateNPCDanger(DeltaTime);

        // 완료 조건 체크
        CheckChapterCompletion();
    }
}

// ============================ 싱글톤 접근 ============================

AGameManager* AGameManager::GetGameManager(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    // 월드에서 GameManager 찾기
    for (TActorIterator<AGameManager> It(World); It; ++It)
    {
        return *It;
    }

    return nullptr;
}

// ============================ 게임 상태 ============================

void AGameManager::ChangeGameState(EGameState NewState)
{
    if (CurrentGameState == NewState)
        return;

    const EGameState OldState = CurrentGameState;
    CurrentGameState = NewState;

    OnGameStateChanged.Broadcast(NewState);

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] State: %s -> %s"),
        *UEnum::GetValueAsString(OldState),
        *UEnum::GetValueAsString(NewState));

    // 상태별 처리
    switch (NewState)
    {
    case EGameState::Gameplay:
        // 게임 재개
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        break;

    case EGameState::Paused:
        // 일시정지
        UGameplayStatics::SetGamePaused(GetWorld(), true);
        break;

    case EGameState::MissionComplete:
        CompleteChapter(true);
        break;

    case EGameState::MissionFailed:
        CompleteChapter(false);
        break;

    default:
        break;
    }
}

// ============================ 챕터 관리 ============================

void AGameManager::LoadChapter(EChapterID ChapterID)
{
    if (!Chapters.Contains(ChapterID))
    {
        UE_LOG(LogGameManager, Error, TEXT("[GameManager] Chapter %s not found!"),
            *UEnum::GetValueAsString(ChapterID));
        return;
    }

    const FChapterData& ChapterData = Chapters[ChapterID];

    if (!ChapterData.bIsUnlocked)
    {
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapter %s is locked!"),
            *UEnum::GetValueAsString(ChapterID));
        return;
    }

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Loading Chapter: %s"),
        *ChapterData.ChapterTitle.ToString());

    CurrentChapterID = ChapterID;

    // 이미 올바른 레벨에 있는지 체크
    UWorld* World = GetWorld();
    if (World && ChapterData.LevelName != NAME_None)
    {
        FString CurrentLevelName = World->GetMapName();
        CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix); // "UEDPIE_0_" 같은 prefix 제거

        FString TargetLevelName = ChapterData.LevelName.ToString();

        UE_LOG(LogGameManager, Log, TEXT("[GameManager] Current Level: '%s', Target Level: '%s'"),
            *CurrentLevelName, *TargetLevelName);

        // 레벨 이름이 포함되어 있으면 (부분 일치)
        if (CurrentLevelName.Contains(TargetLevelName) || TargetLevelName.Contains(CurrentLevelName))
        {
            UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Already in correct level, skipping load"));
            // 레벨 로드 없이 CurrentChapterID만 설정하고 리턴
            return;
        }
    }

    ChangeGameState(EGameState::Loading);

    // 레벨 로드
    if (ChapterData.LevelName != NAME_None)
    {
        UGameplayStatics::OpenLevel(this, ChapterData.LevelName);
    }
}

void AGameManager::StartCurrentChapter()
{
    if (!Chapters.Contains(CurrentChapterID))
    {
        UE_LOG(LogGameManager, Error, TEXT("[GameManager] No valid chapter to start"));
        return;
    }

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] ===== CHAPTER START: %s ====="),
        *ChapterData.ChapterTitle.ToString());

    // 통계 초기화
    ResetSessionStatistics();

    // 챕터 시간 초기화
    ChapterStartTime = GetWorld()->GetTimeSeconds();
    ChapterElapsedTime = 0.f;

    // 인트로 표시
    ChangeGameState(EGameState::ChapterIntro);

    // 인트로 후 게임 시작
    GetWorld()->GetTimerManager().SetTimer(
        IntroTimerHandle,
        [this, ChapterData]()
        {
            // 목표 설정
            ActiveObjectives = ChapterData.ChapterObjectives;
            CurrentScore = 0;

            // 모든 목표 시작
            for (UMissionObjective* Obj : ActiveObjectives)
            {
                if (IsValid(Obj))
                {
                    Obj->StartObjective(GetWorld());
                }
            }

            // 이벤트 바인딩
            BindGameEvents();

            // 게임플레이 시작
            ChangeGameState(EGameState::Gameplay);

            OnChapterStarted.Broadcast(CurrentChapterID);

            UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapter gameplay started - Objectives: %d"),
                ActiveObjectives.Num());
        },
        ChapterData.IntroDuration,
        false
    );
}

void AGameManager::CompleteChapter(bool bSuccess)
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] ===== CHAPTER %s: %s ====="),
        bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
        *ChapterData.ChapterTitle.ToString());

    // 통계 저장
    TotalGameStats.TotalFiresExtinguished += CurrentSessionStats.TotalFiresExtinguished;
    TotalGameStats.TotalNPCsRescued += CurrentSessionStats.TotalNPCsRescued;
    TotalGameStats.TotalBackdraftsTriggered += CurrentSessionStats.TotalBackdraftsTriggered;
    TotalGameStats.TotalGasTanksExploded += CurrentSessionStats.TotalGasTanksExploded;
    TotalGameStats.TotalDoorsBreached += CurrentSessionStats.TotalDoorsBreached;
    TotalGameStats.TotalVentHolesCreated += CurrentSessionStats.TotalVentHolesCreated;
    TotalGameStats.TotalWaterUsed += CurrentSessionStats.TotalWaterUsed;
    TotalGameStats.TotalPlayTime += CurrentSessionStats.TotalPlayTime;

    if (bSuccess)
    {
        ChapterData.bIsCompleted = true;

        // 최고 기록 갱신
        const int32 FinalScore = CalculateFinalScore();
        if (FinalScore > ChapterData.BestScore)
        {
            ChapterData.BestScore = FinalScore;
        }

        if (ChapterElapsedTime > 0.f)
        {
            if (ChapterData.BestTime <= 0.f || ChapterElapsedTime < ChapterData.BestTime)
            {
                ChapterData.BestTime = ChapterElapsedTime;
            }
        }

        // 다음 챕터 언락
        const int32 NextChapterIndex = static_cast<int32>(CurrentChapterID) + 1;
        if (NextChapterIndex < static_cast<int32>(EChapterID::Tutorial))
        {
            const EChapterID NextChapter = static_cast<EChapterID>(NextChapterIndex);
            UnlockChapter(NextChapter);
        }
    }
    else
    {
        TotalGameStats.DeathCount++;
    }

    // 이벤트 언바인딩
    UnbindGameEvents();

    // 저장
    SaveGameProgress();

    OnChapterCompleted.Broadcast(CurrentChapterID, bSuccess);

    ChangeGameState(bSuccess ? EGameState::ChapterComplete : EGameState::GameOver);
}

void AGameManager::RestartChapter()
{
    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Restarting chapter..."));

    UnbindGameEvents();

    // 목표 초기화
    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj))
        {
            Obj->ResetObjective();
        }
    }

    StartCurrentChapter();
}

FChapterData AGameManager::GetCurrentChapterData() const
{
    if (Chapters.Contains(CurrentChapterID))
    {
        return Chapters[CurrentChapterID];
    }
    return FChapterData();
}

bool AGameManager::IsChapterUnlocked(EChapterID ChapterID) const
{
    if (Chapters.Contains(ChapterID))
    {
        return Chapters[ChapterID].bIsUnlocked;
    }
    return false;
}

void AGameManager::UnlockChapter(EChapterID ChapterID)
{
    if (Chapters.Contains(ChapterID))
    {
        Chapters[ChapterID].bIsUnlocked = true;
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapter unlocked: %s"),
            *UEnum::GetValueAsString(ChapterID));
    }
}

// ============================ 미션 목표 관리 ============================

void AGameManager::AddObjective(UMissionObjective* Objective)
{
    if (!IsValid(Objective))
        return;

    ActiveObjectives.Add(Objective);

    // 이미 게임플레이 중이면 즉시 시작
    if (CurrentGameState == EGameState::Gameplay)
    {
        Objective->StartObjective(GetWorld());
    }

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Objective added: %s"),
        *Objective->ObjectiveTitle.ToString());
}

void AGameManager::AddScore(int32 Points)
{
    if (Points <= 0)
        return;

    CurrentScore += Points;

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Score +%d -> %d"), Points, CurrentScore);
}

void AGameManager::SubtractScore(int32 Points)
{
    if (Points <= 0)
        return;

    CurrentScore = FMath::Max(0, CurrentScore - Points);

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Score -%d -> %d"), Points, CurrentScore);
}

int32 AGameManager::CalculateFinalScore() const
{
    int32 Score = CurrentScore;

    if (!Chapters.Contains(CurrentChapterID))
        return Score;

    const FChapterData& ChapterData = Chapters[CurrentChapterID];

    // 시간 보너스
    if (ChapterData.TimeLimit > 0.f)
    {
        const float Remaining = GetChapterRemainingTime();
        if (Remaining > 0.f)
        {
            const int32 Bonus = FMath::FloorToInt(Remaining * TimeBonus);
            Score += Bonus;
            UE_LOG(LogGameManager, Log, TEXT("[GameManager] Time bonus: +%d"), Bonus);
        }
    }

    // 선택 목표 보너스
    int32 OptionalCompleted = 0;
    for (const UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj) && Obj->bIsOptional && Obj->IsCompleted())
        {
            OptionalCompleted++;
        }
    }
    if (OptionalCompleted > 0)
    {
        const int32 Bonus = FMath::FloorToInt(OptionalCompleted * OptionalObjectiveBonus);
        Score += Bonus;
        UE_LOG(LogGameManager, Log, TEXT("[GameManager] Optional bonus: +%d"), Bonus);
    }

    // 완벽 클리어 보너스
    if (AreAllObjectivesComplete())
    {
        Score += FMath::FloorToInt(PerfectClearBonus);
        UE_LOG(LogGameManager, Log, TEXT("[GameManager] Perfect clear bonus: +%.0f"), PerfectClearBonus);
    }

    return FMath::Clamp(Score, 0, ChapterData.TargetScore);
}

bool AGameManager::AreAllObjectivesComplete() const
{
    for (const UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj) && !Obj->IsCompleted())
        {
            return false;
        }
    }
    return ActiveObjectives.Num() > 0;
}

bool AGameManager::AreMandatoryObjectivesComplete() const
{
    for (const UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj) && !Obj->bIsOptional && !Obj->IsCompleted())
        {
            return false;
        }
    }
    return true;
}

// ============================ 화재 시나리오 ============================

void AGameManager::UpdateFireScenario(float DeltaTime)
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    // 모든 목표 업데이트
    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj) && Obj->IsInProgress())
        {
            Obj->UpdateProgress(DeltaTime, GetWorld());
        }
    }

    // 스크립트 기반 화재 발생
    if (ChapterData.ScenarioType == EFireScenarioType::Scripted)
    {
        for (FFireSpawnEvent& Event : ChapterData.FireEvents)
        {
            if (Event.bHasTriggered)
                continue;

            if (ChapterElapsedTime >= Event.TriggerTime)
            {
                // 화재 발생
                if (IsValid(Event.TargetRoom))
                {
                    TriggerFireAtRoom(Event.TargetRoom, Event.FireType, Event.InitialIntensity);
                }
                else if (IsValid(Event.TargetActor))
                {
                    TriggerFireAtActor(Event.TargetActor);
                }

                Event.bHasTriggered = true;
                OnFireEventTriggered.Broadcast(Event.EventID);

                UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Fire event triggered: %s at %.1fs"),
                    *Event.EventID, ChapterElapsedTime);
            }
        }
    }
}

void AGameManager::TriggerFireEvent(const FString& EventID)
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    for (FFireSpawnEvent& Event : ChapterData.FireEvents)
    {
        if (Event.EventID == EventID && !Event.bHasTriggered)
        {
            if (IsValid(Event.TargetRoom))
            {
                TriggerFireAtRoom(Event.TargetRoom, Event.FireType, Event.InitialIntensity);
            }
            else if (IsValid(Event.TargetActor))
            {
                TriggerFireAtActor(Event.TargetActor);
            }

            Event.bHasTriggered = true;
            OnFireEventTriggered.Broadcast(Event.EventID);
            break;
        }
    }
}

void AGameManager::TriggerFireAtRoom(ARoomActor* Room, ECombustibleType FireType, float Intensity)
{
    if (!IsValid(Room))
        return;

    // 방 안의 랜덤 Combustible 점화
    AFireActor* Fire = Room->IgniteRandomCombustibleInRoom(true);

    if (IsValid(Fire))
    {
        Fire->BaseIntensity = Intensity;
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Fire ignited in room: %s"),
            *Room->GetName());
    }
}

void AGameManager::TriggerFireAtActor(AActor* TargetActor)
{
    if (!IsValid(TargetActor))
        return;

    UCombustibleComponent* Comb = TargetActor->FindComponentByClass<UCombustibleComponent>();
    if (IsValid(Comb))
    {
        Comb->ForceIgnite(true);
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Fire ignited at actor: %s"),
            *TargetActor->GetName());
    }
}

int32 AGameManager::GetActiveFireCount() const
{
    int32 TotalFires = 0;

    UWorld* World = GetWorld();
    if (!World)
        return 0;

    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* Room = *It;
        if (IsValid(Room))
        {
            TotalFires += Room->GetActiveFireCount();
        }
    }

    return TotalFires;
}

TArray<AFireActor*> AGameManager::GetAllActiveFires() const
{
    TArray<AFireActor*> Fires;

    UWorld* World = GetWorld();
    if (!World)
        return Fires;

    for (TActorIterator<AFireActor> It(World); It; ++It)
    {
        AFireActor* Fire = *It;
        if (IsValid(Fire))
        {
            Fires.Add(Fire);
        }
    }

    return Fires;
}

// ============================ NPC 구조 시스템 ============================

void AGameManager::UpdateNPCDanger(float DeltaTime)
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    for (FNPCRescueData& NPC : ChapterData.NPCsToRescue)
    {
        if (NPC.bIsRescued || NPC.bIsDead)
            continue;

        ARoomActor* Room = NPC.InitialRoom;
        if (!IsValid(Room))
            continue;

        // 위험한 환경인지 체크
        const bool bInDanger = (Room->State == ERoomState::Fire || Room->State == ERoomState::Risk);

        if (bInDanger)
        {
            NPC.TimeInDanger += DeltaTime;

            // 생존 시간 초과 시 사망
            if (NPC.TimeInDanger >= NPC.MaxSurvivalTime)
            {
                KillNPC(NPC.NPCID, TEXT("화재 노출 시간 초과"));
            }
        }
    }
}

void AGameManager::RescueNPC(const FString& NPCID)
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    for (FNPCRescueData& NPC : ChapterData.NPCsToRescue)
    {
        if (NPC.NPCID == NPCID && !NPC.bIsRescued && !NPC.bIsDead)
        {
            NPC.bIsRescued = true;

            CurrentSessionStats.TotalNPCsRescued++;
            OnNPCRescued.Broadcast(NPC.NPCID, NPC.NPCName);

            UE_LOG(LogGameManager, Warning, TEXT("[GameManager] NPC rescued: %s"),
                *NPC.NPCName.ToString());

            // 점수 추가
            AddScore(200);

            break;
        }
    }
}

void AGameManager::KillNPC(const FString& NPCID, const FString& Reason)
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    for (FNPCRescueData& NPC : ChapterData.NPCsToRescue)
    {
        if (NPC.NPCID == NPCID && !NPC.bIsDead)
        {
            NPC.bIsDead = true;

            OnNPCDied.Broadcast(NPC.NPCID, NPC.NPCName);

            UE_LOG(LogGameManager, Error, TEXT("[GameManager] NPC died: %s - Reason: %s"),
                *NPC.NPCName.ToString(), *Reason);

            // 점수 감점
            SubtractScore(300);

            break;
        }
    }
}

int32 AGameManager::GetRescuedNPCCount() const
{
    if (!Chapters.Contains(CurrentChapterID))
        return 0;

    const FChapterData& ChapterData = Chapters[CurrentChapterID];
    int32 Count = 0;

    for (const FNPCRescueData& NPC : ChapterData.NPCsToRescue)
    {
        if (NPC.bIsRescued)
        {
            Count++;
        }
    }

    return Count;
}

int32 AGameManager::GetTotalNPCCount() const
{
    if (!Chapters.Contains(CurrentChapterID))
        return 0;

    return Chapters[CurrentChapterID].NPCsToRescue.Num();
}

TArray<FNPCRescueData> AGameManager::GetNPCsInDanger() const
{
    TArray<FNPCRescueData> Result;

    if (!Chapters.Contains(CurrentChapterID))
        return Result;

    const FChapterData& ChapterData = Chapters[CurrentChapterID];

    for (const FNPCRescueData& NPC : ChapterData.NPCsToRescue)
    {
        if (!NPC.bIsRescued && !NPC.bIsDead)
        {
            ARoomActor* Room = NPC.InitialRoom;
            if (IsValid(Room) && (Room->State == ERoomState::Fire || Room->State == ERoomState::Risk))
            {
                Result.Add(NPC);
            }
        }
    }

    return Result;
}

// ============================ 통계 ============================

void AGameManager::ResetSessionStatistics()
{
    CurrentSessionStats = FGameStatistics();
    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Session statistics reset"));
}

// ============================ 이벤트 리스닝 ============================

void AGameManager::OnFireExtinguished(AFireActor* Fire)
{
    CurrentSessionStats.TotalFiresExtinguished++;

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Fire extinguished - Total: %d"),
        CurrentSessionStats.TotalFiresExtinguished);
}

void AGameManager::OnBackdraftTriggered()
{
    CurrentSessionStats.TotalBackdraftsTriggered++;

    UE_LOG(LogGameManager, Error, TEXT("[GameManager] BACKDRAFT! Total: %d"),
        CurrentSessionStats.TotalBackdraftsTriggered);
}

void AGameManager::OnGasTankBLEVE(FVector Location)
{
    CurrentSessionStats.TotalGasTanksExploded++;

    UE_LOG(LogGameManager, Error, TEXT("[GameManager] GAS TANK EXPLOSION at %s! Total: %d"),
        *Location.ToString(), CurrentSessionStats.TotalGasTanksExploded);
}

void AGameManager::OnDoorBreached(ADoorActor* Door)
{
    CurrentSessionStats.TotalDoorsBreached++;

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Door breached: %s - Total: %d"),
        *GetNameSafe(Door), CurrentSessionStats.TotalDoorsBreached);
}

void AGameManager::OnVentHoleCreated(int32 HoleCount)
{
    CurrentSessionStats.TotalVentHolesCreated++;

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] VentHole created (Total holes: %d) - Events: %d"),
        HoleCount, CurrentSessionStats.TotalVentHolesCreated);
}

void AGameManager::OnPlayerDeath()
{
    UE_LOG(LogGameManager, Error, TEXT("[GameManager] PLAYER DEATH!"));

    ChangeGameState(EGameState::MissionFailed);
}

void AGameManager::OnWaterUsed(float Amount)
{
    CurrentSessionStats.TotalWaterUsed += Amount;
}

// ============================ 저장/로드 ============================

void AGameManager::SaveGameProgress()
{
    // TODO: SaveGame 시스템 구현
    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Game progress saved"));
}

void AGameManager::LoadGameProgress()
{
    // TODO: SaveGame 시스템 구현
    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Game progress loaded"));
}

// ============================ 유틸리티 ============================

float AGameManager::GetChapterElapsedTime() const
{
    return ChapterElapsedTime;
}

float AGameManager::GetChapterRemainingTime() const
{
    if (!Chapters.Contains(CurrentChapterID))
        return 0.f;

    const FChapterData& ChapterData = Chapters[CurrentChapterID];
    if (ChapterData.TimeLimit <= 0.f)
        return -1.f;

    return FMath::Max(0.f, ChapterData.TimeLimit - ChapterElapsedTime);
}

void AGameManager::DebugTriggerAllFireEvents()
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    FChapterData& ChapterData = Chapters[CurrentChapterID];

    for (FFireSpawnEvent& Event : ChapterData.FireEvents)
    {
        if (!Event.bHasTriggered)
        {
            TriggerFireEvent(Event.EventID);
        }
    }

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] DEBUG: All fire events triggered"));
}

void AGameManager::DebugPrintChapterInfo()
{
    if (!Chapters.Contains(CurrentChapterID))
        return;

    const FChapterData& ChapterData = Chapters[CurrentChapterID];

    UE_LOG(LogGameManager, Warning, TEXT("========== CHAPTER INFO =========="));
    UE_LOG(LogGameManager, Warning, TEXT("Chapter: %s"), *ChapterData.ChapterTitle.ToString());
    UE_LOG(LogGameManager, Warning, TEXT("Fire Events: %d"), ChapterData.FireEvents.Num());
    UE_LOG(LogGameManager, Warning, TEXT("NPCs to Rescue: %d"), ChapterData.NPCsToRescue.Num());
    UE_LOG(LogGameManager, Warning, TEXT("Objectives: %d"), ChapterData.ChapterObjectives.Num());
    UE_LOG(LogGameManager, Warning, TEXT("Time Limit: %.1f"), ChapterData.TimeLimit);
    UE_LOG(LogGameManager, Warning, TEXT("Elapsed Time: %.1f"), ChapterElapsedTime);
    UE_LOG(LogGameManager, Warning, TEXT("Active Fires: %d"), GetActiveFireCount());
    UE_LOG(LogGameManager, Warning, TEXT("NPCs Rescued: %d / %d"), GetRescuedNPCCount(), GetTotalNPCCount());
    UE_LOG(LogGameManager, Warning, TEXT("=================================="));
}

// ============================ 내부 함수 ============================

void AGameManager::CheckChapterCompletion()
{
    if (CurrentGameState != EGameState::Gameplay)
        return;

    // 필수 목표가 모두 완료되었는지 체크
    if (AreMandatoryObjectivesComplete())
    {
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] All mandatory objectives complete!"));
        ChangeGameState(EGameState::MissionComplete);
    }

    // 필수 목표 중 실패한 것이 있는지 체크
    for (const UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj) && !Obj->bIsOptional && Obj->IsFailed())
        {
            UE_LOG(LogGameManager, Error, TEXT("[GameManager] Mandatory objective failed!"));
            ChangeGameState(EGameState::MissionFailed);
            return;
        }
    }
}

void AGameManager::BindGameEvents()
{
    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Binding game events..."));

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] BindGameEvents: World is null, skipping event binding"));
        return;
    }

    // Room 이벤트
    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* Room = *It;
        if (IsValid(Room))
        {
            // Delegate가 유효한지 체크 후 바인딩
            if (Room->OnFireExtinguished.IsBound() == false)
            {
                Room->OnFireExtinguished.AddDynamic(this, &AGameManager::OnFireExtinguished);
            }
            if (Room->OnBackdraft.IsBound() == false)
            {
                Room->OnBackdraft.AddUniqueDynamic(this, &AGameManager::OnBackdraftTriggered);
            }
        }
    }

    // GasTank 이벤트
    for (TActorIterator<AGasTankActor> It(World); It; ++It)
    {
        AGasTankActor* Tank = *It;
        if (IsValid(Tank) && IsValid(Tank->PressureVessel))
        {
            if (Tank->PressureVessel->OnBLEVE.IsBound() == false)
            {
                Tank->PressureVessel->OnBLEVE.AddDynamic(this, &AGameManager::OnGasTankBLEVE);
            }
        }
    }

    // Door 이벤트
    for (TActorIterator<ADoorActor> It(World); It; ++It)
    {
        ADoorActor* Door = *It;
        if (IsValid(Door))
        {
            if (Door->OnVentHoleCreated.IsBound() == false)
            {
                Door->OnVentHoleCreated.AddDynamic(this, &AGameManager::OnVentHoleCreated);
            }
        }
    }
}

void AGameManager::UnbindGameEvents()
{
    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Unbinding game events..."));

    UWorld* World = GetWorld();
    if (!World)
        return;

    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* Room = *It;
        if (IsValid(Room))
        {
            Room->OnFireExtinguished.RemoveDynamic(this, &AGameManager::OnFireExtinguished);
            Room->OnBackdraft.RemoveDynamic(this, &AGameManager::OnBackdraftTriggered);
        }
    }

    for (TActorIterator<AGasTankActor> It(World); It; ++It)
    {
        AGasTankActor* Tank = *It;
        if (IsValid(Tank) && IsValid(Tank->PressureVessel))
        {
            Tank->PressureVessel->OnBLEVE.RemoveDynamic(this, &AGameManager::OnGasTankBLEVE);
        }
    }

    for (TActorIterator<ADoorActor> It(World); It; ++It)
    {
        ADoorActor* Door = *It;
        if (IsValid(Door))
        {
            Door->OnVentHoleCreated.RemoveDynamic(this, &AGameManager::OnVentHoleCreated);
        }
    }
}

void AGameManager::InitializeChapters()
{
    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Initializing chapters..."));

    // BP에서 이미 설정되어 있으면 C++ 초기화 스킵
    if (Chapters.Num() > 0)
    {
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapters already initialized in Blueprint, skipping C++ setup"));

        // 첫 챕터는 항상 언락
        if (Chapters.Contains(EChapterID::Chapter1))
        {
            UnlockChapter(EChapterID::Chapter1);
        }
        if (Chapters.Contains(EChapterID::Tutorial))
        {
            UnlockChapter(EChapterID::Tutorial);
        }

        return;
    }

    // BP에 데이터가 없으면 C++에서 기본값 설정
    UE_LOG(LogGameManager, Log, TEXT("[GameManager] No Blueprint data, using C++ defaults"));

    SetupChapter1();
    SetupChapter2();

    // 첫 챕터는 항상 언락
    UnlockChapter(EChapterID::Chapter1);
    UnlockChapter(EChapterID::Tutorial);
}

void AGameManager::SetupChapter1()
{
    FChapterData Chapter1;
    Chapter1.ChapterID = EChapterID::Chapter1;
    Chapter1.ChapterTitle = FText::FromString(TEXT("Chapter 1: Kitchen Fire"));
    Chapter1.ChapterDescription = FText::FromString(TEXT("Extinguish the kitchen fire and rescue residents"));
    Chapter1.LevelName = FName(TEXT("Chapter1_Kitchen"));

    Chapter1.IntroText = FText::FromString(TEXT("A fire has broken out in the kitchen.\nExtinguish all fires and evacuate residents."));
    Chapter1.SuccessOutroText = FText::FromString(TEXT("Excellent! All residents evacuated safely."));
    Chapter1.FailureOutroText = FText::FromString(TEXT("Mission failed. Be more careful."));
    Chapter1.IntroDuration = 5.f;

    // 화재 시나리오 - 스크립트 방식
    Chapter1.ScenarioType = EFireScenarioType::Scripted;

    // 화재 이벤트 1: 시작 시 주방에서 발생
    FFireSpawnEvent Event1;
    Event1.TriggerTime = 0.f;
    Event1.FireType = ECombustibleType::Oil;
    Event1.InitialIntensity = 1.0f;
    Event1.EventID = TEXT("Kitchen_Initial_Fire");
    Chapter1.FireEvents.Add(Event1);

    // 화재 이벤트 2: 30초 후 거실로 확산
    FFireSpawnEvent Event2;
    Event2.TriggerTime = 30.f;
    Event2.FireType = ECombustibleType::Normal;
    Event2.InitialIntensity = 0.8f;
    Event2.EventID = TEXT("LivingRoom_Spread");
    Chapter1.FireEvents.Add(Event2);

    // 화재 이벤트 3: 60초 후 전기 패널
    FFireSpawnEvent Event3;
    Event3.TriggerTime = 60.f;
    Event3.FireType = ECombustibleType::Electric;
    Event3.InitialIntensity = 1.2f;
    Event3.EventID = TEXT("Electric_Panel_Fire");
    Chapter1.FireEvents.Add(Event3);

    // NPC 구조
    FNPCRescueData NPC1;
    NPC1.NPCName = FText::FromString(TEXT("Kim Chulsoo"));
    NPC1.MaxSurvivalTime = 180.f;
    Chapter1.NPCsToRescue.Add(NPC1);

    FNPCRescueData NPC2;
    NPC2.NPCName = FText::FromString(TEXT("Park Younghee"));
    NPC2.MaxSurvivalTime = 180.f;
    Chapter1.NPCsToRescue.Add(NPC2);

    Chapter1.MinNPCRescueRequired = 2;

    // 목표 (블루프린트에서 설정 가능하도록 비워둠)
    Chapter1.TimeLimit = 300.f; // 5분
    Chapter1.TargetScore = 1000;

    Chapters.Add(EChapterID::Chapter1, Chapter1);
}

void AGameManager::SetupChapter2()
{
    FChapterData Chapter2;
    Chapter2.ChapterID = EChapterID::Chapter2;
    Chapter2.ChapterTitle = FText::FromString(TEXT("Chapter 2: Industrial Facility"));
    Chapter2.ChapterDescription = FText::FromString(TEXT("Stop the fire and prevent gas tank explosion"));
    Chapter2.LevelName = FName(TEXT("Chapter2_Industrial"));

    Chapter2.IntroText = FText::FromString(TEXT("A major fire broke out at industrial facility.\nPrevent gas tank explosion and rescue workers."));
    Chapter2.SuccessOutroText = FText::FromString(TEXT("Crisis averted! Explosion prevented."));
    Chapter2.FailureOutroText = FText::FromString(TEXT("Explosion occurred. Respond faster."));
    Chapter2.IntroDuration = 5.f;

    // 화재 시나리오 - 점진적 방식
    Chapter2.ScenarioType = EFireScenarioType::Progressive;

    // 화재 이벤트 1: 시작 시 창고
    FFireSpawnEvent Event1;
    Event1.TriggerTime = 0.f;
    Event1.FireType = ECombustibleType::Normal;
    Event1.InitialIntensity = 1.5f;
    Event1.EventID = TEXT("Warehouse_Initial");
    Chapter2.FireEvents.Add(Event1);

    // 화재 이벤트 2: 20초 후 유류 저장소
    FFireSpawnEvent Event2;
    Event2.TriggerTime = 20.f;
    Event2.FireType = ECombustibleType::Oil;
    Event2.InitialIntensity = 2.0f;
    Event2.EventID = TEXT("Oil_Storage_Fire");
    Chapter2.FireEvents.Add(Event2);

    // 화재 이벤트 3: 45초 후 가스탱크 근처
    FFireSpawnEvent Event3;
    Event3.TriggerTime = 45.f;
    Event3.FireType = ECombustibleType::Explosive;
    Event3.InitialIntensity = 1.8f;
    Event3.EventID = TEXT("GasTank_Proximity");
    Chapter2.FireEvents.Add(Event3);

    // NPC 구조
    FNPCRescueData NPC1;
    NPC1.NPCName = FText::FromString(TEXT("Worker A"));
    NPC1.MaxSurvivalTime = 240.f;
    Chapter2.NPCsToRescue.Add(NPC1);

    FNPCRescueData NPC2;
    NPC2.NPCName = FText::FromString(TEXT("Worker B"));
    NPC2.MaxSurvivalTime = 240.f;
    Chapter2.NPCsToRescue.Add(NPC2);

    FNPCRescueData NPC3;
    NPC3.NPCName = FText::FromString(TEXT("Worker C"));
    NPC3.MaxSurvivalTime = 240.f;
    Chapter2.NPCsToRescue.Add(NPC3);

    Chapter2.MinNPCRescueRequired = 3;

    Chapter2.TimeLimit = 420.f; // 7분
    Chapter2.TargetScore = 1500;
    Chapter2.bIsUnlocked = false; // 1장 클리어 후 언락

    Chapters.Add(EChapterID::Chapter2, Chapter2);
}