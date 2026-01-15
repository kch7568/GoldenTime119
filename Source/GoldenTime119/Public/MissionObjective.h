// ============================ MissionObjective.h ============================
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RoomActor.h"
#include "MissionObjective.generated.h"

// 목표 타입
UENUM(BlueprintType)
enum class EMissionObjectiveType : uint8
{
    // 화재 진압 관련
    ExtinguishAllFires          UMETA(DisplayName = "모든 화재 진압"),
    ExtinguishFiresInRoom       UMETA(DisplayName = "특정 방 화재 진압"),
    ExtinguishFireCount         UMETA(DisplayName = "N개 화재 진압"),

    // 방 상태 관련
    ClearRoomSmoke              UMETA(DisplayName = "방 연기 제거"),
    StabilizeRoomEnvironment    UMETA(DisplayName = "방 환경 안정화"),
    PreventBackdraft            UMETA(DisplayName = "백드래프트 방지"),

    // 생존 관련
    SurviveForDuration          UMETA(DisplayName = "시간 생존"),
    KeepHealthAbove             UMETA(DisplayName = "체력 유지"),
    KeepOxygenAbove             UMETA(DisplayName = "산소 수치 유지"),

    // 특수 목표
    PreventGasTankExplosion     UMETA(DisplayName = "가스탱크 폭발 방지"),
    OpenVentHolesInDoor         UMETA(DisplayName = "문에 환기구 만들기"),
    BreachDoor                  UMETA(DisplayName = "문 파괴"),
    RescueNPC                   UMETA(DisplayName = "NPC 구조"),

    // 제한 시간
    CompleteBeforeTime          UMETA(DisplayName = "제한 시간 내 완료"),

    Custom                      UMETA(DisplayName = "커스텀")
};

// 목표 상태
UENUM(BlueprintType)
enum class EMissionObjectiveStatus : uint8
{
    NotStarted      UMETA(DisplayName = "시작 전"),
    InProgress      UMETA(DisplayName = "진행 중"),
    Completed       UMETA(DisplayName = "완료"),
    Failed          UMETA(DisplayName = "실패"),
    Optional        UMETA(DisplayName = "선택 목표")
};

// 목표 진행 상황 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnObjectiveProgressChanged,
    class UMissionObjective*, Objective, float, Progress01, FString, ProgressText);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveStatusChanged,
    class UMissionObjective*, Objective, EMissionObjectiveStatus, NewStatus);

/**
 * 미션 목표 클래스
 */
UCLASS(Blueprintable, BlueprintType)
class UMissionObjective : public UObject
{
    GENERATED_BODY()

public:
    UMissionObjective();

    // ============================ 기본 정보 ============================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Basic")
    FString ObjectiveID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Basic")
    EMissionObjectiveType ObjectiveType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Basic")
    FText ObjectiveTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Basic")
    FText ObjectiveDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Basic")
    bool bIsOptional = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Basic")
    int32 ScoreReward = 100;

    // ============================ 상태 ============================

    UPROPERTY(BlueprintReadOnly, Category = "Objective|Status")
    EMissionObjectiveStatus Status = EMissionObjectiveStatus::NotStarted;

    UPROPERTY(BlueprintReadOnly, Category = "Objective|Status")
    float Progress01 = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Objective|Status")
    float StartTime = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Objective|Status")
    float CompletionTime = 0.f;

    // ============================ 타겟 설정 ============================

    // 특정 방 지정 (ExtinguishFiresInRoom, ClearRoomSmoke 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Target")
    TArray<ARoomActor*> TargetRooms;

    // 목표 개수 (ExtinguishFireCount 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Target")
    int32 TargetCount = 1;

    // 현재 달성 개수
    UPROPERTY(BlueprintReadOnly, Category = "Objective|Target")
    int32 CurrentCount = 0;

    // 임계값 (KeepHealthAbove, KeepOxygenAbove, ClearRoomSmoke 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Target")
    float ThresholdValue = 0.5f;

    // 지속 시간 (SurviveForDuration, CompleteBeforeTime 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Target")
    float DurationSeconds = 60.f;

    // 특정 액터 타겟 (가스탱크, 문, NPC 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Target")
    TArray<AActor*> TargetActors;

    // ============================ 조건 검사 ============================

    // 실패 조건 활성화 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Conditions")
    bool bCanFail = false;

    // 실패 조건: 시간 초과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Conditions")
    bool bFailOnTimeout = false;

    // 실패 조건: 플레이어 사망
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Conditions")
    bool bFailOnPlayerDeath = false;

    // 실패 조건: 백드래프트 발생
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Conditions")
    bool bFailOnBackdraft = false;

    // 실패 조건: 가스탱크 폭발
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective|Conditions")
    bool bFailOnGasTankExplosion = false;

    // ============================ 이벤트 ============================

    UPROPERTY(BlueprintAssignable, Category = "Objective|Events")
    FOnObjectiveProgressChanged OnProgressChanged;

    UPROPERTY(BlueprintAssignable, Category = "Objective|Events")
    FOnObjectiveStatusChanged OnStatusChanged;

    // ============================ 메서드 ============================

    UFUNCTION(BlueprintCallable, Category = "Objective")
    void StartObjective(UWorld* World);

    UFUNCTION(BlueprintCallable, Category = "Objective")
    void UpdateProgress(float DeltaSeconds, UWorld* World);

    UFUNCTION(BlueprintCallable, Category = "Objective")
    void CompleteObjective();

    UFUNCTION(BlueprintCallable, Category = "Objective")
    void FailObjective(const FString& Reason);

    UFUNCTION(BlueprintCallable, Category = "Objective")
    void ResetObjective();

    UFUNCTION(BlueprintPure, Category = "Objective")
    FString GetProgressText() const;

    UFUNCTION(BlueprintPure, Category = "Objective")
    bool IsCompleted() const { return Status == EMissionObjectiveStatus::Completed; }

    UFUNCTION(BlueprintPure, Category = "Objective")
    bool IsFailed() const { return Status == EMissionObjectiveStatus::Failed; }

    UFUNCTION(BlueprintPure, Category = "Objective")
    bool IsInProgress() const { return Status == EMissionObjectiveStatus::InProgress; }

    UFUNCTION(BlueprintPure, Category = "Objective")
    float GetElapsedTime(UWorld* World) const;

    UFUNCTION(BlueprintPure, Category = "Objective")
    float GetRemainingTime(UWorld* World) const;

protected:
    // 타입별 진행도 체크 함수들
    void CheckExtinguishAllFires(UWorld* World);
    void CheckExtinguishFiresInRoom(UWorld* World);
    void CheckExtinguishFireCount(UWorld* World);
    void CheckClearRoomSmoke(UWorld* World);
    void CheckStabilizeRoomEnvironment(UWorld* World);
    void CheckPreventBackdraft(UWorld* World);
    void CheckSurviveForDuration(UWorld* World);
    void CheckKeepHealthAbove(UWorld* World);
    void CheckKeepOxygenAbove(UWorld* World);
    void CheckPreventGasTankExplosion(UWorld* World);
    void CheckOpenVentHolesInDoor(UWorld* World);
    void CheckBreachDoor(UWorld* World);
    void CheckCompleteBeforeTime(UWorld* World);

    // 헬퍼 함수
    void UpdateProgressValue(float NewProgress, const FString& ProgressText);
    void ChangeStatus(EMissionObjectiveStatus NewStatus);

    // 모든 Room 찾기
    void FindAllRooms(UWorld* World, TArray<ARoomActor*>& OutRooms);

    // 진행 상황 텍스트 생성
    FString FormatProgressText(int32 Current, int32 Target) const;
    FString FormatPercentText(float Percent01) const;
    FString FormatTimeText(float Seconds) const;
};