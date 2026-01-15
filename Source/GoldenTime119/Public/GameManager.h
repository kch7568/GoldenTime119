// ============================ GameManager.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombustibleComponent.h"
#include "GameManager.generated.h"

// Forward declarations
class UMissionObjective;
class ARoomActor;
class AFireActor;
class AGasTankActor;
class ADoorActor;

// 게임 전체 상태
UENUM(BlueprintType)
enum class EGameState : uint8
{
    MainMenu            UMETA(DisplayName = "메인 메뉴"),
    ChapterSelect       UMETA(DisplayName = "챕터 선택"),
    Loading             UMETA(DisplayName = "로딩"),
    ChapterIntro        UMETA(DisplayName = "챕터 인트로"),
    Gameplay            UMETA(DisplayName = "게임플레이"),
    Paused              UMETA(DisplayName = "일시정지"),
    MissionComplete     UMETA(DisplayName = "미션 완료"),
    MissionFailed       UMETA(DisplayName = "미션 실패"),
    ChapterComplete     UMETA(DisplayName = "챕터 완료"),
    GameOver            UMETA(DisplayName = "게임 오버")
};

// 챕터 정보
UENUM(BlueprintType)
enum class EChapterID : uint8
{
    None                UMETA(DisplayName = "None"),
    Chapter1            UMETA(DisplayName = "Chapter 1"),
    Chapter2            UMETA(DisplayName = "Chapter 2"),
    Chapter3            UMETA(DisplayName = "Chapter 3"),
    Tutorial            UMETA(DisplayName = "Tutorial")
};

// 화재 시나리오 타입
UENUM(BlueprintType)
enum class EFireScenarioType : uint8
{
    Manual              UMETA(DisplayName = "수동 (디자이너가 배치)"),
    Scripted            UMETA(DisplayName = "스크립트 (타이머 기반)"),
    Random              UMETA(DisplayName = "랜덤 (동적 생성)"),
    Progressive         UMETA(DisplayName = "점진적 (단계별 확산)")
};

// 화재 발생 이벤트 데이터
USTRUCT(BlueprintType)
struct FFireSpawnEvent
{
    GENERATED_BODY()

    // 발생 시간 (챕터 시작 후 초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TriggerTime = 0.f;

    // 발생 위치 (Room 또는 Actor)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    class ARoomActor* TargetRoom;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* TargetActor;

    // 화재 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECombustibleType FireType = ECombustibleType::Normal;

    // 초기 강도
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InitialIntensity = 1.0f;

    // 트리거 조건 (옵션)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TriggerCondition;

    // 이벤트 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventID;

    // 이미 발생했는지 여부
    UPROPERTY(BlueprintReadOnly)
    bool bHasTriggered = false;

    FFireSpawnEvent()
    {
        EventID = FGuid::NewGuid().ToString();
        TargetRoom = nullptr;
        TargetActor = nullptr;
    }
};

// NPC 구조 데이터
USTRUCT(BlueprintType)
struct FNPCRescueData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NPCID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText NPCName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* NPCActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    class ARoomActor* InitialRoom;

    UPROPERTY(BlueprintReadOnly)
    bool bIsRescued = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsDead = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSurvivalTime = 300.f; // 5분

    UPROPERTY(BlueprintReadOnly)
    float TimeInDanger = 0.f;

    FNPCRescueData()
    {
        NPCID = FGuid::NewGuid().ToString();
        NPCActor = nullptr;
        InitialRoom = nullptr;
    }
};

// 챕터 데이터
USTRUCT(BlueprintType)
struct FChapterData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Basic")
    EChapterID ChapterID = EChapterID::Chapter1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Basic")
    FText ChapterTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Basic")
    FText ChapterDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Basic")
    FName LevelName;

    // 인트로/아웃트로
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Narrative")
    FText IntroText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Narrative")
    FText SuccessOutroText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Narrative")
    FText FailureOutroText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Narrative")
    float IntroDuration = 5.f;

    // 화재 시나리오
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Fire")
    EFireScenarioType ScenarioType = EFireScenarioType::Scripted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Fire")
    TArray<FFireSpawnEvent> FireEvents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Fire")
    int32 MaxSimultaneousFires = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Fire")
    bool bAllowFireSpread = true;

    // NPC 구조
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Rescue")
    TArray<FNPCRescueData> NPCsToRescue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Rescue")
    int32 MinNPCRescueRequired = 0;

    // 미션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Mission")
    TArray<UMissionObjective*> ChapterObjectives;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Mission")
    float TimeLimit = 600.f; // 10분

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter|Mission")
    int32 TargetScore = 1000;

    // 진행 상황
    UPROPERTY(BlueprintReadOnly, Category = "Chapter|Progress")
    bool bIsUnlocked = false;

    UPROPERTY(BlueprintReadOnly, Category = "Chapter|Progress")
    bool bIsCompleted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Chapter|Progress")
    int32 BestScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Chapter|Progress")
    float BestTime = 0.f;
};

// 게임 통계
USTRUCT(BlueprintType)
struct FGameStatistics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 TotalFiresExtinguished = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalNPCsRescued = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalBackdraftsTriggered = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalGasTanksExploded = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalDoorsBreached = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalVentHolesCreated = 0;

    UPROPERTY(BlueprintReadOnly)
    float TotalWaterUsed = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float TotalPlayTime = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 DeathCount = 0;
};

// 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameStateChanged, EGameState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChapterStarted, EChapterID, ChapterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChapterCompleted, EChapterID, ChapterID, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCRescued, FString, NPCID, FText, NPCName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCDied, FString, NPCID, FText, NPCName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFireEventTriggered, FString, EventID);

/**
 * 게임 전체를 총괄하는 매니저
 * - 챕터 관리
 * - 화재 시나리오 관리
 * - NPC 구조 시스템
 * - 통계 및 진행 상황
 */
UCLASS()
class AGameManager : public AActor
{
    GENERATED_BODY()

public:
    AGameManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // ============================ 싱글톤 접근 ============================

    UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject"))
    static AGameManager* GetGameManager(const UObject* WorldContextObject);

    // ============================ 게임 상태 ============================

    UPROPERTY(BlueprintReadOnly, Category = "Game|State")
    EGameState CurrentGameState = EGameState::MainMenu;

    UPROPERTY(BlueprintReadOnly, Category = "Game|State")
    EChapterID CurrentChapterID = EChapterID::None;

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnGameStateChanged OnGameStateChanged;

    UFUNCTION(BlueprintCallable, Category = "Game|State")
    void ChangeGameState(EGameState NewState);

    UFUNCTION(BlueprintPure, Category = "Game|State")
    bool IsInGameplay() const { return CurrentGameState == EGameState::Gameplay; }

    UFUNCTION(BlueprintPure, Category = "Game|State")
    bool IsPaused() const { return CurrentGameState == EGameState::Paused; }

    // ============================ 챕터 관리 ============================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game|Chapters")
    TMap<EChapterID, FChapterData> Chapters;

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnChapterStarted OnChapterStarted;

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnChapterCompleted OnChapterCompleted;

    UFUNCTION(BlueprintCallable, Category = "Game|Chapters")
    void LoadChapter(EChapterID ChapterID);

    UFUNCTION(BlueprintCallable, Category = "Game|Chapters")
    void StartCurrentChapter();

    UFUNCTION(BlueprintCallable, Category = "Game|Chapters")
    void CompleteChapter(bool bSuccess);

    UFUNCTION(BlueprintCallable, Category = "Game|Chapters")
    void RestartChapter();

    UFUNCTION(BlueprintPure, Category = "Game|Chapters")
    FChapterData GetCurrentChapterData() const;

    UFUNCTION(BlueprintPure, Category = "Game|Chapters")
    bool IsChapterUnlocked(EChapterID ChapterID) const;

    UFUNCTION(BlueprintCallable, Category = "Game|Chapters")
    void UnlockChapter(EChapterID ChapterID);

    // ============================ 미션 목표 관리 ============================

    UPROPERTY(BlueprintReadOnly, Category = "Game|Mission")
    TArray<UMissionObjective*> ActiveObjectives;

    UPROPERTY(BlueprintReadOnly, Category = "Game|Mission")
    int32 CurrentScore = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game|Mission|Scoring")
    float TimeBonus = 100.f; // 남은 시간 1초당 점수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game|Mission|Scoring")
    float OptionalObjectiveBonus = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game|Mission|Scoring")
    float PerfectClearBonus = 500.f;

    UFUNCTION(BlueprintCallable, Category = "Game|Mission")
    void AddObjective(UMissionObjective* Objective);

    UFUNCTION(BlueprintCallable, Category = "Game|Mission")
    void AddScore(int32 Points);

    UFUNCTION(BlueprintCallable, Category = "Game|Mission")
    void SubtractScore(int32 Points);

    UFUNCTION(BlueprintPure, Category = "Game|Mission")
    int32 CalculateFinalScore() const;

    UFUNCTION(BlueprintPure, Category = "Game|Mission")
    bool AreAllObjectivesComplete() const;

    UFUNCTION(BlueprintPure, Category = "Game|Mission")
    bool AreMandatoryObjectivesComplete() const;

    // ============================ 화재 시나리오 ============================

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnFireEventTriggered OnFireEventTriggered;

    UFUNCTION(BlueprintCallable, Category = "Game|Fire")
    void TriggerFireEvent(const FString& EventID);

    UFUNCTION(BlueprintCallable, Category = "Game|Fire")
    void TriggerFireAtRoom(ARoomActor* Room, ECombustibleType FireType, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Game|Fire")
    void TriggerFireAtActor(AActor* TargetActor);

    UFUNCTION(BlueprintPure, Category = "Game|Fire")
    int32 GetActiveFireCount() const;

    UFUNCTION(BlueprintPure, Category = "Game|Fire")
    TArray<class AFireActor*> GetAllActiveFires() const;

    // ============================ NPC 구조 시스템 ============================

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnNPCRescued OnNPCRescued;

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnNPCDied OnNPCDied;

    UFUNCTION(BlueprintCallable, Category = "Game|NPC")
    void RescueNPC(const FString& NPCID);

    UFUNCTION(BlueprintCallable, Category = "Game|NPC")
    void KillNPC(const FString& NPCID, const FString& Reason);

    UFUNCTION(BlueprintPure, Category = "Game|NPC")
    int32 GetRescuedNPCCount() const;

    UFUNCTION(BlueprintPure, Category = "Game|NPC")
    int32 GetTotalNPCCount() const;

    UFUNCTION(BlueprintPure, Category = "Game|NPC")
    TArray<FNPCRescueData> GetNPCsInDanger() const;

    // ============================ 통계 ============================

    UPROPERTY(BlueprintReadOnly, Category = "Game|Statistics")
    FGameStatistics CurrentSessionStats;

    UPROPERTY(BlueprintReadOnly, Category = "Game|Statistics")
    FGameStatistics TotalGameStats;

    UFUNCTION(BlueprintCallable, Category = "Game|Statistics")
    void ResetSessionStatistics();

    UFUNCTION(BlueprintPure, Category = "Game|Statistics")
    FGameStatistics GetCurrentSessionStats() const { return CurrentSessionStats; }

    UFUNCTION(BlueprintPure, Category = "Game|Statistics")
    FGameStatistics GetTotalGameStats() const { return TotalGameStats; }

    // ============================ 이벤트 리스닝 ============================

    UFUNCTION()
    void OnFireExtinguished(class AFireActor* Fire);

    UFUNCTION()
    void OnBackdraftTriggered();

    UFUNCTION()
    void OnGasTankBLEVE(FVector Location);

    UFUNCTION()
    void OnDoorBreached(class ADoorActor* Door);

    UFUNCTION()
    void OnVentHoleCreated(int32 HoleCount);

    UFUNCTION()
    void OnPlayerDeath();

    UFUNCTION()
    void OnWaterUsed(float Amount);

    // ============================ 저장/로드 ============================

    UFUNCTION(BlueprintCallable, Category = "Game|SaveLoad")
    void SaveGameProgress();

    UFUNCTION(BlueprintCallable, Category = "Game|SaveLoad")
    void LoadGameProgress();

    // ============================ 유틸리티 ============================

    UFUNCTION(BlueprintPure, Category = "Game|Utility")
    float GetChapterElapsedTime() const;

    UFUNCTION(BlueprintPure, Category = "Game|Utility")
    float GetChapterRemainingTime() const;

    UFUNCTION(BlueprintCallable, Category = "Game|Debug")
    void DebugTriggerAllFireEvents();

    UFUNCTION(BlueprintCallable, Category = "Game|Debug")
    void DebugPrintChapterInfo();

protected:
    // 내부 함수
    void UpdateFireScenario(float DeltaTime);
    void UpdateNPCDanger(float DeltaTime);
    void CheckChapterCompletion();
    void BindGameEvents();
    void UnbindGameEvents();
    void InitializeChapters();
    void SetupChapter1();
    void SetupChapter2();

    // 시간 추적
    float ChapterStartTime = 0.f;
    float ChapterElapsedTime = 0.f;

    // 타이머
    FTimerHandle IntroTimerHandle;
    FTimerHandle FireScenarioTimerHandle;
};