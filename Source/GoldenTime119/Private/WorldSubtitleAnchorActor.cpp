#include "WorldSubtitleAnchorActor.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

AWorldSubtitleAnchorActor::AWorldSubtitleAnchorActor()
{
	// 월드 고정은 Tick이 사실상 필요 없지만,
	// bFaceCameraYawOnly가 켜질 수 있으니 일단 유지.
}

void AWorldSubtitleAnchorActor::UpdatePlacement(float DeltaSeconds)
{
	if (!bFaceCameraYawOnly)
		return;

	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;

	const FVector CamLoc = Cam->GetCameraLocation();
	const FVector MyLoc = GetActorLocation();

	FRotator LookAt = (CamLoc - MyLoc).Rotation();
	LookAt.Pitch = 0.f;
	LookAt.Roll = 0.f;

	SetActorRotation(LookAt);
}
