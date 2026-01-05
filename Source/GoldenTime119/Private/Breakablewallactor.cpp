// ============================ BreakableWallActor.cpp ============================
#include "BreakableWallActor.h"
#include "BreakableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogBreakableWall, Log, All);

ABreakableWallActor::ABreakableWallActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // 온전한 벽
    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    WallMesh->SetupAttachment(Root);
    WallMesh->SetVisibility(true);

    // 손상된 벽 (균열 있음)
    DamagedWallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DamagedWallMesh"));
    DamagedWallMesh->SetupAttachment(Root);
    DamagedWallMesh->SetVisibility(false);

    // 심하게 손상된 벽
    HeavyDamagedWallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeavyDamagedWallMesh"));
    HeavyDamagedWallMesh->SetupAttachment(Root);
    HeavyDamagedWallMesh->SetVisibility(false);

    // 충돌 박스
    BlockingCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockingCollision"));
    BlockingCollision->SetupAttachment(Root);
    BlockingCollision->SetBoxExtent(FVector(10.f, 100.f, 150.f));
    BlockingCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BlockingCollision->SetCollisionResponseToAllChannels(ECR_Block);

    // Breakable 컴포넌트
    Breakable = CreateDefaultSubobject<UBreakableComponent>(TEXT("Breakable"));
    Breakable->MaxHP = 120.f;
    Breakable->Material = EBreakableMaterial::Drywall; // 석고보드
    Breakable->RequiredTool = EBreakToolType::Axe;
}

void ABreakableWallActor::BeginPlay()
{
    Super::BeginPlay();

    // 충돌 박스 크기 설정
    if (BlockingCollision)
    {
        BlockingCollision->SetBoxExtent(WallExtent);
    }

    // 이벤트 바인딩
    if (Breakable)
    {
        Breakable->OnBroken.AddDynamic(this, &ABreakableWallActor::OnWallBroken);
        Breakable->OnBreakableStateChanged.AddDynamic(this, &ABreakableWallActor::OnWallStateChanged);
    }

    UE_LOG(LogBreakableWall, Log, TEXT("[Wall] %s initialized - HP:%.1f Material:%d"),
        *GetName(), Breakable ? Breakable->MaxHP : 0.f, Breakable ? (int32)Breakable->Material : 0);
}

void ABreakableWallActor::OnWallStateChanged(EBreakableState NewState)
{
    UE_LOG(LogBreakableWall, Log, TEXT("[Wall] %s state changed to %d"), *GetName(), (int32)NewState);
    UpdateWallVisuals(NewState);
}

void ABreakableWallActor::UpdateWallVisuals(EBreakableState State)
{
    // 모든 메시 숨기기
    if (WallMesh) WallMesh->SetVisibility(false);
    if (DamagedWallMesh) DamagedWallMesh->SetVisibility(false);
    if (HeavyDamagedWallMesh) HeavyDamagedWallMesh->SetVisibility(false);

    // 상태에 맞는 메시 표시
    switch (State)
    {
    case EBreakableState::Intact:
        if (WallMesh) WallMesh->SetVisibility(true);
        break;

    case EBreakableState::Damaged:
        if (DamagedWallMesh && DamagedWallMesh->GetStaticMesh())
            DamagedWallMesh->SetVisibility(true);
        else if (WallMesh)
            WallMesh->SetVisibility(true); // 폴백
        break;

    case EBreakableState::HeavyDamaged:
        if (HeavyDamagedWallMesh && HeavyDamagedWallMesh->GetStaticMesh())
            HeavyDamagedWallMesh->SetVisibility(true);
        else if (DamagedWallMesh && DamagedWallMesh->GetStaticMesh())
            DamagedWallMesh->SetVisibility(true);
        else if (WallMesh)
            WallMesh->SetVisibility(true); // 폴백
        break;

    case EBreakableState::Broken:
        // 모두 숨김 유지 (구멍이 뚫림)
        break;
    }
}

void ABreakableWallActor::OnWallBroken()
{
    UE_LOG(LogBreakableWall, Error, TEXT("[Wall] ====== %s BROKEN ======"), *GetName());

    // 벽 메시 모두 숨기기
    UpdateWallVisuals(EBreakableState::Broken);

    // 구멍 만들기 (충돌 해제)
    CreateHole();

    // 잔해 스폰
    SpawnDebris();
}

void ABreakableWallActor::CreateHole()
{
    // 충돌 해제하여 통과 가능하게
    if (BlockingCollision)
    {
        BlockingCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    UE_LOG(LogBreakableWall, Log, TEXT("[Wall] Hole created - size: %s"), *HoleSize.ToString());
}

void ABreakableWallActor::SpawnDebris()
{
    if (DebrisMeshes.Num() == 0)
    {
        UE_LOG(LogBreakableWall, Log, TEXT("[Wall] No debris meshes assigned"));
        return;
    }

    const FVector WallLocation = GetActorLocation();

    for (int32 i = 0; i < DebrisCount; i++)
    {
        UStaticMesh* DebrisMesh = DebrisMeshes[FMath::RandRange(0, DebrisMeshes.Num() - 1)];
        if (!DebrisMesh)
            continue;

        // 구멍 영역 내 랜덤 위치 - float 명시
        const float OffsetX = FMath::RandRange((float)(-HoleSize.X), (float)(HoleSize.X));
        const float OffsetY = FMath::RandRange((float)(-HoleSize.Y * 0.5), (float)(HoleSize.Y * 0.5));
        const float OffsetZ = FMath::RandRange(0.f, (float)(HoleSize.Z));
        const FVector SpawnOffset(OffsetX, OffsetY, OffsetZ);
        const FVector SpawnLocation = WallLocation + SpawnOffset;

        const FRotator SpawnRotation = FRotator(
            FMath::RandRange(-180.f, 180.f),
            FMath::RandRange(-180.f, 180.f),
            FMath::RandRange(-180.f, 180.f)
        );

        UStaticMeshComponent* DebrisComp = NewObject<UStaticMeshComponent>(this);
        if (DebrisComp)
        {
            DebrisComp->SetStaticMesh(DebrisMesh);
            DebrisComp->SetWorldLocation(SpawnLocation);
            DebrisComp->SetWorldRotation(SpawnRotation);
            DebrisComp->SetWorldScale3D(FVector(FMath::RandRange(0.3f, 0.8f)));
            DebrisComp->SetSimulatePhysics(true);
            DebrisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            DebrisComp->RegisterComponent();

            // 양쪽으로 튕기는 임펄스
            const float Side = (i % 2 == 0) ? 1.f : -1.f;
            const FVector ImpulseDir = FVector(
                Side,
                FMath::RandRange(-0.3f, 0.3f),
                FMath::RandRange(0.f, 0.5f)
            ).GetSafeNormal();
            DebrisComp->AddImpulse(ImpulseDir * DebrisImpulseStrength, NAME_None, true);
        }
    }

    UE_LOG(LogBreakableWall, Log, TEXT("[Wall] Spawned %d debris pieces"), DebrisCount);
}

bool ABreakableWallActor::IsBroken() const
{
    return Breakable && Breakable->IsBroken();
}

float ABreakableWallActor::GetDamageRatio() const
{
    if (!Breakable)
        return 0.f;

    return 1.f - Breakable->GetHPRatio();
}