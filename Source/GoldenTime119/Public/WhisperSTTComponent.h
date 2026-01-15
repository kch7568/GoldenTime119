#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WhisperSTTComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWhisperSTTFinished, bool, bSuccess, const FString&, Text);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UWhisperSTTComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Whisper|Events")
	FOnWhisperSTTFinished OnFinished;

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void RunWhisperOnWav(const FString& WavPath);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisper|Config")
	FString Language = TEXT("ko");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisper|Config")
	float PollIntervalSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisper|Config")
	float TimeoutSeconds = 20.0f;

protected:
	virtual void BeginPlay() override;

private:
	FString ExePathAbs;
	FString ModelPathAbs;
	FString OutputBaseAbs;
	double StartTimeSec = 0.0;
	FTimerHandle PollTimer;

	void PollResultFile();
};
