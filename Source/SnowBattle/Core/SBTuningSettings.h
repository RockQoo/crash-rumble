#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SBTuningSettings.generated.h"

/** 옵션창 한 줄. 레지스트리(GetItems)에 등록된 항목만 옵션창/저장/콘솔에 노출된다. */
struct FSBTuningItem
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
 * 기본값은 여기 멤버 초기값이고, 옵션창에서 바꾼 값은 USBTuningSubsystem이 SaveGame으로 저장한다.
 * 새 수치 추가: 멤버 추가 → SBTuningSettings.cpp의 GetItems()에 SB_TUNE 한 줄.
 */
UCLASS()
class SNOWBATTLE_API USBTuningSettings : public UObject
{
	GENERATED_BODY()

public:
	static const TArray<FSBTuningItem>& GetItems();
	static const FSBTuningItem* FindItem(FName Name);

	// 옵션창
	UPROPERTY() bool bPauseOnOptions = false;

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
	UPROPERTY() bool bHitParticles = true;
	UPROPERTY() int32 HitParticleCount = 20;
	UPROPERTY() float HitParticleSpeed = 750.f;
	UPROPERTY() float HitParticleLifetime = 0.6f;
	UPROPERTY() float HitParticleSize = 0.22f;
	UPROPERTY() bool bParticleStretch = true;
	UPROPERTY() float ParticleStretch = 0.0015f;
	UPROPERTY() bool bParticleBounce = true;
	UPROPERTY() float ParticleBounce = 0.45f;
	UPROPERTY() bool bHitBurst = true;
	UPROPERTY() float HitBurstSize = 1.f;
	UPROPERTY() float HitBurstDuration = 0.09f;
	UPROPERTY() bool bShockwave = true;
	UPROPERTY() int32 ShockwaveCount = 16;
	UPROPERTY() float ShockwaveSpeed = 900.f;
	UPROPERTY() float ShockwaveSize = 0.12f;
	UPROPERTY() bool bKillExplosion = true;
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
