#include "Core/CRTuningSettings.h"

float FCRTuningItem::GetValue(const void* Container) const
{
	if (const FFloatProperty* P = CastField<FFloatProperty>(Property))
	{
		return P->GetPropertyValue_InContainer(Container);
	}
	if (const FIntProperty* P = CastField<FIntProperty>(Property))
	{
		return static_cast<float>(P->GetPropertyValue_InContainer(Container));
	}
	if (const FBoolProperty* P = CastField<FBoolProperty>(Property))
	{
		return P->GetPropertyValue_InContainer(Container) ? 1.f : 0.f;
	}
	return 0.f;
}

void FCRTuningItem::SetValue(void* Container, float Value) const
{
	if (!IsBool())
	{
		Value = FMath::Clamp(Value, Min, Max);
	}
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		FloatProp->SetPropertyValue_InContainer(Container, Value);
	}
	else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		IntProp->SetPropertyValue_InContainer(Container, FMath::RoundToInt(Value));
	}
	else if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		BoolProp->SetPropertyValue_InContainer(Container, Value > 0.5f);
	}
}

bool FCRTuningItem::IsBool() const
{
	return Property && Property->IsA<FBoolProperty>();
}

#define CR_TUNE(Cat, Member, LabelText, InMin, InMax, InStep) \
	Items.Add({ FText::FromString(TEXT(Cat)), FText::FromString(TEXT(LabelText)), \
		FindFProperty<FProperty>(UCRTuningSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(UCRTuningSettings, Member)), \
		InMin, InMax, InStep })

#define CR_TUNE_BOOL(Cat, Member, LabelText) CR_TUNE(Cat, Member, LabelText, 0.f, 1.f, 1.f)

const TArray<FCRTuningItem>& UCRTuningSettings::GetItems()
{
	static TArray<FCRTuningItem> Items;
	if (Items.Num() == 0)
	{
		CR_TUNE_BOOL("옵션창", bPauseOnOptions, "옵션창 열면 일시정지");

		CR_TUNE_BOOL("툰 / 화면", bToonShading, "툰 셰이딩 (끄면 예전 조명)");
		CR_TUNE_BOOL("툰 / 화면", bToonFloor, "바닥도 툰 (체크무늬)");
		CR_TUNE_BOOL("툰 / 화면", bAnimePostProcess, "포스트 프로세스 (노출 고정, 비네트·모션 블러 끔)");
		CR_TUNE("툰 / 화면", ExposureBias, "노출 보정", -4.f, 4.f, 0.1f);
		CR_TUNE_BOOL("툰 / 화면", bBlobShadow, "블롭 그림자");
		CR_TUNE("툰 / 화면", BlobShadowStrength, "블롭 그림자 진하기", 0.f, 1.f, 0.05f);
		CR_TUNE("툰 / 화면", BlobShadowSize, "블롭 그림자 크기", 0.3f, 3.f, 0.05f);

		CR_TUNE("2코마", FxFps, "이펙트 초당 그림 (0 = 매끄럽게, 12 = 2코마, 8 = 3코마) · T", 0.f, 60.f, 1.f);
		CR_TUNE("2코마", CharFps, "캐릭터 초당 그림 (0 = 매끄럽게) · Y", 0.f, 60.f, 1.f);

		CR_TUNE_BOOL("숨쉬기", bBreathing, "켜기");
		CR_TUNE("숨쉬기", BreathAmplitude, "몸 늘어남 비율", 0.f, 0.2f, 0.005f);
		CR_TUNE("숨쉬기", BreathSpeed, "초당 숨 횟수", 0.1f, 4.f, 0.05f);
		CR_TUNE("숨쉬기", BreathHeadLag, "머리 늦게 따라옴(라디안)", 0.f, 2.f, 0.05f);
		CR_TUNE("숨쉬기", BreathHatLag, "모자 더 늦게(라디안)", 0.f, 2.f, 0.05f);
		CR_TUNE("숨쉬기", BreathMoveBounce, "걸을 때 통통 튐(cm)", 0.f, 30.f, 0.5f);

		CR_TUNE("이동", MoveSpeed, "이동 속도", 100.f, 2000.f, 10.f);
		CR_TUNE("이동", MaxAcceleration, "가속도", 200.f, 20000.f, 100.f);
		CR_TUNE("이동", TurnSpeed, "회전 속도", 1.f, 40.f, 0.5f);

		CR_TUNE("카메라", MouseSensitivity, "마우스 감도", 0.1f, 5.f, 0.05f);
		CR_TUNE_BOOL("카메라", bInvertMouseY, "마우스 Y 반전");
		CR_TUNE("카메라", ViewBlendSpeed, "시점 전환 속도", 1.f, 30.f, 0.5f);
		CR_TUNE("카메라", ShoulderArmLength, "숄더뷰 거리", 100.f, 1000.f, 10.f);
		CR_TUNE("카메라", ShoulderOffsetY, "숄더뷰 좌우 오프셋", -200.f, 200.f, 5.f);
		CR_TUNE("카메라", ShoulderOffsetZ, "숄더뷰 높이 오프셋", 0.f, 300.f, 5.f);
		CR_TUNE("카메라", ShoulderFOV, "숄더뷰 FOV", 50.f, 120.f, 1.f);
		CR_TUNE("카메라", TopDownArmLength, "탑다운 거리", 400.f, 4000.f, 50.f);
		CR_TUNE("카메라", TopDownPitch, "탑다운 각도", 30.f, 89.f, 1.f);
		CR_TUNE("카메라", TopDownFOV, "탑다운 FOV", 30.f, 120.f, 1.f);
		CR_TUNE("카메라", FreeCameraSpeed, "자유 카메라 속도 (맵 제작)", 100.f, 6000.f, 50.f);
		CR_TUNE("카메라", FreeCameraFastMultiplier, "자유 카메라 Shift 배속", 1.f, 10.f, 0.5f);

		CR_TUNE("손", HandBobAmplitude, "손 흔들림 크기", 0.f, 30.f, 0.5f);
		CR_TUNE("손", HandBobSpeed, "손 흔들림 속도", 0.f, 10.f, 0.1f);

		CR_TUNE("전투", MaxHealth, "최대 체력", 1.f, 5000.f, 10.f);
		CR_TUNE("전투", RespawnDelay, "쓰러진 뒤 부활까지(초)", 0.f, 10.f, 0.1f);

		CR_TUNE("칼", SwordDamage, "데미지", 0.f, 200.f, 1.f);
		CR_TUNE("칼", SwordInterval, "공격 간격(초)", 0.05f, 2.f, 0.01f);
		CR_TUNE("칼", SwordSwingDuration, "휘두르는 시간(초)", 0.05f, 1.f, 0.01f);
		CR_TUNE("칼", SwordArcAngle, "휘두르는 각도", 30.f, 300.f, 5.f);
		CR_TUNE("칼", SwordLength, "칼 길이", 30.f, 300.f, 5.f);
		CR_TUNE("칼", SwordHitRadius, "판정 두께", 2.f, 60.f, 1.f);
		CR_TUNE("칼", SwordKnockback, "넉백 세기", 0.f, 3000.f, 10.f);
		CR_TUNE("칼", SwordHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		CR_TUNE("칼", SwordShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		CR_TUNE("레이저총", LaserDamage, "데미지", 0.f, 200.f, 1.f);
		CR_TUNE("레이저총", LaserPierce, "관통 수", 0.f, 50.f, 1.f);
		CR_TUNE("레이저총", LaserInterval, "연사 간격(초)", 0.02f, 1.f, 0.005f);
		CR_TUNE("레이저총", LaserRange, "사거리", 500.f, 20000.f, 100.f);
		CR_TUNE("레이저총", LaserBeamDuration, "빔 표시 시간(초)", 0.01f, 0.5f, 0.005f);
		CR_TUNE("레이저총", LaserBeamWidth, "빔 두께", 0.5f, 30.f, 0.5f);
		CR_TUNE("레이저총", LaserKnockback, "넉백 세기", 0.f, 3000.f, 10.f);
		CR_TUNE("레이저총", LaserHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		CR_TUNE("레이저총", LaserShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		CR_TUNE("투사체총", ProjectileDamage, "데미지", 0.f, 200.f, 1.f);
		CR_TUNE("투사체총", ProjectilePierce, "관통 수", 0.f, 50.f, 1.f);
		CR_TUNE("투사체총", ProjectileInterval, "연사 간격(초)", 0.02f, 2.f, 0.01f);
		CR_TUNE("투사체총", ProjectileSpeed, "탄속", 200.f, 15000.f, 50.f);
		CR_TUNE("투사체총", ProjectileGravity, "중력 배율", 0.f, 3.f, 0.05f);
		CR_TUNE("투사체총", ProjectileLifetime, "수명(초)", 0.2f, 10.f, 0.1f);
		CR_TUNE("투사체총", ProjectileSize, "탄 크기", 0.2f, 5.f, 0.1f);
		CR_TUNE("투사체총", ProjectileKnockback, "넉백 세기", 0.f, 3000.f, 10.f);
		CR_TUNE("투사체총", ProjectileHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		CR_TUNE("투사체총", ProjectileShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		CR_TUNE_BOOL("피격 번쩍임", bHitFlash, "켜기");
		CR_TUNE("피격 번쩍임", HitFlashDuration, "번쩍임 시간(초)", 0.01f, 1.f, 0.01f);
		CR_TUNE("피격 번쩍임", HitFlashIntensity, "번쩍임 밝기", 0.f, 50.f, 0.5f);

		CR_TUNE_BOOL("데미지 숫자", bDamageNumbers, "켜기");
		CR_TUNE("데미지 숫자", DamageNumberDuration, "표시 시간(초)", 0.1f, 3.f, 0.05f);
		CR_TUNE("데미지 숫자", DamageNumberRise, "떠오르는 높이", 0.f, 300.f, 5.f);
		CR_TUNE("데미지 숫자", DamageNumberScale, "크기", 0.3f, 5.f, 0.1f);

		CR_TUNE_BOOL("히트스톱", bHitStop, "켜기");
		CR_TUNE_BOOL("히트스톱", bHitStopGlobal, "화면 전체 정지 (끄면 때린 쪽/맞은 쪽만)");
		CR_TUNE("히트스톱", HitStopTimeScale, "정지 중 시간 배율", 0.f, 1.f, 0.01f);

		CR_TUNE_BOOL("넉백", bKnockback, "켜기");
		CR_TUNE("넉백", KnockbackUpRatio, "위쪽 비율", 0.f, 2.f, 0.05f);

		CR_TUNE_BOOL("카메라 흔들림", bCameraShake, "켜기");
		CR_TUNE("카메라 흔들림", CameraShakeIntensity, "세기(도)", 0.f, 10.f, 0.1f);
		CR_TUNE("카메라 흔들림", CameraShakeDuration, "시간(초)", 0.02f, 1.f, 0.01f);
		CR_TUNE("카메라 흔들림", CameraShakeFrequency, "빈도", 1.f, 80.f, 1.f);

		CR_TUNE_BOOL("피격 파티클", bHitParticles, "켜기");
		CR_TUNE("피격 파티클", HitParticleCount, "개수", 0.f, 64.f, 1.f);
		CR_TUNE("피격 파티클", HitParticleSpeed, "속도", 0.f, 3000.f, 10.f);
		CR_TUNE("피격 파티클", HitParticleLifetime, "수명(초)", 0.05f, 2.f, 0.01f);
		CR_TUNE("피격 파티클", HitParticleSize, "크기", 0.01f, 2.f, 0.01f);
		CR_TUNE_BOOL("피격 파티클", bParticleStretch, "속도 방향으로 늘이기");
		CR_TUNE("피격 파티클", ParticleStretch, "늘이기 정도", 0.f, 0.02f, 0.0005f);
		CR_TUNE_BOOL("피격 파티클", bParticleBounce, "바닥 튕김");
		CR_TUNE("피격 파티클", ParticleBounce, "튕김 탄성", 0.f, 1.f, 0.05f);

		CR_TUNE_BOOL("피격 섬광", bHitBurst, "켜기");
		CR_TUNE("피격 섬광", HitBurstSize, "크기", 0.05f, 5.f, 0.05f);
		CR_TUNE("피격 섬광", HitBurstDuration, "시간(초)", 0.01f, 0.5f, 0.01f);

		CR_TUNE_BOOL("충격파 링", bShockwave, "켜기");
		CR_TUNE("충격파 링", ShockwaveCount, "조각 수", 4.f, 64.f, 1.f);
		CR_TUNE("충격파 링", ShockwaveSpeed, "퍼지는 속도", 100.f, 4000.f, 50.f);
		CR_TUNE("충격파 링", ShockwaveSize, "조각 크기", 0.02f, 1.f, 0.01f);

		CR_TUNE_BOOL("처치 폭발", bKillExplosion, "켜기");
		CR_TUNE("처치 폭발", KillExplosionCount, "파편 수", 0.f, 200.f, 1.f);
		CR_TUNE("처치 폭발", KillExplosionSpeed, "속도 배율", 0.2f, 5.f, 0.1f);
		CR_TUNE("처치 폭발", KillExplosionSize, "크기 배율", 0.2f, 5.f, 0.1f);

		CR_TUNE("사망 연출", DeathKnockbackScale, "몸 넉백 배율", 0.f, 10.f, 0.1f);
		CR_TUNE_BOOL("사망 연출", bDeathPopOff, "투구(모자)/무기 벗겨짐");
		CR_TUNE("사망 연출", DeathPopSpeed, "튀어나가는 세기", 0.f, 3000.f, 50.f);
		CR_TUNE("사망 연출", DeathPopUp, "위로 튀는 세기", 0.f, 3000.f, 50.f);
		CR_TUNE("사망 연출", DeathSpin, "회전 속도(도/초)", 0.f, 3000.f, 30.f);
		CR_TUNE("사망 연출", DeathGravity, "중력", 100.f, 6000.f, 100.f);
		CR_TUNE("사망 연출", DeathBounce, "바닥 튕김 탄성", 0.f, 1.f, 0.05f);

		CR_TUNE_BOOL("색종이", bConfetti, "켜기 (처치 시)");
		CR_TUNE("색종이", ConfettiCount, "개수", 0.f, 200.f, 1.f);
		CR_TUNE("색종이", ConfettiSize, "크기", 0.02f, 1.f, 0.01f);
		CR_TUNE("색종이", ConfettiSpeed, "터지는 속도", 0.f, 3000.f, 25.f);
		CR_TUNE("색종이", ConfettiLifetime, "수명(초)", 0.2f, 5.f, 0.1f);
		CR_TUNE("색종이", ConfettiGravity, "중력", 0.f, 3000.f, 25.f);
		CR_TUNE("색종이", ConfettiDrag, "공기 저항", 0.f, 10.f, 0.1f);
		CR_TUNE("색종이", ConfettiFlutter, "팔랑임 세기", 0.f, 1000.f, 10.f);
		CR_TUNE("색종이", ConfettiSpin, "회전 속도(도/초)", 0.f, 3000.f, 30.f);

		CR_TUNE_BOOL("총 반동", bWeaponRecoil, "켜기 (킥백)");
		CR_TUNE("총 반동", RecoilKickback, "킥백 거리", 0.f, 60.f, 0.5f);
		CR_TUNE("총 반동", RecoilPitch, "총구 들림(도)", 0.f, 60.f, 0.5f);
		CR_TUNE("총 반동", RecoilStiffness, "복귀 스프링 강도", 10.f, 2000.f, 10.f);
		CR_TUNE("총 반동", RecoilDamping, "복귀 감쇠", 0.f, 100.f, 0.5f);
		CR_TUNE("총 반동", RecoilJitter, "연사 떨림", 0.f, 10.f, 0.1f);

		CR_TUNE_BOOL("카메라 반동", bCameraRecoil, "켜기");
		CR_TUNE("카메라 반동", CameraRecoilPitch, "반동(도/발)", 0.f, 5.f, 0.05f);
		CR_TUNE("카메라 반동", CameraRecoilRecover, "복귀 속도", 0.5f, 40.f, 0.5f);

		CR_TUNE_BOOL("총구 화염", bMuzzleFlash, "켜기");
		CR_TUNE("총구 화염", MuzzleFlashDuration, "화염 시간(초)", 0.01f, 0.3f, 0.005f);
		CR_TUNE("총구 화염", MuzzleLightIntensity, "빛 세기", 0.f, 50000.f, 500.f);

		CR_TUNE_BOOL("큐브 잔해", bCubeDebris, "켜기 (유타폰 큐브)");
		CR_TUNE("큐브 잔해", DebrisCountScale, "개수 배율", 0.f, 5.f, 0.1f);
		CR_TUNE("큐브 잔해", DebrisSize, "크기 배율", 0.2f, 4.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisGravity, "중력", 0.f, 5000.f, 50.f);

		CR_TUNE_BOOL("구체 폭발", bSphereExplosion, "켜기 (처치, 미사일)");
		CR_TUNE("구체 폭발", ExplosionRadius, "처치 폭발 반경", 10.f, 300.f, 5.f);

		CR_TUNE_BOOL("히트 스파크 별", bHitStar, "켜기");
		CR_TUNE("히트 스파크 별", HitStarSize, "크기 배율", 0.2f, 4.f, 0.05f);
		CR_TUNE_BOOL("십자 반짝임", bSparkle, "켜기 (무기 전환, 필살기)");

		CR_TUNE_BOOL("임팩트 프레임", bImpactFrame, "켜기 · I");
		CR_TUNE_BOOL("임팩트 프레임", bImpactOnPlayerKill, "플레이어 처치 때도");
		CR_TUNE("임팩트 프레임", ImpactFrameDuration, "장당 시간(초)", 0.01f, 0.3f, 0.005f);
		CR_TUNE("임팩트 프레임", ImpactFrameCooldown, "쿨다운(초)", 0.f, 3.f, 0.05f);

		CR_TUNE_BOOL("스미어", bSmear, "켜기 (빠른 손)");
		CR_TUNE("스미어", SmearMinSpeed, "몸 기준 최소 속도", 50.f, 3000.f, 25.f);
		CR_TUNE_BOOL("카나다 빛", bKanada, "켜기");
		CR_TUNE("카나다 빛", KanadaSize, "크기 배율", 0.2f, 4.f, 0.05f);
		CR_TUNE_BOOL("먼지 링", bDustRing, "켜기 (착지)");
		CR_TUNE("먼지 링", DustRingMinSpeed, "착지 최소 속도", 100.f, 3000.f, 25.f);
		CR_TUNE_BOOL("집중선", bFocusLines, "켜기");
		CR_TUNE("집중선", SprintFocus, "달리기 집중선 세기", 0.f, 1.f, 0.05f);

		CR_TUNE("번개", LightningDamage, "데미지", 0.f, 500.f, 1.f);
		CR_TUNE("번개", LightningInterval, "발사 간격(초)", 0.05f, 3.f, 0.01f);
		CR_TUNE("번개", LightningRange, "사거리", 200.f, 6000.f, 50.f);
		CR_TUNE("번개", LightningKnockback, "넉백", 0.f, 3000.f, 25.f);
		CR_TUNE("번개", LightningHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		CR_TUNE("번개", LightningShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		CR_TUNE("미사일", MissileDamage, "데미지(폭발)", 0.f, 500.f, 1.f);
		CR_TUNE("미사일", MissileInterval, "일제 사격 간격(초)", 0.1f, 5.f, 0.05f);
		CR_TUNE("미사일", MissileCount, "한 번에 발수", 1.f, 30.f, 1.f);
		CR_TUNE("미사일", MissileRadius, "폭발 반경", 20.f, 600.f, 5.f);
		CR_TUNE("미사일", MissileStartSpeed, "출발 속도", 50.f, 3000.f, 10.f);
		CR_TUNE("미사일", MissileAcceleration, "가속도", 0.f, 8000.f, 50.f);
		CR_TUNE("미사일", MissileMaxSpeed, "최대 속도", 100.f, 6000.f, 50.f);
		CR_TUNE("미사일", MissileTurnStart, "처음 선회 속도(도/초)", 0.f, 720.f, 5.f);
		CR_TUNE("미사일", MissileTurnEnd, "나중 선회 속도(도/초)", 0.f, 2000.f, 10.f);
		CR_TUNE("미사일", MissileTurnRampTime, "선회 세지는 시간(초)", 0.05f, 4.f, 0.05f);
		CR_TUNE("미사일", MissileKnockback, "넉백", 0.f, 3000.f, 25.f);
		CR_TUNE("미사일", MissileHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		CR_TUNE("미사일", MissileShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		CR_TUNE("폴리곤 불꽃", FlameDamage, "데미지", 0.f, 500.f, 1.f);
		CR_TUNE("폴리곤 불꽃", FlameInterval, "발사 간격(초)", 0.1f, 5.f, 0.05f);
		CR_TUNE("폴리곤 불꽃", FlameRadius, "반경", 20.f, 400.f, 5.f);
		CR_TUNE("폴리곤 불꽃", FlameHeight, "높이", 20.f, 800.f, 10.f);
		CR_TUNE("폴리곤 불꽃", FlameDuration, "지속(초)", 0.2f, 5.f, 0.05f);
		CR_TUNE("폴리곤 불꽃", FlameRange, "사거리", 200.f, 5000.f, 50.f);
		CR_TUNE("폴리곤 불꽃", FlameKnockback, "넉백", 0.f, 3000.f, 25.f);

		CR_TUNE("펀치", PunchDamage, "데미지", 0.f, 500.f, 1.f);
		CR_TUNE("펀치", PunchInterval, "간격(초)", 0.05f, 3.f, 0.01f);
		CR_TUNE("펀치", PunchReach, "주먹 뻗는 거리", 10.f, 300.f, 5.f);
		CR_TUNE("펀치", PunchRadius, "판정 반경", 5.f, 200.f, 1.f);
		CR_TUNE("펀치", PunchKnockback, "넉백", 0.f, 4000.f, 25.f);
		CR_TUNE("펀치", PunchHitStop, "때린 쪽 히트스톱(초)", 0.f, 0.5f, 0.005f);
		CR_TUNE("펀치", PunchShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		CR_TUNE("필살기", SuperFreezeDuration, "시간 정지(초)", 0.1f, 3.f, 0.05f);
		CR_TUNE("필살기", SuperMissileCount, "미사일 수", 1.f, 60.f, 1.f);
		CR_TUNE("필살기", SuperCooldown, "쿨다운(초)", 0.f, 30.f, 0.5f);
		CR_TUNE("필살기", SuperZoom, "줌 FOV 배율", 0.3f, 1.f, 0.05f);

		CR_TUNE("달리기", SprintMultiplier, "달리기 배율 (Shift)", 1.f, 4.f, 0.05f);

		CR_TUNE("다수 타격 테스트", MonsterMaxHealth, "몬스터 최대 체력", 1.f, 5000.f, 10.f);
		CR_TUNE("다수 타격 테스트", MonsterMoveSpeed, "몬스터 이동 속도 (0 = 정지)", 0.f, 1000.f, 10.f);
		CR_TUNE("다수 타격 테스트", SpawnRadius, "생성 반경", 300.f, 3800.f, 50.f);

		CR_TUNE("팀 전투", BattleMoveSpeed, "이동 속도", 50.f, 1000.f, 10.f);
		CR_TUNE("팀 전투", BattleAttackRange, "칼 공격 시작 거리", 80.f, 400.f, 5.f);
		CR_TUNE("팀 전투", BattleGunRange, "총 사격 시작 거리", 200.f, 3000.f, 50.f);
		CR_TUNE("팀 전투", BattleSwordRatio, "무기 비율: 칼", 0.f, 10.f, 1.f);
		CR_TUNE("팀 전투", BattleLaserRatio, "무기 비율: 레이저", 0.f, 10.f, 1.f);
		CR_TUNE("팀 전투", BattleProjectileRatio, "무기 비율: 투사체", 0.f, 10.f, 1.f);
		CR_TUNE("팀 전투", BattleTeamDistance, "진영 간 거리(절반)", 200.f, 2000.f, 50.f);
		CR_TUNE("팀 전투", BattleSpread, "진영 퍼짐 반경", 50.f, 1500.f, 25.f);
		CR_TUNE_BOOL("팀 전투", bBattleSpectate, "관전 (플레이어 숨김)");
	}
	return Items;
}

#undef CR_TUNE_BOOL
#undef CR_TUNE

const FCRTuningItem* UCRTuningSettings::FindItem(FName Name)
{
	return GetItems().FindByPredicate([Name](const FCRTuningItem& Item) { return Item.GetName() == Name; });
}
