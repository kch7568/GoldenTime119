#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/CriticalSection.h"

#include "AudioCaptureCore.h"
#include "AudioCaptureDeviceInterface.h"

#include "PTTAudioRecorderComponent.generated.h"

// ====== Legacy (WAV) ======
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPTTRecordedWavReady,
	bool, bSuccess,
	const FString&, WavPathOrError
);

// ====== Realtime Streaming ======
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnPTTPcm16FrameReady,
	const TArray<uint8>&, Pcm16BytesLE,   // little-endian, mono
	int32, SampleRate,                    // e.g. 24000
	int32, NumChannels,                   // always 1 in this component output
	float, FrameDurationSec               // e.g. 0.02
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnPTTCaptureFinalized,
	bool, bSuccess,
	float, TotalDurationSec,
	const FString&, ErrorOrInfo
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UPTTAudioRecorderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Realtime Events ======
	UPROPERTY(BlueprintAssignable, Category = "PTT|Events")
	FOnPTTPcm16FrameReady OnPcm16FrameReady;

	UPROPERTY(BlueprintAssignable, Category = "PTT|Events")
	FOnPTTCaptureFinalized OnCaptureFinalized;

	// ====== Legacy WAV Events ======
	UPROPERTY(BlueprintAssignable, Category = "PTT|Events")
	FOnPTTRecordedWavReady OnWavReady;

	// ====== BP API ======
	UFUNCTION(BlueprintCallable, Category = "PTT")
	void StartPTT();

	// Realtime: Stop only (no file). Triggers OnCaptureFinalized.
	UFUNCTION(BlueprintCallable, Category = "PTT")
	void StopPTT();

	// Legacy: Stop + Save wav. Keeps compatibility with older pipeline.
	UFUNCTION(BlueprintCallable, Category = "PTT")
	void StopPTTAndSave(const FString& OptionalWavPath = TEXT(""));

	// ====== Config ======
	// OpenAudioCaptureStream 원하는 SR/CH이지만, 실제 디바이스가 다를 수 있음(콜백 SampleRate가 진짜 값)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTT|Config")
	int32 DesiredSampleRate = 48000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTT|Config")
	int32 DesiredNumChannels = 1;

	// Realtime용 출력 샘플레이트(권장 24000)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTT|Realtime")
	int32 OutputSampleRate = 24000;

	// 프레임 길이(초). 0.02(20ms) 권장
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTT|Realtime", meta = (ClampMin = "0.01", ClampMax = "0.08"))
	float FrameDurationSec = 0.02f;

	// Stop 직후, 남은 잔여 샘플을 frame으로 flush할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTT|Realtime")
	bool bFlushRemainderOnStop = true;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FCaptureImpl
	{
		Audio::FAudioCapture AudioCapture;
		FCriticalSection Mutex;

		bool bCapturing = false;

		// Input format (from device callback)
		int32 InSampleRate = 0;
		int32 InNumChannels = 0;

		// Realtime output format
		int32 OutSampleRate = 24000;
		int32 OutNumChannels = 1;

		// Accumulator in float mono (after downmix, before resample)
		TArray<float> MonoFloatAccum;

		// For duration accounting (in output samples)
		int64 TotalOutSamples = 0;

		// Frame size in output samples
		int32 OutFrameSamples = 0;
		float FrameDurationSec = 0.02f;
	};

	TUniquePtr<FCaptureImpl> Capture;

	// ====== Helpers ======
	static int16 FloatToPcm16(float S);

	// Downmix interleaved float -> mono float
	static void DownmixToMono(
		const float* InInterleaved,
		int32 NumFrames,
		int32 NumChannels,
		TArray<float>& OutMono
	);

	// Resample mono float: InSR -> OutSR (48k->24k fast path + fallback linear)
	static void ResampleMono(
		const TArray<float>& InMono,
		int32 InSampleRate,
		TArray<float>& InOutMonoAccum,
		int32 OutSampleRate
	);

	// Pop N output-samples from accum, encode to PCM16 bytes and broadcast frame
	void EmitFrames_Locked();

	// Legacy WAV writer
	static void WriteWaveFilePCM16(
		const TArray<int16>& Pcm16,
		int32 SampleRate,
		int32 NumChannels,
		TArray<uint8>& OutWavBytes
	);

	// Internal shared stop
	void StopInternal(bool bSaveWav, const FString& OptionalWavPath);
};
