#include "Core/SBTuningSettings.h"

float FSBTuningItem::GetValue(const void* Container) const
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

void FSBTuningItem::SetValue(void* Container, float Value) const
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

bool FSBTuningItem::IsBool() const
{
	return Property && Property->IsA<FBoolProperty>();
}

#define SB_TUNE(Cat, Member, LabelText, InMin, InMax, InStep) \
	Items.Add({ FText::FromString(TEXT(Cat)), FText::FromString(TEXT(LabelText)), \
		FindFProperty<FProperty>(USBTuningSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(USBTuningSettings, Member)), \
		InMin, InMax, InStep })

#define SB_TUNE_BOOL(Cat, Member, LabelText) SB_TUNE(Cat, Member, LabelText, 0.f, 1.f, 1.f)

const TArray<FSBTuningItem>& USBTuningSettings::GetItems()
{
	static TArray<FSBTuningItem> Items;
	if (Items.Num() == 0)
	{
		SB_TUNE_BOOL("옵션창", bPauseOnOptions, "옵션창 열면 일시정지");

		SB_TUNE("이동", MoveSpeed, "이동 속도", 100.f, 2000.f, 10.f);
		SB_TUNE("이동", MaxAcceleration, "가속도", 200.f, 20000.f, 100.f);
		SB_TUNE("이동", TurnSpeed, "회전 속도", 1.f, 40.f, 0.5f);

		SB_TUNE("카메라", MouseSensitivity, "마우스 감도", 0.1f, 5.f, 0.05f);
		SB_TUNE_BOOL("카메라", bInvertMouseY, "마우스 Y 반전");
		SB_TUNE("카메라", ViewBlendSpeed, "시점 전환 속도", 1.f, 30.f, 0.5f);
		SB_TUNE("카메라", ShoulderArmLength, "숄더뷰 거리", 100.f, 1000.f, 10.f);
		SB_TUNE("카메라", ShoulderOffsetY, "숄더뷰 좌우 오프셋", -200.f, 200.f, 5.f);
		SB_TUNE("카메라", ShoulderOffsetZ, "숄더뷰 높이 오프셋", 0.f, 300.f, 5.f);
		SB_TUNE("카메라", ShoulderFOV, "숄더뷰 FOV", 50.f, 120.f, 1.f);
		SB_TUNE("카메라", TopDownArmLength, "탑다운 거리", 400.f, 4000.f, 50.f);
		SB_TUNE("카메라", TopDownPitch, "탑다운 각도", 30.f, 89.f, 1.f);
		SB_TUNE("카메라", TopDownFOV, "탑다운 FOV", 30.f, 120.f, 1.f);

		SB_TUNE("손", HandBobAmplitude, "손 흔들림 크기", 0.f, 30.f, 0.5f);
		SB_TUNE("손", HandBobSpeed, "손 흔들림 속도", 0.f, 10.f, 0.1f);

		SB_TUNE("전투", MaxHealth, "최대 체력", 1.f, 5000.f, 10.f);
		SB_TUNE("전투", RespawnDelay, "쓰러진 뒤 부활까지(초)", 0.f, 10.f, 0.1f);

		SB_TUNE("칼", SwordDamage, "데미지", 0.f, 200.f, 1.f);
		SB_TUNE("칼", SwordInterval, "공격 간격(초)", 0.05f, 2.f, 0.01f);
		SB_TUNE("칼", SwordSwingDuration, "휘두르는 시간(초)", 0.05f, 1.f, 0.01f);
		SB_TUNE("칼", SwordArcAngle, "휘두르는 각도", 30.f, 300.f, 5.f);
		SB_TUNE("칼", SwordLength, "칼 길이", 30.f, 300.f, 5.f);
		SB_TUNE("칼", SwordHitRadius, "판정 두께", 2.f, 60.f, 1.f);
		SB_TUNE("칼", SwordKnockback, "넉백 세기", 0.f, 3000.f, 10.f);
		SB_TUNE("칼", SwordHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		SB_TUNE("칼", SwordShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		SB_TUNE("레이저총", LaserDamage, "데미지", 0.f, 200.f, 1.f);
		SB_TUNE("레이저총", LaserPierce, "관통 수", 0.f, 50.f, 1.f);
		SB_TUNE("레이저총", LaserInterval, "연사 간격(초)", 0.02f, 1.f, 0.005f);
		SB_TUNE("레이저총", LaserRange, "사거리", 500.f, 20000.f, 100.f);
		SB_TUNE("레이저총", LaserBeamDuration, "빔 표시 시간(초)", 0.01f, 0.5f, 0.005f);
		SB_TUNE("레이저총", LaserBeamWidth, "빔 두께", 0.5f, 30.f, 0.5f);
		SB_TUNE("레이저총", LaserKnockback, "넉백 세기", 0.f, 3000.f, 10.f);
		SB_TUNE("레이저총", LaserHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		SB_TUNE("레이저총", LaserShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		SB_TUNE("투사체총", ProjectileDamage, "데미지", 0.f, 200.f, 1.f);
		SB_TUNE("투사체총", ProjectilePierce, "관통 수", 0.f, 50.f, 1.f);
		SB_TUNE("투사체총", ProjectileInterval, "연사 간격(초)", 0.02f, 2.f, 0.01f);
		SB_TUNE("투사체총", ProjectileSpeed, "탄속", 200.f, 15000.f, 50.f);
		SB_TUNE("투사체총", ProjectileGravity, "중력 배율", 0.f, 3.f, 0.05f);
		SB_TUNE("투사체총", ProjectileLifetime, "수명(초)", 0.2f, 10.f, 0.1f);
		SB_TUNE("투사체총", ProjectileSize, "탄 크기", 0.2f, 5.f, 0.1f);
		SB_TUNE("투사체총", ProjectileKnockback, "넉백 세기", 0.f, 3000.f, 10.f);
		SB_TUNE("투사체총", ProjectileHitStop, "히트스톱(초)", 0.f, 0.5f, 0.005f);
		SB_TUNE("투사체총", ProjectileShake, "카메라 흔들림 배율", 0.f, 5.f, 0.05f);

		SB_TUNE_BOOL("타격감", bHitFlash, "피격 번쩍임");
		SB_TUNE("타격감", HitFlashDuration, "번쩍임 시간(초)", 0.01f, 1.f, 0.01f);
		SB_TUNE("타격감", HitFlashIntensity, "번쩍임 밝기", 0.f, 50.f, 0.5f);
		SB_TUNE_BOOL("타격감", bDamageNumbers, "데미지 숫자");
		SB_TUNE("타격감", DamageNumberDuration, "숫자 표시 시간(초)", 0.1f, 3.f, 0.05f);
		SB_TUNE("타격감", DamageNumberRise, "숫자 떠오르는 높이", 0.f, 300.f, 5.f);
		SB_TUNE("타격감", DamageNumberScale, "숫자 크기", 0.3f, 5.f, 0.1f);
		SB_TUNE_BOOL("타격감", bHitStop, "히트스톱");
		SB_TUNE_BOOL("타격감", bHitStopGlobal, "히트스톱 화면 전체 정지");
		SB_TUNE("타격감", HitStopTimeScale, "히트스톱 중 시간 배율", 0.f, 1.f, 0.01f);
		SB_TUNE_BOOL("타격감", bKnockback, "넉백");
		SB_TUNE("타격감", KnockbackUpRatio, "넉백 위쪽 비율", 0.f, 2.f, 0.05f);
		SB_TUNE_BOOL("타격감", bCameraShake, "카메라 흔들림");
		SB_TUNE("타격감", CameraShakeIntensity, "흔들림 세기(도)", 0.f, 10.f, 0.1f);
		SB_TUNE("타격감", CameraShakeDuration, "흔들림 시간(초)", 0.02f, 1.f, 0.01f);
		SB_TUNE("타격감", CameraShakeFrequency, "흔들림 빈도", 1.f, 80.f, 1.f);
		SB_TUNE_BOOL("타격감", bHitParticles, "피격 파티클");
		SB_TUNE("타격감", HitParticleCount, "파티클 개수", 0.f, 64.f, 1.f);
		SB_TUNE("타격감", HitParticleSpeed, "파티클 속도", 0.f, 3000.f, 10.f);
		SB_TUNE("타격감", HitParticleLifetime, "파티클 수명(초)", 0.05f, 2.f, 0.01f);
		SB_TUNE("타격감", HitParticleSize, "파티클 크기", 0.01f, 1.f, 0.01f);
		SB_TUNE_BOOL("타격감", bHitBurst, "피격 섬광");
		SB_TUNE("타격감", HitBurstSize, "섬광 크기", 0.05f, 3.f, 0.05f);
		SB_TUNE("타격감", HitBurstDuration, "섬광 시간(초)", 0.01f, 0.5f, 0.01f);

		SB_TUNE_BOOL("사격 반동", bWeaponRecoil, "총 반동 (킥백)");
		SB_TUNE("사격 반동", RecoilKickback, "킥백 거리", 0.f, 60.f, 0.5f);
		SB_TUNE("사격 반동", RecoilPitch, "총구 들림(도)", 0.f, 60.f, 0.5f);
		SB_TUNE("사격 반동", RecoilStiffness, "복귀 스프링 강도", 10.f, 2000.f, 10.f);
		SB_TUNE("사격 반동", RecoilDamping, "복귀 감쇠", 0.f, 100.f, 0.5f);
		SB_TUNE("사격 반동", RecoilJitter, "연사 떨림", 0.f, 10.f, 0.1f);
		SB_TUNE_BOOL("사격 반동", bCameraRecoil, "카메라 반동");
		SB_TUNE("사격 반동", CameraRecoilPitch, "카메라 반동(도/발)", 0.f, 5.f, 0.05f);
		SB_TUNE("사격 반동", CameraRecoilRecover, "카메라 반동 복귀 속도", 0.5f, 40.f, 0.5f);
		SB_TUNE_BOOL("사격 반동", bMuzzleFlash, "총구 화염");
		SB_TUNE("사격 반동", MuzzleFlashDuration, "화염 시간(초)", 0.01f, 0.3f, 0.005f);
		SB_TUNE("사격 반동", MuzzleLightIntensity, "화염 빛 세기", 0.f, 50000.f, 500.f);

		SB_TUNE("다수 타격 테스트", MonsterMoveSpeed, "몬스터 이동 속도 (0 = 정지)", 0.f, 1000.f, 10.f);
		SB_TUNE("다수 타격 테스트", SpawnRadius, "생성 반경", 300.f, 3800.f, 50.f);
	}
	return Items;
}

#undef SB_TUNE_BOOL
#undef SB_TUNE

const FSBTuningItem* USBTuningSettings::FindItem(FName Name)
{
	return GetItems().FindByPredicate([Name](const FSBTuningItem& Item) { return Item.GetName() == Name; });
}
