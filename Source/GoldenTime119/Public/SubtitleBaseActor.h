#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubtitleBaseActor.generated.h"

class UWidgetComponent;
class UUserWidget;
class UBorder;
class UTextBlock;

UCLASS(Abstract)
class GOLDENTIME119_API ASubtitleBaseActor : public AActor
{
	GENERATED_BODY()

public:
	ASubtitleBaseActor();

	// ===== Display API =====
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ShowSubtitle(const FText& Subtitle, float MinHoldSeconds = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ShowSubtitlePages(const TArray<FText>& Pages);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void HideNow();

	UFUNCTION()
	void HandleTutorialSubtitle(const FText& Subtitle, float MinHoldSeconds);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void StopPages();

	// ===== Widget =====
	UPROPERTY(EditAnywhere, Category = "Subtitle|Widget")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Subtitle|Widget")
	TObjectPtr<UWidgetComponent> WidgetComp;

	// ===== Timing =====
	UPROPERTY(EditAnywhere, Category = "Subtitle|Timing", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FadeInSeconds = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Subtitle|Timing", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FadeOutSeconds = 0.15f;

	/** 페이지 간격(초): FadeIn+Hold+FadeOut 포함 전체 주기 */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Timing", meta = (ClampMin = "0.2", ClampMax = "20.0"))
	float PageIntervalSeconds = 2.8f;

	UPROPERTY(EditAnywhere, Category = "Subtitle|Timing", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float ExtraHoldOnLastPageSeconds = 0.6f;

	// ===== Scale / Quality (핵심) =====

	/** BP에서 정한 위젯 폭(px)이 무엇이든, 월드에서 이 폭(cm)로 보이게 자동 맞춤 */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale", meta = (ClampMin = "1.0", ClampMax = "500.0"))
	float TargetWorldWidthCm = 60.f;

	/** 픽셀 밀도(선명도) 업스케일: 1=기본, 2=2배 해상도 */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float PixelDensityMultiplier = 2.0f;

	/** 거리별 스케일 보정(가독성) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale")
	bool bScaleByDistance = true;

	/** 이 거리에서 DistanceScale=1 (cm) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale", meta = (ClampMin = "10.0", ClampMax = "5000.0", EditCondition = "bScaleByDistance"))
	float ReferenceDistanceCm = 150.f;

	/** (distance/refDistance)^power */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale", meta = (ClampMin = "0.1", ClampMax = "4.0", EditCondition = "bScaleByDistance"))
	float DistanceScalePower = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale", meta = (ClampMin = "0.1", ClampMax = "10.0", EditCondition = "bScaleByDistance"))
	float MinDistanceScale = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale", meta = (ClampMin = "0.1", ClampMax = "10.0", EditCondition = "bScaleByDistance"))
	float MaxDistanceScale = 1.1f;

	/** 스케일 변화 보간 속도(0=즉시) */
	UPROPERTY(EditAnywhere, Category = "Subtitle|Scale", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float ScaleInterpSpeed = 12.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 파생 클래스에서 월드 앵커/팔로우 등 배치 업데이트 */
	virtual void UpdatePlacement(float DeltaSeconds) {}

private:
	// ===== Widget refs =====
	UPROPERTY()
	TObjectPtr<UUserWidget> WidgetInstance;

	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> SubtitleText;

	// ===== Paging state =====
	TArray<FText> CurrentPages;
	int32 CurrentPageIndex = 0;
	FTimerHandle PageTimer;

	// ===== Fade state =====
	enum class ESubtitleFadeState : uint8 { Hidden, FadingIn, Holding, FadingOut };
	ESubtitleFadeState FadeState = ESubtitleFadeState::Hidden;

	float FadeElapsed = 0.f;
	float ActiveFadeIn = 0.15f;
	float ActiveHold = 0.f;
	float ActiveFadeOut = 0.15f;

	// ===== Scale smoothing =====
	FVector SmoothedWorldScale = FVector::OneVector;

	// ===== Internals =====
	void EnsureWidgetReady();
	void CacheWidgetRefs();

	void ApplyOpacity(float Opacity01);
	void SetSubtitleText(const FText& Text);
	void ResetFadeState();

	float CalcVisibleHoldBase() const;
	void StartFadeSequence(const FText& Text, float InFadeIn, float InHold, float InFadeOut);

	void ShowPage(int32 PageIndex, float HoldSeconds);
	void ScheduleNextPage(float InDelaySeconds);
	void AdvancePage();

	// 핵심: BP DesiredSize 기반 DrawSize 업스케일 + 월드 크기 자동 맞춤 + 거리 보정
	void UpdateRenderSizeAndWorldScale(float DeltaSeconds);

	// local player view (PIE 크래시 방지)
	bool TryGetLocalPlayerView(FVector& OutCamLoc, FRotator& OutCamRot) const;
};
