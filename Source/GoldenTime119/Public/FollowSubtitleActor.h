#pragma once

#include "CoreMinimal.h"
#include "SubtitleBaseActor.h"
#include "FollowSubtitleActor.generated.h"

UCLASS()
class GOLDENTIME119_API AFollowSubtitleActor : public ASubtitleBaseActor
{
	GENERATED_BODY()

public:
	AFollowSubtitleActor();

	/** 카메라 앞 거리(cm) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Follow", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float FollowDistance = 120.f;

	/** 카메라 기준 위로 올리는 높이(cm) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Follow", meta = (ClampMin = "-100.0", ClampMax = "200.0"))
	float HeightOffset = -10.f;

	/** 위치 보간 속도 (높을수록 더 빨리 따라감) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Follow", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float LocationInterpSpeed = 12.f;

	/** 회전 보간 속도 */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Follow", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float RotationInterpSpeed = 12.f;

	/** 회전은 Yaw만 따라갈지(권장) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Follow")
	bool bYawOnly = true;

	/** 항상 카메라를 바라보게(기본 true) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Follow")
	bool bFaceCamera = true;

protected:
	virtual void BeginPlay() override;
	virtual void UpdatePlacement(float DeltaSeconds) override;

private:
	FVector SmoothedLoc;
	FRotator SmoothedRot;
	bool bInitialized = false;
};
