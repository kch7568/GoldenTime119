#include "ValveActor.h"
#include "Kismet/GameplayStatics.h"

AValveActor::AValveActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    PipeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PipeMesh"));
    PipeMesh->SetupAttachment(Root);

    ValveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ValveMesh"));
    ValveMesh->SetupAttachment(PipeMesh);
}

void AValveActor::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        EnableInput(PC);
        if (InputComponent)
        {
            InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AValveActor::OnFKeyPressed);
            InputComponent->BindKey(EKeys::F, IE_Released, this, &AValveActor::OnFKeyReleased);
        }
    }
}

void AValveActor::OnFKeyPressed() { bIsPressing = true; }
void AValveActor::OnFKeyReleased() { bIsPressing = false; }

void AValveActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsPressing && CurrentRotationSum < TargetRotation)
    {
        float RotationThisFrame = RotationSpeed * DeltaTime;
        ValveMesh->AddLocalRotation(FRotator(0, RotationThisFrame, 0));
        CurrentRotationSum += RotationThisFrame;

        if (CurrentRotationSum >= TargetRotation && !bIsTriggered)
        {
            if (TargetSprinkler)
            {
                TargetSprinkler->ActivateWater();
                bIsTriggered = true;
            }
        }
    }
}