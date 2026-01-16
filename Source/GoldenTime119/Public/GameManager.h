// ============================ GameManager.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionObjective.h"
#include "GameManager.generated.h"

// 전방 선언
class ARoomActor;
class AFireActor;
class APawn;
class AActor;
class UVitalComponent;
class UCombustibleComponent;

// 게임 상태
UENUM(BlueprintType)
enum class EGameState : uint8
{
    NotStarted      UMETA(DisplayName = "시작 전"),
    Starting        UMETA(DisplayName = "시작 중"),
    InProgress      UMETA(DisplayName = "진행 중"),
    Paused          UMETA(DisplayName = "일시정지"),
    MissionComplete UMETA(DisplayName = "미션 완료"),
    MissionFailed   UMETA(DisplayName = "미션 실패"),
    Ended           UMETA(DisplayName = "종료")
};

// 챕터 정의
UENUM(BlueprintType)
enum class EChapter : uint8
{
    Chapter1        UMETA(DisplayName = "챕터 1 - 기본 화재 진압"),
    Chapter2        UMETA(DisplayName = "챕터 2 - TBD"),
    Chapter3        UMETA(DisplayName = "챕터 3 - TBD"),
    Custom          UMETA(DisplayName = "커스텀")
};

// 게임 상태 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameStateChanged,
    EGameState, OldState, EGameState, NewState);

// 미션 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionStarted, EChapter, Chapter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMissionCompleted, EChapter, Chapter, int32, TotalScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMissionFailed, EChapter, Chapter, FString, Reason);

// 목표 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveCompleted, UMissionObjective*, Objective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveFailed, UMissionObjective*, Objective);

// 플레이어 바이탈 경고 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVitalWarning, FString, VitalType, float, Value01);

/**
 * VR 소방안전 게임 매니저
 * - 미션 관리
 * - 화재 시스템 제어
 * - 게임 상태 관리
 * - 이벤트 중계
 */
UCLASS(Blueprintable, BlueprintType)
class AGameManager : public AActor
{
    GENERATED_BODY()

public:
    AGameManager();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // ============================ 게임 상태 ============================

    UPROPERTY(BlueprintReadOnly, Category = "GameManager|State")
    EGameState CurrentGameState = EGameState::NotStarted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|State")
    EChapter CurrentChapter = EChapter::Chapter1;

    UPROPERTY(BlueprintReadOnly, Category = "GameManager|State")
    float GameStartTime = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "GameManager|State")
    float GameEndTime = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "GameManager|State")
    int32 TotalScore = 0;

    // ============================ 미션 목표 ============================

    // 현재 활성화된 목표들
    UPROPERTY(BlueprintReadOnly, Category = "GameManager|Mission")
    TArray<UMissionObjective*> ActiveObjectives;

    // 완료된 목표들
    UPROPERTY(BlueprintReadOnly, Category = "GameManager|Mission")
    TArray<UMissionObjective*> CompletedObjectives;

    // 실패한 목표들
    UPROPERTY(BlueprintReadOnly, Category = "GameManager|Mission")
    TArray<UMissionObjective*> FailedObjectives;

    // 현재 진행 중인 목표 인덱스 (순차 진행용)
    UPROPERTY(BlueprintReadOnly, Category = "GameManager|Mission")
    int32 CurrentObjectiveIndex = 0;

    // 순차 진행 모드 (true: 목표를 순서대로 진행, false: 모든 목표 동시 진행)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Mission")
    bool bSequentialObjectives = true;

    // ============================ 화재 시스템 설정 ============================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Fire")
    int32 InitialFireCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Fire")
    bool bAutoStartFire = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Fire")
    float FireStartDelay = 3.f;

    // 화재가 발생할 수 있는 방 목록 (비어있으면 모든 방)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Fire")
    TArray<ARoomActor*> PotentialFireRooms;

    // ============================ 플레이어 ============================

    UPROPERTY(BlueprintReadOnly, Category = "GameManager|Player")
    APawn* PlayerCharacter;

    // 바이탈 경고 임계값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Player")
    float VitalWarningThreshold = 0.3f; // 30% 이하 시 경고

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Player")
    float VitalCriticalThreshold = 0.15f; // 15% 이하 시 위험

    // ============================ NPC (요구조자) ============================

    // 침실에 있는 요구조자들 (나중에 구체적인 NPC 클래스로 교체)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|NPC")
    TArray<AActor*> NPCsToRescue;

    // 구조된 NPC들
    UPROPERTY(BlueprintReadOnly, Category = "GameManager|NPC")
    TArray<AActor*> RescuedNPCs;

    // ============================ 탈출 지점 ============================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameManager|Escape")
    AActor* ExitPoint;

    // ============================ 이벤트 ============================

    UPROPERTY(BlueprintAssignable, Category = "GameManager|Events")
    FOnGameStateChanged OnGameStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "GameManager|Events")
    FOnMissionStarted OnMissionStarted;

    UPROPERTY(BlueprintAssignable, Category = "GameManager|Events")
    FOnMissionCompleted OnMissionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "GameManager|Events")
    FOnMissionFailed OnMissionFailed;

    UPROPERTY(BlueprintAssignable, Category = "GameManager|Events")
    FOnObjectiveCompleted OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "GameManager|Events")
    FOnObjectiveFailed OnObjectiveFailed;

    UPROPERTY(BlueprintAssignable, Category = "GameManager|Events")
    FOnVitalWarning OnVitalWarning;

    // ============================ 게임 제어 메서드 ============================

    UFUNCTION(BlueprintCallable, Category = "GameManager")
    void StartGame();

    UFUNCTION(BlueprintCallable, Category = "GameManager")
    void PauseGame();

    UFUNCTION(BlueprintCallable, Category = "GameManager")
    void ResumeGame();

    UFUNCTION(BlueprintCallable, Category = "GameManager")
    void RestartGame();

    UFUNCTION(BlueprintCallable, Category = "GameManager")
    void EndGame(bool bSuccess, const FString& Reason = TEXT(""));

    // ============================ 미션 관리 메서드 ============================

    UFUNCTION(BlueprintCallable, Category = "GameManager|Mission")
    void SetupChapter(EChapter Chapter);

    UFUNCTION(BlueprintCallable, Category = "GameManager|Mission")
    void AddObjective(UMissionObjective* Objective);

    UFUNCTION(BlueprintCallable, Category = "GameManager|Mission")
    void StartNextObjective();

    UFUNCTION(BlueprintCallable, Category = "GameManager|Mission")
    void CompleteCurrentObjective();

    UFUNCTION(BlueprintPure, Category = "GameManager|Mission")
    UMissionObjective* GetCurrentObjective() const;

    UFUNCTION(BlueprintPure, Category = "GameManager|Mission")
    int32 GetTotalObjectiveCount() const { return ActiveObjectives.Num(); }

    UFUNCTION(BlueprintPure, Category = "GameManager|Mission")
    int32 GetCompletedObjectiveCount() const { return CompletedObjectives.Num(); }

    UFUNCTION(BlueprintPure, Category = "GameManager|Mission")
    float GetMissionProgress01() const;

    // ============================ 화재 시스템 메서드 ============================

    UFUNCTION(BlueprintCallable, Category = "GameManager|Fire")
    void StartInitialFire();

    UFUNCTION(BlueprintCallable, Category = "GameManager|Fire")
    void CreateFireAtRandomLocation();

    UFUNCTION(BlueprintCallable, Category = "GameManager|Fire")
    void CreateFireInRoom(ARoomActor* Room);

    UFUNCTION(BlueprintPure, Category = "GameManager|Fire")
    int32 GetTotalActiveFireCount() const;

    UFUNCTION(BlueprintPure, Category = "GameManager|Fire")
    TArray<ARoomActor*> GetAllRooms() const;

    // ============================ NPC 관리 메서드 ============================

    UFUNCTION(BlueprintCallable, Category = "GameManager|NPC")
    void RegisterNPC(AActor* NPC);

    UFUNCTION(BlueprintCallable, Category = "GameManager|NPC")
    void RescueNPC(AActor* NPC);

    UFUNCTION(BlueprintPure, Category = "GameManager|NPC")
    int32 GetRescuedNPCCount() const { return RescuedNPCs.Num(); }

    UFUNCTION(BlueprintPure, Category = "GameManager|NPC")
    int32 GetTotalNPCCount() const { return NPCsToRescue.Num(); }

    // ============================ 플레이어 바이탈 체크 ============================

    UFUNCTION(BlueprintPure, Category = "GameManager|Player")
    bool IsPlayerAlive() const;

    UFUNCTION(BlueprintPure, Category = "GameManager|Player")
    float GetPlayerHealth01() const;

    UFUNCTION(BlueprintPure, Category = "GameManager|Player")
    float GetPlayerOxygen01() const;

    UFUNCTION(BlueprintPure, Category = "GameManager|Player")
    float GetPlayerTemperature() const;

    // ============================ 유틸리티 ============================

    UFUNCTION(BlueprintPure, Category = "GameManager")
    float GetElapsedGameTime() const;

    UFUNCTION(BlueprintPure, Category = "GameManager")
    FString GetGameStateString() const;

    UFUNCTION(BlueprintPure, Category = "GameManager")
    FString GetChapterString() const;

protected:
    // ============================ 내부 메서드 ============================

    void ChangeGameState(EGameState NewState);

    // 챕터별 미션 설정
    void SetupChapter1();
    void SetupChapter2(); // 확장용
    void SetupChapter3(); // 확장용

    // 이벤트 바인딩
    void BindFireActorEvents(AFireActor* Fire);
    void BindRoomActorEvents(ARoomActor* Room);
    void BindPlayerEvents();

    // 목표 이벤트 핸들러
    UFUNCTION()
    void OnObjectiveProgressChanged(UMissionObjective* Objective, float Progress01, FString ProgressText);

    UFUNCTION()
    void OnObjectiveStatusChanged(UMissionObjective* Objective, EMissionObjectiveStatus NewStatus);

    // 화재 이벤트 핸들러
    UFUNCTION()
    void OnFireExtinguished(AFireActor* Fire);

    UFUNCTION()
    void OnFireSpawned(AFireActor* Fire);

    // 백드래프트 이벤트 핸들러
    UFUNCTION()
    void OnBackdraftOccurred();

    // 플레이어 바이탈 체크
    void CheckPlayerVitals(float DeltaTime);

    // 미션 완료/실패 체크
    void CheckMissionCompletion();

    // 모든 목표 업데이트
    void UpdateAllObjectives(float DeltaTime);

    // 플레이어 찾기
    void FindPlayerCharacter();

    // 씬에서 액터들 찾기
    void FindSceneActors();

private:
    // 바이탈 경고 쿨다운
    float VitalWarningCooldown = 0.f;
    const float VitalWarningInterval = 2.f;

    // 초기화 완료 플래그
    bool bIsInitialized = false;

    // 게임 시작 타이머
    FTimerHandle FireStartTimerHandle;
};