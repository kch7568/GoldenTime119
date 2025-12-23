#include "SprinklerActor.h"

ASprinklerActor::ASprinklerActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    SprinklerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SprinklerMesh"));
    SprinklerMesh->SetupAttachment(Root);

    WaterVFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WaterVFX"));
    WaterVFX->SetupAttachment(SprinklerMesh);
    WaterVFX->bAutoActivate = false;
}

void ASprinklerActor::ActivateWater()
{
    if (WaterVFX) WaterVFX->Activate();
}