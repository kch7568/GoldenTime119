#include "SprinklerActor.h"
#include "CombustibleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSprinkler, Log, All);

ASprinklerActor::ASprinklerActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    SprinklerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SprinklerMesh"));
    SprinklerMesh->SetupAttachment(Root);

    WaterVFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WaterVFX"));
    WaterVFX->SetupAttachment(SprinklerMesh);
    WaterVFX->bAutoActivate = false;

    SprinklerAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SprinklerAudio"));
    SprinklerAudio->SetupAttachment(Root);
    SprinklerAudio->bAutoActivate = false;
}

void ASprinklerActor::ActivateWater()
{
    if (WaterVFX)
    {
        WaterVFX->Activate();
        bIsSprinkling = true;
        UE_LOG(LogSprinkler, Warning, TEXT("[Sprinkler] Water Activated: %s"), *GetName());
    }

    if (SprinklerAudio && WaterSound)
    {
        SprinklerAudio->SetSound(WaterSound);
        SprinklerAudio->Play();
        UE_LOG(LogSprinkler, Warning, TEXT("[Sprinkler] Sound Started!"));
    }
}

void ASprinklerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsSprinkling)
    {
        TimerAcc += DeltaTime;
        if (TimerAcc >= CheckInterval)
        {
            TimerAcc = 0.0f;
            CheckAndApplyWater();
        }
    }
}

void ASprinklerActor::CheckAndApplyWater()
{
    FVector StartLocation = GetActorLocation();
    FVector EndLocation = StartLocation + (FVector::UpVector * -1.0f * TraceDistance);

    // 1) 물이 닿는 범위 내의 모든 액터 찾기
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<AActor*> OutActors;

    bool bHit = UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        EndLocation,
        ExtinguishRadius,
        ObjectTypes,
        AActor::StaticClass(),
        ActorsToIgnore,
        OutActors
    );

    if (bHit)
    {
        int32 AppliedCount = 0;

        for (AActor* Actor : OutActors)
        {
            if (!IsValid(Actor)) continue;

            // 가연물 컴포넌트를 찾아서 물 입력
            UCombustibleComponent* Comb = Actor->FindComponentByClass<UCombustibleComponent>();
            if (IsValid(Comb))
            {
                // 거리 기반 강도 조절
                FVector CombLoc = Actor->GetActorLocation();
                float Dist = FVector::Dist(CombLoc, EndLocation);
                float DistAlpha = FMath::Clamp(1.f - (Dist / ExtinguishRadius), 0.f, 1.f);

                // 물 입력 + 수증기 사운드 트리거
                float WaterAmount = WaterIntensity * DistAlpha * CheckInterval;

                // 디버그 로그 추가
                UE_LOG(LogSprinkler, Warning, TEXT("[Sprinkler] Calling AddWaterContact on %s (Amount=%.3f, Trigger=%d)"),
                    *Actor->GetName(), WaterAmount, bTriggerSteamSound);

                Comb->AddWaterContact(WaterAmount, bTriggerSteamSound);

                AppliedCount++;

                UE_LOG(LogSprinkler, VeryVerbose,
                    TEXT("[Sprinkler] Water Applied to %s (Dist=%.1f, Amount=%.3f, Burning=%d)"),
                    *Actor->GetName(), Dist, WaterAmount, Comb->IsBurning());
            }
        }

        if (AppliedCount > 0)
        {
            UE_LOG(LogSprinkler, Log,
                TEXT("[Sprinkler] Water applied to %d combustible(s) in range"), AppliedCount);
        }
    }

    // 디버그 시각화
    DrawDebugSphere(GetWorld(), EndLocation, ExtinguishRadius, 12, FColor::Blue, false, CheckInterval, 0, 1.0f);
}