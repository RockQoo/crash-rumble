#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CRTuningSettings.generated.h"

/** 옵션창 한 줄. 레지스트리(GetItems)에 등록된 항목만 옵션창/저장/콘솔에 노출된다. */
struct FCRTuningItem
{
	FText Category;
	FText Label;
	FProperty* Property = nullptr; // float / int32 / bool
	float Min = 0.f;
	float Max = 1.f;
	float Step = 0.f;              // 0이면 스핀박스 기본 이동량

	FName GetName() const { return Property ? Property->GetFName() : NAME_None; }
	float GetValue(const void* Container) const;
	void SetValue(void* Container, float Value) const;
	bool IsBool() const;
};

/**
 * 인게임에서 조정 가능한 모든 튜닝 수치.
 * 기본값은 여기 멤버 초기값이고, 옵션창에서 바꾼 값은 UCRTuningSubsystem이 SaveGame으로 저장한다.
 * 새 수치 추가: 멤버 추가 → CRTuningSettings.cpp의 GetItems()에 CR_TUNE 한 줄.
 */
UCLASS()
class CRASHRUMBLE_API UCRTuningSettings : public UObject
{
	GENERATED_BODY()

public:
	static const TArray<FCRTuningItem>& GetItems();
	static const FCRTuningItem* FindItem(FName Name);

	// 옵션창
	UPROPERTY() bool bPauseOnOptions = false;

	// 툰 / 화면 (10단계)
	UPROPERTY() bool bToonShading = true;
	UPROPERTY() bool bAnimePostProcess = true;
	UPROPERTY() float ExposureBias = 0.f;
	UPROPERTY() bool bToonFloor = true;
	UPROPERTY() bool bBlobShadow = true;
	UPROPERTY() float BlobShadowStrength = 0.6f;
	UPROPERTY() float BlobShadowSize = 1.1f;

	// 2코마 (초당 그림 수, 0 = 매 프레임)
	UPROPERTY() int32 FxFps = 12;
	UPROPERTY() int32 CharFps = 0;

	// 숨쉬기 (Idle)
	UPROPERTY() bool bBreathing = true;
	UPROPERTY() float BreathAmplitude = 0.045f;
	UPROPERTY() float BreathSpeed = 1.1f;
	UPROPERTY() float BreathHeadLag = 0.6f;
	UPROPERTY() float BreathHatLag = 0.5f;
	UPROPERTY() float BreathMoveBounce = 6.f;

	// 이동
	UPROPERTY() float MoveSpeed = 600.f;
	UPROPERTY() float MaxAcceleration = 4096.f;
	UPROPERTY() float TurnSpeed = 14.f;

	// 카메라 - 공통
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() bool bInvertMouseY = false;
	UPROPERTY() float ViewBlendSpeed = 6.f;

	// 카메라 - 숄더뷰
	UPROPERTY() float ShoulderArmLength = 320.f;
	UPROPERTY() float ShoulderOffsetY = 70.f;
	UPROPERTY() float ShoulderOffsetZ = 70.f;
	UPROPERTY() float ShoulderFOV = 90.f;

	// 카메라 - 탑다운
	UPROPERTY() float TopDownArmLength = 1200.f;
	UPROPERTY() float TopDownPitch = 60.f;
	UPROPERTY() float TopDownFOV = 70.f;

	// 카메라 - 자유 카메라 (맵 제작 모드)
	UPROPERTY() float FreeCameraSpeed = 1200.f;
	UPROPERTY() float FreeCameraFastMultiplier = 3.f;

	// 손
	UPROPERTY() float HandBobAmplitude = 4.f;
	UPROPERTY() float HandBobSpeed = 2.f;

	// 전투 공통
	UPROPERTY() float MaxHealth = 1000.f;
	UPROPERTY() float RespawnDelay = 3.f;

	// 칼
	UPROPERTY() float SwordDamage = 25.f;
	UPROPERTY() float SwordInterval = 0.45f;
	UPROPERTY() float SwordSwingDuration = 0.18f;
	UPROPERTY() float SwordArcAngle = 150.f;
	UPROPERTY() float SwordLength = 90.f;
	UPROPERTY() float SwordHitRadius = 14.f;
	UPROPERTY() float SwordKnockback = 700.f;
	UPROPERTY() float SwordHitStop = 0.09f;
	UPROPERTY() float SwordShake = 1.f;

	// 레이저총
	UPROPERTY() int32 LaserPierce = 0;
	UPROPERTY() float LaserDamage = 10.f;
	UPROPERTY() float LaserInterval = 0.08f;
	UPROPERTY() float LaserRange = 5000.f;
	UPROPERTY() float LaserBeamDuration = 0.05f;
	UPROPERTY() float LaserBeamWidth = 4.f;
	UPROPERTY() float LaserKnockback = 60.f;
	UPROPERTY() float LaserHitStop = 0.f;
	UPROPERTY() float LaserShake = 0.15f;

	// 투사체총
	UPROPERTY() int32 ProjectilePierce = 0;
	UPROPERTY() float ProjectileDamage = 20.f;
	UPROPERTY() float ProjectileInterval = 0.25f;
	UPROPERTY() float ProjectileSpeed = 3000.f;
	UPROPERTY() float ProjectileGravity = 0.f;
	UPROPERTY() float ProjectileLifetime = 3.f;
	UPROPERTY() float ProjectileSize = 1.f;
	UPROPERTY() float ProjectileKnockback = 350.f;
	UPROPERTY() float ProjectileHitStop = 0.04f;
	UPROPERTY() float ProjectileShake = 0.5f;

	// 타격감 - 피격
	UPROPERTY() bool bHitFlash = true;
	UPROPERTY() float HitFlashDuration = 0.12f;
	UPROPERTY() float HitFlashIntensity = 6.f;
	UPROPERTY() bool bDamageNumbers = true;
	UPROPERTY() float DamageNumberDuration = 0.8f;
	UPROPERTY() float DamageNumberRise = 80.f;
	UPROPERTY() float DamageNumberScale = 1.6f;
	UPROPERTY() bool bHitStop = true;
	UPROPERTY() bool bHitStopGlobal = false;
	UPROPERTY() float HitStopTimeScale = 0.02f;
	UPROPERTY() bool bKnockback = true;
	UPROPERTY() float KnockbackUpRatio = 0.25f;
	UPROPERTY() bool bCameraShake = true;
	UPROPERTY() float CameraShakeIntensity = 1.5f;
	UPROPERTY() float CameraShakeDuration = 0.2f;
	UPROPERTY() float CameraShakeFrequency = 25.f;
	UPROPERTY() bool bHitParticles = false;
	UPROPERTY() int32 HitParticleCount = 20;
	UPROPERTY() float HitParticleSpeed = 750.f;
	UPROPERTY() float HitParticleLifetime = 0.6f;
	UPROPERTY() float HitParticleSize = 0.22f;
	UPROPERTY() bool bParticleStretch = true;
	UPROPERTY() float ParticleStretch = 0.0015f;
	UPROPERTY() bool bParticleBounce = true;
	UPROPERTY() float ParticleBounce = 0.45f;
	UPROPERTY() bool bHitBurst = false;
	UPROPERTY() float HitBurstSize = 1.f;
	UPROPERTY() float HitBurstDuration = 0.09f;
	UPROPERTY() bool bShockwave = false;
	UPROPERTY() int32 ShockwaveCount = 16;
	UPROPERTY() float ShockwaveSpeed = 900.f;
	UPROPERTY() float ShockwaveSize = 0.12f;
	UPROPERTY() bool bKillExplosion = false;
	UPROPERTY() int32 KillExplosionCount = 28;
	UPROPERTY() float KillExplosionSpeed = 1.3f;
	UPROPERTY() float KillExplosionSize = 0.9f;

	// 사망 연출: 몸은 뒤로 넘어지고, 투구(모자)와 무기가 벗겨져 튀어 나간다
	UPROPERTY() float DeathKnockbackScale = 1.5f;
	UPROPERTY() bool bDeathPopOff = true;
	UPROPERTY() float DeathPopSpeed = 450.f;
	UPROPERTY() float DeathPopUp = 750.f;
	UPROPERTY() float DeathSpin = 900.f;
	UPROPERTY() float DeathGravity = 2200.f;
	UPROPERTY() float DeathBounce = 0.4f;

	// 색종이 (처치 시)
	UPROPERTY() bool bConfetti = true;
	UPROPERTY() int32 ConfettiCount = 40;
	UPROPERTY() float ConfettiSize = 0.16f;
	UPROPERTY() float ConfettiSpeed = 650.f;
	UPROPERTY() float ConfettiLifetime = 1.6f;
	UPROPERTY() float ConfettiGravity = 350.f;
	UPROPERTY() float ConfettiDrag = 2.5f;
	UPROPERTY() float ConfettiFlutter = 120.f;
	UPROPERTY() float ConfettiSpin = 720.f;

	// 애니 연출 (12단계)
	UPROPERTY() bool bCubeDebris = true;
	UPROPERTY() float DebrisCountScale = 1.f;
	UPROPERTY() float DebrisSize = 1.f;
	UPROPERTY() float DebrisGravity = 1400.f;
	UPROPERTY() bool bSphereExplosion = true;
	UPROPERTY() float ExplosionRadius = 70.f;
	UPROPERTY() bool bHitStar = true;
	UPROPERTY() float HitStarSize = 1.f;
	UPROPERTY() bool bSparkle = true;
	UPROPERTY() bool bImpactFrame = true;
	UPROPERTY() float ImpactFrameDuration = 0.045f;
	UPROPERTY() float ImpactFrameCooldown = 0.4f;
	UPROPERTY() bool bImpactOnPlayerKill = false;
	UPROPERTY() bool bSmear = true;
	UPROPERTY() float SmearMinSpeed = 400.f;
	UPROPERTY() bool bKanada = true;
	UPROPERTY() float KanadaSize = 1.f;
	UPROPERTY() bool bDustRing = true;
	UPROPERTY() float DustRingMinSpeed = 450.f;
	UPROPERTY() bool bFocusLines = true;
	UPROPERTY() float SprintFocus = 0.35f;

	// 타격감 - 사격 반동
	UPROPERTY() bool bWeaponRecoil = true;
	UPROPERTY() float RecoilKickback = 12.f;
	UPROPERTY() float RecoilPitch = 12.f;
	UPROPERTY() float RecoilStiffness = 250.f;
	UPROPERTY() float RecoilDamping = 18.f;
	UPROPERTY() float RecoilJitter = 1.5f;
	UPROPERTY() bool bCameraRecoil = true;
	UPROPERTY() float CameraRecoilPitch = 0.6f;
	UPROPERTY() float CameraRecoilRecover = 8.f;
	UPROPERTY() bool bMuzzleFlash = true;
	UPROPERTY() float MuzzleFlashDuration = 0.04f;
	UPROPERTY() float MuzzleLightIntensity = 8000.f;

	// 번개 (13단계)
	UPROPERTY() float LightningDamage = 30.f;
	UPROPERTY() float LightningInterval = 0.35f;
	UPROPERTY() float LightningRange = 2500.f;
	UPROPERTY() float LightningKnockback = 250.f;
	UPROPERTY() float LightningHitStop = 0.05f;
	UPROPERTY() float LightningShake = 0.6f;

	// 미사일 (이타노 서커스)
	UPROPERTY() float MissileDamage = 40.f;
	UPROPERTY() float MissileInterval = 1.2f;
	UPROPERTY() int32 MissileCount = 6;
	UPROPERTY() float MissileRadius = 170.f;
	UPROPERTY() float MissileStartSpeed = 350.f;
	UPROPERTY() float MissileAcceleration = 1600.f;
	UPROPERTY() float MissileMaxSpeed = 1700.f;
	UPROPERTY() float MissileTurnStart = 30.f;
	UPROPERTY() float MissileTurnEnd = 600.f;
	UPROPERTY() float MissileTurnRampTime = 1.f;
	UPROPERTY() float MissileKnockback = 700.f;
	UPROPERTY() float MissileHitStop = 0.06f;
	UPROPERTY() float MissileShake = 1.5f;

	// 폴리곤 불꽃
	UPROPERTY() float FlameDamage = 15.f;
	UPROPERTY() float FlameInterval = 0.6f;
	UPROPERTY() float FlameRadius = 90.f;
	UPROPERTY() float FlameHeight = 220.f;
	UPROPERTY() float FlameDuration = 1.1f;
	UPROPERTY() float FlameRange = 1500.f;
	UPROPERTY() float FlameKnockback = 200.f;

	// 펀치 (우클릭)
	UPROPERTY() float PunchDamage = 35.f;
	UPROPERTY() float PunchInterval = 0.35f;
	UPROPERTY() float PunchReach = 75.f;
	UPROPERTY() float PunchRadius = 45.f;
	UPROPERTY() float PunchKnockback = 900.f;
	UPROPERTY() float PunchHitStop = 0.07f;
	UPROPERTY() float PunchShake = 1.2f;

	// 필살기 (F)
	UPROPERTY() float SuperFreezeDuration = 0.8f;
	UPROPERTY() int32 SuperMissileCount = 24;
	UPROPERTY() float SuperCooldown = 3.f;
	UPROPERTY() float SuperZoom = 0.8f;

	// 달리기 (Shift)
	UPROPERTY() float SprintMultiplier = 1.6f;

	// 다수 타격 테스트
	UPROPERTY() float MonsterMaxHealth = 100.f;
	UPROPERTY() float MonsterMoveSpeed = 100.f;
	UPROPERTY() float SpawnRadius = 1500.f;

	// 팀 전투 (레드 vs 블루)
	UPROPERTY() float BattleMoveSpeed = 350.f;
	UPROPERTY() float BattleAttackRange = 150.f;
	UPROPERTY() float BattleGunRange = 800.f;
	UPROPERTY() int32 BattleSwordRatio = 4;
	UPROPERTY() int32 BattleLaserRatio = 3;
	UPROPERTY() int32 BattleProjectileRatio = 3;
	UPROPERTY() float BattleTeamDistance = 700.f;
	UPROPERTY() float BattleSpread = 350.f;
	UPROPERTY() bool bBattleSpectate = true;
};
