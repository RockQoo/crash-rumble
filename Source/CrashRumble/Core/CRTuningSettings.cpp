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

		CR_TUNE("벤치마크", BenchPerTeam, "팀당 유닛 수", 10.f, 300.f, 10.f);
		CR_TUNE("벤치마크", BenchDuration, "측정 시간(초)", 5.f, 120.f, 5.f);
		CR_TUNE("벤치마크", BenchWarmup, "측정 전 준비 시간(초)", 0.f, 20.f, 1.f);
		CR_TUNE("벤치마크", BenchLaserRatio, "무기 비율: 레이저", 0.f, 10.f, 1.f);
		CR_TUNE("벤치마크", BenchProjectileRatio, "무기 비율: 투사체", 0.f, 10.f, 1.f);
		CR_TUNE("벤치마크", BenchMissileRatio, "무기 비율: 미사일", 0.f, 10.f, 1.f);
		CR_TUNE("벤치마크", BenchCraterRadius, "미사일 크레이터 반경", 0.f, 600.f, 10.f);
		CR_TUNE("벤치마크", BenchGunCraterRadius, "총알·레이저 크레이터 반경 (0 = 블럭 1칸 피해)", 0.f, 300.f, 5.f);
		CR_TUNE("벤치마크", BenchDamageScale, "피해 배율 (1 = 원래 무기 피해)", 0.05f, 2.f, 0.05f);
		CR_TUNE("벤치마크", BenchTeamDistance, "진영 간 거리(절반, 맵 끝까지 약 3800)", 300.f, 3800.f, 50.f);
		CR_TUNE("벤치마크", BenchFrontWidth, "전열 폭 (가로로 서는 길이)", 500.f, 7800.f, 100.f);
		CR_TUNE("벤치마크", BenchPerRow, "한 줄에 서는 수", 1.f, 100.f, 1.f);
		CR_TUNE("벤치마크", BenchRowSpacing, "줄 간격", 100.f, 1000.f, 25.f);
		CR_TUNE("벤치마크", BenchEngageRange, "교전 시작 거리 (밖이면 앞으로 전진)", 300.f, 5000.f, 50.f);
		CR_TUNE("벤치마크", BenchStrafe, "싸우며 옆걸음 (0 = 서서 쏨)", 0.f, 1.f, 0.05f);
		CR_TUNE("벤치마크", BenchCameraDistance, "관전 카메라 거리", 1000.f, 10000.f, 100.f);
		CR_TUNE("벤치마크", BenchCameraPitch, "관전 카메라 내려다보는 각도", 10.f, 89.f, 1.f);
		CR_TUNE_BOOL("벤치마크", bBenchTrace, "Unreal Insights 트레이스 저장");
		CR_TUNE_BOOL("벤치마크", bBenchCurrentMap, "지금 맵에서 측정 (끄면 큰 평지)");
		CR_TUNE_BOOL("벤치마크", bBenchLegacyBlocks, "블럭 갱신 옛 방식 (최적화 전 비교용)");

		CR_TUNE_BOOL("툰 / 화면", bToonShading, "툰 셰이딩 (끄면 예전 조명)");
		CR_TUNE_BOOL("툰 / 화면", bToonFloor, "바닥도 툰 (체크무늬)");
		CR_TUNE_BOOL("툰 / 화면", bAnimePostProcess, "포스트 프로세스 (노출 고정, 비네트·모션 블러 끔)");
		CR_TUNE("툰 / 화면", ExposureBias, "노출 보정", -4.f, 4.f, 0.1f);
		CR_TUNE_BOOL("툰 / 화면", bBlobShadow, "블롭 그림자");
		CR_TUNE("툰 / 화면", BlobShadowStrength, "블롭 그림자 진하기", 0.f, 1.f, 0.05f);
		CR_TUNE("툰 / 화면", BlobShadowSize, "블롭 그림자 크기", 0.3f, 3.f, 0.05f);

		CR_TUNE("2코마", FxFps, "이펙트 초당 그림 (0 = 매끄럽게, 12 = 2코마, 8 = 3코마) · T", 0.f, 60.f, 1.f);
		CR_TUNE("2코마", CharFps, "캐릭터 초당 그림 (0 = 매끄럽게) · Y", 0.f, 60.f, 1.f);

		CR_TUNE_BOOL("캐릭터 LOD", bCharLod, "켜기 (끄면 모두 매 프레임)");
		CR_TUNE("캐릭터 LOD", CharLodNear, "가까움 → 중간 거리 (카메라에서)", 200.f, 10000.f, 100.f);
		CR_TUNE("캐릭터 LOD", CharLodFar, "중간 → 멀리 거리", 500.f, 20000.f, 100.f);
		CR_TUNE("캐릭터 LOD", CharLodMidInterval, "중간: 몇 프레임에 한 번 갱신", 1.f, 10.f, 1.f);
		CR_TUNE_BOOL("캐릭터 LOD", bCharLodDebug, "구간을 모자 색으로 표시 (초록/노랑/빨강)");
		CR_TUNE("캐릭터 LOD", CharLodLineCount, "미리보기: 줄 세울 수", 2.f, 60.f, 1.f);
		CR_TUNE("캐릭터 LOD", CharLodLineSpacing, "미리보기: 간격", 50.f, 1000.f, 25.f);

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
		CR_TUNE("큐브 잔해", DebrisMinSize, "최소 크기(cm)", 1.f, 100.f, 0.5f);
		CR_TUNE("큐브 잔해", DebrisMaxSize, "최대 크기(cm, 작은 것 위주로 섞임)", 1.f, 200.f, 0.5f);
		CR_TUNE("큐브 잔해", DebrisBury, "표면 안쪽에서 시작 (크기 비율)", 0.f, 2.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisNormalSpeedMin, "튀는 속도 최소 (맞은 면 방향)", 0.f, 3000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisNormalSpeedMax, "튀는 속도 최대 (맞은 면 방향)", 0.f, 3000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisSideSpeedMin, "옆으로 최소", 0.f, 3000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisSideSpeedMax, "옆으로 최대", 0.f, 3000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisUpSpeedMin, "위로 최소", 0.f, 3000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisUpSpeedMax, "위로 최대", 0.f, 3000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisSpinMin, "회전 최소(도/초)", 0.f, 5000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisSpinMax, "회전 최대(도/초)", 0.f, 5000.f, 10.f);
		CR_TUNE("큐브 잔해", DebrisLifeMin, "수명 최소(초)", 0.05f, 5.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisLifeMax, "수명 최대(초)", 0.05f, 5.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisShrinkFrom, "작아지기 시작 (수명 비율)", 0.f, 1.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisBounceUp, "튕김: 수직 속도 배율", 0.f, 1.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisBounceSide, "튕김: 수평 속도 배율", 0.f, 1.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisBounceSpin, "튕김: 회전 배율", 0.f, 1.f, 0.05f);
		CR_TUNE("큐브 잔해", DebrisKillCount, "처치 시 개수", 0.f, 100.f, 1.f);

		CR_TUNE_BOOL("구체 폭발", bSphereExplosion, "켜기 (처치, 미사일)");
		CR_TUNE("구체 폭발", ExplosionRadius, "처치 폭발 반경", 10.f, 300.f, 5.f);
		CR_TUNE("구체 폭발", FireballCount, "불덩이 개수", 0.f, 50.f, 1.f);
		CR_TUNE("구체 폭발", FireballSpreadMin, "불덩이 퍼짐 최소 (반경 비율)", 0.f, 2.f, 0.05f);
		CR_TUNE("구체 폭발", FireballSpreadMax, "불덩이 퍼짐 최대 (반경 비율)", 0.f, 2.f, 0.05f);
		CR_TUNE("구체 폭발", FireballSizeMin, "불덩이 크기 최소 (반경 비율)", 0.05f, 2.f, 0.01f);
		CR_TUNE("구체 폭발", FireballSizeMax, "불덩이 크기 최대 (반경 비율)", 0.05f, 2.f, 0.01f);
		CR_TUNE("구체 폭발", FireballDelayMax, "불덩이 시작 지연 최대(초)", 0.f, 0.5f, 0.01f);
		CR_TUNE("구체 폭발", FireballLifeMin, "불덩이 수명 최소(초)", 0.05f, 3.f, 0.05f);
		CR_TUNE("구체 폭발", FireballLifeMax, "불덩이 수명 최대(초)", 0.05f, 3.f, 0.05f);
		CR_TUNE("구체 폭발", FireballInflateTime, "부푸는 시간(초)", 0.01f, 1.f, 0.01f);
		CR_TUNE("구체 폭발", FireGlowTime, "색 단계 1 흰노랑 발광 끝(초)", 0.f, 1.f, 0.01f);
		CR_TUNE("구체 폭발", FireOrangeTime, "색 단계 2 주황 끝(초)", 0.f, 1.f, 0.01f);
		CR_TUNE("구체 폭발", FireRedTime, "색 단계 3 빨강 끝(초, 그 뒤 연기)", 0.f, 2.f, 0.01f);
		CR_TUNE("구체 폭발", FireballRiseFrom, "떠오르며 작아지기 시작 (수명 비율)", 0.f, 1.f, 0.05f);
		CR_TUNE("구체 폭발", FireballRiseSpeed, "떠오르는 속도", 0.f, 500.f, 5.f);
		CR_TUNE("구체 폭발", ExplosionFlashTime, "섬광 1장(흰 큰 구) 시간", 0.f, 0.5f, 0.01f);
		CR_TUNE("구체 폭발", ExplosionFlashEnd, "섬광 2장(노란 작은 구) 끝", 0.f, 0.5f, 0.01f);
		CR_TUNE("구체 폭발", ExplosionFlashSize, "섬광 크기 (반경 비율)", 0.f, 5.f, 0.05f);
		CR_TUNE("구체 폭발", ShockCubeCount, "충격파 큐브 개수", 0.f, 64.f, 1.f);
		CR_TUNE("구체 폭발", ShockSpreadStart, "충격파 시작 반경 (배)", 0.f, 5.f, 0.05f);
		CR_TUNE("구체 폭발", ShockSpreadEnd, "충격파 끝 반경 (배)", 0.f, 10.f, 0.1f);
		CR_TUNE("구체 폭발", ShockDuration, "충격파 시간(초)", 0.05f, 2.f, 0.01f);

		CR_TUNE_BOOL("히트 스파크 별", bHitStar, "켜기");
		CR_TUNE("히트 스파크 별", HitStarSize, "크기 배율", 0.2f, 4.f, 0.05f);
		CR_TUNE("히트 스파크 별", HitStarDuration, "전체 시간(초, 3장)", 0.05f, 2.f, 0.01f);
		CR_TUNE("히트 스파크 별", HitStarBrightness, "1장 흰색 밝기", 0.f, 20.f, 0.1f);
		CR_TUNE("히트 스파크 별", HitStarStage2Scale, "2장 크기 배율", 0.f, 3.f, 0.05f);
		CR_TUNE("히트 스파크 별", HitStarStage3Scale, "3장 크기 배율", 0.f, 3.f, 0.05f);
		CR_TUNE_BOOL("십자 반짝임", bSparkle, "켜기 (무기 전환, 필살기)");
		CR_TUNE("십자 반짝임", SparkleSize, "무기 전환 반짝임 크기(cm)", 1.f, 200.f, 1.f);
		CR_TUNE("십자 반짝임", SparklePopTime, "튀어나오는 시간(초)", 0.01f, 1.f, 0.01f);
		CR_TUNE("십자 반짝임", SparkleDuration, "전체 시간(초)", 0.05f, 3.f, 0.01f);
		CR_TUNE("십자 반짝임", SparkleSpin, "도는 각도(도)", 0.f, 360.f, 5.f);
		CR_TUNE("십자 반짝임", SparkleBrightness, "밝기 (높으면 블룸이 번짐)", 0.f, 20.f, 0.1f);

		CR_TUNE_BOOL("임팩트 프레임", bImpactFrame, "켜기 · I");
		CR_TUNE_BOOL("임팩트 프레임", bImpactOnPlayerKill, "플레이어 처치 때도");
		CR_TUNE("임팩트 프레임", ImpactFrameDuration, "장당 시간(초)", 0.01f, 0.3f, 0.005f);
		CR_TUNE("임팩트 프레임", ImpactFrameCooldown, "쿨다운(초)", 0.f, 3.f, 0.05f);

		CR_TUNE_BOOL("스미어", bSmear, "켜기 (빠른 손)");
		CR_TUNE("스미어", SmearMinSpeed, "몸 기준 최소 속도", 50.f, 3000.f, 25.f);
		CR_TUNE("스미어", SmearThickness, "굵기 (손 지름 비율)", 0.1f, 3.f, 0.05f);
		CR_TUNE_BOOL("카나다 빛", bKanada, "켜기");
		CR_TUNE("카나다 빛", KanadaSize, "크기 배율", 0.2f, 4.f, 0.05f);
		CR_TUNE("카나다 빛", KanadaCount, "가시 개수", 1.f, 48.f, 1.f);
		CR_TUNE("카나다 빛", KanadaDuration, "전체 시간(초)", 0.05f, 2.f, 0.01f);
		CR_TUNE("카나다 빛", KanadaLengthMin, "가시 길이 최소 (크기 비율)", 0.f, 3.f, 0.05f);
		CR_TUNE("카나다 빛", KanadaLengthMax, "가시 길이 최대 (크기 비율)", 0.f, 3.f, 0.05f);
		CR_TUNE("카나다 빛", KanadaWidthMin, "가시 폭 최소 (크기 비율)", 0.f, 1.f, 0.01f);
		CR_TUNE("카나다 빛", KanadaWidthMax, "가시 폭 최대 (크기 비율)", 0.f, 1.f, 0.01f);
		CR_TUNE("카나다 빛", KanadaBlackRatio, "마지막 장 검은 가시 비율", 0.f, 1.f, 0.05f);
		CR_TUNE_BOOL("먼지 링", bDustRing, "켜기 (착지)");
		CR_TUNE("먼지 링", DustRingMinSpeed, "착지 최소 속도", 100.f, 3000.f, 25.f);
		CR_TUNE("먼지 링", DustRingDuration, "시간(초)", 0.05f, 3.f, 0.05f);
		CR_TUNE("먼지 링", DustRingStartScale, "링 시작 크기", 0.f, 5.f, 0.05f);
		CR_TUNE("먼지 링", DustRingEndScale, "링 끝 크기", 0.f, 10.f, 0.1f);
		CR_TUNE("먼지 링", DustPuffCount, "먼지 구체 개수", 0.f, 32.f, 1.f);
		CR_TUNE("먼지 링", DustPuffSpeed, "먼지 밀려나는 속도", 0.f, 3000.f, 10.f);
		CR_TUNE("먼지 링", DustPuffSize, "먼지 구체 크기", 0.f, 3.f, 0.01f);
		CR_TUNE_BOOL("집중선", bFocusLines, "켜기");
		CR_TUNE("집중선", SprintFocus, "달리기 집중선 세기", 0.f, 1.f, 0.05f);

		CR_TUNE("번개", LightningDamage, "데미지", 0.f, 500.f, 1.f);
		CR_TUNE("번개", LightningInterval, "발사 간격(초)", 0.05f, 3.f, 0.01f);
		CR_TUNE("번개", LightningRange, "사거리", 200.f, 6000.f, 50.f);
		CR_TUNE("번개", LightningKnockback, "넉백", 0.f, 3000.f, 25.f);
		CR_TUNE("번개", LightningHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		CR_TUNE("번개", LightningShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);
		CR_TUNE("번개", LightningBoltDuration, "줄기 보이는 시간(초)", 0.05f, 3.f, 0.01f);
		CR_TUNE("번개", LightningSegmentLength, "꺾이는 간격(cm)", 10.f, 500.f, 5.f);
		CR_TUNE("번개", LightningMinPoints, "꺾임 최소 개수", 2.f, 64.f, 1.f);
		CR_TUNE("번개", LightningMaxPoints, "꺾임 최대 개수", 2.f, 64.f, 1.f);
		CR_TUNE("번개", LightningJitterRatio, "흔들림 폭 (길이 비율)", 0.f, 1.f, 0.01f);
		CR_TUNE("번개", LightningJitterMax, "흔들림 폭 최대(cm)", 0.f, 500.f, 5.f);
		CR_TUNE("번개", LightningThickStart, "굵기 시작(cm)", 0.5f, 100.f, 0.5f);
		CR_TUNE("번개", LightningThickEnd, "굵기 끝(cm)", 0.5f, 100.f, 0.5f);
		CR_TUNE("번개", LightningBranchCount, "가지 개수", 0.f, 10.f, 1.f);
		CR_TUNE("번개", LightningBranchMin, "가지 길이 최소 (비율)", 0.f, 1.f, 0.01f);
		CR_TUNE("번개", LightningBranchMax, "가지 길이 최대 (비율)", 0.f, 1.f, 0.01f);

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

		CR_TUNE("미사일 연기", MissileSmokeSize, "연기 덩이 크기 (1 = 지름 100cm)", 0.02f, 2.f, 0.01f);
		CR_TUNE("미사일 연기", MissileSmokeSpacing, "연기 간격 (cm, 작을수록 촘촘)", 5.f, 200.f, 1.f);
		CR_TUNE("미사일 연기", MissileSmokeLifetime, "연기 수명(초)", 0.1f, 5.f, 0.05f);
		CR_TUNE("미사일 연기", MissileSmokeRise, "떠오르는 속도 (cm/s)", 0.f, 300.f, 5.f);
		CR_TUNE("미사일 연기", MissileSmokeRingEvery, "도넛 링 간격 (연기 N개마다, 0 = 끔)", 0.f, 50.f, 1.f);
		CR_TUNE("미사일 연기", MissileSmokeRingSize, "도넛 링 최대 지름 (cm)", 10.f, 300.f, 5.f);
		CR_TUNE("미사일 연기", MissileSmokeInflateTime, "부푸는 시간(초, 55% → 100%)", 0.01f, 2.f, 0.01f);
		CR_TUNE("미사일 연기", MissileSmokeGlowTime, "처음 흰색 발광 시간(초)", 0.f, 1.f, 0.01f);
		CR_TUNE("미사일 연기", MissileSmokeRingDuration, "도넛 링 시간(초)", 0.05f, 3.f, 0.01f);

		CR_TUNE("폴리곤 불꽃", FlameDamage, "데미지", 0.f, 500.f, 1.f);
		CR_TUNE("폴리곤 불꽃", FlameInterval, "발사 간격(초)", 0.1f, 5.f, 0.05f);
		CR_TUNE("폴리곤 불꽃", FlameRadius, "반경", 20.f, 400.f, 5.f);
		CR_TUNE("폴리곤 불꽃", FlameHeight, "높이", 20.f, 800.f, 10.f);
		CR_TUNE("폴리곤 불꽃", FlameDuration, "지속(초)", 0.2f, 5.f, 0.05f);
		CR_TUNE("폴리곤 불꽃", FlameRange, "사거리", 200.f, 5000.f, 50.f);
		CR_TUNE("폴리곤 불꽃", FlameKnockback, "넉백", 0.f, 3000.f, 25.f);
		CR_TUNE("폴리곤 불꽃", FlameCount, "삼각뿔 개수", 1.f, 128.f, 1.f);
		CR_TUNE("폴리곤 불꽃", FlameGrowTime, "자라는 시간(초)", 0.01f, 2.f, 0.01f);
		CR_TUNE("폴리곤 불꽃", FlameSinkFrom, "가라앉기 시작 (수명 비율)", 0.f, 1.f, 0.05f);
		CR_TUNE("폴리곤 불꽃", FlameTilt, "기울기 최대(도)", 0.f, 90.f, 1.f);
		CR_TUNE("폴리곤 불꽃", FlameSizeMin, "삼각뿔 굵기 최소 (반경 비율)", 0.01f, 1.f, 0.01f);
		CR_TUNE("폴리곤 불꽃", FlameSizeMax, "삼각뿔 굵기 최대 (반경 비율)", 0.01f, 1.f, 0.01f);

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
		CR_TUNE("필살기", SuperWorldDilation, "시간 정지 중 월드 속도", 0.001f, 1.f, 0.005f);

		CR_TUNE("달리기", SprintMultiplier, "달리기 배율 (Shift)", 1.f, 4.f, 0.05f);

		CR_TUNE_BOOL("블럭 지형", bBlockRegen, "부서진 블럭 다시 자람");
		CR_TUNE("블럭 지형", BlockRegenDelay, "재생 기본 대기(초)", 0.5f, 60.f, 0.5f);
		CR_TUNE("블럭 지형", BlockRegenRandom, "재생 랜덤 추가(초)", 0.f, 10.f, 0.1f);
		CR_TUNE("블럭 지형", BlockRegenPerLayer, "층마다 추가 대기(초)", 0.f, 2.f, 0.01f);
		CR_TUNE("블럭 지형", BlockRegenPerFrame, "한 프레임 최대 재생 수", 1.f, 500.f, 1.f);
		CR_TUNE("블럭 지형", BlockFallGravity, "무너지는 블럭 중력", 100.f, 5000.f, 50.f);
		CR_TUNE("블럭 지형", BlockCollapseImpact, "임팩트 프레임 붕괴 블럭 수", 1.f, 200.f, 1.f);
		CR_TUNE("블럭 지형", MissileCraterRadius, "미사일 크레이터 반경", 0.f, 500.f, 5.f);
		CR_TUNE("블럭 지형", GunCraterRadius, "총알·레이저 크레이터 반경 (0 = 블럭 1칸 피해)", 0.f, 300.f, 5.f);
		CR_TUNE("블럭 지형", LightningBlockDamage, "번개 블럭 피해", 0.f, 10.f, 1.f);
		CR_TUNE("블럭 지형", BlockRegrowDuration, "다시 자라는 연출 시간(초)", 0.01f, 3.f, 0.01f);
		CR_TUNE("블럭 지형", BlockRegrowStartScale, "다시 자랄 때 시작 크기", 0.f, 1.f, 0.01f);
		CR_TUNE("블럭 지형", BlockRegrowBrightness, "다시 자랄 때 시작 밝기", 1.f, 10.f, 0.1f);

		CR_TUNE("훈련장", BarrelHealth, "드럼통 HP (맞는 횟수)", 1.f, 20.f, 1.f);
		CR_TUNE("훈련장", BarrelBlastRadius, "드럼통 폭발 피해 반경", 50.f, 800.f, 10.f);
		CR_TUNE("훈련장", BarrelDamage, "드럼통 폭발 피해", 0.f, 1000.f, 5.f);
		CR_TUNE("훈련장", BarrelExplosionScale, "드럼통 폭발 크기 배율 (불덩이·카나다 빛·잔해)", 0.3f, 4.f, 0.1f);
		CR_TUNE("훈련장", BarrelCraterRadius, "드럼통 크레이터 반경", 0.f, 600.f, 10.f);
		CR_TUNE("훈련장", BarrelSmokeCount, "드럼통 연기 덩이 수", 0.f, 60.f, 1.f);
		CR_TUNE("훈련장", BarrelSmokeSize, "드럼통 연기 크기", 0.1f, 5.f, 0.05f);
		CR_TUNE("훈련장", BarrelSmokeSpread, "드럼통 연기 뭉침 반경 (작을수록 한 덩어리)", 0.f, 800.f, 5.f);
		CR_TUNE("훈련장", BarrelSmokeColumnHeight, "드럼통 연기 기둥 높이 (덩이를 세로로 쌓음)", 0.f, 1000.f, 10.f);
		CR_TUNE("훈련장", BarrelSmokeOrangeTime, "드럼통 연기 주황 → 빨강 (초)", 0.f, 2.f, 0.02f);
		CR_TUNE("훈련장", BarrelSmokeRedTime, "드럼통 연기 빨강 → 어두워지기 시작 (초)", 0.f, 3.f, 0.02f);
		CR_TUNE("훈련장", BarrelSmokeDarkTime, "드럼통 연기 어두운 적갈색이 되기까지 (초)", 0.01f, 3.f, 0.02f);
		CR_TUNE("훈련장", BarrelSmokeLifetime, "드럼통 연기 수명(초)", 0.2f, 8.f, 0.1f);
		CR_TUNE("훈련장", BarrelSmokeUpSpeed, "드럼통 연기 솟는 속도", 0.f, 2000.f, 10.f);
		CR_TUNE("훈련장", BarrelSmokeOutSpeed, "드럼통 연기 옆으로 퍼지는 속도", 0.f, 1500.f, 10.f);
		CR_TUNE("훈련장", BarrelSmokeDrag, "드럼통 연기 감속 (클수록 빨리 멈춤)", 0.f, 10.f, 0.1f);
		CR_TUNE("훈련장", BarrelSmokeRise, "드럼통 연기 멈춘 뒤 떠오르는 속도", 0.f, 500.f, 5.f);
		CR_TUNE("훈련장", BarrelSmokeEndScale, "드럼통 연기 끝 크기 배율 (계속 부푼다)", 0.5f, 5.f, 0.05f);
		CR_TUNE("훈련장", BarrelSmokeShrinkFrom, "드럼통 연기 작아지기 시작 (수명 비율)", 0.f, 0.99f, 0.01f);
		CR_TUNE("훈련장", BarrelSmokeDelay, "드럼통 연기 시작 흩뜨림(초)", 0.f, 1.f, 0.01f);
		CR_TUNE("훈련장", BarrelRespawnDelay, "드럼통 다시 생김(초)", 0.5f, 60.f, 0.5f);
		CR_TUNE("훈련장", DpsWindow, "DPS 측정 구간(초)", 0.5f, 10.f, 0.5f);

		CR_TUNE("다수 타격 테스트", MonsterMaxHealth, "몬스터 최대 체력", 1.f, 5000.f, 10.f);
		CR_TUNE("다수 타격 테스트", MonsterMoveSpeed, "몬스터 이동 속도 (0 = 정지)", 0.f, 1000.f, 10.f);
		CR_TUNE("다수 타격 테스트", SpawnRadius, "생성 반경", 300.f, 3800.f, 50.f);

		CR_TUNE("팀 전투", BattleMoveSpeed, "이동 속도", 50.f, 1000.f, 10.f);
		CR_TUNE("팀 전투", BattleAttackRange, "칼 공격 시작 거리", 80.f, 400.f, 5.f);
		CR_TUNE("팀 전투", BattleGunRange, "총 사격 시작 거리", 200.f, 3000.f, 50.f);
		CR_TUNE("팀 전투", BattleSwordRatio, "무기 비율: 칼", 0.f, 10.f, 1.f);
		CR_TUNE("팀 전투", BattleLaserRatio, "무기 비율: 레이저", 0.f, 10.f, 1.f);
		CR_TUNE("팀 전투", BattleProjectileRatio, "무기 비율: 투사체", 0.f, 10.f, 1.f);
		CR_TUNE("팀 전투", BattleMissileRatio, "무기 비율: 미사일", 0.f, 10.f, 1.f);
		CR_TUNE("팀 전투", BattleStrafe, "싸우며 옆걸음 (0 = 서서 쏨)", 0.f, 1.f, 0.05f);
		CR_TUNE("팀 전투", BattleTeamDistance, "진영 간 거리(절반)", 200.f, 2000.f, 50.f);
		CR_TUNE("팀 전투", BattleSpread, "진영 퍼짐 반경", 50.f, 1500.f, 25.f);
		CR_TUNE_BOOL("팀 전투", bBattleSpectate, "관전 (플레이어 숨김)");
	}
	return Items;
}

#undef CR_TUNE_BOOL
#undef CR_TUNE

FText UCRTuningSettings::GetCategoryDescription(const FString& Category)
{
	static TMap<FString, FText> Descriptions;
	if (Descriptions.Num() == 0)
	{
		Descriptions.Add(TEXT("옵션창"), FText::FromString(TEXT("이 옵션창 자체의 동작.")));
		Descriptions.Add(TEXT("벤치마크"), FText::FromString(TEXT("버튼 하나로 성능 측정: 큰 평지(80m, 블럭 약 13만 개) 양 끝에 레드/블루가 여러 줄로 서서, 레이저·투사체·미사일을 섞어 들고 앞으로 전진하다 교전 거리 안의 적과 옆걸음치며 싸운다 (전선 전체에서 흩어져 싸움). 죽으면 자기 진영 뒷줄 아무 데서나 다시 떨어지고, 땅이 크게 파인다. 준비 시간 뒤 측정 시간 동안 FPS와 게임/렌더/GPU 시간, 유닛·미사일·블럭 수를 기록해 화면과 Saved/Profiling/CRBench/에 저장 (트레이스를 켜면 Unreal Insights용 .utrace도). 시작하면 옵션창은 닫힌다 (옵션창도 프레임을 먹으므로). 측정이 끝나도 전투는 계속되고, 중지하면 원래 맵으로.")));
		Descriptions.Add(TEXT("툰 / 화면"), FText::FromString(TEXT("애니풍 렌더링 바탕. 툰 셰이딩(2단 명암, 보라 그림자, 실루엣 어둡게), 노출 고정, 바닥 체크무늬, 캐릭터 밑 반투명 원판 그림자.")));
		Descriptions.Add(TEXT("2코마"), FText::FromString(TEXT("이펙트/캐릭터를 초당 12장처럼 끊어서 그린다 (애니 2코마). 계산은 매 프레임. T: 이펙트, Y: 캐릭터 토글.")));
		Descriptions.Add(TEXT("캐릭터 LOD"), FText::FromString(TEXT("카메라에서 먼 눈사람은 파츠 움직임(숨쉬기, 손 흔들림, 무기 자세)을 덜 갱신한다. 가까움: 매 프레임 / 중간: N프레임에 한 번 / 멀리: 멈춤 (이동·회전·넘어짐은 그대로). 눈사람 하나가 파츠 수십 개라, 파츠를 움직이는 비용이 다수 전투에서 가장 크다. 미리보기: 카메라 앞에 한 줄로 세운다 (기존 몬스터는 정리). 구간 색 표시를 켜고 비교.")));
		Descriptions.Add(TEXT("숨쉬기"), FText::FromString(TEXT("가만히 있을 때 몸통이 위아래로 늘었다 줄고, 머리와 모자가 조금씩 늦게 따라온다. 걸을 때는 통통 튄다.")));
		Descriptions.Add(TEXT("이동"), FText::FromString(TEXT("플레이어 이동 속도, 가속, 회전, Shift 달리기 배율.")));
		Descriptions.Add(TEXT("카메라"), FText::FromString(TEXT("숄더뷰 / 탑다운 / 맵 제작 자유 카메라 수치와 마우스 감도.")));
		Descriptions.Add(TEXT("손"), FText::FromString(TEXT("떠 있는 손이 위아래로 둥둥 흔들리는 정도.")));
		Descriptions.Add(TEXT("전투"), FText::FromString(TEXT("체력과 부활 시간.")));
		Descriptions.Add(TEXT("칼"), FText::FromString(TEXT("1번 칼: 손이 호를 그리며 휘두르고 칼날 궤적에 닿은 적을 때린다.")));
		Descriptions.Add(TEXT("레이저총"), FText::FromString(TEXT("2번 레이저: 히트스캔 연사, 하늘색 빔.")));
		Descriptions.Add(TEXT("투사체총"), FText::FromString(TEXT("3번 투사체: 날아가는 주황 탄.")));
		Descriptions.Add(TEXT("피격 번쩍임"), FText::FromString(TEXT("맞은 눈사람이 하얗게 번쩍인다.")));
		Descriptions.Add(TEXT("데미지 숫자"), FText::FromString(TEXT("맞은 자리에 떠오르는 데미지 숫자.")));
		Descriptions.Add(TEXT("히트스톱"), FText::FromString(TEXT("맞는 순간 때린 쪽과 맞은 쪽을 아주 잠깐 멈춘다.")));
		Descriptions.Add(TEXT("넉백"), FText::FromString(TEXT("맞으면 뒤로 밀려난다.")));
		Descriptions.Add(TEXT("카메라 흔들림"), FText::FromString(TEXT("때렸을 때 플레이어 카메라가 흔들린다.")));
		Descriptions.Add(TEXT("피격 파티클"), FText::FromString(TEXT("[예전 연출, 기본 끔] 맞은 자리에서 튀는 길쭉한 구 불똥. 지금은 큐브 잔해가 대신한다.")));
		Descriptions.Add(TEXT("피격 섬광"), FText::FromString(TEXT("[예전 연출, 기본 끔] 맞은 자리의 둥근 섬광. 지금은 히트 스파크 별이 대신한다.")));
		Descriptions.Add(TEXT("충격파 링"), FText::FromString(TEXT("[예전 연출, 기본 끔] 수평으로 퍼지는 구 조각 링.")));
		Descriptions.Add(TEXT("처치 폭발"), FText::FromString(TEXT("[예전 연출, 기본 끔] 처치 시 눈 폭발. 지금은 구체 폭발이 대신한다.")));
		Descriptions.Add(TEXT("사망 연출"), FText::FromString(TEXT("죽으면 몸이 맞은 방향으로 넘어지고, 모자와 무기가 벗겨져 튀어 나가 바닥에서 튕긴다.")));
		Descriptions.Add(TEXT("색종이"), FText::FromString(TEXT("처치 시 터지는 색종이.")));
		Descriptions.Add(TEXT("총 반동"), FText::FromString(TEXT("쏠 때 손과 총이 뒤로 튀었다가 스프링처럼 돌아온다. 연사하면 덜덜 떨림.")));
		Descriptions.Add(TEXT("카메라 반동"), FText::FromString(TEXT("쏠 때 카메라가 위로 살짝 튀었다가 돌아온다.")));
		Descriptions.Add(TEXT("총구 화염"), FText::FromString(TEXT("쏠 때 총구에 번쩍이는 발광 구와 순간 조명.")));
		Descriptions.Add(TEXT("큐브 잔해"), FText::FromString(TEXT("유타폰 큐브: 맞은 표면 안쪽에서 작은 큐브들이 튀어나와 날아가고, 돌고, 바닥에서 튕기고, 작아져 사라진다. 블럭이 부서질 때도 같은 잔해.")));
		Descriptions.Add(TEXT("구체 폭발"), FText::FromString(TEXT("처치·미사일·드럼통 폭발: 섬광 2장 → 불덩이 구체들이 부풀며 흰노랑 → 주황 → 빨강 → 연기로 단계 색이 바뀌고, 바닥에 큐브 충격파 링이 퍼진다.")));
		Descriptions.Add(TEXT("히트 스파크 별"), FText::FromString(TEXT("맞은 자리에 8각 별 3장: 흰색 → 무기 색 → 어둡고 작게. 장마다 회전이 바뀐다.")));
		Descriptions.Add(TEXT("십자 반짝임"), FText::FromString(TEXT("4각 별 두 개가 튀어나와 돌며 작아진다. 무기 전환(총구), 필살기(눈), 크리스탈 블럭, 드럼통 재생.")));
		Descriptions.Add(TEXT("임팩트 프레임"), FText::FromString(TEXT("타격 순간 화면 전체가 2장: 주인공·밝은 이펙트는 검은 실루엣 + 나머지 흰색 → 반전. 펀치, 미사일 폭발, 큰 붕괴, 필살기. I 토글.")));
		Descriptions.Add(TEXT("스미어"), FText::FromString(TEXT("손이 아주 빨리 움직이면 지난 그림과 이번 그림 사이를 늘린 구로 채운다 (펀치할 때 잘 보임).")));
		Descriptions.Add(TEXT("카나다 빛"), FText::FromString(TEXT("각진 가시 12개가 화면 쪽으로 방사: 흰색 → 노랑 → 검은 가시 몇 개. 미사일 폭발, 펀치, 필살기 발사.")));
		Descriptions.Add(TEXT("먼지 링"), FText::FromString(TEXT("높은 곳에서 착지하면 바닥에 납작한 도넛이 퍼지고 먼지 구체가 밀려난다. 블럭이 떨어져 부서질 때도.")));
		Descriptions.Add(TEXT("집중선"), FText::FromString(TEXT("화면 가장자리에서 가운데로 모이는 선. 필살기(최대), 큰 붕괴, Shift 달리기.")));
		Descriptions.Add(TEXT("번개"), FText::FromString(TEXT("4번 번개: 지그재그 줄기 + 가지 2개 + 양 끝 섬광. 그릴 때마다 모양이 새로 바뀐다.")));
		Descriptions.Add(TEXT("미사일"), FText::FromString(TEXT("5번 미사일 일제 사격(이타노 서커스): 느리게 출발해 크게 돌다가 급하게 모인다.")));
		Descriptions.Add(TEXT("미사일 연기"), FText::FromString(TEXT("미사일 뒤에 남는 연기 덩이와 가끔 생기는 도넛 링. 흰 발광 → 흰 연기 → 회색, 위로 떠오르며 사라진다.")));
		Descriptions.Add(TEXT("폴리곤 불꽃"), FText::FromString(TEXT("6번 불꽃: 조준 지점 바닥에 삼각뿔들이 솟는다. 자홍 → 분홍 → 하늘색 끝, 장마다 모양이 바뀐다.")));
		Descriptions.Add(TEXT("펀치"), FText::FromString(TEXT("우클릭 왼손 펀치: 손이 한 프레임에 뻗고(스미어), 맞으면 때린 쪽 히트스톱 + 별 + 카나다 빛 + 임팩트 프레임.")));
		Descriptions.Add(TEXT("필살기"), FText::FromString(TEXT("F: 오른손을 들고 시간 정지(배경 2톤, 집중선, 줌인) → 해제 순간 미사일 일제 사격.")));
		Descriptions.Add(TEXT("달리기"), FText::FromString(TEXT("Shift: 빨라지고 화면 가장자리에 집중선.")));
		Descriptions.Add(TEXT("블럭 지형"), FText::FromString(TEXT("부서진 블럭이 다시 자라는 시간과 연출, 무너지는 블럭, 미사일 크레이터.")));
		Descriptions.Add(TEXT("훈련장"), FText::FromString(TEXT("폭발 드럼통(TNT)과 DPS 허수아비. 드럼통은 구체 폭발 + 큐브 잔해 + 카나다 빛 + 연기 기둥(불덩이가 주황 → 빨강 → 적갈색 연기가 되어 솟고, 아래 덩이부터 작아지며 사라짐), 주변 드럼통 연쇄 폭발. 폭발 수치는 미사일과 별개. 미리보기: 앞에 드럼통을 놓고 1초 뒤 터뜨린다.")));
		Descriptions.Add(TEXT("다수 타격 테스트"), FText::FromString(TEXT("몬스터 체력, 이동 속도, 생성 반경.")));
		Descriptions.Add(TEXT("팀 전투"), FText::FromString(TEXT("레드 vs 블루 팀 전투 수치.")));
	}
	const FText* Found = Descriptions.Find(Category);
	return Found ? *Found : FText::GetEmpty();
}

bool UCRTuningSettings::HasPreview(const FString& Category)
{
	static const TSet<FString> Previews = { TEXT("큐브 잔해"), TEXT("구체 폭발"), TEXT("히트 스파크 별"), TEXT("십자 반짝임"), TEXT("임팩트 프레임"), TEXT("스미어"), TEXT("카나다 빛"), TEXT("먼지 링"), TEXT("집중선"), TEXT("번개"), TEXT("미사일"), TEXT("미사일 연기"), TEXT("폴리곤 불꽃"), TEXT("펀치"), TEXT("필살기"), TEXT("피격 번쩍임"), TEXT("데미지 숫자"), TEXT("히트스톱"), TEXT("넉백"), TEXT("카메라 흔들림"), TEXT("사망 연출"), TEXT("색종이"), TEXT("훈련장"), TEXT("캐릭터 LOD") };
	return Previews.Contains(Category);
}

const FCRTuningItem* UCRTuningSettings::FindItem(FName Name)
{
	return GetItems().FindByPredicate([Name](const FCRTuningItem& Item) { return Item.GetName() == Name; });
}
