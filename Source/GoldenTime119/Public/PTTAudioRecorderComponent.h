#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/CriticalSection.h"

#include "AudioCaptureCore.h"
#include "AudioCaptureDeviceInterface.h"

#include "PTTAudioRecorderComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPTTRecordedWavReady, bool, bSuccess, const FString&, WavPathOrError);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UPTTAudioRecorderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "PTT|Events")
	FOnPTTRecordedWavReady OnWavReady;

	UFUNCTION(BlueprintCallable, Category = "PTT")
	void StartPTT();

	UFUNCTION(BlueprintCallable, Category = "PTT")
	void StopPTTAndSave(const FString& OptionalWavPath = TEXT(""));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTT|Config")
	int32 DesiredSampleRate = 48000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTT|Config")
	int32 DesiredNumChannels = 1;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FCaptureImpl
	{
		Audio::FAudioCapture AudioCapture;
		FCriticalSection Mutex;

		TArray<float> Buffer;      // interleaved float
		int32 SampleRate = 0;
		int32 NumChannels = 0;
		bool bCapturing = false;
	};

	TUniquePtr<FCaptureImpl> Capture;

	static int16 FloatToPcm16(float S);
	static void WriteWaveFilePCM16(const TArray<int16>& Pcm16, int32 SampleRate, int32 NumChannels, TArray<uint8>& OutWavBytes);
};
