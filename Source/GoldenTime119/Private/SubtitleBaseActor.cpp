#include "SubtitleBaseActor.h"

#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ASubtitleBaseActor::ASubtitleBaseActor()
{
	PrimaryActorTick.bCanEverTick = true;

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
	SetRootComponent(WidgetComp);

	WidgetComp->SetWidgetSpace(EWidgetSpace::World);

	// 픽셀밀도/물리크기 분리를 위해 우리가 DrawSize를 직접 세팅
	WidgetComp->SetDrawAtDesiredSize(false);

	WidgetComp->SetTwoSided(true);
	WidgetComp->SetPivot(FVector2D(0.5f, 0.5f));
	WidgetComp->SetVisibility(true);

	SmoothedWorldScale = FVector(0.01f);
	WidgetComp->SetWorldScale3D(SmoothedWorldScale);
}

void ASubtitleBaseActor::BeginPlay()
{
	Super::BeginPlay();

	if (!WidgetComp)
		return;

	if (WidgetClass)
	{
		WidgetComp->SetWidgetClass(WidgetClass);
	}

	EnsureWidgetReady();
	CacheWidgetRefs();

	ResetFadeState();
	ApplyOpacity(0.f);
}

void ASubtitleBaseActor::Tick(float DeltaSeconds)
{
	if (!GetWorld() || GetWorld()->bIsTearingDown)
		return;

	Super::Tick(DeltaSeconds);

	UpdatePlacement(DeltaSeconds);

	// 크기/선명도/거리보정은 여기서 일괄 처리
	UpdateRenderSizeAndWorldScale(DeltaSeconds);

	if (!RootBorder || !SubtitleText)
		return;

	switch (FadeState)
	{
	case ESubtitleFadeState::FadingIn:
	{
		FadeElapsed += DeltaSeconds;
		const float Den = FMath::Max(0.01f, ActiveFadeIn);
		const float Alpha = FMath::Clamp(FadeElapsed / Den, 0.f, 1.f);
		ApplyOpacity(Alpha);

		if (Alpha >= 1.f)
		{
			FadeState = ESubtitleFadeState::Holding;
			FadeElapsed = 0.f;
		}
		break;
	}
	case ESubtitleFadeState::Holding:
	{
		FadeElapsed += DeltaSeconds;
		if (FadeElapsed >= ActiveHold)
		{
			FadeState = ESubtitleFadeState::FadingOut;
			FadeElapsed = 0.f;
		}
		break;
	}
	case ESubtitleFadeState::FadingOut:
	{
		FadeElapsed += DeltaSeconds;
		const float Den = FMath::Max(0.01f, ActiveFadeOut);
		const float Alpha = 1.f - FMath::Clamp(FadeElapsed / Den, 0.f, 1.f);
		ApplyOpacity(Alpha);

		if (Alpha <= 0.f)
		{
			ResetFadeState();
			ApplyOpacity(0.f);
		}
		break;
	}
	default:
		break;
	}
}

void ASubtitleBaseActor::EnsureWidgetReady()
{
	if (!WidgetComp)
		return;

	if (WidgetInstance)
		return;

	WidgetComp->InitWidget();
	WidgetInstance = WidgetComp->GetUserWidgetObject();

#if !UE_BUILD_SHIPPING
	if (!WidgetInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Subtitle] WidgetInstance NULL. Set WidgetClass on the actor instance."));
	}
#endif
}

void ASubtitleBaseActor::CacheWidgetRefs()
{
	if (!WidgetInstance)
		return;

	RootBorder = Cast<UBorder>(WidgetInstance->GetWidgetFromName(TEXT("RootBorder")));
	SubtitleText = Cast<UTextBlock>(WidgetInstance->GetWidgetFromName(TEXT("SubtitleText")));

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Subtitle] CacheWidgetRefs: Border=%s Text=%s"),
		RootBorder ? TEXT("OK") : TEXT("NULL"),
		SubtitleText ? TEXT("OK") : TEXT("NULL"));
#endif
}

void ASubtitleBaseActor::ApplyOpacity(float Opacity01)
{
	if (!RootBorder) return;
	RootBorder->SetRenderOpacity(FMath::Clamp(Opacity01, 0.f, 1.f));
}

void ASubtitleBaseActor::SetSubtitleText(const FText& Text)
{
	if (!SubtitleText) return;
	SubtitleText->SetText(Text);
}

void ASubtitleBaseActor::ResetFadeState()
{
	FadeState = ESubtitleFadeState::Hidden;
	FadeElapsed = 0.f;
}

float ASubtitleBaseActor::CalcVisibleHoldBase() const
{
	return FMath::Max(0.f, PageIntervalSeconds - FadeInSeconds - FadeOutSeconds);
}

void ASubtitleBaseActor::StartFadeSequence(const FText& Text, float InFadeIn, float InHold, float InFadeOut)
{
	EnsureWidgetReady();
	CacheWidgetRefs();

	if (!RootBorder || !SubtitleText)
		return;

	SetSubtitleText(Text);

	ActiveFadeIn = FMath::Max(0.01f, InFadeIn);
	ActiveHold = FMath::Max(0.f, InHold);
	ActiveFadeOut = FMath::Max(0.01f, InFadeOut);

	FadeElapsed = 0.f;
	FadeState = ESubtitleFadeState::FadingIn;
	ApplyOpacity(0.f);
}

void ASubtitleBaseActor::ShowSubtitle(const FText& Subtitle, float MinHoldSeconds)
{
	StopPages();
	const float Hold = FMath::Max(0.f, MinHoldSeconds);
	StartFadeSequence(Subtitle, FadeInSeconds, Hold, FadeOutSeconds);
}

void ASubtitleBaseActor::ShowSubtitlePages(const TArray<FText>& Pages)
{
	StopPages();

	CurrentPages = Pages;
	CurrentPageIndex = 0;

	if (CurrentPages.Num() <= 0)
		return;

	const bool bIsLast = (CurrentPages.Num() == 1);
	const float BaseHold = CalcVisibleHoldBase();
	const float Hold = bIsLast ? (BaseHold + ExtraHoldOnLastPageSeconds) : BaseHold;

	ShowPage(0, Hold);

	if (CurrentPages.Num() >= 2)
	{
		ScheduleNextPage(PageIntervalSeconds);
	}
}

void ASubtitleBaseActor::ShowPage(int32 PageIndex, float HoldSeconds)
{
	if (!CurrentPages.IsValidIndex(PageIndex))
		return;

	StartFadeSequence(CurrentPages[PageIndex], FadeInSeconds, HoldSeconds, FadeOutSeconds);
}

void ASubtitleBaseActor::ScheduleNextPage(float InDelaySeconds)
{
	GetWorldTimerManager().ClearTimer(PageTimer);
	GetWorldTimerManager().SetTimer(
		PageTimer, this, &ASubtitleBaseActor::AdvancePage,
		FMath::Max(0.01f, InDelaySeconds), false);
}

void ASubtitleBaseActor::AdvancePage()
{
	if (CurrentPages.Num() <= 0)
	{
		StopPages();
		return;
	}

	CurrentPageIndex++;

	if (!CurrentPages.IsValidIndex(CurrentPageIndex))
	{
		StopPages();
		return;
	}

	const bool bIsLast = (CurrentPageIndex == CurrentPages.Num() - 1);
	const float BaseHold = CalcVisibleHoldBase();
	const float Hold = bIsLast ? (BaseHold + ExtraHoldOnLastPageSeconds) : BaseHold;

	ShowPage(CurrentPageIndex, Hold);

	if (!bIsLast)
	{
		ScheduleNextPage(PageIntervalSeconds);
	}
}

void ASubtitleBaseActor::HideNow()
{
	StopPages();
	EnsureWidgetReady();
	CacheWidgetRefs();
	ApplyOpacity(0.f);
	ResetFadeState();
}

void ASubtitleBaseActor::StopPages()
{
	GetWorldTimerManager().ClearTimer(PageTimer);
	CurrentPages.Reset();
	CurrentPageIndex = 0;
	ResetFadeState();
}

void ASubtitleBaseActor::HandleTutorialSubtitle(const FText& Subtitle, float MinHoldSeconds)
{
	ShowSubtitle(Subtitle, MinHoldSeconds);
}

// ===== Local view (PIE/서버 크래시 방지) =====
bool ASubtitleBaseActor::TryGetLocalPlayerView(FVector& OutCamLoc, FRotator& OutCamRot) const
{
	if (GetNetMode() == NM_DedicatedServer)
		return false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->IsLocalController())
		return false;

	PC->GetPlayerViewPoint(OutCamLoc, OutCamRot);
	return true;
}

// ===== 핵심: BP 크기 기반 + 선명도(PD) + 물리폭(cm) 자동 맞춤 + 거리 보정 =====
void ASubtitleBaseActor::UpdateRenderSizeAndWorldScale(float DeltaSeconds)
{
	if (!WidgetComp || !WidgetInstance)
		return;

	// 1) BP가 결정한 “기준 픽셀 크기”
	const FVector2D Desired = WidgetInstance->GetDesiredSize();
	const float DesiredX = FMath::Max(1.f, Desired.X);
	const float DesiredY = FMath::Max(1.f, Desired.Y);

	// 2) 픽셀 밀도 업스케일(선명도 증가용)
	const float PD = FMath::Clamp(PixelDensityMultiplier, 1.f, 4.f);
	const FVector2D DrawSizeF(DesiredX * PD, DesiredY * PD);

	// 너무 큰 렌더타겟 방지 (필요 시 조정)
	const int32 DrawW = (int32)FMath::Clamp(DrawSizeF.X, 64.f, 4096.f);
	const int32 DrawH = (int32)FMath::Clamp(DrawSizeF.Y, 64.f, 4096.f);
	WidgetComp->SetDrawSize(FIntPoint(DrawW, DrawH));

	// 3) 월드에서 “TargetWorldWidthCm”로 보이도록 자동 스케일 계산
	// UWidgetComponent의 월드 폭 = DrawSize(px) * WorldScale  (대략적인 스케일링 모델)
	// 여기서는 "BP의 기준폭(DesiredX)"를 기준으로 물리폭을 맞추는 것이 목적이므로 DesiredX 사용.
	// PD는 DrawSize에만 적용되고 물리폭에는 영향 없게 하려면 PD는 여기서 제외.
	float WorldScaleFromWidth = TargetWorldWidthCm / DesiredX;

	// 4) 거리 보정(가독성): 멀수록 약간 커지게 (폭발 방지 clamp)
	float DistScale = 1.f;
	if (bScaleByDistance)
	{
		FVector CamLoc; FRotator CamRot;
		if (TryGetLocalPlayerView(CamLoc, CamRot))
		{
			const float Dist = FVector::Dist(CamLoc, GetActorLocation());
			const float Ref = FMath::Max(1.f, ReferenceDistanceCm);
			const float Ratio = FMath::Max(0.01f, Dist / Ref);
			const float Pow = FMath::Clamp(DistanceScalePower, 0.1f, 4.f);
			const float Mul = FMath::Pow(Ratio, Pow);
			DistScale = FMath::Clamp(Mul, MinDistanceScale, MaxDistanceScale);
		}
	}

	const float FinalScale = WorldScaleFromWidth * DistScale;

	const FVector TargetWorldScale(FinalScale);

	if (ScaleInterpSpeed <= 0.f)
	{
		SmoothedWorldScale = TargetWorldScale;
	}
	else
	{
		SmoothedWorldScale = FMath::VInterpTo(SmoothedWorldScale, TargetWorldScale, DeltaSeconds, ScaleInterpSpeed);
	}

	WidgetComp->SetWorldScale3D(SmoothedWorldScale);
}
