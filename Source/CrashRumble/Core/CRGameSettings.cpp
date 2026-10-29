#include "Core/CRGameSettings.h"

#include "Materials/MaterialInterface.h"

UCRGameSettings::UCRGameSettings()
{
	CharacterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBCharacter.M_SBCharacter")));
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBGlow.M_SBGlow")));
	ParticleMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBParticle.M_SBParticle")));
}

UMaterialInterface* UCRGameSettings::LoadParticleMaterial()
{
	return GetDefault<UCRGameSettings>()->ParticleMaterial.LoadSynchronous();
}

UMaterialInterface* UCRGameSettings::LoadCharacterMaterial()
{
	return GetDefault<UCRGameSettings>()->CharacterMaterial.LoadSynchronous();
}

UMaterialInterface* UCRGameSettings::LoadGlowMaterial()
{
	return GetDefault<UCRGameSettings>()->GlowMaterial.LoadSynchronous();
}
