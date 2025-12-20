#include "VRPlayerController.h"

AVRPlayerController::AVRPlayerController()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AVRPlayerController::BeginPlay()
{
    Super::BeginPlay();
}
