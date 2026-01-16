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

void UPTTAudioRecorderComponent::DownmixToMono(
	const float* InInterleaved,
	int32 NumFrames,
	int32 NumChannels,
	TArray<float>& OutMono
)
{
	OutMono.Reset();
	if (!InInterleaved || NumFrames <= 0 || NumChannels <= 0)
		return;

	OutMono.AddUninitialized(NumFrames);

	if (NumChannels == 1)
	{
		FMemory::Memcpy(OutMono.GetData(), InInterleaved, NumFrames * sizeof(float));
		return;
	}

	for (int32 f = 0; f < NumFrames; ++f)
	{
		double Acc = 0.0;
		const int32 Base = f * NumChannels;
		for (int32 c = 0; c < NumChannels; ++c)
		{
			Acc += (double)InInterleaved[Base + c];
		}
		OutMono[f] = (float)(Acc / (double)NumChannels);
	}
}

// InMono는 "이번 콜백 분량의 mono"이고,
// InOutMonoAccum는 "Output sample rate 기준으로 resample된 누적 버퍼"로 씁니다.
void UPTTAudioRecorderComponent::ResampleMono(
	const TArray<float>& InMono,
	int32 InSampleRate,
	TArray<float>& InOutMonoAccum,
	int32 OutSampleRate
)
{
	if (InMono.Num() == 0 || InSampleRate <= 0 || OutSampleRate <= 0)
		return;

	// Fast path: 48000 -> 24000 (downsample by 2)
	if (InSampleRate == 48000 && OutSampleRate == 24000)
	{
		const int32 InN = InMono.Num();
		const int32 OutN = InN / 2;
		const int32 Old = InOutMonoAccum.Num();
		InOutMonoAccum.AddUninitialized(OutN);

		for (int32 i = 0; i < OutN; ++i)
		{
			// simple pick (could average 2 samples if desired)
			InOutMonoAccum[Old + i] = InMono[i * 2];
		}
		return;
	}

	// General linear resample
	const double Ratio = (double)OutSampleRate / (double)InSampleRate; // out per in
	const int32 InN = InMono.Num();
	const int32 OutN = FMath::Max(1, (int32)FMath::FloorToInt((double)InN * Ratio));

	const int32 Old = InOutMonoAccum.Num();
	InOutMonoAccum.AddUninitialized(OutN);

	for (int32 i = 0; i < OutN; ++i)
	{
		const double InPos = (double)i / Ratio; // position in input
		const int32 i0 = FMath::Clamp((int32)FMath::FloorToInt(InPos), 0, InN - 1);
		const int32 i1 = FMath::Clamp(i0 + 1, 0, InN - 1);
		const double t = InPos - (double)i0;

		const float s0 = InMono[i0];
		const float s1 = InMono[i1];
		InOutMonoAccum[Old + i] = FMath::Lerp(s0, s1, (float)t);
	}
}

void UPTTAudioRecorderComponent::EmitFrames_Locked()
{
	if (!Capture.IsValid())
		return;

	// accum = output SR mono float
	const int32 FrameSamples = Capture->OutFrameSamples;
	if (FrameSamples <= 0)
		return;

	while (Capture->MonoFloatAccum.Num() >= FrameSamples)
	{
		// Pop first FrameSamples from accum
		TArray<uint8> Bytes;
		Bytes.Reserve(FrameSamples * sizeof(int16));

		for (int32 i = 0; i < FrameSamples; ++i)
		{
			const int16 P = FloatToPcm16(Capture->MonoFloatAccum[i]);
			Bytes.Append(reinterpret_cast<const uint8*>(&P), sizeof(int16));
		}

		// Remove used samples
		Capture->MonoFloatAccum.RemoveAt(0, FrameSamples, false);

		Capture->TotalOutSamples += FrameSamples;

		// NOTE: Broadcasting while holding lock is okay if receivers are lightweight,
		// but safer is unlock then broadcast. 여기선 간단히 "복사본"만 만들어서 락 풀고 브로드캐스트.
		const int32 SR = Capture->OutSampleRate;
		const float Dur = Capture->FrameDurationSec;

		// unlock-broadcast pattern
		Capture->Mutex.Unlock();
		OnPcm16FrameReady.Broadcast(Bytes, SR, 1, Dur);
		Capture->Mutex.Lock();
	}
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

	{
		FScopeLock Lock(&Capture->Mutex);
		if (Capture->bCapturing)
			return;

		Capture->bCapturing = false;
		Capture->InSampleRate = 0;
		Capture->InNumChannels = 0;

		Capture->OutSampleRate = FMath::Max(8000, OutputSampleRate);
		Capture->OutNumChannels = 1;

		Capture->FrameDurationSec = FMath::Clamp(FrameDurationSec, 0.01f, 0.08f);
		Capture->OutFrameSamples = FMath::Max(1, (int32)FMath::RoundToInt((float)Capture->OutSampleRate * Capture->FrameDurationSec));

		Capture->MonoFloatAccum.Reset();
		Capture->TotalOutSamples = 0;
	}

	Audio::FAudioCaptureDeviceParams Params;
	Params.DeviceIndex = INDEX_NONE;

	Audio::FOnAudioCaptureFunction OnCapture =
		[this](const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double /*StreamTime*/, bool /*bOverflow*/)
		{
			if (!Capture.IsValid())
				return;

			FScopeLock Lock(&Capture->Mutex);

			if (!Capture->bCapturing)
				return;

			Capture->InSampleRate = SampleRate;
			Capture->InNumChannels = NumChannels;

			const float* AudioFloats = static_cast<const float*>(InAudio);

			// 1) downmix to mono float
			TArray<float> Mono;
			DownmixToMono(AudioFloats, NumFrames, NumChannels, Mono);

			// 2) resample mono -> output sample rate, append into accum
			ResampleMono(Mono, SampleRate, Capture->MonoFloatAccum, Capture->OutSampleRate);

			// 3) emit frames if enough
			// lock is held. EmitFrames_Locked will unlock-broadcast-lock internally.
			EmitFrames_Locked();
		};

	constexpr uint32 NumFramesDesired = 1024;

	if (!Capture->AudioCapture.OpenAudioCaptureStream(Params, MoveTemp(OnCapture), NumFramesDesired))
	{
		OnCaptureFinalized.Broadcast(false, 0.f, TEXT("Failed to open audio capture stream"));
		return;
	}

	if (!Capture->AudioCapture.StartStream())
	{
		Capture->AudioCapture.CloseStream();
		OnCaptureFinalized.Broadcast(false, 0.f, TEXT("Failed to start audio capture stream"));
		return;
	}

	{
		FScopeLock Lock(&Capture->Mutex);
		Capture->bCapturing = true;
	}
}

void UPTTAudioRecorderComponent::StopPTT()
{
	StopInternal(false, TEXT(""));
}

void UPTTAudioRecorderComponent::StopPTTAndSave(const FString& OptionalWavPath)
{
	StopInternal(true, OptionalWavPath);
}

void UPTTAudioRecorderComponent::StopInternal(bool bSaveWav, const FString& OptionalWavPath)
{
	if (!Capture.IsValid())
	{
		if (bSaveWav)
			OnWavReady.Broadcast(false, TEXT("No capture impl"));
		OnCaptureFinalized.Broadcast(false, 0.f, TEXT("No capture impl"));
		return;
	}

	bool bWasCapturing = false;
	{
		FScopeLock Lock(&Capture->Mutex);
		bWasCapturing = Capture->bCapturing;
		Capture->bCapturing = false;
	}

	if (!bWasCapturing)
	{
		if (bSaveWav)
			OnWavReady.Broadcast(false, TEXT("Not recording"));
		OnCaptureFinalized.Broadcast(false, 0.f, TEXT("Not recording"));
		return;
	}

	// Stop/Close stream
	Capture->AudioCapture.StopStream();
	Capture->AudioCapture.CloseStream();

	// Flush remainder (optional)
	float TotalDurationSec = 0.f;
	TArray<int16> AllPcm16; // only for legacy save (optional)
	int32 SR = OutputSampleRate;

	{
		FScopeLock Lock(&Capture->Mutex);

		SR = Capture->OutSampleRate;
		const int32 FrameSamples = Capture->OutFrameSamples;

		if (bFlushRemainderOnStop && Capture->MonoFloatAccum.Num() > 0)
		{
			// emit full frames already possible
			EmitFrames_Locked();

			// if still has remainder < FrameSamples, you can choose:
			// (a) pad to full frame and emit one last frame
			// (b) drop remainder
			// 여기서는 "패딩 후 1프레임 방출"로 처리
			if (Capture->MonoFloatAccum.Num() > 0)
			{
				const int32 Rem = Capture->MonoFloatAccum.Num();
				const int32 Pad = FMath::Max(0, FrameSamples - Rem);
				if (Pad > 0)
				{
					Capture->MonoFloatAccum.AddZeroed(Pad);
				}
				EmitFrames_Locked();
			}
		}

		TotalDurationSec = (SR > 0) ? ((float)Capture->TotalOutSamples / (float)SR) : 0.f;

		if (bSaveWav)
		{
			// For wav save: we don't have "all samples" unless we also accumulated them.
			// 간단하게: frame을 뿌리는 누적 과정에서 전체를 쌓지 않았기 때문에
			// 여기서는 "Stop 직전 accum을 포함해 남아있는 샘플만"으로는 부족함.
			// => WAV 저장 기능을 계속 쓰려면, 별도 FullAccum을 두거나, 기존 방식으로 Buffer를 따로 유지해야 함.
			// 실무에서는 보통 Realtime 모드에서 WAV 저장을 끄므로, 명확히 실패 처리.
			AllPcm16.Reset();
		}
	}

	OnCaptureFinalized.Broadcast(true, TotalDurationSec, FString::Printf(TEXT("Captured %.2fs @ %dHz mono"), TotalDurationSec, SR));

	// Legacy WAV save: 명시적으로 비활성 처리(설계상 충돌 방지)
	if (bSaveWav)
	{
		OnWavReady.Broadcast(false, TEXT("WAV save disabled in Realtime-streaming mode (no full buffer)."));
	}
}

void UPTTAudioRecorderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Capture.IsValid())
	{
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
