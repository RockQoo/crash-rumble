#include "Core/CRGameSettings.h"

#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"

UCRGameSettings::UCRGameSettings()
{
	CharacterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBCharacter.M_SBCharacter")));
	ToonMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_CRToon.M_CRToon")));
	ToonParameters = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/Materials/MPC_CRToon.MPC_CRToon")));
	BlobShadowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_CRBlobShadow.M_CRBlobShadow")));
	ScreenFxMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_CRScreenFx.M_CRScreenFx")));
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBGlow.M_SBGlow")));
	ParticleMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SBParticle.M_SBParticle")));
	ModeMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/L_Grid.L_Grid")));
}

UMaterialInterface* UCRGameSettings::LoadParticleMaterial()
{
	return GetDefault<UCRGameSettings>()->ParticleMaterial.LoadSynchronous();
}

UMaterialInterface* UCRGameSettings::LoadCharacterMaterial()
{
	return GetDefault<UCRGameSettings>()->CharacterMaterial.LoadSynchronous();
}

UMaterialInterface* UCRGameSettings::LoadToonMaterial()
{
	return GetDefault<UCRGameSettings>()->ToonMaterial.LoadSynchronous();
}

UMaterialInterface* UCRGameSettings::LoadGlowMaterial()
{
	return GetDefault<UCRGameSettings>()->GlowMaterial.LoadSynchronous();
}

UMaterialInterface* UCRGameSettings::LoadSurfaceMaterial(const UObject* WorldContext)
{
	if (UCRTuningSubsystem::GetSettings(WorldContext).bToonShading)
	{
		if (UMaterialInterface* Toon = LoadToonMaterial())
		{
			return Toon;
		}
	}
	return LoadCharacterMaterial();
}
