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
        // 1. 현재 손의 로컬 위치 계산
        FVector CurrentLocalPos = ValveMesh->GetComponentTransform().InverseTransformPosition(CurrentGrabbingController->GetComponentLocation());

        if (bIsFirstTick) {
            PreviousLocalHandPos = CurrentLocalPos;
            bIsFirstTick = false;
            return;
        }

        // 2. 이전 위치와의 차이(Y축)를 구해 절대값으로 더함 (좌우 어디로 흔들든 누적됨)
        float MoveDistance = FMath::Abs(CurrentLocalPos.Y - PreviousLocalHandPos.Y);
        CurrentRotationSum += (MoveDistance * Sensitivity);

        // 3. 180도(TargetRotation)까지만 제한하여 적용
        CurrentRotationSum = FMath::Min(CurrentRotationSum, TargetRotation);
        ValveMesh->SetRelativeRotation(FRotator(0, CurrentRotationSum, 0));

        // 4. 다음 프레임 계산을 위해 현재 위치를 이전 위치로 저장
        PreviousLocalHandPos = CurrentLocalPos;

        // 5. 목표 도달 시 스프링클러 발동
        if (CurrentRotationSum >= TargetRotation && !bIsTriggered) {
            if (TargetSprinkler) {
                TargetSprinkler->ActivateWater();
                bIsTriggered = true;
                bIsValveBeingTurned = false; // 완료 시 더 이상 계산 안 함
            }
        }
    }
}
void AValveActor::OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
    if (!bIsTriggered)
    {
        bIsValveBeingTurned = true;
        CurrentGrabbingController = GrabbingController;

        // 잡는 순간을 기준으로 삼기 위해 플래그 설정
        bIsFirstTick = true;
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