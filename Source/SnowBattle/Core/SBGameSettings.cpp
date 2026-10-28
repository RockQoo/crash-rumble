#include "Core/SBGameSettings.h"

#include "Materials/MaterialInterface.h"

USBGameSettings::USBGameSettings()
{
	CharacterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBCharacter.M_SBCharacter")));
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBGlow.M_SBGlow")));
	ParticleMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBParticle.M_SBParticle")));
}

UMaterialInterface* USBGameSettings::LoadParticleMaterial()
{
	return GetDefault<USBGameSettings>()->ParticleMaterial.LoadSynchronous();
}

UMaterialInterface* USBGameSettings::LoadCharacterMaterial()
{
	return GetDefault<USBGameSettings>()->CharacterMaterial.LoadSynchronous();
}

UMaterialInterface* USBGameSettings::LoadGlowMaterial()
{
	return GetDefault<USBGameSettings>()->GlowMaterial.LoadSynchronous();
}
