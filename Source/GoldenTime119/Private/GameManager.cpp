// ============================ GameManager.cpp ============================
#include "GameManager.h"
#include "RoomActor.h"
#include "FireActor.h"
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
    PrimaryActorTick.TickInterval = 0.1f; // 0.1초마다 업데이트
}

void AGameManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] BeginPlay - Initializing..."));

    // 플레이어 찾기
    FindPlayerCharacter();

    // 씬 액터들 찾기
    FindSceneActors();

    // 플레이어 이벤트 바인딩
    BindPlayerEvents();

    bIsInitialized = true;

    // 자동 시작 설정이 되어있으면 게임 시작
    if (bAutoStartFire)
    {
        StartGame();
    }
}

void AGameManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsInitialized) return;

    // 게임 진행 중일 때만 업데이트
    if (CurrentGameState == EGameState::InProgress)
    {
        // 모든 목표 업데이트
        UpdateAllObjectives(DeltaTime);

        // 플레이어 바이탈 체크
        CheckPlayerVitals(DeltaTime);

        // 미션 완료/실패 체크
        CheckMissionCompletion();
    }
}

void AGameManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(FireStartTimerHandle);

    Super::EndPlay(EndPlayReason);
}

// ============================ 게임 제어 메서드 ============================

void AGameManager::StartGame()
{
    if (CurrentGameState != EGameState::NotStarted && CurrentGameState != EGameState::Ended)
    {
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Game already started"));
        return;
    }

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Starting Game - Chapter %s"),
        *GetChapterString());

    ChangeGameState(EGameState::Starting);

    GameStartTime = GetWorld()->GetTimeSeconds();
    TotalScore = 0;

    // 챕터 설정
    SetupChapter(CurrentChapter);

    // 화재 시작 (딜레이 적용)
    if (bAutoStartFire)
    {
        GetWorld()->GetTimerManager().SetTimer(
            FireStartTimerHandle,
            this,
            &AGameManager::StartInitialFire,
            FireStartDelay,
            false
        );
    }

    // 게임 진행 상태로 전환
    ChangeGameState(EGameState::InProgress);

    // 첫 번째 목표 시작 (순차 모드일 경우)
    if (bSequentialObjectives && ActiveObjectives.Num() > 0)
    {
        StartNextObjective();
    }
    else
    {
        // 동시 진행 모드: 모든 목표 시작
        for (UMissionObjective* Obj : ActiveObjectives)
        {
            if (IsValid(Obj))
            {
                Obj->StartObjective(GetWorld());
            }
        }
    }

    OnMissionStarted.Broadcast(CurrentChapter);
}

void AGameManager::PauseGame()
{
    if (CurrentGameState != EGameState::InProgress) return;

    ChangeGameState(EGameState::Paused);
    UGameplayStatics::SetGamePaused(GetWorld(), true);

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Game Paused"));
}

void AGameManager::ResumeGame()
{
    if (CurrentGameState != EGameState::Paused) return;

    ChangeGameState(EGameState::InProgress);
    UGameplayStatics::SetGamePaused(GetWorld(), false);

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Game Resumed"));
}

void AGameManager::RestartGame()
{
    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Restarting Game"));

    // 게임 상태 리셋
    CurrentGameState = EGameState::NotStarted;
    CurrentObjectiveIndex = 0;
    TotalScore = 0;

    // 목표들 리셋
    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj))
        {
            Obj->ResetObjective();
        }
    }

    CompletedObjectives.Empty();
    FailedObjectives.Empty();
    RescuedNPCs.Empty();

    // 레벨 리로드 (블루프린트에서 구현 가능하도록 이벤트 발생)
    // UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));

    // 재시작
    StartGame();
}

void AGameManager::EndGame(bool bSuccess, const FString& Reason)
{
    if (CurrentGameState == EGameState::Ended) return;

    GameEndTime = GetWorld()->GetTimeSeconds();

    if (bSuccess)
    {
        ChangeGameState(EGameState::MissionComplete);
        OnMissionCompleted.Broadcast(CurrentChapter, TotalScore);

        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Mission COMPLETED - Score: %d"), TotalScore);
    }
    else
    {
        ChangeGameState(EGameState::MissionFailed);
        OnMissionFailed.Broadcast(CurrentChapter, Reason);

        UE_LOG(LogGameManager, Error, TEXT("[GameManager] Mission FAILED - Reason: %s"), *Reason);
    }

    ChangeGameState(EGameState::Ended);
}

// ============================ 미션 관리 메서드 ============================

void AGameManager::SetupChapter(EChapter Chapter)
{
    // 기존 목표들 클리어
    ActiveObjectives.Empty();
    CompletedObjectives.Empty();
    FailedObjectives.Empty();
    CurrentObjectiveIndex = 0;

    switch (Chapter)
    {
    case EChapter::Chapter1:
        SetupChapter1();
        break;

    case EChapter::Chapter2:
        SetupChapter2();
        break;

    case EChapter::Chapter3:
        SetupChapter3();
        break;

    default:
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Unknown chapter"));
        break;
    }

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapter Setup Complete - %d objectives"),
        ActiveObjectives.Num());
}

void AGameManager::SetupChapter1()
{
    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Setting up Chapter 1"));

    // Objective 1: Extinguish All Fires
    UMissionObjective* FireObjective = NewObject<UMissionObjective>(this);
    FireObjective->ObjectiveType = EMissionObjectiveType::ExtinguishAllFires;
    FireObjective->ObjectiveTitle = FText::FromString(TEXT("Extinguish All Fires"));
    FireObjective->ObjectiveDescription = FText::FromString(TEXT("Put out all fires"));
    FireObjective->ScoreReward = 300;
    FireObjective->bCanFail = true;
    FireObjective->bFailOnTimeout = false;
    FireObjective->TargetCount = InitialFireCount;
    AddObjective(FireObjective);

    // Objective 2: Survival
    UMissionObjective* SurvivalObjective = NewObject<UMissionObjective>(this);
    SurvivalObjective->ObjectiveType = EMissionObjectiveType::KeepHealthAbove;
    SurvivalObjective->ObjectiveTitle = FText::FromString(TEXT("Survival"));
    SurvivalObjective->ObjectiveDescription = FText::FromString(TEXT("Keep your health above 1%"));
    SurvivalObjective->ScoreReward = 200;
    SurvivalObjective->ThresholdValue = 0.01f;
    SurvivalObjective->bCanFail = true;
    SurvivalObjective->bFailOnPlayerDeath = true;
    SurvivalObjective->DurationSeconds = 600.f;
    SurvivalObjective->bIsOptional = false;
    AddObjective(SurvivalObjective);

    // Objective 3: Rescue NPCs
    UMissionObjective* RescueObjective = NewObject<UMissionObjective>(this);
    RescueObjective->ObjectiveType = EMissionObjectiveType::RescueNPC;
    RescueObjective->ObjectiveTitle = FText::FromString(TEXT("Rescue Victims"));
    RescueObjective->ObjectiveDescription = FText::FromString(TEXT("Rescue 2 victims in the bedroom"));
    RescueObjective->ScoreReward = 500;
    RescueObjective->TargetNPCs = NPCsToRescue;
    RescueObjective->bCanFail = false;
    AddObjective(RescueObjective);

    // Objective 4: Escape
    if (IsValid(ExitPoint))
    {
        UMissionObjective* EscapeObjective = NewObject<UMissionObjective>(this);
        EscapeObjective->ObjectiveType = EMissionObjectiveType::EscapeToExitPoint;
        EscapeObjective->ObjectiveTitle = FText::FromString(TEXT("Escape"));
        EscapeObjective->ObjectiveDescription = FText::FromString(TEXT("Escape to a safe location"));
        EscapeObjective->ScoreReward = 300;
        EscapeObjective->ExitPoint = ExitPoint;
        EscapeObjective->ExitReachDistance = 200.f;
        EscapeObjective->MaxDistanceForProgress = 5000.f;
        EscapeObjective->bCanFail = false;
        AddObjective(EscapeObjective);
    }

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapter 1 setup complete - %d objectives"),
        ActiveObjectives.Num());
}

void AGameManager::SetupChapter2()
{
    // 챕터 2는 추후 확장
    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapter 2 - Not implemented yet"));
}

void AGameManager::SetupChapter3()
{
    // 챕터 3은 추후 확장
    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Chapter 3 - Not implemented yet"));
}

void AGameManager::AddObjective(UMissionObjective* Objective)
{
    if (!IsValid(Objective)) return;

    ActiveObjectives.Add(Objective);

    // 이벤트 바인딩
    Objective->OnProgressChanged.AddDynamic(this, &AGameManager::OnObjectiveProgressChanged);
    Objective->OnStatusChanged.AddDynamic(this, &AGameManager::OnObjectiveStatusChanged);

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Objective Added: %s"),
        *Objective->ObjectiveTitle.ToString());
}

void AGameManager::StartNextObjective()
{
    if (!bSequentialObjectives) return;
    if (CurrentObjectiveIndex >= ActiveObjectives.Num()) return;

    UMissionObjective* NextObjective = ActiveObjectives[CurrentObjectiveIndex];
    if (IsValid(NextObjective))
    {
        NextObjective->StartObjective(GetWorld());
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Started Objective %d: %s"),
            CurrentObjectiveIndex + 1, *NextObjective->ObjectiveTitle.ToString());
    }
}

void AGameManager::CompleteCurrentObjective()
{
    if (!bSequentialObjectives) return;

    UMissionObjective* CurrentObj = GetCurrentObjective();
    if (IsValid(CurrentObj) && !CurrentObj->IsCompleted())
    {
        CurrentObj->CompleteObjective();
    }

    CurrentObjectiveIndex++;

    // 다음 목표 시작
    if (CurrentObjectiveIndex < ActiveObjectives.Num())
    {
        StartNextObjective();
    }
}

UMissionObjective* AGameManager::GetCurrentObjective() const
{
    if (bSequentialObjectives && CurrentObjectiveIndex < ActiveObjectives.Num())
    {
        return ActiveObjectives[CurrentObjectiveIndex];
    }

    return nullptr;
}

float AGameManager::GetMissionProgress01() const
{
    if (ActiveObjectives.Num() == 0) return 0.f;

    int32 TotalCompleted = CompletedObjectives.Num();
    int32 TotalObjectives = ActiveObjectives.Num();

    return (float)TotalCompleted / (float)TotalObjectives;
}

// ============================ 화재 시스템 메서드 ============================

void AGameManager::StartInitialFire()
{
    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Starting initial fires..."));

    for (int32 i = 0; i < InitialFireCount; ++i)
    {
        CreateFireAtRandomLocation();
    }
}

void AGameManager::CreateFireAtRandomLocation()
{
    TArray<ARoomActor*> AvailableRooms;

    // PotentialFireRooms가 설정되어 있으면 그것 사용, 아니면 모든 방
    if (PotentialFireRooms.Num() > 0)
    {
        AvailableRooms = PotentialFireRooms;
    }
    else
    {
        AvailableRooms = GetAllRooms();
    }

    if (AvailableRooms.Num() == 0)
    {
        UE_LOG(LogGameManager, Error, TEXT("[GameManager] No rooms available for fire"));
        return;
    }

    // 랜덤 방 선택
    int32 RandomIndex = FMath::RandRange(0, AvailableRooms.Num() - 1);
    ARoomActor* SelectedRoom = AvailableRooms[RandomIndex];

    CreateFireInRoom(SelectedRoom);
}

void AGameManager::CreateFireInRoom(ARoomActor* Room)
{
    if (!IsValid(Room))
    {
        UE_LOG(LogGameManager, Error, TEXT("[GM] CreateFireInRoom: Room is NULL!"));
        return;
    }
    UE_LOG(LogGameManager, Warning, TEXT("[GM] CreateFireInRoom: %s"), *Room->GetName());

    // RoomActor가 이미 등록한 가연물 목록 사용
    TArray<UCombustibleComponent*> Combustibles;
    Room->GetCombustiblesInRoom(Combustibles, true); // true = 이미 타고 있는 것 제외

    UE_LOG(LogGameManager, Warning, TEXT("[GM] Found %d combustibles in %s"),
        Combustibles.Num(), *Room->GetName());

    if (Combustibles.Num() == 0)
    {
        UE_LOG(LogGameManager, Error, TEXT("[GM] NO COMBUSTIBLES! Cannot ignite."));
        return;
    }

    // 랜덤 가연물 선택
    int32 RandomIndex = FMath::RandRange(0, Combustibles.Num() - 1);
    UCombustibleComponent* SelectedCombustible = Combustibles[RandomIndex];

    if (IsValid(SelectedCombustible))
    {
        // RoomActor의 IgniteActor를 사용하여 발화
        Room->IgniteActor(SelectedCombustible->GetOwner());

        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Fire started in room %s at %s"),
            *Room->GetName(), *SelectedCombustible->GetOwner()->GetName());
    }
}

int32 AGameManager::GetTotalActiveFireCount() const
{
    int32 TotalCount = 0;

    for (TActorIterator<AFireActor> It(GetWorld()); It; ++It)
    {
        AFireActor* Fire = *It;
        if (IsValid(Fire))
        {
            TotalCount++;
        }
    }

    return TotalCount;
}

TArray<ARoomActor*> AGameManager::GetAllRooms() const
{
    TArray<ARoomActor*> Rooms;

    for (TActorIterator<ARoomActor> It(GetWorld()); It; ++It)
    {
        ARoomActor* Room = *It;
        if (IsValid(Room))
        {
            Rooms.Add(Room);
        }
    }

    return Rooms;
}

// ============================ NPC 관리 메서드 ============================

void AGameManager::RegisterNPC(AActor* NPC)
{
    if (!IsValid(NPC)) return;
    if (NPCsToRescue.Contains(NPC)) return;

    NPCsToRescue.Add(NPC);

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] NPC Registered: %s"), *NPC->GetName());
}

void AGameManager::RescueNPC(AActor* NPC)
{
    if (!IsValid(NPC)) return;
    if (!NPCsToRescue.Contains(NPC)) return;
    if (RescuedNPCs.Contains(NPC)) return;

    RescuedNPCs.Add(NPC);

    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] NPC Rescued: %s (%d/%d)"),
        *NPC->GetName(), RescuedNPCs.Num(), NPCsToRescue.Num());

    // 모든 목표에 알림
    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj))
        {
            Obj->NotifyNPCRescued(NPC);
        }
    }
}

// ============================ 플레이어 바이탈 체크 ============================

bool AGameManager::IsPlayerAlive() const
{
    if (!IsValid(PlayerCharacter)) return false;

    UVitalComponent* Vital = PlayerCharacter->FindComponentByClass<UVitalComponent>();
    if (!IsValid(Vital)) return true;

    return Vital->GetHp01() > 0.f;
}

float AGameManager::GetPlayerHealth01() const
{
    if (!IsValid(PlayerCharacter)) return 0.f;

    UVitalComponent* Vital = PlayerCharacter->FindComponentByClass<UVitalComponent>();
    if (!IsValid(Vital)) return 1.f;

    return Vital->GetHp01();
}

float AGameManager::GetPlayerOxygen01() const
{
    if (!IsValid(PlayerCharacter)) return 0.f;

    UVitalComponent* Vital = PlayerCharacter->FindComponentByClass<UVitalComponent>();
    if (!IsValid(Vital)) return 1.f;

    return Vital->GetO201();
}

float AGameManager::GetPlayerTemperature() const
{
    if (!IsValid(PlayerCharacter)) return 0.f;

    UVitalComponent* Vital = PlayerCharacter->FindComponentByClass<UVitalComponent>();
    if (!IsValid(Vital)) return 0.f;

    return Vital->GetTemp01();
}

void AGameManager::CheckPlayerVitals(float DeltaTime)
{
    if (!IsValid(PlayerCharacter)) return;

    UVitalComponent* Vital = PlayerCharacter->FindComponentByClass<UVitalComponent>();
    if (!IsValid(Vital)) return;

    const float HP = Vital->GetHp01();
    const float O2 = Vital->GetO201();
    const float Temp = Vital->GetTemp01();

    // 플레이어 사망 체크
    if (HP <= 0.f)
    {
        // 모든 목표에 알림
        for (UMissionObjective* Obj : ActiveObjectives)
        {
            if (IsValid(Obj))
            {
                Obj->NotifyPlayerDeath();
            }
        }

        EndGame(false, TEXT("Player died"));
        return;
    }

    // 바이탈 경고 쿨다운 감소
    if (VitalWarningCooldown > 0.f)
    {
        VitalWarningCooldown -= DeltaTime;
        return;
    }

    // 위험 수준 체크
    if (HP < VitalCriticalThreshold)
    {
        OnVitalWarning.Broadcast(TEXT("Health"), HP);
        VitalWarningCooldown = VitalWarningInterval;
    }
    else if (O2 < VitalCriticalThreshold)
    {
        OnVitalWarning.Broadcast(TEXT("Oxygen"), O2);
        VitalWarningCooldown = VitalWarningInterval;
    }
    else if (Temp > 0.7f) // 70% 이상 위험 체온
    {
        OnVitalWarning.Broadcast(TEXT("Temperature"), Temp);
        VitalWarningCooldown = VitalWarningInterval;
    }
    // 경고 수준 체크
    else if (HP < VitalWarningThreshold)
    {
        OnVitalWarning.Broadcast(TEXT("Health"), HP);
        VitalWarningCooldown = VitalWarningInterval * 2.f;
    }
    else if (O2 < VitalWarningThreshold)
    {
        OnVitalWarning.Broadcast(TEXT("Oxygen"), O2);
        VitalWarningCooldown = VitalWarningInterval * 2.f;
    }
}

// ============================ 내부 메서드 ============================

void AGameManager::ChangeGameState(EGameState NewState)
{
    if (CurrentGameState == NewState) return;

    EGameState OldState = CurrentGameState;
    CurrentGameState = NewState;

    OnGameStateChanged.Broadcast(OldState, NewState);

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] State Changed: %s -> %s"),
        *UEnum::GetValueAsString(OldState),
        *UEnum::GetValueAsString(NewState));
}

void AGameManager::BindFireActorEvents(AFireActor* Fire)
{
    if (!IsValid(Fire)) return;

    // FireActor 이벤트 바인딩
    // Note: FireActor.h에 델리게이트가 정의되어 있지 않으므로
    // RoomActor의 OnFireExtinguished/OnFireStarted 이벤트를 활용
    // 또는 추후 FireActor에 델리게이트 추가 필요
}

void AGameManager::BindRoomActorEvents(ARoomActor* Room)
{
    if (!IsValid(Room)) return;

    // RoomActor 이벤트 바인딩
    Room->OnBackdraft.AddDynamic(this, &AGameManager::OnBackdraftOccurred);

    // 화재 진압 이벤트 바인딩
    Room->OnFireExtinguished.AddDynamic(this, &AGameManager::OnFireExtinguished);
    Room->OnFireSpawned.AddDynamic(this, &AGameManager::OnFireSpawned);
}

void AGameManager::BindPlayerEvents()
{
    // 플레이어 바이탈 이벤트는 CheckPlayerVitals에서 폴링 방식으로 체크
}

void AGameManager::OnObjectiveProgressChanged(UMissionObjective* Objective, float Progress01, FString ProgressText)
{
    if (!IsValid(Objective)) return;

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Objective Progress: %s - %.1f%% (%s)"),
        *Objective->ObjectiveTitle.ToString(), Progress01 * 100.f, *ProgressText);
}

void AGameManager::OnObjectiveStatusChanged(UMissionObjective* Objective, EMissionObjectiveStatus NewStatus)
{
    if (!IsValid(Objective)) return;

    if (NewStatus == EMissionObjectiveStatus::Completed)
    {
        if (!CompletedObjectives.Contains(Objective))
        {
            CompletedObjectives.Add(Objective);
            TotalScore += Objective->ScoreReward;

            OnObjectiveCompleted.Broadcast(Objective);

            UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Objective COMPLETED: %s (+%d score)"),
                *Objective->ObjectiveTitle.ToString(), Objective->ScoreReward);

            // 순차 모드: 다음 목표 시작
            if (bSequentialObjectives)
            {
                CurrentObjectiveIndex++;
                StartNextObjective();
            }
        }
    }
    else if (NewStatus == EMissionObjectiveStatus::Failed)
    {
        if (!FailedObjectives.Contains(Objective))
        {
            FailedObjectives.Add(Objective);

            OnObjectiveFailed.Broadcast(Objective);

            UE_LOG(LogGameManager, Error, TEXT("[GameManager] Objective FAILED: %s"),
                *Objective->ObjectiveTitle.ToString());

            // 필수 목표 실패 시 게임 종료
            if (!Objective->bIsOptional)
            {
                EndGame(false, FString::Printf(TEXT("Failed: %s"), *Objective->ObjectiveTitle.ToString()));
            }
        }
    }
}

void AGameManager::OnFireExtinguished(AFireActor* Fire)
{
    if (!IsValid(Fire)) return;

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Fire Extinguished: %s"), *Fire->GetName());

    // 모든 목표에 알림
    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj))
        {
            Obj->NotifyFireExtinguished();
        }
    }
}

void AGameManager::OnFireSpawned(AFireActor* Fire)
{
    if (!IsValid(Fire)) return;

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Fire Spawned: %s"), *Fire->GetName());

    // 새로 생성된 Fire에 이벤트 바인딩 (필요시)
    BindFireActorEvents(Fire);
}

void AGameManager::OnBackdraftOccurred()
{
    UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Backdraft Occurred!"));

    // 모든 목표에 알림
    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj))
        {
            Obj->NotifyBackdraftOccurred();
        }
    }
}

void AGameManager::CheckMissionCompletion()
{
    if (CurrentGameState != EGameState::InProgress) return;

    // 모든 필수 목표 완료 체크
    bool bAllRequiredComplete = true;

    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (!IsValid(Obj)) continue;
        if (Obj->bIsOptional) continue;

        if (!Obj->IsCompleted())
        {
            bAllRequiredComplete = false;
            break;
        }
    }

    // 모든 필수 목표 완료 시 미션 성공
    if (bAllRequiredComplete)
    {
        EndGame(true, TEXT("All objectives completed"));
    }
}

void AGameManager::UpdateAllObjectives(float DeltaTime)
{
    for (UMissionObjective* Obj : ActiveObjectives)
    {
        if (IsValid(Obj) && Obj->IsInProgress())
        {
            Obj->UpdateProgress(DeltaTime, GetWorld());
        }
    }
}

void AGameManager::FindPlayerCharacter()
{
    // VR Template 기반: BP_VRPawn은 "Player" 태그를 가짐
    // 먼저 태그로 찾기 시도
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), FoundActors);

    if (FoundActors.Num() > 0 && IsValid(FoundActors[0]))
    {
        PlayerCharacter = Cast<APawn>(FoundActors[0]);

        if (IsValid(PlayerCharacter))
        {
            UE_LOG(LogGameManager, Log, TEXT("[GameManager] Player found by tag: %s"),
                *PlayerCharacter->GetName());
            return;
        }
    }

    // 태그로 못 찾으면 PlayerController를 통해 찾기
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (IsValid(PlayerPawn))
    {
        PlayerCharacter = PlayerPawn;

        UE_LOG(LogGameManager, Log, TEXT("[GameManager] Player character found: %s"),
            *PlayerCharacter->GetName());
    }
    else
    {
        UE_LOG(LogGameManager, Warning, TEXT("[GameManager] Player pawn not found"));
    }
}

void AGameManager::FindSceneActors()
{
    // Room 액터들 찾아서 이벤트 바인딩
    for (TActorIterator<ARoomActor> It(GetWorld()); It; ++It)
    {
        ARoomActor* Room = *It;
        if (IsValid(Room))
        {
            BindRoomActorEvents(Room);

            // PotentialFireRooms가 비어있으면 자동으로 추가
            if (PotentialFireRooms.Num() == 0)
            {
                PotentialFireRooms.Add(Room);
            }
        }
    }

    // 기존 Fire 액터들 찾아서 이벤트 바인딩
    for (TActorIterator<AFireActor> It(GetWorld()); It; ++It)
    {
        AFireActor* Fire = *It;
        if (IsValid(Fire))
        {
            BindFireActorEvents(Fire);
        }
    }

    // NPC 찾기 (태그로 찾기)
    TArray<AActor*> FoundNPCs;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("NPC"), FoundNPCs);

    for (AActor* NPC : FoundNPCs)
    {
        if (IsValid(NPC))
        {
            RegisterNPC(NPC);
        }
    }

    UE_LOG(LogGameManager, Log, TEXT("[GameManager] Scene actors found - Rooms: %d, NPCs: %d"),
        PotentialFireRooms.Num(), NPCsToRescue.Num());
}

// ============================ 유틸리티 ============================

float AGameManager::GetElapsedGameTime() const
{
    if (GameStartTime <= 0.f) return 0.f;

    if (CurrentGameState == EGameState::InProgress || CurrentGameState == EGameState::Paused)
    {
        return GetWorld()->GetTimeSeconds() - GameStartTime;
    }
    else if (CurrentGameState == EGameState::Ended)
    {
        return GameEndTime - GameStartTime;
    }

    return 0.f;
}

FString AGameManager::GetGameStateString() const
{
    return UEnum::GetValueAsString(CurrentGameState);
}

FString AGameManager::GetChapterString() const
{
    return UEnum::GetValueAsString(CurrentChapter);
}