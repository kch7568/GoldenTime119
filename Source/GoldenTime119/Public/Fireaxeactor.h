// ============================ FireAxeActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakableComponent.h"
#include "FireAxeActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAxeHit, AActor*, HitActor, FVector, HitLocation, float, DamageDealt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxeSwing, float, SwingSpeed);

UCLASS()
class GOLDENTIME119_API AFireAxeActor : public AActor
{
    GENERATED_BODY()

public:
    AFireAxeActor();

    // ===== 컴포넌트 =====

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> AxeMesh;

    // 도끼날 충돌 감지용
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> BladeCollision;

    // 손잡이 그랩 위치
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> GripPoint;

    // ===== 도끼 속성 =====

    // 도구 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Properties")
    EBreakToolType ToolType = EBreakToolType::Axe;

    // 기본 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Properties")
    float BaseDamage = 25.f;

    // 최소 스윙 속도 (cm/s) - 이 이상이어야 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Properties")
    float MinSwingSpeed = 200.f;

    // 최대 스윙 속도 (데미지 최대치)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Properties")
    float MaxSwingSpeed = 800.f;

    // 스윙 속도에 따른 데미지 배율 (최대)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Properties")
    float MaxSpeedDamageMultiplier = 2.5f;

    // 연속 타격 쿨다운
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Properties")
    float HitCooldown = 0.3f;

    // ===== VR 설정 =====

    // VR 컨트롤러 속도 추적 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|VR")
    bool bTrackControllerVelocity = true;

    // 속도 샘플링 개수 (평균 계산용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|VR")
    int32 VelocitySampleCount = 5;

    // ===== 햅틱 =====

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Haptic")
    float HitHapticIntensity = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axe|Haptic")
    float HitHapticDuration = 0.15f;

    // ===== VFX/SFX =====

    UPROPERTY(EditAnywhere, Category = "Axe|Audio")
    TObjectPtr<USoundBase> SwingSound;

    UPROPERTY(EditAnywhere, Category = "Axe|Audio")
    TObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditAnywhere, Category = "Axe|Audio")
    TObjectPtr<USoundBase> HitMetalSound;

    UPROPERTY(EditAnywhere, Category = "Axe|Audio")
    TObjectPtr<USoundBase> HitWoodSound;

    // ===== 델리게이트 =====

    UPROPERTY(BlueprintAssignable, Category = "Axe|Events")
    FOnAxeHit OnAxeHit;

    UPROPERTY(BlueprintAssignable, Category = "Axe|Events")
    FOnAxeSwing OnAxeSwing;

    // ===== 함수 =====

    // 현재 스윙 속도 반환
    UFUNCTION(BlueprintCallable, Category = "Axe")
    float GetCurrentSwingSpeed() const;

    // 스윙 강도 (0~1)
    UFUNCTION(BlueprintCallable, Category = "Axe")
    float GetSwingIntensity() const;

    // VR 컨트롤러 속도 설정 (VR Pawn에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Axe|VR")
    void SetControllerVelocity(FVector Velocity);

    // 수동 스윙 (비VR 테스트용)
    UFUNCTION(BlueprintCallable, Category = "Axe|Debug")
    void SimulateSwing(float Speed = 500.f);

    // 햅틱 재생 요청 (블루프린트에서 구현)
    UFUNCTION(BlueprintImplementableEvent, Category = "Axe|VR")
    void PlayHitHaptic(float Intensity, float Duration);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // 충돌 처리
    UFUNCTION()
    void OnBladeOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 데미지 계산 및 적용
    float CalculateAndApplyDamage(AActor* HitActor, const FHitResult& HitResult);

    // 히트 이펙트 재생
    void PlayHitEffects(const FHitResult& HitResult, EBreakableMaterial Material);

    // 속도 추적
    void UpdateVelocityTracking();

    // 속도 샘플
    TArray<FVector> VelocitySamples;
    FVector CurrentVelocity = FVector::ZeroVector;
    FVector LastPosition = FVector::ZeroVector;

    // 쿨다운
    float LastHitTime = -1.f;

    // 최근 히트한 액터 (중복 방지)
    UPROPERTY()
    TWeakObjectPtr<AActor> LastHitActor;
    float LastHitActorTime = -1.f;
};