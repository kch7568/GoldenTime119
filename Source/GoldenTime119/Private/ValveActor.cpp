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

    if (bIsValveBeingTurned && CurrentGrabbingController)
    {
        // 1. 손의 회전 변화량 계산 (손목을 비트는 Roll 양을 사용할지, Yaw를 사용할지는 취향 차이입니다)
        FRotator CurrentHandRot = CurrentGrabbingController->GetComponentRotation();
        FRotator DeltaRot = CurrentHandRot - InitialHandRotation;

        // 2. 밸브의 초기 Yaw 각도에 손의 회전 변화량을 더함
        // 밸브를 옆으로 돌리기 위해 'Yaw'를 조절합니다.
        float NewRotation = FMath::Clamp(InitialValveRotation.Yaw + DeltaRot.Roll, 0.0f, TargetRotation);

        // 3. FRotator의 두 번째 인자인 Yaw에 NewRotation을 넣습니다.
        ValveMesh->SetRelativeRotation(FRotator(0, NewRotation, 0));

        // 4. 목표 각도 도달 체크
        if (NewRotation >= TargetRotation && !bIsTriggered)
        {
            if (TargetSprinkler)
            {
                TargetSprinkler->ActivateWater();
                bIsTriggered = true;
                bIsValveBeingTurned = false;
            }
        }
    }
}
void AValveActor::OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
    if (!bIsTriggered) // 이미 다 열린 게 아니라면
    {
        bIsValveBeingTurned = true;
        CurrentGrabbingController = GrabbingController;

        // 잡는 순간의 상태를 스냅샷처럼 저장하여 기준점으로 삼음
        InitialHandRotation = GrabbingController->GetComponentRotation();
        InitialValveRotation = ValveMesh->GetRelativeRotation();
    }
}
void AValveActor::OnReleased_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
    // 손을 떼면 더 이상 회전시키지 않도록 플래그를 꺼줍니다.
    if (CurrentGrabbingController == GrabbingController)
    {
        bIsValveBeingTurned = false;
        CurrentGrabbingController = nullptr;
        UE_LOG(LogTemp, Log, TEXT("[Valve] Valve Released by Left Hand!"));
    }
}