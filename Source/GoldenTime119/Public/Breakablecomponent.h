// ============================ BreakableComponent.h ============================
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "BreakableComponent.generated.h"

// 파괴에 필요한 도구 타입
UENUM(BlueprintType)
enum class EBreakToolType : uint8
{
    Any         UMETA(DisplayName = "Any"),         // 아무 도구나
    Axe         UMETA(DisplayName = "Axe"),         // 도끼 필요
    Hammer      UMETA(DisplayName = "Hammer"),      // 해머 필요
    Saw         UMETA(DisplayName = "Saw"),         // 톱 필요
    Crowbar     UMETA(DisplayName = "Crowbar"),     // 빠루 필요
};

// 오브젝트 재질 (데미지 계산용)
UENUM(BlueprintType)
enum class EBreakableMaterial : uint8
{
    Wood        UMETA(DisplayName = "Wood"),
    Metal       UMETA(DisplayName = "Metal"),
    Glass       UMETA(DisplayName = "Glass"),
    Drywall     UMETA(DisplayName = "Drywall"),     // 석고보드
    Concrete    UMETA(DisplayName = "Concrete"),
};

// 파괴 상태
UENUM(BlueprintType)
enum class EBreakableState : uint8
{
    Intact,         // 멀쩡함
    Damaged,        // 손상됨 (균열)
    HeavyDamaged,   // 심하게 손상
    Broken          // 파괴됨
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBreakableStateChanged, EBreakableState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamageReceived, float, Damage, float, RemainingHP);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UBreakableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakableComponent();

    // ===== 기본 속성 =====

    // 최대 내구도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Properties")
    float MaxHP = 100.f;

    // 현재 내구도
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Breakable|Runtime")
    float CurrentHP = 100.f;

    // 재질
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Properties")
    EBreakableMaterial Material = EBreakableMaterial::Wood;

    // 필요 도구
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Properties")
    EBreakToolType RequiredTool = EBreakToolType::Axe;

    // 잘못된 도구 사용 시 데미지 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Properties")
    float WrongToolDamageMultiplier = 0.2f;

    // 현재 상태
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Breakable|Runtime")
    EBreakableState BreakState = EBreakableState::Intact;

    // ===== 상태 임계값 =====

    // Damaged 상태 전환 HP 비율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DamagedThreshold = 0.7f;

    // HeavyDamaged 상태 전환 HP 비율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HeavyDamagedThreshold = 0.3f;

    // ===== 파괴 설정 =====

    // 파괴 시 통과 가능 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Destruction")
    bool bAllowPassthroughWhenBroken = true;

    // 파괴 후 자동 제거 시간 (0이면 제거 안 함)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Destruction", meta = (ClampMin = "0.0"))
    float DestroyAfterBrokenDelay = 0.f;

    // ===== VFX/SFX =====

    UPROPERTY(EditAnywhere, Category = "Breakable|Audio")
    TObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditAnywhere, Category = "Breakable|Audio")
    TObjectPtr<USoundBase> CrackSound;

    UPROPERTY(EditAnywhere, Category = "Breakable|Audio")
    TObjectPtr<USoundBase> BreakSound;

    UPROPERTY(EditAnywhere, Category = "Breakable|VFX")
    TObjectPtr<UParticleSystem> HitParticle;

    UPROPERTY(EditAnywhere, Category = "Breakable|VFX")
    TObjectPtr<UParticleSystem> BreakParticle;

    // ===== One-shot / Pitch Randomize =====

    // 도끼(Axe)로 타격 시, HitSound를 one-shot으로 재생 + 랜덤 피치 적용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Audio|OneShot")
    bool bAxeHitOneShotRandomPitch = true;

    // 도끼 타격 시 피치 범위
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Audio|OneShot", meta = (EditCondition = "bAxeHitOneShotRandomPitch", ClampMin = "0.1"))
    float AxeHitPitchMin = 0.92f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Audio|OneShot", meta = (EditCondition = "bAxeHitOneShotRandomPitch", ClampMin = "0.1"))
    float AxeHitPitchMax = 1.08f;

    // 파괴 시 BreakSound를 one-shot으로 재생 + 랜덤 피치 적용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Audio|OneShot")
    bool bBreakOneShotRandomPitch = false;

    // 파괴 사운드 피치 범위
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Audio|OneShot", meta = (EditCondition = "bBreakOneShotRandomPitch", ClampMin = "0.1"))
    float BreakPitchMin = 0.95f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Audio|OneShot", meta = (EditCondition = "bBreakOneShotRandomPitch", ClampMin = "0.1"))
    float BreakPitchMax = 1.05f;

    // ===== 델리게이트 =====

    UPROPERTY(BlueprintAssignable, Category = "Breakable|Events")
    FOnBreakableStateChanged OnBreakableStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Breakable|Events")
    FOnBroken OnBroken;

    UPROPERTY(BlueprintAssignable, Category = "Breakable|Events")
    FOnDamageReceived OnDamageReceived;

    // ===== 함수 =====

    // 데미지 적용
    UFUNCTION(BlueprintCallable, Category = "Breakable")
    float ApplyDamage(float BaseDamage, EBreakToolType ToolUsed, FVector HitLocation, FVector HitNormal);

    // 즉시 파괴
    UFUNCTION(BlueprintCallable, Category = "Breakable")
    void ForceBreak();

    // HP 비율 반환
    UFUNCTION(BlueprintCallable, Category = "Breakable")
    float GetHPRatio() const;

    // 파괴 여부
    UFUNCTION(BlueprintCallable, Category = "Breakable")
    bool IsBroken() const;

    // 도구 적합성 확인
    UFUNCTION(BlueprintCallable, Category = "Breakable")
    bool IsCorrectTool(EBreakToolType ToolUsed) const;

    // 재질별 도구 데미지 배율
    UFUNCTION(BlueprintCallable, Category = "Breakable")
    float GetToolEffectiveness(EBreakToolType ToolUsed) const;

protected:
    virtual void BeginPlay() override;

private:
    void UpdateBreakState();
    void SetBreakState(EBreakableState NewState);
    void ExecuteBreak(FVector HitLocation);
    void PlayHitEffects(EBreakToolType ToolUsed, FVector Location, FVector Normal);
    void PlayBreakEffects(FVector Location);

    float GetRandomPitch(float MinPitch, float MaxPitch) const;

private:
    // 중복 파괴/중복 사운드 방지
    UPROPERTY(VisibleInstanceOnly, Category = "Breakable|Runtime")
    bool bHasBroken = false;
};
