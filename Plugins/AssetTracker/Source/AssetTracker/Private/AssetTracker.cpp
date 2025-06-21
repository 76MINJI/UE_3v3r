// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTracker.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/Material.h"
#include "EngineUtils.h"  // TActorIterator를 위해 필요
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSample.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"                 // FBase64::Decode


#define LOCTEXT_NAMESPACE "FAssetTrackerModule"

void FAssetTrackerModule::StartupModule()
{
    LoadMetaJson();
    TagExistingAssets();

    if (GIsEditor && !IsRunningCommandlet())
    {
        if (GEditor)
        {
            // 액터 이벤트들
            GEditor->OnActorMoved().AddRaw(this, &FAssetTrackerModule::OnActorMoved);

            // 레벨 액터 추가/삭제 델리게이트 추가
            if (GEngine)
            {
                GEngine->OnLevelActorAdded().AddRaw(this, &FAssetTrackerModule::OnActorAdded);
                GEngine->OnLevelActorDeleted().AddRaw(this, &FAssetTrackerModule::OnActorDeleted);
            }
        }

        FEditorDelegates::OnAssetPostImport.AddRaw(this, &FAssetTrackerModule::OnAssetImported);
        FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FAssetTrackerModule::OnObjectPropertyChanged);
    }

    UE_LOG(LogTemp, Warning, TEXT("AssetTracker: Startup complete, %d entries loaded"), MetaMap.Num());

    // 디버깅: 로드된 uuid와 chatId 출력
    for (const auto& Pair : MetaMap)
    {
        const FString& Filename = Pair.Key;
        const FMetaEntry& Entry = Pair.Value;

        UE_LOG(LogTemp, Log,
            TEXT("AssetTracker: Loaded meta -> %s : uuid=%s, chatId=%d"),
            *Filename,
            *Entry.Uuid,
            Entry.ChatId
        );
    }
}

void FAssetTrackerModule::ShutdownModule()
{
    FEditorDelegates::OnAssetPostImport.RemoveAll(this);
    FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);


    if (GIsEditor)
    {
        if (GEditor)
        {
            if (GEditor->OnActorMoved().IsBoundToObject(this))
            {
                GEditor->OnActorMoved().RemoveAll(this);
            }
        }
        if (GEngine)
        {
            if (GEngine->OnLevelActorAdded().IsBoundToObject(this))
            {
                GEngine->OnLevelActorAdded().RemoveAll(this);
            }
            if (GEngine->OnLevelActorDeleted().IsBoundToObject(this))
            {
                GEngine->OnLevelActorDeleted().RemoveAll(this);
            }
        }
    }
}

void FAssetTrackerModule::OnActorMoved(AActor* Actor)
{
    if (!Actor) return;

    FString UUID = GetUUIDFromActorMaterials(Actor);
    if (!UUID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s moved — UUID: %s — Location: %s"),
            *Actor->GetName(), *UUID, *Actor->GetActorLocation().ToCompactString());
    }
    //if (!Actor) return;

    //FString UUID = GetUUIDFromActorMaterials(Actor);
    //if (UUID.IsEmpty()) return;

    //FTransform CurrentTransform = Actor->GetActorTransform();
    //FTransform* LastTransform = PreviousActorTransforms.Find(Actor);

    //if (!LastTransform)
    //{
    //   PreviousActorTransforms.Add(Actor, CurrentTransform);
    //    return;
    //}

    // 변경 항목 판별
    //const double Tolerance = 0.01;
    //bool bLocationChanged = !CurrentTransform.GetLocation().Equals(LastTransform->GetLocation(), Tolerance);
    //bool bRotationChanged = !CurrentTransform.GetRotation().Rotator().Equals(LastTransform->GetRotation().Rotator(), Tolerance);
    //bool bScaleChanged = !CurrentTransform.GetScale3D().Equals(LastTransform->GetScale3D(), Tolerance);

    // 로그 및 전송
    //FString TimeStr = FDateTime::UtcNow().ToIso8601();

    //if (bLocationChanged)
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s location changed — UUID: %s — Pos: %s — Time: %s"),
    //        *Actor->GetName(), *UUID, *CurrentTransform.GetLocation().ToCompactString(), *TimeStr);
    //    SendActorTrackLog(Actor, UUID, TEXT("RelativeLocation"));
    //}

    //if (bRotationChanged)
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s rotation changed — UUID: %s — Rot: %s — Time: %s"),
    //        *Actor->GetName(), *UUID, *CurrentTransform.GetRotation().Rotator().ToCompactString(), *TimeStr);
    //    SendActorTrackLog(Actor, UUID, TEXT("RelativeRotation"));
    //}

    //if (bScaleChanged)
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s scale changed — UUID: %s — Scale: %s — Time: %s"),
    //        *Actor->GetName(), *UUID, *CurrentTransform.GetScale3D().ToCompactString(), *TimeStr);
    //    SendActorTrackLog(Actor, UUID, TEXT("RelativeScale3D"));
    //}

    // 캐시 업데이트
    //PreviousActorTransforms[Actor] = CurrentTransform;
}

void FAssetTrackerModule::OnActorAdded(AActor* Actor)
{
    if (!Actor) return;

    // 약간의 지연 후 체크 (액터가 완전히 초기화된 후)
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, Actor]()
        {
            if (IsValid(Actor))
            {
                FString UUID = GetUUIDFromActorMaterials(Actor);
                if (!UUID.IsEmpty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s added — UUID: %s — Location: %s"),
                        *Actor->GetName(), *UUID, *Actor->GetActorLocation().ToCompactString());
                }
            }
        }, 0.1f, false);
}

void FAssetTrackerModule::OnActorDeleted(AActor* Actor)
{
    if (!Actor) return;

    FString UUID = GetUUIDFromActorMaterials(Actor);
    if (!UUID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s deleted — UUID: %s"),
            *Actor->GetName(), *UUID);
    }
}

void FAssetTrackerModule::OnMaterialUsageChanged()
{
    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] Material usage changed - scanning all actors"));

    // 현재 레벨의 모든 액터를 스캔
    if (UWorld* World = GEditor->GetEditorWorldContext().World())
    {
        for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
        {
            AActor* Actor = *ActorItr;
            if (Actor)
            {
                FString UUID = GetUUIDFromActorMaterials(Actor);
                if (!UUID.IsEmpty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] Found AI asset usage in %s — UUID: %s"),
                        *Actor->GetName(), *UUID);
                }
            }
        }
    }
}

void FAssetTrackerModule::LoadMetaJson()
{
    FString JsonPath = FPaths::ProjectContentDir() / TEXT("segments.json");
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("AssetTracker: segments.json not found at %s"), *JsonPath);
        return;
    }

    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    if (!FJsonSerializer::Deserialize(Reader, JsonArray))
    {
        UE_LOG(LogTemp, Warning, TEXT("AssetTracker: Failed to parse segments.json"));
        return;
    }

    MetaMap.Empty();  // 이전 내용이 있다면 초기화

    for (auto& Entry : JsonArray)
    {
        if (TSharedPtr<FJsonObject> Obj = Entry->AsObject())
        {
            FString UUID = Obj->GetStringField(TEXT("uuid"));
            //FString Filename = Obj->GetStringField(TEXT("filename"));

            // chatId (문자열이든 숫자든, JSON에 "chatId": 8 혹은 "8" 둘 다 처리)
            int32 ChatId = 0;
            if (Obj->TryGetNumberField(TEXT("chatId"), ChatId) == false)
            {
                ChatId = FCString::Atoi(*Obj->GetStringField(TEXT("chatId")));
            }
            FString Base64Data = Obj->GetStringField(TEXT("base64Image"));

            TArray<uint8> ImageBytes;
            FBase64::Decode(Base64Data, ImageBytes);

            // MetaMap.Add(Name, UUID);
            // 맵에 저장
            FMetaEntry EntryData;
            EntryData.Uuid = UUID;
            EntryData.ChatId = ChatId;
            MetaMap.Add(UUID, EntryData);

            UE_LOG(LogTemp, Log,
                TEXT("AssetTracker: uuid=%s, chatId=%d"),
                *UUID, ChatId
            );
        }
    }
}

void FAssetTrackerModule::TagExistingAssets()
{
    if (MetaMap.Num() == 0) return;

    auto& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    FARFilter Filter;
    Filter.ClassNames.Add(UTexture2D::StaticClass()->GetFName());
    Filter.PackagePaths.Add("/Game");
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    int32 TaggedCount = 0;
    for (auto& Data : AssetList)
    {
        FString AssetName = Data.AssetName.ToString();
        if (MetaMap.Contains(AssetName))
        {
            const FMetaEntry& Entry = MetaMap[AssetName];

            UObject* AssetObj = Data.GetAsset();
            if (AssetObj)
            {
                UEditorAssetLibrary::SetMetadataTag(AssetObj, TEXT("uuid"), Entry.Uuid);

                // chatId 태그 문자열로 저장
                UEditorAssetLibrary::SetMetadataTag(
                    AssetObj,
                    TEXT("chatId"),
                    FString::FromInt(Entry.ChatId)
                );

                TaggedCount++;
                UE_LOG(LogTemp, Log, TEXT("AssetTracker: Tagged existing %s →  uuid:%s, chatId:%d"), *AssetName, *Entry.Uuid, Entry.ChatId);

                // 디버깅: 태그가 제대로 설정되었는지 확인
                FString CheckUuid = UEditorAssetLibrary::GetMetadataTag(AssetObj, TEXT("uuid"));
                FString CheckChatId = UEditorAssetLibrary::GetMetadataTag(AssetObj, TEXT("chatId"));
                UE_LOG(LogTemp, Log, TEXT("AssetTracker: Verification - %s has uuid:%s, chatId:%s"),*AssetName, *CheckUuid, *CheckChatId);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("AssetTracker: Tagged %d assets out of %d found textures"), TaggedCount, AssetList.Num());
}

void FAssetTrackerModule::OnAssetImported(UFactory* Factory, UObject* CreatedObject)
{
    if (!CreatedObject->IsA<UTexture2D>()) return;

    FString AssetName = CreatedObject->GetName();
    if (MetaMap.Contains(AssetName))
    {   
        const FMetaEntry& Entry = MetaMap[AssetName];

        UEditorAssetLibrary::SetMetadataTag(CreatedObject, TEXT("uuid"), Entry.Uuid);
        UEditorAssetLibrary::SetMetadataTag(CreatedObject, TEXT("chatId"), FString::FromInt(Entry.ChatId));

        UE_LOG(LogTemp, Log, TEXT("AssetTracker: Tagged imported %s → uuid:%s, chatId:%d"), *AssetName, *Entry.Uuid, Entry.ChatId);
    }
}

void FAssetTrackerModule::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
    if (!Object) return;

    FString ObjectClass = Object->GetClass()->GetName();
    FString Property = PropertyChangedEvent.GetPropertyName().ToString();

    UE_LOG(LogTemp, Log, TEXT(">> [Debug] Property Changed — Object: %s (%s), Property: %s"),
        *Object->GetName(), *ObjectClass, *Property);

    AActor* Actor = nullptr;
    if (Object->IsA<AActor>())
    {
        Actor = Cast<AActor>(Object);
    }
    else if (UActorComponent* Comp = Cast<UActorComponent>(Object))
    {
        Actor = Comp->GetOwner();
    }
    else if (UMaterialInterface* Mat = Cast<UMaterialInterface>(Object))
    {
        // 머티리얼이 변경된 경우, 해당 머티리얼을 사용하는 모든 액터를 찾아서 로그
        CheckMaterialUsageInLevel(Mat);
        return;
    }

    if (!Actor) return;

    //FString UUID = GetUUIDFromActorMaterials(Actor);
    //if (!UUID.IsEmpty())
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s changed (%s) — UUID: %s — Location: %s"),
    //        *Actor->GetName(), *Property, *UUID, *Actor->GetActorLocation().ToCompactString());
    //}
    FString UUID = GetUUIDFromActorMaterials(Actor);
    if (UUID.IsEmpty()) return;

    // 해당 UUID에 대응하는 chatId 찾기
    int32 ChatId = 0;
    for (const auto& Pair : MetaMap)
    {
        const FMetaEntry& Entry = Pair.Value;
        if (Entry.Uuid == UUID)
        {
            ChatId = Entry.ChatId;
            break;
        }
    }

    // Transform 변경 감지 블록 삽입
    FTransform CurrentTransform = Actor->GetActorTransform();
    FTransform* LastTransform = PreviousActorTransforms.Find(Actor);

    if (!LastTransform)
    {
        PreviousActorTransforms.Add(Actor, CurrentTransform);
        return;
    }

    const double Tolerance = 0.01;
    bool bLocationChanged = !CurrentTransform.GetLocation().Equals(LastTransform->GetLocation(), Tolerance);
    bool bRotationChanged = !CurrentTransform.GetRotation().Rotator().Equals(LastTransform->GetRotation().Rotator(), Tolerance);
    bool bScaleChanged = !CurrentTransform.GetScale3D().Equals(LastTransform->GetScale3D(), Tolerance);

    FString TimeStr = FDateTime::UtcNow().ToIso8601();

    if (bLocationChanged)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrackLogGG] %s location changed — UUID: %s — Pos: %s — Time: %s"),
            *Actor->GetName(), *UUID, *CurrentTransform.GetLocation().ToCompactString(), *TimeStr);
        SendActorTrackLog(Actor, UUID, ChatId, TEXT("RelativeLocation"));
    }

    if (bRotationChanged)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s rotation changed — UUID: %s — Rot: %s — Time: %s"),
            *Actor->GetName(), *UUID, *CurrentTransform.GetRotation().Rotator().ToCompactString(), *TimeStr);
        SendActorTrackLog(Actor, UUID, ChatId, TEXT("RelativeRotation"));
    }

    if (bScaleChanged)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrackLog] %s scale changed — UUID: %s — Scale: %s — Time: %s"),
            *Actor->GetName(), *UUID, *CurrentTransform.GetScale3D().ToCompactString(), *TimeStr);
        SendActorTrackLog(Actor, UUID, ChatId, TEXT("RelativeScale3D"));
    }

    // 캐시 갱신
    PreviousActorTransforms[Actor] = CurrentTransform;
}

void FAssetTrackerModule::CheckMaterialUsageInLevel(UMaterialInterface* Material)
{
    if (!Material) return;

    // 해당 머티리얼이 AI 에셋을 사용하는지 확인
    FString UUID = GetUUIDFromMaterial(Material);
    if (UUID.IsEmpty()) return;

    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] Material %s (UUID: %s) was modified"), *Material->GetName(), *UUID);

    // MaterialInstance 검사
    if (UMaterialInstance* MatInst = Cast<UMaterialInstance>(Material))
    {
        UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Processing MaterialInstance"));

        TArray<FMaterialParameterInfo> ParamInfos;
        TArray<FGuid> ParamGuids;
        MatInst->GetAllTextureParameterInfo(ParamInfos, ParamGuids);

        UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Found %d texture parameters"), ParamInfos.Num());

        for (const FMaterialParameterInfo& Info : ParamInfos)
        {
            UTexture* Tex = nullptr;
            if (MatInst->GetTextureParameterValue(Info, Tex) && Tex)
            {
                UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Checking texture parameter %s -> texture %s"),
                    *Info.Name.ToString(), *Tex->GetName());

                FString FoundUUID = UEditorAssetLibrary::GetMetadataTag(Tex, TEXT("uuid"));
                UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Texture %s UUID: '%s'"),
                    *Tex->GetName(), *FoundUUID);

                if (!FoundUUID.IsEmpty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("Found UUID %s in MaterialInstance %s, parameter %s, texture %s"),
                        *FoundUUID, *MatInst->GetName(), *Info.Name.ToString(), *Tex->GetName());
                    return;
                }
            }
        }
    }

    // 일반 Material 검사
    UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Processing base Material"));

    TArray<UTexture*> Textures;
    Material->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, false);

    UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Found %d used textures"), Textures.Num());

    for (UTexture* Tex : Textures)
    {
        if (!Tex) continue;

        UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Checking used texture %s"), *Tex->GetName());

        FString FoundUUID = UEditorAssetLibrary::GetMetadataTag(Tex, TEXT("uuid"));
        UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: Texture %s UUID: '%s'"),
            *Tex->GetName(), *FoundUUID);

        if (!FoundUUID.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("Found UUID %s in Material %s, texture %s"),
                *FoundUUID, *Material->GetName(), *Tex->GetName());
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("GetUUIDFromMaterial: No UUID found in material %s"), *Material->GetName());
    return;

}

bool FAssetTrackerModule::IsActorUsingMaterial(AActor* Actor, UMaterialInterface* Material)
{
    if (!Actor || !Material) return false;

    TArray<UActorComponent*> Components = Actor->GetComponents().Array();
    for (UActorComponent* Comp : Components)
    {
        UMeshComponent* Mesh = Cast<UMeshComponent>(Comp);
        if (!Mesh) continue;

        for (int i = 0; i < Mesh->GetNumMaterials(); ++i)
        {
            UMaterialInterface* ActorMat = Mesh->GetMaterial(i);
            if (ActorMat == Material)
            {
                return true;
            }
        }
    }
    return false;
}

FString FAssetTrackerModule::GetUUIDFromMaterial(UMaterialInterface* Material)
{
    if (!Material) return FString();

    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] GetUUIDFromMaterial: Analyzing material: %s (%s)"),
        *Material->GetName(), *Material->GetClass()->GetName());

    // ① MaterialInstance 먼저 검사
    if (UMaterialInstance* MatInst = Cast<UMaterialInstance>(Material))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] → Checking as MaterialInstance"));

        TArray<FMaterialParameterInfo> ParamInfos;
        TArray<FGuid> ParamGuids;
        MatInst->GetAllTextureParameterInfo(ParamInfos, ParamGuids);

        for (const FMaterialParameterInfo& Info : ParamInfos)
        {
            UTexture* Tex = nullptr;
            if (MatInst->GetTextureParameterValue(Info, Tex) && Tex)
            {
                FString UUID = UEditorAssetLibrary::GetMetadataTag(Tex, TEXT("uuid"));
                if (!UUID.IsEmpty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] ✅ Found UUID %s in MaterialInstance %s via param %s"),
                        *UUID, *MatInst->GetName(), *Info.Name.ToString());
                    return UUID;
                }
            }
        }
    }

    // ② Base Material에서 GetUsedTextures 시도
    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] → Checking as base Material"));

    TArray<UTexture*> Textures;
    Material->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, false);
    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Found %d used textures"), Textures.Num());

    for (UTexture* Tex : Textures)
    {
        FString UUID = UEditorAssetLibrary::GetMetadataTag(Tex, TEXT("uuid"));
        UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] - UsedTexture: %s → %s"), *Tex->GetName(), *UUID);
        if (!UUID.IsEmpty())
        {
            return UUID;
        }
    }

    // ③ Material Graph 내부 Expression 수동 탐색 (💥 핵심)
#if WITH_EDITOR
    if (UMaterial* BaseMat = Cast<UMaterial>(Material))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] → Scanning Expressions (manual)"));

        UMaterialEditorOnlyData* EditorOnlyData = BaseMat->GetEditorOnlyData();
        if (EditorOnlyData)
        {
            for (UMaterialExpression* Expr : EditorOnlyData->ExpressionCollection.Expressions)
            {
                if (UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expr))
                {
                    UTexture* Tex = TextureSample->Texture;
                    if (Tex)
                    {
                        FString UUID = UEditorAssetLibrary::GetMetadataTag(Tex, TEXT("uuid"));
                        UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] - Expression Texture: %s → %s"), *Tex->GetName(), *UUID);

                        if (!UUID.IsEmpty())
                        {
                            return UUID;
                        }
                    }
                }
            }
        }
    }
#endif

    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] ❌ No UUID found in material: %s"), *Material->GetName());
    return FString();
}


FString FAssetTrackerModule::GetUUIDFromActorMaterials(AActor* Actor)
{
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Actor is null"));
        return FString();
    }

    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Analyzing actor: %s"), *Actor->GetName());

    // StaticMeshActor 우선 처리
    if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor))
    {
        if (UStaticMeshComponent* SMComp = SMActor->GetStaticMeshComponent())
        {
            int32 MaterialCount = SMComp->GetNumMaterials();
            UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] StaticMeshComponent has %d materials"), MaterialCount);

            for (int i = 0; i < MaterialCount; ++i)
            {
                UMaterialInterface* Mat = SMComp->GetMaterial(i);
                if (!Mat)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Material %d is null"), i);
                    continue;
                }

                UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Checking StaticMesh material %d: %s (%s)"),
                    i, *Mat->GetName(), *Mat->GetClass()->GetName());

                FString UUID = GetUUIDFromMaterial(Mat);
                if (!UUID.IsEmpty())
                {
                    return UUID;
                }
            }
        }
    }

    // 기타 모든 MeshComponent 탐색
    TArray<UActorComponent*> Components = Actor->GetComponents().Array();
    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Actor has %d components"), Components.Num());

    for (UActorComponent* Comp : Components)
    {
        UMeshComponent* Mesh = Cast<UMeshComponent>(Comp);
        if (!Mesh) continue;

        int32 MatCount = Mesh->GetNumMaterials();
        UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] MeshComponent: %s has %d materials"), *Mesh->GetName(), MatCount);

        for (int i = 0; i < MatCount; ++i)
        {
            UMaterialInterface* Mat = Mesh->GetMaterial(i);
            if (!Mat)
            {
                UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Material %d is null"), i);
                continue;
            }

            UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] Checking material %d: %s (%s)"),
                i, *Mat->GetName(), *Mat->GetClass()->GetName());

            FString UUID = GetUUIDFromMaterial(Mat);
            if (!UUID.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] ✅ UUID found in actor %s: %s"),
                    *Actor->GetName(), *UUID);
                return UUID;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[UUID DEBUG] ❌ No UUID found for actor: %s"), *Actor->GetName());
    return FString();
}


UWorld* FAssetTrackerModule::GetWorld() const
{
    if (GEditor)
    {
        return GEditor->GetEditorWorldContext().World();
    }
    return nullptr;
}

void FAssetTrackerModule::SendActorTrackLog(AActor* Actor, const FString& UUID, int32 ChatId, const FString& Property)
{
    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] Enter: %s / %s / %d / %s"),
        *Actor->GetName(), *UUID, ChatId, *Property);

    if (!Actor || UUID.IsEmpty()) return;

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField("actorName", Actor->GetName());
    JsonObject->SetStringField("uuid", UUID);
    JsonObject->SetNumberField(TEXT("chatId"), ChatId);
    JsonObject->SetStringField("changeType", Property);

    // 변경 시간 (ISO 8601 형식)
    FString Timestamp = FDateTime::UtcNow().ToIso8601();
    JsonObject->SetStringField("timestamp", Timestamp);

   /* if (Property == "RelativeLocation")
    {
       FVector Loc = Actor->GetActorLocation();
        TSharedPtr<FJsonObject> LocObj = MakeShareable(new FJsonObject);
        LocObj->SetNumberField("x", Loc.X);
        LocObj->SetNumberField("y", Loc.Y);
        LocObj->SetNumberField("z", Loc.Z);
        JsonObject->SetObjectField("location", LocObj);
    }
    else if (Property == "RelativeRotation")
    {
        FRotator Rot = Actor->GetActorRotation();
        TSharedPtr<FJsonObject> RotObj = MakeShareable(new FJsonObject);
        RotObj->SetNumberField("pitch", Rot.Pitch);
        RotObj->SetNumberField("yaw", Rot.Yaw);
        RotObj->SetNumberField("roll", Rot.Roll);
        JsonObject->SetObjectField("rotation", RotObj);
    }
    else if (Property == "RelativeScale3D")
    {
        FVector Scale = Actor->GetActorScale3D();
        TSharedPtr<FJsonObject> ScaleObj = MakeShareable(new FJsonObject);
        ScaleObj->SetNumberField("x", Scale.X);
        ScaleObj->SetNumberField("y", Scale.Y);
        ScaleObj->SetNumberField("z", Scale.Z);
        JsonObject->SetObjectField("scale", ScaleObj);
    }*/

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    //FJsonSerializer::Serialize(JsonObject, Writer);



    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

    FString Url = FString::Printf(TEXT("http://13.125.77.82:8080/api/v1/unreal-history/%d"), ChatId);  // TODO : 나중에 /%d 전까지 배포 주소로 수정
    //Request->SetURL(TEXT("https://httpbin.org/post"));
    Request->SetURL(Url);
    //Request->SetURL(TEXT("http://localhost:8080/api/v1/unreal-history/{chatId}"));
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    //Request->SetHeader(TEXT("user-no"), TEXT("4"));

    Request->SetContentAsString(OutputString);

    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] JSON Sent: %s"), *OutputString);
    UE_LOG(LogTemp, Warning, TEXT("[TrackLog] POST to %s"), *Url);

    Request->OnProcessRequestComplete().BindRaw(this, &FAssetTrackerModule::OnHttpResponse);

    Request->ProcessRequest();
}

// 콜백 함수
void FAssetTrackerModule::OnHttpResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bWasSuccessful
)
{
    if (bWasSuccessful && Response.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("HTTP Success: %d %s"),
            Response->GetResponseCode(),
            *Response->GetContentAsString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HTTP Failed"));
    }

    // 응답 상태 코드 가져옴
    int32 StatusCode = Response->GetResponseCode();

    // 응답 본문을 문자열로 가져옴
    FString ResponseContent = Response->GetContentAsString();

    // 요청이 완료되었음을 출력
    UE_LOG(LogTemp, Warning, TEXT("HTTP request completed. Status Code: %d, URL: %s"), StatusCode, *Request->GetURL());
    UE_LOG(LogTemp, Warning, TEXT("Response Content: %s"), *ResponseContent);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAssetTrackerModule, AssetTracker)