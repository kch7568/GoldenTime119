// VRPawn.cpp
#include "VRPawn.h"
#include "GrabInteractable.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRPawn, Log, All);

AVRPawn::AVRPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // VR Root
    VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
    SetRootComponent(VRRoot);

    // VR Camera (HMD)
    VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
    VRCamera->SetupAttachment(VRRoot);

    // Left Motion Controller
    LeftMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftMotionController"));
    LeftMotionController->SetupAttachment(VRRoot);
    LeftMotionController->SetTrackingMotionSource(FName("Left"));

    // Right Motion Controller
    RightMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightMotionController"));
    RightMotionController->SetupAttachment(VRRoot);
    RightMotionController->SetTrackingMotionSource(FName("Right"));

    // Left Grab Sphere
    LeftGrabSphere = CreateDefaultSubobject<USphereComponent>(TEXT("LeftGrabSphere"));
    LeftGrabSphere->SetupAttachment(LeftMotionController);
    LeftGrabSphere->SetSphereRadius(GrabRadius);
    LeftGrabSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    LeftGrabSphere->SetGenerateOverlapEvents(true);

    // Right Grab Sphere
    RightGrabSphere = CreateDefaultSubobject<USphereComponent>(TEXT("RightGrabSphere"));
    RightGrabSphere->SetupAttachment(RightMotionController);
    RightGrabSphere->SetSphereRadius(GrabRadius);
    RightGrabSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    RightGrabSphere->SetGenerateOverlapEvents(true);

    // Left Hand Mesh (optional)
    LeftHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LeftHandMesh"));
    LeftHandMesh->SetupAttachment(LeftMotionController);

    // Right Hand Mesh (optional)
    RightHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RightHandMesh"));
    RightHandMesh->SetupAttachment(RightMotionController);
}

void AVRPawn::BeginPlay()
{
    Super::BeginPlay();

    SetupEnhancedInput();

    if (bEnableKeyboardTest)
    {
        SetupKeyboardTest();
    }

    UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] BeginPlay - VR Ready"));
    UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] Keyboard Test: %s"), bEnableKeyboardTest ? TEXT("ON") : TEXT("OFF"));
}

void AVRPawn::SetupEnhancedInput()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());

    if (Subsystem && VRMappingContext)
    {
        Subsystem->AddMappingContext(VRMappingContext, 0);
        UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] Enhanced Input Mapping Context Added"));
    }
}

void AVRPawn::SetupKeyboardTest()
{
    if (bInputBound) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    EnableInput(PC);

    if (InputComponent)
    {
        // G키: 오른손 Grab 테스트
        InputComponent->BindAction("TestGrabRight", IE_Pressed, this, &AVRPawn::TryGrabRight);
        InputComponent->BindAction("TestGrabRight", IE_Released, this, &AVRPawn::TryReleaseRight);

        // H키: 왼손 Grab 테스트
        InputComponent->BindAction("TestGrabLeft", IE_Pressed, this, &AVRPawn::TryGrabLeft);
        InputComponent->BindAction("TestGrabLeft", IE_Released, this, &AVRPawn::TryReleaseLeft);

        bInputBound = true;
        UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] Keyboard Test: G=RightGrab, H=LeftGrab"));
    }
}

void AVRPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 잡고 있는 오브젝트 위치 업데이트는 Attach로 자동 처리됨
}

void AVRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EnhancedInput) return;

    // Left Grab
    if (IA_GrabLeft)
    {
        EnhancedInput->BindAction(IA_GrabLeft, ETriggerEvent::Started, this, &AVRPawn::TryGrabLeft);
        EnhancedInput->BindAction(IA_GrabLeft, ETriggerEvent::Completed, this, &AVRPawn::TryReleaseLeft);
    }

    // Right Grab
    if (IA_GrabRight)
    {
        EnhancedInput->BindAction(IA_GrabRight, ETriggerEvent::Started, this, &AVRPawn::TryGrabRight);
        EnhancedInput->BindAction(IA_GrabRight, ETriggerEvent::Completed, this, &AVRPawn::TryReleaseRight);
    }
}

// ============================================================
// Grab 함수들
// ============================================================

void AVRPawn::TryGrabLeft()
{
    if (bIsLeftGrabbing) return;

    AActor* NearestActor = GetNearestGrabbableActor(LeftGrabSphere);
    if (!IsValid(NearestActor)) return;

    // IGrabInteractable 인터페이스 확인
    if (NearestActor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
    {
        bool bCanGrab = IGrabInteractable::Execute_CanBeGrabbed(NearestActor);
        if (!bCanGrab) return;

        LeftGrabbedActor = NearestActor;
        bIsLeftGrabbing = true;

        // OnGrabbed 호출 (왼손 = true)
        IGrabInteractable::Execute_OnGrabbed(NearestActor, LeftMotionController, true);

        // 물리 비활성화 및 부착
        if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(NearestActor->GetRootComponent()))
        {
            PrimComp->SetSimulatePhysics(false);
        }
        NearestActor->AttachToComponent(LeftMotionController, FAttachmentTransformRules::KeepWorldTransform);

        UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] Left Hand Grabbed: %s"), *GetNameSafe(NearestActor));
    }
}

void AVRPawn::TryReleaseLeft()
{
    if (!bIsLeftGrabbing || !IsValid(LeftGrabbedActor)) return;

    // OnReleased 호출
    if (LeftGrabbedActor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
    {
        IGrabInteractable::Execute_OnReleased(LeftGrabbedActor, LeftMotionController, true);
    }

    // 부착 해제
    LeftGrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] Left Hand Released: %s"), *GetNameSafe(LeftGrabbedActor));

    LeftGrabbedActor = nullptr;
    bIsLeftGrabbing = false;
}

void AVRPawn::TryGrabRight()
{
    if (bIsRightGrabbing) return;

    AActor* NearestActor = GetNearestGrabbableActor(RightGrabSphere);
    if (!IsValid(NearestActor)) return;

    // IGrabInteractable 인터페이스 확인
    if (NearestActor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
    {
        bool bCanGrab = IGrabInteractable::Execute_CanBeGrabbed(NearestActor);
        if (!bCanGrab) return;

        RightGrabbedActor = NearestActor;
        bIsRightGrabbing = true;

        // OnGrabbed 호출 (오른손 = false)
        IGrabInteractable::Execute_OnGrabbed(NearestActor, RightMotionController, false);

        // 물리 비활성화 및 부착
        if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(NearestActor->GetRootComponent()))
        {
            PrimComp->SetSimulatePhysics(false);
        }
        NearestActor->AttachToComponent(RightMotionController, FAttachmentTransformRules::KeepWorldTransform);

        UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] Right Hand Grabbed: %s"), *GetNameSafe(NearestActor));
    }
}

void AVRPawn::TryReleaseRight()
{
    if (!bIsRightGrabbing || !IsValid(RightGrabbedActor)) return;

    // OnReleased 호출
    if (RightGrabbedActor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
    {
        IGrabInteractable::Execute_OnReleased(RightGrabbedActor, RightMotionController, false);
    }

    // 부착 해제
    RightGrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    UE_LOG(LogVRPawn, Warning, TEXT("[VRPawn] Right Hand Released: %s"), *GetNameSafe(RightGrabbedActor));

    RightGrabbedActor = nullptr;
    bIsRightGrabbing = false;
}

AActor* AVRPawn::GetNearestGrabbableActor(USphereComponent* GrabSphere)
{
    if (!IsValid(GrabSphere)) return nullptr;

    TArray<AActor*> OverlappingActors;
    GrabSphere->GetOverlappingActors(OverlappingActors);

    AActor* NearestActor = nullptr;
    float NearestDistance = FLT_MAX;

    FVector SphereLocation = GrabSphere->GetComponentLocation();

    for (AActor* Actor : OverlappingActors)
    {
        if (!IsValid(Actor)) continue;
        if (Actor == this) continue;

        // IGrabInteractable 인터페이스 구현 여부 확인
        if (!Actor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
        {
            continue;
        }

        float Distance = FVector::Dist(SphereLocation, Actor->GetActorLocation());
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestActor = Actor;
        }
    }

    return NearestActor;
}