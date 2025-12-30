// VRHandController.cpp
#include "VRHandController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRHand, Log, All);

AVRHandController::AVRHandController()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Motion Controller
    MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionController"));
    MotionController->SetupAttachment(Root);

    // Grab Sphere (손 주변 감지 영역)
    GrabSphere = CreateDefaultSubobject<USphereComponent>(TEXT("GrabSphere"));
    GrabSphere->SetupAttachment(MotionController);
    GrabSphere->SetSphereRadius(GrabRadius);
    GrabSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    GrabSphere->SetGenerateOverlapEvents(true);

    // Hand Mesh (옵션)
    HandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMesh"));
    HandMesh->SetupAttachment(MotionController);
}

void AVRHandController::BeginPlay()
{
    Super::BeginPlay();
    SetupMotionController();

    UE_LOG(LogVRHand, Warning, TEXT("[VRHand] %s Hand Controller Ready"),
        IsLeftHand() ? TEXT("Left") : TEXT("Right"));
}

void AVRHandController::SetupMotionController()
{
    if (IsValid(MotionController))
    {
        // 왼손/오른손에 따라 Motion Source 설정
        MotionController->SetTrackingMotionSource(
            IsLeftHand() ? FName("Left") : FName("Right")
        );
    }
}

void AVRHandController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 잡고 있는 오브젝트가 있으면 위치 업데이트
    if (bIsGrabbing && IsValid(GrabbedActor))
    {
        GrabbedActor->SetActorLocation(MotionController->GetComponentLocation());
        GrabbedActor->SetActorRotation(MotionController->GetComponentRotation());
    }
}

void AVRHandController::TryGrab()
{
    if (bIsGrabbing) return;

    AActor* NearestActor = GetNearestGrabbableActor();
    if (!IsValid(NearestActor)) return;

    // IGrabInteractable 인터페이스 확인
    if (NearestActor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
    {
        // CanBeGrabbed 확인
        bool bCanGrab = IGrabInteractable::Execute_CanBeGrabbed(NearestActor);
        if (!bCanGrab) return;

        // 잡기 실행
        GrabbedActor = NearestActor;
        bIsGrabbing = true;

        // OnGrabbed 호출
        IGrabInteractable::Execute_OnGrabbed(NearestActor, MotionController, IsLeftHand());

        // 물리 비활성화 및 부착
        if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(NearestActor->GetRootComponent()))
        {
            PrimComp->SetSimulatePhysics(false);
        }
        NearestActor->AttachToComponent(MotionController, FAttachmentTransformRules::KeepWorldTransform);

        UE_LOG(LogVRHand, Warning, TEXT("[VRHand] %s Hand Grabbed: %s"),
            IsLeftHand() ? TEXT("Left") : TEXT("Right"), *GetNameSafe(NearestActor));
    }
}

void AVRHandController::TryRelease()
{
    if (!bIsGrabbing || !IsValid(GrabbedActor)) return;

    // OnReleased 호출
    if (GrabbedActor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
    {
        IGrabInteractable::Execute_OnReleased(GrabbedActor, MotionController, IsLeftHand());
    }

    // 부착 해제
    GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // 물리 다시 활성화 (옵션)
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
    {
        PrimComp->SetSimulatePhysics(true);
    }

    UE_LOG(LogVRHand, Warning, TEXT("[VRHand] %s Hand Released: %s"),
        IsLeftHand() ? TEXT("Left") : TEXT("Right"), *GetNameSafe(GrabbedActor));

    GrabbedActor = nullptr;
    bIsGrabbing = false;
}

AActor* AVRHandController::GetNearestGrabbableActor()
{
    TArray<AActor*> OverlappingActors;
    GrabSphere->GetOverlappingActors(OverlappingActors);

    AActor* NearestActor = nullptr;
    float NearestDistance = FLT_MAX;

    FVector HandLocation = MotionController->GetComponentLocation();

    for (AActor* Actor : OverlappingActors)
    {
        if (!IsValid(Actor)) continue;
        if (Actor == this) continue;

        // IGrabInteractable 인터페이스 구현 여부 확인
        if (!Actor->GetClass()->ImplementsInterface(UGrabInteractable::StaticClass()))
        {
            continue;
        }

        float Distance = FVector::Dist(HandLocation, Actor->GetActorLocation());
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestActor = Actor;
        }
    }

    return NearestActor;
}