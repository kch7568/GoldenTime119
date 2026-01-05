// ============================ BreakableWallActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakableComponent.h"
#include "BreakableWallActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class GOLDENTIME119_API ABreakableWallActor : public AActor
{
    GENERATED_BODY()

public:
    ABreakableWallActor();

    // ===== 컴포넌트 =====

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> Root;

    // 벽 메시 (온전한 상태)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WallMesh;

    // 손상된 벽 메시 (Damaged 상태에서 표시)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> DamagedWallMesh;

    // 심하게 손상된 벽 메시 (HeavyDamaged 상태)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> HeavyDamagedWallMesh;

    // 파괴 가능 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBreakableComponent> Breakable;

    // 충돌 박스
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> BlockingCollision;

    // ===== 벽 속성 =====

    // 벽 크기 (에디터에서 조정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
    FVector WallExtent = FVector(10.f, 100.f, 150.f);

    // 뚫린 구멍 크기 (파괴 시)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
    FVector HoleSize = FVector(20.f, 80.f, 120.f);

    // ===== 잔해 =====

    UPROPERTY(EditAnywhere, Category = "Wall|Debris")
    TArray<TObjectPtr<UStaticMesh>> DebrisMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Debris")
    int32 DebrisCount = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Debris")
    float DebrisImpulseStrength = 300.f;

    // ===== 함수 =====

    UFUNCTION(BlueprintCallable, Category = "Wall")
    bool IsBroken() const;

    UFUNCTION(BlueprintCallable, Category = "Wall")
    float GetDamageRatio() const;

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void OnWallBroken();

    UFUNCTION()
    void OnWallStateChanged(EBreakableState NewState);

    void UpdateWallVisuals(EBreakableState State);
    void SpawnDebris();
    void CreateHole();
};