#include "PTTAudioRecorderComponent.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

int16 UPTTAudioRecorderComponent::FloatToPcm16(float S)
{
	S = FMath::Clamp(S, -1.0f, 1.0f);
	const int32 V = FMath::RoundToInt(S * 32767.0f);
	return (int16)FMath::Clamp(V, -32768, 32767);
}

void UPTTAudioRecorderComponent::WriteWaveFilePCM16(const TArray<int16>& Pcm16, int32 SampleRate, int32 NumChannels, TArray<uint8>& OutWavBytes)
{
	const int16 BitsPerSample = 16;
	const int32 ByteRate = SampleRate * NumChannels * (BitsPerSample / 8);
	const int16 BlockAlign = NumChannels * (BitsPerSample / 8);

	const int32 DataSize = Pcm16.Num() * sizeof(int16);
	const int32 RiffChunkSize = 36 + DataSize;

	auto AppendStr4 = [&](const char* S) { OutWavBytes.Append(reinterpret_cast<const uint8*>(S), 4); };
	auto Append32 = [&](int32 V) { OutWavBytes.Append(reinterpret_cast<uint8*>(&V), 4); };
	auto Append16 = [&](int16 V) { OutWavBytes.Append(reinterpret_cast<uint8*>(&V), 2); };

	OutWavBytes.Reset();

	AppendStr4("RIFF"); Append32(RiffChunkSize); AppendStr4("WAVE");

	AppendStr4("fmt "); Append32(16);
	Append16(1); // PCM
	Append16((int16)NumChannels);
	Append32(SampleRate);
	Append32(ByteRate);
	Append16(BlockAlign);
	Append16(BitsPerSample);

	AppendStr4("data"); Append32(DataSize);
	OutWavBytes.Append(reinterpret_cast<const uint8*>(Pcm16.GetData()), DataSize);
}

void UPTTAudioRecorderComponent::StartPTT()
{
	if (!Capture.IsValid())
	{
		Capture = MakeUnique<FCaptureImpl>();
	}

	// 이미 캡처 중이면 무시
	{
		FScopeLock Lock(&Capture->Mutex);
		if (Capture->bCapturing)
		{
			return;
		}

		Capture->Buffer.Reset();
		Capture->SampleRate = 0;
		Capture->NumChannels = 0;
	}

	Audio::FAudioCaptureDeviceParams Params;
	Params.DeviceIndex = INDEX_NONE;

	Audio::FOnAudioCaptureFunction OnCapture =
		[this](const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double /*StreamTime*/, bool /*bOverflow*/)
		{
			if (!Capture.IsValid())
				return;

			FScopeLock Lock(&Capture->Mutex);

			// Stop에서 bCapturing=false로 먼저 내려주면 이후 콜백은 즉시 무시됨
			if (!Capture->bCapturing)
				return;

			Capture->SampleRate = SampleRate;
			Capture->NumChannels = NumChannels;

			const float* AudioFloats = static_cast<const float*>(InAudio);
			const int32 NumSamples = NumFrames * NumChannels;

			const int32 OldNum = Capture->Buffer.Num();
			Capture->Buffer.AddUninitialized(NumSamples);
			FMemory::Memcpy(Capture->Buffer.GetData() + OldNum, AudioFloats, NumSamples * sizeof(float));
		};

	constexpr uint32 NumFramesDesired = 1024;

	if (!Capture->AudioCapture.OpenAudioCaptureStream(Params, MoveTemp(OnCapture), NumFramesDesired))
	{
		OnWavReady.Broadcast(false, TEXT("Failed to open audio capture stream"));
		return;
	}

	if (!Capture->AudioCapture.StartStream())
	{
		Capture->AudioCapture.CloseStream();
		OnWavReady.Broadcast(false, TEXT("Failed to start audio capture stream"));
		return;
	}

	// StartStream 성공 후 캡처 상태 true
	{
		FScopeLock Lock(&Capture->Mutex);
		Capture->bCapturing = true;
	}
}

void UPTTAudioRecorderComponent::StopPTTAndSave(const FString& OptionalWavPath)
{
	UE_LOG(LogTemp, Warning, TEXT("[PTT] StopPTTAndSave ENTER (CaptureValid=%d)"),
		Capture.IsValid() ? 1 : 0);
	if (!Capture.IsValid())
	{
		OnWavReady.Broadcast(false, TEXT("No capture impl"));
		return;
	}

	// Stop 시작에서 먼저 bCapturing=false로 내려서 콜백 차단
	bool bWasCapturing = false;
	{
		FScopeLock Lock(&Capture->Mutex);
		bWasCapturing = Capture->bCapturing;
		Capture->bCapturing = false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[PTT] StopPTTAndSave WasCapturing=%d"), bWasCapturing);
	if (!bWasCapturing)
	{
		OnWavReady.Broadcast(false, TEXT("Not recording"));
		return;
	}
	// 이제 안전하게 Stop/Close
	Capture->AudioCapture.StopStream();
	Capture->AudioCapture.CloseStream();

	// 버퍼 복사
	TArray<float> FloatSamples;
	int32 SampleRate = 0;
	int32 NumChannels = 0;

	{
		FScopeLock Lock(&Capture->Mutex);
		FloatSamples = Capture->Buffer;
		SampleRate = (Capture->SampleRate > 0) ? Capture->SampleRate : DesiredSampleRate;
		NumChannels = (Capture->NumChannels > 0) ? Capture->NumChannels : DesiredNumChannels;
	}

	if (FloatSamples.Num() == 0)
	{
		OnWavReady.Broadcast(false, TEXT("No audio captured"));
		return;
	}

	// mono 다운믹스 + PCM16
	TArray<int16> Pcm16;

	if (NumChannels <= 1)
	{
		Pcm16.Reserve(FloatSamples.Num());
		for (float S : FloatSamples)
		{
			Pcm16.Add(FloatToPcm16(S));
		}
		NumChannels = 1;
	}
	else
	{
		const int32 Frames = FloatSamples.Num() / NumChannels;
		Pcm16.Reserve(Frames);

		for (int32 f = 0; f < Frames; ++f)
		{
			double Acc = 0.0;
			for (int32 c = 0; c < NumChannels; ++c)
			{
				Acc += FloatSamples[f * NumChannels + c];
			}
			const float Mono = (float)(Acc / (double)NumChannels);
			Pcm16.Add(FloatToPcm16(Mono));
		}

		NumChannels = 1;
	}

	// 저장 경로
	FString WavPath = OptionalWavPath;
	if (WavPath.IsEmpty())
	{
		WavPath = FPaths::ProjectSavedDir() / TEXT("Whisper/ptt.wav");
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(WavPath), true);

	TArray<uint8> WavBytes;
	WriteWaveFilePCM16(Pcm16, SampleRate, NumChannels, WavBytes);

	if (!FFileHelper::SaveArrayToFile(WavBytes, *WavPath))
	{
		OnWavReady.Broadcast(false, FString::Printf(TEXT("Failed to write wav: %s"), *WavPath));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[PTT] WAV SAVED: %s (bytes=%d)"), *WavPath, WavBytes.Num());

	OnWavReady.Broadcast(true, WavPath);
}

void UPTTAudioRecorderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Capture.IsValid())
	{
		// EndPlay에서도 동일하게 “먼저 bCapturing=false” → Stop/Close
		bool bWasCapturing = false;
		{
			FScopeLock Lock(&Capture->Mutex);
			bWasCapturing = Capture->bCapturing;
			Capture->bCapturing = false;
		}

		if (bWasCapturing)
		{
			Capture->AudioCapture.StopStream();
			Capture->AudioCapture.CloseStream();
		}
	}

	Super::EndPlay(EndPlayReason);
}
