#include "SprinklerActor.h"
#include "FireActor.h" 
#include "Kismet/KismetSystemLibrary.h" 
#include "DrawDebugHelpers.h"
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

    // 오디오 컴포넌트 초기화
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
    }

    // 소리 재생 로직 추가
    if (SprinklerAudio && WaterSound)
    {
        SprinklerAudio->SetSound(WaterSound);
        SprinklerAudio->Play();
        UE_LOG(LogTemp, Warning, TEXT("[Sprinkler] Sound Started!"));
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
            CheckAndExtinguishFire();
        }
    }
}

void ASprinklerActor::CheckAndExtinguishFire()
{
    // 1. 시작 지점 및 끝 지점 설정 
    FVector StartLocation = GetActorLocation();
 
    FVector EndLocation = StartLocation + (FVector::UpVector * -1.0f * TraceDistance);

    // 2. 오브젝트 타입 설정
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<AActor*> OutActors;

    // 3. 구체 오버랩 검사
    bool bHit = UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        EndLocation,
        ExtinguishRadius,
        ObjectTypes,
        AFireActor::StaticClass(),
        ActorsToIgnore,
        OutActors
    );

    if (bHit)
    {
        for (AActor* Actor : OutActors)
        {
            AFireActor* Fire = Cast<AFireActor>(Actor);
            if (Fire)
            {
                Fire->Extinguish(); 
                UE_LOG(LogTemp, Warning, TEXT("[Sprinkler] Fire Extinguished: %s"), *Fire->GetName());
            }
        }
    }
    DrawDebugSphere(GetWorld(), EndLocation, ExtinguishRadius, 12, FColor::Blue, false, CheckInterval, 0, 1.0f);
 }