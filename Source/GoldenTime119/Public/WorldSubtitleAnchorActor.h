#pragma once

#include "CoreMinimal.h"
#include "SubtitleBaseActor.h"
#include "WorldSubtitleAnchorActor.generated.h"

UCLASS()
class GOLDENTIME119_API AWorldSubtitleAnchorActor : public ASubtitleBaseActor
{
	GENERATED_BODY()

public:
	AWorldSubtitleAnchorActor();

	/** 필요 시: Yaw만 카메라를 향하도록 */
	UPROPERTY(EditAnywhere, Category = "Subtitle|WorldAnchor")
	bool bFaceCameraYawOnly = false;

protected:
	virtual void UpdatePlacement(float DeltaSeconds) override;
};
