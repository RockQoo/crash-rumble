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
	}
	return Items;
}

#undef SB_TUNE_BOOL
#undef SB_TUNE

const FSBTuningItem* USBTuningSettings::FindItem(FName Name)
{
	return GetItems().FindByPredicate([Name](const FSBTuningItem& Item) { return Item.GetName() == Name; });
}
