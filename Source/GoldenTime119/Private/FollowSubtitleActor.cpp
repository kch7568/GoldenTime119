#include "FollowSubtitleActor.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

AFollowSubtitleActor::AFollowSubtitleActor()
{
	// 팔로우는 Tick 필요
	PrimaryActorTick.bCanEverTick = true;
}

void AFollowSubtitleActor::BeginPlay()
{
	Super::BeginPlay();

	SmoothedLoc = GetActorLocation();
	SmoothedRot = GetActorRotation();
	bInitialized = false;
}

void AFollowSubtitleActor::UpdatePlacement(float DeltaSeconds)
{
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;

	const FVector CamLoc = Cam->GetCameraLocation();
	const FRotator CamRot = Cam->GetCameraRotation();

	const FVector Forward = CamRot.Vector();
	const FVector Up = FVector::UpVector;

	// 목표 위치: 카메라 앞 + 높이 오프셋
	const FVector TargetLoc = CamLoc + Forward * FollowDistance + Up * HeightOffset;

	// 목표 회전: 기본은 카메라를 바라보게(빌보드), 또는 카메라 회전과 유사하게
	FRotator TargetRot = GetActorRotation();

	if (bFaceCamera)
	{
		// Actor가 "카메라를 향하도록": 카메라->자막 방향이 아니라 "자막->카메라" 방향
		const FVector MyToCam = (CamLoc - TargetLoc);
		TargetRot = MyToCam.Rotation();
	}
	else
	{
		TargetRot = CamRot;
	}

	if (bYawOnly)
	{
		TargetRot.Pitch = 0.f;
		TargetRot.Roll = 0.f;
	}

	if (!bInitialized)
	{
		SmoothedLoc = TargetLoc;
		SmoothedRot = TargetRot;
		bInitialized = true;
	}
	else
	{
		SmoothedLoc = FMath::VInterpTo(SmoothedLoc, TargetLoc, DeltaSeconds, LocationInterpSpeed);
		SmoothedRot = FMath::RInterpTo(SmoothedRot, TargetRot, DeltaSeconds, RotationInterpSpeed);
	}

	SetActorLocation(SmoothedLoc);
	SetActorRotation(SmoothedRot);
}
