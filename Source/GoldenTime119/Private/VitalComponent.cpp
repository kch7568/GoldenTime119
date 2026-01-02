// ============================ VitalComponent.cpp ============================
// - Player vitals authority: HP / Temp / O2 (all normalized 0..1)
// - Temp rule: requires BOTH (Room heat) AND (Fire proximity)
// - Updates at fixed cadence (default 20Hz) and broadcasts only when meaningfully changed
//
// Dependencies:
// 1) ARoomActor must provide:
//    - FRoomEnvSnapshot ARoomActor::GetEnvSnapshot() const;
//    - float ARoomActor::GetNearestFireDistance(const FVector& WorldPos) const;   // you add this helper
//
// 2) VitalComponent.h is assumed to declare:
//    - UVitalComponent class
//    - OnVitals01Changed delegate (Hp01, Temp01, O201)
//    - SetCurrentRoom(ARoomActor*)
//    - GetHp01/GetTemp01/GetO201
//
// NOTE: Units: Unreal distance is cm.

#include "VitalComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

UVitalComponent::UVitalComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // ===== Defaults (safe starting state) =====
    Hp01 = 1.f;
    Temp01 = 0.f;
    O201 = 1.f;

    PrevHp01 = PrevTemp01 = PrevO201 = -1.f;

    // ===== Update cadence =====
    UpdateInterval = 0.05f; // 20Hz
    Acc = 0.f;

    // ===== Room->Vital tuning =====
    HeatDangerRef = 600.f;     // aligns with Room's ~650C upper target region
    O2ResponseSpeed = 2.0f;    // O2 follows room faster
    TempResponseSpeed = 1.2f;  // temp follows room a bit slower

    // ===== Damage thresholds (tune) =====
    SmokeSafe = 0.35f;
    HypoxiaSafe = 0.22f;
    TempSafe01 = 0.55f;

    // ===== DPS scalars (tune) =====
    SmokeDps = 0.10f;
    HypoxiaDps = 0.25f;
    HeatDps = 0.15f;

    // ===== O2 "breath penalty" by smoke =====
    SmokeBreathMul = 0.6f;

    // ===== Broadcast threshold =====
    Epsilon = 0.002f;

    // ===== Proximity heat tuning (cm) =====
    ProxInnerCm = 80.f;    // within this => max proximity heat
    ProxOuterCm = 300.f;   // beyond this => no proximity heat
    ProxPow = 1.8f;

    // ===== Temp combine tuning =====
    EnvPow = 1.15f;
    ProxEnvPow = 1.0f;
}

void UVitalComponent::BeginPlay()
{
    Super::BeginPlay();

    // Optional: immediate broadcast for UI sync on spawn
    BroadcastIfChanged(true);
}

void UVitalComponent::SetCurrentRoom(ARoomActor* InRoom)
{
    CurrentRoom = InRoom;

    // Optional: when room changes, force UI refresh next tick
    PrevHp01 = PrevTemp01 = PrevO201 = -1.f;
}

void UVitalComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // If paused / no world, skip.
    if (DeltaTime <= 0.f)
        return;

    Acc += DeltaTime;
    if (Acc < UpdateInterval)
        return;

    // Step as "accumulated time" to keep stable under variable FPS.
    const float Step = Acc;
    Acc = 0.f;

    StepVitals(Step);
    BroadcastIfChanged(false);
}

static FORCEINLINE float Clamp01(float X) { return FMath::Clamp(X, 0.f, 1.f); }

void UVitalComponent::StepVitals(float Dt)
{
    if (Dt <= 0.f)
        return;

    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
        return;

    if (!IsValid(CurrentRoom))
    {
        // No room -> relax to safe baseline slowly (optional behavior)
        const float RelaxSpeed = 0.6f;
        O201 = FMath::FInterpTo(O201, 1.f, Dt, RelaxSpeed);
        Temp01 = FMath::FInterpTo(Temp01, 0.f, Dt, RelaxSpeed);
        // HP does not auto-heal here (keep authority elsewhere if you want)
        O201 = Clamp01(O201);
        Temp01 = Clamp01(Temp01);
        return;
    }

    const FRoomEnvSnapshot S = CurrentRoom->GetEnvSnapshot();

    // -----------------------
    // 1) O2 Vital (room oxygen + smoke breathing penalty)
    // -----------------------
    const float Smoke = Clamp01(S.Smoke);
    const float RoomO2 = Clamp01(S.Oxygen);

    const float BreathPenalty = Clamp01(1.f - (SmokeBreathMul * FMath::Pow(Smoke, 1.2f)));
    const float O2Target = RoomO2 * BreathPenalty;

    O201 = FMath::FInterpTo(O201, O2Target, Dt, O2ResponseSpeed);
    O201 = Clamp01(O201);

    // -----------------------
    // 2) Temp Vital (requires BOTH env heat AND fire proximity)
    // -----------------------
    // 2-A) env heat 0..1 (Heat-based). If you prefer NP.UpperTempC, replace this mapping.
    const float EnvHeat01 = Clamp01(S.Heat / FMath::Max(1.f, HeatDangerRef));

    // 2-B) proximity heat 0..1 (nearest active fire distance)
    float ProxHeat01 = 0.f;
    const float Dist = CurrentRoom->GetNearestFireDistance(Owner->GetActorLocation());

    if (Dist < ProxOuterCm)
    {
        const float Den = FMath::Max(1.f, (ProxOuterCm - ProxInnerCm));
        const float T = 1.f - Clamp01((Dist - ProxInnerCm) / Den); // 1 near, 0 far
        ProxHeat01 = Clamp01(FMath::Pow(T, FMath::Max(0.1f, ProxPow)));
    }

    // 2-C) combine (AND-strong): if either is low, target stays low
    const float TempTarget01 =
        Clamp01(FMath::Pow(EnvHeat01, FMath::Max(0.1f, EnvPow)) *
            FMath::Pow(ProxHeat01, FMath::Max(0.1f, ProxEnvPow)));

    Temp01 = FMath::FInterpTo(Temp01, TempTarget01, Dt, TempResponseSpeed);
    Temp01 = Clamp01(Temp01);

    // -----------------------
    // 3) HP damage (smoke + hypoxia + heat)
    // -----------------------
    const float SmokeOver = FMath::Max(0.f, Smoke - SmokeSafe);
    const float HypoOver = FMath::Max(0.f, HypoxiaSafe - RoomO2);
    const float TempOver = FMath::Max(0.f, Temp01 - TempSafe01);

    const float SmokeDmg = SmokeDps * FMath::Pow(SmokeOver, 1.4f);
    const float HypoDmg = HypoxiaDps * FMath::Pow(HypoOver, 1.6f);
    const float HeatDmg = HeatDps * FMath::Pow(TempOver, 1.4f);

    const float TotalDmg = (SmokeDmg + HypoDmg + HeatDmg) * Dt;
    Hp01 = Clamp01(Hp01 - TotalDmg);
}

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
