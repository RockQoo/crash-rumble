#include "Core/SBGameSettings.h"

#include "Materials/MaterialInterface.h"

USBGameSettings::USBGameSettings()
{
	CharacterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBCharacter.M_SBCharacter")));
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBGlow.M_SBGlow")));
}

UMaterialInterface* USBGameSettings::LoadCharacterMaterial()
{
	return GetDefault<USBGameSettings>()->CharacterMaterial.LoadSynchronous();
}

UMaterialInterface* USBGameSettings::LoadGlowMaterial()
{
	return GetDefault<USBGameSettings>()->GlowMaterial.LoadSynchronous();
}
