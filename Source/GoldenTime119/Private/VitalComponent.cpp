// ============================ VitalComponent.cpp ============================
#include "VitalComponent.h"

// FRoomEnvSnapshot 사용 위해 cpp에서 include
#include "RoomActor.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "InputCoreTypes.h" // EKeys

static FORCEINLINE float Clamp01(float X) { return FMath::Clamp(X, 0.f, 1.f); }

// ---------- ctor ----------
UVitalComponent::UVitalComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // defaults
    Hp01 = 1.f;
    Temp01 = 0.f;
    O201 = 1.f;

    PrevHp01 = PrevTemp01 = PrevO201 = -1.f;

    UpdateInterval = 0.05f;
    Acc = 0.f;

    HeatDangerRef = 600.f;
    O2ResponseSpeed = 2.0f;
    TempResponseSpeed = 1.2f;

    SmokeSafe = 0.35f;
    HypoxiaSafe = 0.22f;
    TempSafe01 = 0.55f;

    SmokeDps = 0.10f;
    HypoxiaDps = 0.25f;
    HeatDps = 0.15f;

    SmokeBreathMul = 0.6f;

    Epsilon = 0.002f;

    ProxInnerCm = 80.f;
    ProxOuterCm = 300.f;
    ProxPow = 1.8f;

    EnvPow = 1.15f;
    ProxEnvPow = 1.0f;

    // debug
    bDebugOverrideVitals = false;
    bEnableDebugHotkeys = true;
    DebugStepPerSecond = 0.15f;

    DebugHp01 = Hp01;
    DebugTemp01 = Temp01;
    DebugO201 = O201;
}

// ---------- BeginPlay ----------
void UVitalComponent::BeginPlay()
{
    Super::BeginPlay();

    // 1. 카메라 컴포넌트 찾기
    if (AActor* Owner = GetOwner())
    {
        PlayerCamera = Owner->FindComponentByClass<UCameraComponent>();
    }

    // 2. 동적 머티리얼 인스턴스 생성 및 카메라에 적용
    if (SmokePostProcessMaterial && PlayerCamera)
    {
        SmokeMID = UMaterialInstanceDynamic::Create(SmokePostProcessMaterial, this);

        // 카메라의 포스트 프로세스 설정에 머티리얼 추가
        FWeightedBlendable SmokeBlendable;
        SmokeBlendable.Weight = 1.0f;
        SmokeBlendable.Object = SmokeMID;

        PlayerCamera->PostProcessSettings.WeightedBlendables.Array.Add(SmokeBlendable);
    }

    BroadcastIfChanged(true);
}

// ---------- Room binding ----------
void UVitalComponent::SetCurrentRoom(ARoomActor* InRoom)
{
    CurrentRoom = InRoom;
    PrevHp01 = PrevTemp01 = PrevO201 = -1.f;

    UE_LOG(LogTemp, Warning, TEXT("[Vital] SetCurrentRoom -> %s"), *GetNameSafe(InRoom));

    // 방 들어갈 때 즉시 UI 갱신 보고 싶으면 강제 브로드캐스트 1회
    BroadcastIfChanged(true);
}


// ---------- Tick ----------
void UVitalComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (DeltaTime <= 0.f)
        return;

    Acc += DeltaTime;
    if (Acc < UpdateInterval)
        return;

    const float Step = Acc;
    Acc = 0.f;

    // 1) 디버그 핫키: (가장 우선)
    //    - 눌리는 동안 계속 증감
    //    - 이때는 room/화재 영향 계산을 스킵하고 “수동 값”이 우선
    if (bEnableDebugHotkeys)
    {
        const bool bUsedHotkey = ProcessDebugHotkeys(Step);
        if (bUsedHotkey)
            return; // 이미 브로드캐스트까지 했음
    }

    // 2) Details 디버그 override 모드
    if (bDebugOverrideVitals)
    {
        ApplyDebugToLive(false);
        return;
    }

    // 시야 효과는 부드러워야 하므로 UpdateInterval과 별개로 매 프레임 Tick에서 실행 추천
    UpdateSmokeVisualEffect(DeltaTime);

    // 3) 정상 갱신
    StepVitals(Step);
    BroadcastIfChanged(false);
}
void UVitalComponent::UpdateSmokeVisualEffect(float DeltaTime)
{
    // 필수 데이터가 없거나 방 밖에 있으면 효과를 즉시 끕니다.
    if (!SmokeMID || !PlayerCamera || !CurrentRoom)
    {
        if (SmokeMID) SmokeMID->SetScalarParameterValue(SmokeAlphaParamName, 0.f);
        return;
    }

    const float EyeZ = PlayerCamera->GetComponentLocation().Z;
    const float NPZ = CurrentRoom->GetNeutralPlaneZ();

    float TargetAlpha = 0.f;

    // 플레이어의 눈이 중성대(연기 경계선)보다 높을 때만 효과 적용
    if (EyeZ > NPZ)
    {
        // 연기 속에 얼마나 깊게 들어갔는가? (가시거리 60cm 설정)
        // 60cm 이상 깊이 들어가면 앞이 아예 안 보임
        const float MaxBlindDepth = 60.0f;

        float Depth = EyeZ - NPZ;
        TargetAlpha = FMath::Clamp(Depth / MaxBlindDepth, 0.f, 1.f);

        // 지수 보정: 연기 층에 살짝 발만 들였을 때는 덜 흐리게
        TargetAlpha = FMath::Pow(TargetAlpha, 1.5f);

        TargetAlpha *= 0.5f;
    }

    // 실제 머티리얼 파라미터 업데이트
    SmokeMID->SetScalarParameterValue(SmokeAlphaParamName, TargetAlpha);

    // 디버그 메시지 (수치 확인용)
    if (GEngine)
    {
        FString Msg = FString::Printf(TEXT("Smoke Alpha: %.2f | Depth: %.1f cm"), TargetAlpha, (EyeZ - NPZ));
        GEngine->AddOnScreenDebugMessage(1, DeltaTime, FColor::Cyan, Msg);
    }
}
// ---------- Debug hotkeys (1/2/3 down, 7/8/9 up) ----------
bool UVitalComponent::ProcessDebugHotkeys(float Dt)
{
    UWorld* World = GetWorld();
    if (!World)
        return false;

    APlayerController* PC = World->GetFirstPlayerController();
    if (!IsValid(PC))
        return false;

    // 키가 하나라도 눌리면 debug override를 켜고, 그 값으로 움직이게
    bool bAnyPressed = false;

    const float Step = DebugStepPerSecond * Dt;

    auto IsDown = [&](const FKey& K) { return PC->IsInputKeyDown(K); };

    // 감소
    if (IsDown(EKeys::One)) { DebugHp01 = Clamp01(DebugHp01 - Step); bAnyPressed = true; }
    if (IsDown(EKeys::Two)) { DebugTemp01 = Clamp01(DebugTemp01 - Step); bAnyPressed = true; }
    if (IsDown(EKeys::Three)) { DebugO201 = Clamp01(DebugO201 - Step); bAnyPressed = true; }

    // 증가
    if (IsDown(EKeys::Seven)) { DebugHp01 = Clamp01(DebugHp01 + Step); bAnyPressed = true; }
    if (IsDown(EKeys::Eight)) { DebugTemp01 = Clamp01(DebugTemp01 + Step); bAnyPressed = true; }
    if (IsDown(EKeys::Nine)) { DebugO201 = Clamp01(DebugO201 + Step); bAnyPressed = true; }

    if (!bAnyPressed)
        return false;

    // “핫키 입력이 있었다” -> 즉시 적용 + 즉시 브로드캐스트
    bDebugOverrideVitals = true;
    ApplyDebugToLive(false);     // ApplyDebugToLive 내부에서 BroadcastIfChanged 호출
    return true;
}

// ---------- Apply debug values to live + broadcast gating ----------
void UVitalComponent::ApplyDebugToLive(bool bForceBroadcast)
{
    Hp01 = Clamp01(DebugHp01);
    Temp01 = Clamp01(DebugTemp01);
    O201 = Clamp01(DebugO201);

    if (bForceBroadcast)
    {
        PrevHp01 = PrevTemp01 = PrevO201 = -1.f;
        BroadcastIfChanged(true);
    }
    else
    {
        BroadcastIfChanged(false);
    }
}

// ---------- Core step ----------
void UVitalComponent::StepVitals(float Dt)
{
    if (Dt <= 0.f)
        return;

    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
        return;

    ARoomActor* Room = CurrentRoom.Get();
    if (!IsValid(Room))
    {
        // No room -> relax (optional)
        const float RelaxSpeed = 0.6f;
        O201 = FMath::FInterpTo(O201, 1.f, Dt, RelaxSpeed);
        Temp01 = FMath::FInterpTo(Temp01, 0.f, Dt, RelaxSpeed);
        O201 = Clamp01(O201);
        Temp01 = Clamp01(Temp01);
        return;
    }

    const FRoomEnvSnapshot S = Room->GetEnvSnapshot();

    // 1) O2
    const float Smoke = Clamp01(S.Smoke);
    const float RoomO2 = Clamp01(S.Oxygen);

    const float BreathPenalty = Clamp01(1.f - (SmokeBreathMul * FMath::Pow(Smoke, 1.2f)));
    const float O2Target = RoomO2 * BreathPenalty;

    O201 = FMath::FInterpTo(O201, O2Target, Dt, O2ResponseSpeed);
    O201 = Clamp01(O201);

    // 2) Temp: EnvHeat AND Proximity
    const float EnvHeat01 = Clamp01(S.Heat / FMath::Max(1.f, HeatDangerRef));

    float ProxHeat01 = 0.f;
    const float Dist = Room->GetNearestFireDistance(Owner->GetActorLocation());

    if (Dist < ProxOuterCm)
    {
        const float Den = FMath::Max(1.f, (ProxOuterCm - ProxInnerCm));
        const float T = 1.f - Clamp01((Dist - ProxInnerCm) / Den);
        ProxHeat01 = Clamp01(FMath::Pow(T, FMath::Max(0.1f, ProxPow)));
    }

    const float TempTarget01 =
        Clamp01(
            FMath::Pow(EnvHeat01, FMath::Max(0.1f, EnvPow)) *
            FMath::Pow(ProxHeat01, FMath::Max(0.1f, ProxEnvPow))
        );

    Temp01 = FMath::FInterpTo(Temp01, TempTarget01, Dt, TempResponseSpeed);
    Temp01 = Clamp01(Temp01);

    // 3) HP damage
    const float SmokeOver = FMath::Max(0.f, Smoke - SmokeSafe);
    const float HypoOver = FMath::Max(0.f, HypoxiaSafe - RoomO2);
    const float TempOver = FMath::Max(0.f, Temp01 - TempSafe01);

    const float SmokeDmg = SmokeDps * FMath::Pow(SmokeOver, 1.4f);
    const float HypoDmg = HypoxiaDps * FMath::Pow(HypoOver, 1.6f);
    const float HeatDmg = HeatDps * FMath::Pow(TempOver, 1.4f);

    const float TotalDmg = (SmokeDmg + HypoDmg + HeatDmg) * Dt;
    Hp01 = Clamp01(Hp01 - TotalDmg);
}

// ---------- Broadcast gating ----------
void UVitalComponent::BroadcastIfChanged(bool bForce)
{
    const bool bChanged =
        bForce ||
        (FMath::Abs(Hp01 - PrevHp01) > Epsilon) ||
        (FMath::Abs(Temp01 - PrevTemp01) > Epsilon) ||
        (FMath::Abs(O201 - PrevO201) > Epsilon);

    if (!bChanged)
        return;

    PrevHp01 = Hp01;
    PrevTemp01 = Temp01;
    PrevO201 = O201;

    OnVitals01Changed.Broadcast(Hp01, Temp01, O201);
}

// ===== Debug API (BlueprintCallable) =====
void UVitalComponent::DebugSetVitals01(float InHp01, float InTemp01, float InO201, bool bForceBroadcast)
{
    DebugHp01 = Clamp01(InHp01);
    DebugTemp01 = Clamp01(InTemp01);
    DebugO201 = Clamp01(InO201);

    bDebugOverrideVitals = true;
    ApplyDebugToLive(bForceBroadcast);
}

void UVitalComponent::DebugApplyOverride(bool bForceBroadcast)
{
    bDebugOverrideVitals = true;
    ApplyDebugToLive(bForceBroadcast);
}

void UVitalComponent::DebugClearOverride(bool bForceBroadcast)
{
    bDebugOverrideVitals = false;

    if (bForceBroadcast)
    {
        PrevHp01 = PrevTemp01 = PrevO201 = -1.f;
        BroadcastIfChanged(true);
    }
}
