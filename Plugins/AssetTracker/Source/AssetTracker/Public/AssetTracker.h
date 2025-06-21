// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Http.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"



// Forward declarations
class UFactory;
class UMaterialInterface;
class AActor;
class UWorld;

struct FMetaEntry
{
    FString Uuid;
    int32   ChatId;
};

class FAssetTrackerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
    void LoadMetaJson();
    void TagExistingAssets();
    void OnAssetImported(UFactory* Factory, UObject* CreatedObject);
    void OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);
    FString GetUUIDFromActorMaterials(AActor* Actor);
    void OnLevelActorModified(AActor* Actor);
    void OnActorMoved(AActor* Actor);
    void OnActorAdded(AActor* Actor);

    TMap<AActor*, FTransform> PreviousActorTransforms;

    FString GetUUIDFromMaterial(UMaterialInterface* Material);
    void OnActorDeleted(AActor* Actor);
    bool IsActorUsingMaterial(AActor* Actor, UMaterialInterface* Material);
    void CheckMaterialUsageInLevel(UMaterialInterface* Material);
    void OnMaterialUsageChanged();

    void SendActorTrackLog(AActor* Actor, const FString& UUID, int32 ChatId, const FString& Property);
    void OnHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    // Utility functions
    UWorld* GetWorld() const;



    TMap<FString, FMetaEntry> MetaMap;
};
