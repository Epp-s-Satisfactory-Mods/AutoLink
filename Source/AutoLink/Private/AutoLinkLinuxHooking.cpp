#include "AutoLinkLinuxHooking.h"
#include "AutoLinkDebugSettings.h"
#include "AutoLinkLogMacros.h"

#include "FGBuildEffectActor.h"
#include "FGBuildableBeam.h"
#include "FGBuildableSignSupport.h"
#include "FGBuildableConveyorBelt.h"
#include "FGBuildablePipeBase.h"
#include "FGColoredInstanceMeshProxy.h"
#include "FGFactoryLegsComponent.h"

#include "Patching/NativeHookManager.h"

void AutoLinkLinuxHooking::RegisterLinuxOnlyHooks()
{
    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, CreateVisuals, [](auto& scope, AFGBuildEffectActor* self)
        {
            AL_LOG("AFGBuildEffectActor::CreateVisuals START");
            scope.Cancel();
            CreateVisualsCustom(self);
            AL_LOG("AFGBuildEffectActor::CreateVisuals END");
        });
}

// Below here is a custom version of AFGBuildEffectActor::CreateVisuals

template<typename T>
struct FProxyComponent
{
    FProxyComponent() {}
    FProxyComponent(T* mesh, const FTransform& worldTrans, TArray<UMaterialInterface*>& overrideMaterials,
        UClass* defaultAnimState, const TArray<float>& customizationData, AActor* source = nullptr, int32 lightweightIndex = INDEX_NONE, UClass* lightweightClass = nullptr) :
        Mesh(mesh),
        WorldTransform(worldTrans),
        OverrideMaterials(overrideMaterials),
        DefaultAnimState(defaultAnimState),
        PrimitiveData(customizationData),
        Source(source),
        Index(lightweightIndex),
        Class(lightweightClass)
    {
    }

    T* Mesh;
    FTransform WorldTransform;
    TArray<UMaterialInterface*> OverrideMaterials;
    UClass* DefaultAnimState = nullptr;
    TArray<float> PrimitiveData;
    AActor* Source = nullptr;

    // ! Lightweight lookup data.s
    // Lightweight index.
    int32 Index = INDEX_NONE;
    UClass* Class = nullptr;
};

template< typename T >
T* DuplicateComponent(USceneComponent* attachParent, T* templateComponent, const FName& componentName)
{
    T* newComponent = NewObject< T >(attachParent, templateComponent->GetClass(), componentName, RF_NoFlags, templateComponent);
    newComponent->SetMobility(EComponentMobility::Movable);

    return newComponent;
}

void AutoLinkLinuxHooking::CreateVisualsCustom(AFGBuildEffectActor* self)
{
    FBox OutBounds;
    OutBounds.Init();

    self->mVolume = 0;

    // New system.
    TArray<AFGBuildable*> DefaultActors;
    TArray<AFGBuildablePipeBase*> PipeSplineActors;
    TArray<AFGBuildableConveyorBelt*> BeltSplineActors;

    // Gathered meshes to display with the build effect.
    TArray<FProxyComponent<UStaticMesh>> ProxyData;
    TArray<FProxyComponent<USkeletalMesh>> ProxySkelData;

    for (TWeakObjectPtr< AActor > Actor : self->mSourceActors)
    {
        if (AActor* ResolvedActor = Actor.Get())
        {
            if (AFGBuildablePipeBase* PipelineActor = Cast<AFGBuildablePipeBase>(ResolvedActor))
            {
                PipeSplineActors.Add(PipelineActor);
            }
            else if (AFGBuildableConveyorBelt* BeltActor = Cast<AFGBuildableConveyorBelt>(ResolvedActor))
            {
                BeltSplineActors.Add(BeltActor);
            }
            else if (AFGBuildable* DefaultActor = Cast<AFGBuildable>(ResolvedActor))
            {
                DefaultActors.Add(DefaultActor);
            }
        }
    }

    // TODO post 1.0 make this more uniform? this is still the legacy path but not worth reworking during the last week of pre CC
    // Sort and order Belts & Pipes.
    {
        TArray<AActor*> OutSplineBuildings;
        TArray<USplineComponent*> sourceSplines;
        sourceSplines.Append(self->GetBeltSourceSplinesOrdered(BeltSplineActors, OutSplineBuildings));
        sourceSplines.Append(self->GetPipeSourceSplineOrdered(PipeSplineActors, OutSplineBuildings));

        /* BEGIN AUTOLINK FIX */

        /* The Get*SourceSplinesOrdered functions can follow links to outside of the build effect actor. Our hackey fix is
        to use the complete CreateVisuals from the game (huge thanks to Archengius on the Satisfactory Modding Discord) but
        insert code here to simply remove any actors that are not already tracked by the build effect actor.

        See AutoLinkWindowsHooking.cpp for a more detailed explanation of what/why.  But we have to do it like this on Linux
        instead of the way we do it on Windows because Linux crashes on any construction when you try to hook GetBeltSourceSplinesOrdered */

        AL_LOG("BEGIN CUSTOM AUTOLINK CREATEVISUALS FIX")

        sourceSplines = sourceSplines.FilterByPredicate(
            [&](USplineComponent* spline)
            {
                return BeltSplineActors.Contains(spline->GetOwner()) || PipeSplineActors.Contains(spline->GetOwner());
            });

        OutSplineBuildings = OutSplineBuildings.FilterByPredicate(
            [&](AActor* actor)
            {
                return BeltSplineActors.Contains(actor) || PipeSplineActors.Contains(actor);
            });

        AL_LOG("END CUSTOM AUTOLINK CREATEVISUALS FIX")

        /* END AUTOLINK FIX */

        int32 Index = 0;
        for (AActor* sourceActor : OutSplineBuildings)
        {
            if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(sourceActor))
            {
                TArray<float> PrimitiveData = Belt->GetCustomizationData_Native().Data;

                const float splineLength = Belt->GetSplineComponent()->GetSplineLength();
                const int32 numMeshes = FMath::Max(1, FMath::RoundToInt(splineLength / Belt->GetMeshLength()) + 1);
                const USplineComponent* Spline = Belt->GetSplineComponent();
                const float segmentLength = splineLength / numMeshes;

                // Build instances.
                for (int32 i = 0; i < numMeshes; i++)
                {
                    const float startDistance = (float)i * segmentLength;
                    const float endDistance = (float)(i + 1) * segmentLength;

                    const FVector startPos = Spline->GetLocationAtDistanceAlongSpline(startDistance, ESplineCoordinateSpace::World);
                    const FVector startTangent = Spline->GetTangentAtDistanceAlongSpline(startDistance, ESplineCoordinateSpace::World).GetSafeNormal() * segmentLength;
                    const FVector endPos = Spline->GetLocationAtDistanceAlongSpline(endDistance, ESplineCoordinateSpace::World);
                    const FVector endTangent = Spline->GetTangentAtDistanceAlongSpline(endDistance, ESplineCoordinateSpace::World).GetSafeNormal() * segmentLength;

                    USplineMeshComponent* SplineSegment = NewObject<USplineMeshComponent>(self);
                    SplineSegment->SetWorldTransform(FTransform::Identity);
                    SplineSegment->bAffectDistanceFieldLighting = false;
                    SplineSegment->SetMobility(EComponentMobility::Movable);
                    SplineSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    SplineSegment->SetStaticMesh(Belt->GetSplineMesh());
                    SplineSegment->SetStartAndEnd(startPos, startTangent, endPos, endTangent);
                    self->ResolveMaterial(SplineSegment, TArray<UMaterialInterface*>());

                    for (int32 j = 0; j < PrimitiveData.Num(); j++)
                    {
                        SplineSegment->SetCustomPrimitiveDataFloat(j, PrimitiveData[j]);
                    }

                    SplineSegment->SetScalarParameterValueOnMaterials(FName("Index"), Index);
                    SplineSegment->SetForcedLodModel(1);
                    SplineSegment->RegisterComponent();

                    // add mesh to spline components.
                    self->mSplineBuildableComponents.Add(SplineSegment);

                    if (sourceActor)
                    {
                        TArray< UMeshComponent* >& Target = self->mActorToComponentsMap.FindOrAdd(sourceActor);
                        Target.Add(SplineSegment);
                    }

                    Index++;
                }
            }
            else if (AFGBuildablePipeBase* Pipe = Cast<AFGBuildablePipeBase>(sourceActor))
            {
                TArray<float> PrimitiveData = Pipe->GetCustomizationData_Native().Data;

                const float splineLength = Pipe->GetSplineComponent()->GetSplineLength();
                const int32 numMeshes = FMath::Max(1, FMath::RoundToInt(splineLength / Pipe->GetMeshLength()) + 1);
                const USplineComponent* Spline = Pipe->GetSplineComponent();
                const float segmentLength = splineLength / numMeshes;

                // Build instances.
                for (int32 i = 0; i < numMeshes; i++)
                {
                    const float startDistance = (float)i * segmentLength;
                    const float endDistance = (float)(i + 1) * segmentLength;

                    const FVector startPos = Spline->GetLocationAtDistanceAlongSpline(startDistance, ESplineCoordinateSpace::World);
                    const FVector startTangent = Spline->GetTangentAtDistanceAlongSpline(startDistance, ESplineCoordinateSpace::World).GetSafeNormal() * segmentLength;
                    const FVector endPos = Spline->GetLocationAtDistanceAlongSpline(endDistance, ESplineCoordinateSpace::World);
                    const FVector endTangent = Spline->GetTangentAtDistanceAlongSpline(endDistance, ESplineCoordinateSpace::World).GetSafeNormal() * segmentLength;

                    USplineMeshComponent* SplineSegment = NewObject<USplineMeshComponent>(self);
                    SplineSegment->SetWorldTransform(FTransform::Identity);
                    SplineSegment->bAffectDistanceFieldLighting = false;
                    SplineSegment->SetMobility(EComponentMobility::Movable);
                    SplineSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    SplineSegment->SetStaticMesh(Pipe->GetSplineMesh());
                    SplineSegment->SetStartAndEnd(startPos, startTangent, endPos, endTangent);
                    self->ResolveMaterial(SplineSegment, { Pipe->mSplineMeshMaterial });

                    for (int32 j = 0; j < PrimitiveData.Num(); j++)
                    {
                        SplineSegment->SetCustomPrimitiveDataFloat(j, PrimitiveData[j]);
                    }

                    SplineSegment->SetScalarParameterValueOnMaterials(FName("Index"), Index);
                    SplineSegment->SetForcedLodModel(1);
                    SplineSegment->RegisterComponent();

                    // add mesh to spline components.
                    self->mSplineBuildableComponents.Add(SplineSegment);
                    Index++;

                    if (Pipe)
                    {
                        TArray< UMeshComponent* >& Target = self->mActorToComponentsMap.FindOrAdd(Pipe);
                        Target.Add(SplineSegment);
                    }
                }
            }
        }

        // dupe spline separately, these should have the same order as the generated components.
        for (const auto splineComponent : sourceSplines)
        {
            USplineComponent* Spline = DuplicateComponent<USplineComponent>(self->GetRootComponent(), splineComponent, NAME_None);
            Spline->AttachToComponent(self->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
            Spline->SetWorldTransform(splineComponent->GetComponentTransform());
            Spline->RegisterComponent();
            self->mOrganizedSplines.Add(Spline);
        }
    }
    // Handle lightweight actor instances.
    for (const auto& LightweightActor : self->LightweightActors)
    {
        const FRuntimeBuildableInstanceData& RuntimeData = LightweightActor.RuntimeData;
        for (auto InstanceData : LightweightActor.Instances->GetInstanceData())
        {
            FProxyComponent<UStaticMesh> newProxy = FProxyComponent<UStaticMesh>(
                InstanceData.StaticMesh,
                InstanceData.RelativeTransform * RuntimeData.Transform,
                InstanceData.OverridenMaterials,
                nullptr,
                RuntimeData.CustomizationData.Data,
                nullptr, /* we have no real actor*/
                LightweightActor.Index,
                LightweightActor.BuildableClass // TODO make this cleaner.
            );

            ProxyData.Add(newProxy);
        }
    }

    // Get staticmesh data.
    for (AFGBuildable* Actor : DefaultActors)
    {
        TArray<float> CustomizationData = Actor->GetCustomizationData_Native().Data;

        if (Actor->DoesContainLightweightInstances_Native())
        {
            float ScaleX = 1;
            float ScaleY = 1;
            float ScaleZ = 1;

            // !Begin special cases
            if (AFGBuildableBeam* BeamActor = Cast<AFGBuildableBeam>(Actor))
            {
                //  Beam length / Mesh length.
                ScaleZ = BeamActor->GetLength() / BeamActor->GetDefaultLength();
            }
            else if (AFGBuildableSignSupport* Support = Cast<AFGBuildableSignSupport>(Actor))
            {
                ScaleX = Support->mPoleScale.X;
                ScaleY = Support->mPoleScale.Y;
            }
            // End


            for (FInstanceData& InstanceData : Actor->GetActorLightweightInstanceData_Implementation())
            {
                FTransform T = InstanceData.RelativeTransform * Actor->GetTransform();
                T.SetScale3D(InstanceData.RelativeTransform.GetScale3D() * FVector(ScaleX, ScaleY, ScaleZ));
                ProxyData.Add({ InstanceData.StaticMesh,
                                    T,
                                    InstanceData.OverridenMaterials,
                                    nullptr,
                                    CustomizationData,
                                    Actor });
            }
        }

        // TODO 1.0 somebody did a refactor on the legs that broke this logic, we have to ensure this doesn't cause issues
        // else
        {
            //////////////////////////////////
            // Handle static meshes.
            for (auto Component : TInlineComponentArray<UStaticMeshComponent*>{ Actor })
            {

                if (Component->ComponentHasTag("BuildEffectIgnore") || Component->ComponentHasTag("CustomMaterial"))
                {
                    continue;
                }

                if (UFGColoredInstanceMeshProxy* ProxySMC = Cast<UFGColoredInstanceMeshProxy>(Component))
                {
                    if (ProxySMC->ShouldBlockInstancing() && !ProxySMC->IsVisible())
                    {
                        continue;
                    }
                }
                else if (!Component->IsVisible())
                {
                    continue;
                }

                ProxyData.Add({ Component->GetStaticMesh(),
                                    Component->GetComponentTransform(),//AccumulatedTransform,
                                    Component->OverrideMaterials,
                                    nullptr,
                                    CustomizationData,
                                    Actor });
            }

            //////////////////////////////////
            // Handle skeletal meshes.
            for (auto Component : TInlineComponentArray<USkeletalMeshComponent*>{ Actor })
            {
                if (Component->ComponentHasTag("BuildEffectIgnore") || Component->ComponentHasTag("CustomMaterial") || !Component->IsVisible())
                {
                    continue;
                }

                ProxySkelData.Add({
                    Component->GetSkeletalMeshAsset(),
                    Component->GetComponentTransform(),
                    Component->OverrideMaterials,
                    Component->ComponentTags.Contains("ForceIdle") ? nullptr : Component->GetAnimClass(),
                    CustomizationData,
                    Actor });
            }

            // TODO post 1.0 this should handled by running GetActorLightweightInstanceData_Implementation.
            // Handle factory legs
            if (UFGFactoryLegsComponent* factoryLegsComponent = Actor->FindComponentByClass<UFGFactoryLegsComponent>())
            {
                TArray<FInstanceData> legsInstanceData;
                factoryLegsComponent->CreateLegInstances(legsInstanceData);

                for (FInstanceData& InstanceData : legsInstanceData)
                {
                    ProxyData.Add(FProxyComponent<UStaticMesh>(InstanceData.StaticMesh, InstanceData.RelativeTransform * Actor->GetTransform(), InstanceData.OverridenMaterials, nullptr, CustomizationData, Actor));
                }
            }
        }
    }

    // Build static meshes.
    for (FProxyComponent<UStaticMesh>& Proxy : ProxyData)
    {
        if (Proxy.Mesh)
        {
            UStaticMeshComponent* BuildEffectProxyMeshComponent = NewObject<UStaticMeshComponent>(self);
            BuildEffectProxyMeshComponent->SetStaticMesh(Proxy.Mesh);
            BuildEffectProxyMeshComponent->SetWorldTransform(Proxy.WorldTransform);
            BuildEffectProxyMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            BuildEffectProxyMeshComponent->SetForcedLodModel(1);

            if (!self->mIsBlueprint)
            {
                BuildEffectProxyMeshComponent->SetCastShadow(false);
                BuildEffectProxyMeshComponent->SetAffectDistanceFieldLighting(true);
            }

            BuildEffectProxyMeshComponent->OverrideMaterials = Proxy.OverrideMaterials;
            BuildEffectProxyMeshComponent->bDisallowNanite = true;

            for (int32 i = 0; i < Proxy.PrimitiveData.Num(); i++)
            {
                BuildEffectProxyMeshComponent->SetCustomPrimitiveDataFloat(i, Proxy.PrimitiveData[i]);
            }

            self->ResolveMaterial(BuildEffectProxyMeshComponent, Proxy.OverrideMaterials);

            BuildEffectProxyMeshComponent->RegisterComponent();
            self->mMeshComponents.Add(BuildEffectProxyMeshComponent);

            // Default actor
            if (Proxy.Source)
            {
                TArray< UMeshComponent* >& Target = self->mActorToComponentsMap.FindOrAdd(Proxy.Source);
                Target.Add(BuildEffectProxyMeshComponent);
            }

            // Lightweight actor
            if (Proxy.Class)
            {
                uint32 UniqueHash = HashCombine(GetTypeHash(Proxy.Class), GetTypeHash(Proxy.Index));
                TArray< UMeshComponent* >& Target = self->mHashToInstanceArrayMap.FindOrAdd(UniqueHash);
                Target.Add(BuildEffectProxyMeshComponent);
            }

            // Update bounds
            FVector Min, Max;
            BuildEffectProxyMeshComponent->GetLocalBounds(Min, Max);
            self->mVolume += FBox(Min, Max).GetVolume();

            if (!self->mIsBlueprint)
            {
                auto ShadowCaster = DuplicateObject(BuildEffectProxyMeshComponent, self);
                ShadowCaster->SetCastShadow(true);
                ShadowCaster->SetCastHiddenShadow(true);
                ShadowCaster->SetHiddenInGame(true);
                ShadowCaster->SetAffectDistanceFieldLighting(false);
                ShadowCaster->RegisterComponent();

                self->mMeshComponents.Add(ShadowCaster);
                self->mMainMeshes.Add(ShadowCaster);
            }
        }
    }

    // Build skeletal meshes.
    for (FProxyComponent<USkeletalMesh>& Proxy : ProxySkelData)
    {
        USkeletalMeshComponent* BuildEffectSkeletalMeshProxyComponent = NewObject<USkeletalMeshComponent>(self);
        BuildEffectSkeletalMeshProxyComponent->SetSkeletalMesh(Proxy.Mesh);
        BuildEffectSkeletalMeshProxyComponent->SetWorldTransform(Proxy.WorldTransform);
        BuildEffectSkeletalMeshProxyComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BuildEffectSkeletalMeshProxyComponent->SetAnimInstanceClass(Proxy.DefaultAnimState);
        BuildEffectSkeletalMeshProxyComponent->SetForcedLOD(1);
        BuildEffectSkeletalMeshProxyComponent->bPauseAnims = true;
        //BuildEffectSkeletalMeshProxyComponent->bDisallowNanite = true;    // UE 5 future.
        for (int32 i = 0; i < Proxy.PrimitiveData.Num(); i++)
        {
            BuildEffectSkeletalMeshProxyComponent->SetCustomPrimitiveDataFloat(i, Proxy.PrimitiveData[i]);
        }

        // Resolve materials.
        self->ResolveMaterial(BuildEffectSkeletalMeshProxyComponent, Proxy.OverrideMaterials);

        BuildEffectSkeletalMeshProxyComponent->RegisterComponent();
        self->mMeshComponents.Add(BuildEffectSkeletalMeshProxyComponent);

        if (Proxy.Source)
        {
            TArray< UMeshComponent* >& Target = self->mActorToComponentsMap.FindOrAdd(Proxy.Source);
            Target.Add(BuildEffectSkeletalMeshProxyComponent);
        }

        self->mVolume += BuildEffectSkeletalMeshProxyComponent->GetLocalBounds().GetBox().GetVolume();
    }

    // Assign final data for splines.
    for (auto Spline : self->mSplineBuildableComponents)
    {
        Spline->SetScalarParameterValueOnMaterials(FName("NumInstances"), self->mSplineBuildableComponents.Num());
    }

    FVector Origin, Extent;
    self->GetActorBounds(false, Origin, Extent, false);

    // Move bounds.
    self->mBounds = FBox(Origin - Extent, Origin + Extent);
}