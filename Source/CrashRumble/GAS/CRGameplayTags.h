#pragma once

#include "NativeGameplayTags.h"

namespace CRTags
{
	/** SetByCaller 데미지 양. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	/** SetByCaller 지속시간 (화상 등). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Duration);

	/** 쓰러진 상태. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
}
