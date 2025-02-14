
#include "AutoLinkDebugging.h"
#include "AutoLinkDebugSettings.h"
#include "AutoLinkLogMacros.h"

#include "AbstractInstanceManager.h"
#include "BlueprintHookManager.h"
#include "FGBlueprintHologram.h"
#include "FGBlueprintProxy.h"
#include "FGBlueprintSubsystem.h"
#include "FGBuildable.h"
#include "FGBuildableConveyorBelt.h"
#include "FGBuildablePipeline.h"
#include "FGBuildablePipelineAttachment.h"
#include "FGBuildableRailroadSignal.h"
#include "FGBuildableRailroadSwitchControl.h"
#include "FGBuildableSubsystem.h"
#include "FGBuildEffectActor.h"
#include "FGBuildGun.h"
#include "FGBuildGunBuild.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeSubsystem.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "FGRailroadSignalHologram.h"
#include "Patching/NativeHookManager.h"

void AutoLinkDebugging::RegisterDebugHooks()
{
    // So we can inspect object connections in the world by middle-clicking on them
    SUBSCRIBE_METHOD(
        UFGBuildGunState::OnRecipeSampled,
        [](auto& scope, UFGBuildGunState* buildGunState, TSubclassOf<class UFGRecipe> recipe)
        {
            // Resolve the actor at the hit result
            auto buildGun = buildGunState->GetBuildGun();
            auto& hitResult = buildGun->GetHitResult();
            auto actor = hitResult.GetActor();
            if (actor && actor->IsA(AAbstractInstanceManager::StaticClass()))
            {
                if (auto manager = AAbstractInstanceManager::GetInstanceManager(actor))
                {
                    FInstanceHandle handle;
                    if (manager->ResolveHit(hitResult, handle))
                    {
                        actor = handle.GetOwner();
                    }
                }
            }

            if (!actor)
            {
                AL_LOG("UFGBuildGunState::OnRecipeSampled. No actor resolved.");
                scope(buildGunState, recipe);
                return;
            }

            AL_LOG("UFGBuildGunState::OnRecipeSampled. Actor is %s (%s) at %x.", *actor->GetName(), *actor->GetClass()->GetName(), actor);

            bool dumpedAtLeastOnce = false;
            if (auto conveyorLift = Cast<AFGBuildableConveyorLift>(actor))
            {
                DumpConveyorLift(TEXT("UFGBuildGunState::OnRecipeSampled"), conveyorLift);
                dumpedAtLeastOnce = true;
            }
            else if (auto conveyor = Cast<AFGBuildableConveyorBase>(actor))
            {
                DumpConveyor(TEXT("UFGBuildGunState::OnRecipeSampled"), conveyor);
                dumpedAtLeastOnce = true;
            }
            else if (auto integrant = Cast<IFGFluidIntegrantInterface>(actor))
            {
                DumpFluidIntegrant(TEXT("UFGBuildGunState::OnRecipeSampled"), integrant);
                dumpedAtLeastOnce = true;
            }
            else if (auto signal = Cast<AFGBuildableRailroadSignal>(actor))
            {
                DumpRailSignal(TEXT("UFGBuildGunState::OnRecipeSampled"), signal);
                dumpedAtLeastOnce = true;
            }
            else if (auto track = Cast<AFGBuildableRailroadTrack>(actor))
            {
                DumpRailTrack(TEXT("UFGBuildGunState::OnRecipeSampled"), track, false);
                //auto subsystem = AFGRailroadSubsystem::Get(actor->GetWorld());
                //DumpRailSubsystem(TEXT("UFGBuildGunState::OnRecipeSampled"), subsystem);
                dumpedAtLeastOnce = true;
            }

            if (!dumpedAtLeastOnce)
            {
                TInlineComponentArray<UFGFactoryConnectionComponent*> factoryConnections;
                actor->GetComponents(factoryConnections);
                for (auto connectionComponent : factoryConnections)
                {
                    DumpConnection(TEXT("UFGBuildGunState::OnRecipeSampled"), connectionComponent);
                }
                TInlineComponentArray<UFGPipeConnectionComponent*> pipeConnections;
                actor->GetComponents(pipeConnections);
                for (auto connectionComponent : pipeConnections)
                {
                    DumpConnection(TEXT("UFGBuildGunState::OnRecipeSampled"), connectionComponent);
                }
                TInlineComponentArray<UFGPipeConnectionComponentHyper*> hyperConnections;
                actor->GetComponents(hyperConnections);
                for (auto connectionComponent : hyperConnections)
                {
                    DumpConnection(TEXT("UFGBuildGunState::OnRecipeSampled"), connectionComponent);
                }
            }

            AL_LOG("UFGBuildGunState::OnRecipeSampled. Actor %s (%s) at %x dumped.", *actor->GetName(), *actor->GetClass()->GetName(), actor);

            scope(buildGunState, recipe);
        });

    if (!AL_DEBUG_ENABLE_MOD)
    {
        RegisterDebugTraceForDisabledModFunctions();
    }

    if (AL_REGISTER_GENERAL_DEBUG_TRACE_HOOKS)
    {
        RegisterGeneralDebugTraceHooks();
    }

    if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
    {
        RegisterBuildEffectTraceHooks();
    }

    if (AL_REGISTER_RAIL_TRACE_HOOKS)
    {
        RegisterRailTraceHooks();
    }

    if (AL_REGISTER_PIPE_TRACE_HOOKS)
    {
        RegisterPipeTraceHooks();
    }
}

void AutoLinkDebugging::RegisterGeneralDebugTraceHooks()
{
    /* AFGBuildableHologram */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableHologram, ConfigureActor,
        [](auto& scope, const AFGBuildableHologram* self, AFGBuildable* buildable)
        {
            AL_LOG("AFGBuildableHologram::ConfigureActor START %s (%s). Buildable: %s", *self->GetName(), *self->GetClass()->GetName(), *buildable->GetName());
            scope(self, buildable);
            AL_LOG("AFGBuildableHologram::ConfigureActor END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableHologram, ConfigureComponents,
        [&](auto& scope, const AFGBuildableHologram* self, AFGBuildable* buildable)
        {
            AL_LOG("AFGBuildableHologram::ConfigureComponents START %s (%s). Buildable: %s", *self->GetName(), *self->GetClass()->GetName(), *buildable->GetName());
            scope(self, buildable);
            AL_LOG("AFGBuildableHologram::ConfigureComponents END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, BeginPlay, [&](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::BeginPlay START %s", *self->GetName());
        scope(self);
        AL_LOG("AFGBuildable::BeginPlay END");
        });

    /* AFGBlueprintHologram */

    SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintHologram, BeginPlay, [&](auto& scope, AFGBlueprintHologram* self) {
        AL_LOG("AFGBlueprintHologram::BeginPlay START %s. World %s", *self->GetName(), *self->GetWorld()->GetName());
        scope(self);
        AL_LOG("AFGBlueprintHologram::BeginPlay END %s", *self->GetName());
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintHologram, Construct, [](auto& scope, AFGBlueprintHologram* self, TArray< AActor* >& out_children, FNetConstructionID NetConstructionID) {
        AL_LOG("AFGBlueprintHologram::Construct START %s", *self->GetName());
        scope(self, out_children, NetConstructionID);
        AL_LOG("AFGBlueprintHologram::Construct END %s", *self->GetName());
        });

    // These seem to be called on every frame that a blueprint hologram is out
    //SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintHologram, PreHologramPlacement, [](auto& scope, AFGBlueprintHologram* self, const FHitResult& hitResult) {
    //    AL_LOG("AFGBlueprintHologram::PreHologramPlacement START");
    //    scope(self, hitResult);
    //    AL_LOG("AFGBlueprintHologram::PreHologramPlacement END");
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintHologram, PostHologramPlacement, [](auto& scope, AFGBlueprintHologram* self, const FHitResult& hitResult) {
    //    AL_LOG("AFGBlueprintHologram::PostHologramPlacement START");
    //    scope(self, hitResult);
    //    AL_LOG("AFGBlueprintHologram::PostHologramPlacement END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintHologram, LoadBlueprintToOtherWorld, [](auto& scope, AFGBlueprintHologram* self) {
        AL_LOG("AFGBlueprintHologram::LoadBlueprintToOtherWorld START %s. World %s", *self->GetName(), *self->GetWorld()->GetName());
        scope(self);
        AL_LOG("AFGBlueprintHologram::LoadBlueprintToOtherWorld END %s", *self->GetName());
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintHologram, SetupComponent, [](auto& scope, AFGBlueprintHologram* self, USceneComponent* attachParent, UActorComponent* componentTemplate, const FName& componentName, const FName& attachSocketName) {
    //    AL_LOG("AFGBlueprintHologram::SetupComponent START %s (componentTemplate: %s [%s]) (attachParent: %s [%s]) attachSocketName: %s", *componentName.ToString(), *componentTemplate->GetName(), *componentTemplate->GetClass()->GetName(), *attachParent->GetName(), *attachParent->GetClass()->GetName(), *attachSocketName.ToString());
    //    scope(self, attachParent, componentTemplate, componentName, attachSocketName);
    //    AL_LOG("AFGBlueprintHologram::SetupComponent END");
    //    });

    /* AFGBlueprintProxy */

    SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintProxy, BeginPlay, [](auto& scope, AFGBlueprintProxy* self) {
        AL_LOG("AFGBlueprintProxy::BeginPlay START %s. World %s", *self->GetName(), *self->GetWorld()->GetName());
        scope(self);
        AL_LOG("AFGBlueprintProxy::BeginPlay END %s", *self->GetName());
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintProxy, RegisterBuildable, [](auto& scope, AFGBlueprintProxy* self, AFGBuildable* buildable) {
        AL_LOG("AFGBlueprintProxy::RegisterBuildable START. World %s", *self->GetWorld()->GetName());
        scope(self, buildable);
        AL_LOG("AFGBlueprintProxy::RegisterBuildable END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintProxy, GetBuildables, [](auto& scope, const AFGBlueprintProxy* self) {
    //    AL_LOG("AFGBlueprintProxy::GetBuildables START");
    //    scope(self);
    //    AL_LOG("AFGBlueprintProxy::GetBuildables END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBlueprintProxy, CollectBuildables, [](auto& scope, const AFGBlueprintProxy* self, TArray< class AFGBuildable* >& out_buildables) {
        AL_LOG("AFGBlueprintProxy::CollectBuildables START");
        scope(self, out_buildables);
        AL_LOG("AFGBlueprintProxy::CollectBuildables END");
        });

    /* UFGBuildGunStateBuild */

    SUBSCRIBE_UOBJECT_METHOD(UFGBuildGunStateBuild, PrimaryFire_Implementation, [](auto& scope, UFGBuildGunStateBuild* self) {
        AL_LOG("UFGBuildGunStateBuild::PrimaryFire_Implementation START");
        scope(self);
        AL_LOG("UFGBuildGunStateBuild::PrimaryFire_Implementation END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGBuildGunStateBuild, PrimaryFireRelease_Implementation, [](auto& scope, UFGBuildGunStateBuild* self) {
        AL_LOG("UFGBuildGunStateBuild::PrimaryFireRelease_Implementation START");
        scope(self);
        AL_LOG("UFGBuildGunStateBuild::PrimaryFireRelease_Implementation END");
        });

    /* AFGBuildable */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, OnConstruction, [](auto& scope, AFGBuildable* self, const FTransform& transform) {
        AL_LOG("AFGBuildable::OnConstruction START %s (%s). World %s", *self->GetName(), *self->GetClass()->GetName(), *self->GetWorld()->GetName());
        scope(self, transform);
        AL_LOG("AFGBuildable::OnConstruction END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, BlueprintCleanUpFaultyConnectionHookups, [](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::BlueprintCleanUpFaultyConnectionHookups START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildable::BlueprintCleanUpFaultyConnectionHookups END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, PreSerializedToBlueprint, [](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::PreSerializedToBlueprint START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildable::PreSerializedToBlueprint END");
        });

    //Crashes
    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, PostSerializedToBlueprint, [](auto& scope, AFGBuildable* self) {
    //    AL_LOG("AFGBuildable::PostSerializedToBlueprint START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    AL_LOG("AFGBuildable::PostSerializedToBlueprint END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, PostSerializedFromBlueprint, [](auto& scope, AFGBuildable* self, bool isBlueprintWorld) {
        AL_LOG("AFGBuildable::PostSerializedFromBlueprint START isBlueprintWorld: %d, %s (%s)", isBlueprintWorld, *self->GetName(), *self->GetClass()->GetName());
        scope(self, isBlueprintWorld);
        AL_LOG("AFGBuildable::PostSerializedFromBlueprint END");
        });

    /* AFGBuildableSubsystem */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableSubsystem, SpawnPendingConstructionHologram, [](auto& scope, AFGBuildableSubsystem* self, FNetConstructionID netConstructionID, class AFGHologram* templateHologram, class AFGBuildGun* instigatingBuildGun) {
        AL_LOG("AFGBuildableSubsystem::SpawnPendingConstructionHologram START %s (%s); ID: %s", *self->GetName(), *self->GetClass()->GetName(), *netConstructionID.ToString());
        scope(self, netConstructionID, templateHologram, instigatingBuildGun);
        AL_LOG("AFGBuildableSubsystem::SpawnPendingConstructionHologram END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableSubsystem, AddPendingConstructionHologram, [](auto& scope, AFGBuildableSubsystem* self, FNetConstructionID netConstructionID, class AFGHologram* hologram ) {
        AL_LOG("AFGBuildableSubsystem::AddPendingConstructionHologram START %s (%s); ID: %s", *self->GetName(), *self->GetClass()->GetName(), *netConstructionID.ToString());
        scope(self, netConstructionID, hologram);
        AL_LOG("AFGBuildableSubsystem::AddPendingConstructionHologram END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableSubsystem, RemovePendingConstructionHologram, [](auto& scope, AFGBuildableSubsystem* self, FNetConstructionID netConstructionID) {
        AL_LOG("AFGBuildableSubsystem::RemovePendingConstructionHologram START %s (%s); ID: %s", *self->GetName(), *self->GetClass()->GetName(), *netConstructionID.ToString());
        scope(self, netConstructionID);
        AL_LOG("AFGBuildableSubsystem::RemovePendingConstructionHologram END");
        });
}

void AutoLinkDebugging::RegisterDebugTraceForDisabledModFunctions()
{
    if (AL_DEBUG_ENABLE_MOD) return;

    if (!AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS) return; // At the moment, they are all build effect trace hooks

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, GetBeltSourceSplinesOrdered, [](auto& scope, const AFGBuildEffectActor* self, const TArray<class AFGBuildableConveyorBelt*>& inBelts, TArray<AActor*>& orderedActors) {
        AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        int i = 0;
        AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered BEFORE: inBelts: %d", inBelts.Num());
        for (auto belt : inBelts)
        {
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered BEFORE:\t inBelts[%d]: %s", i++, *belt->GetName());
        }
        auto splines = scope(self, inBelts, orderedActors);
        AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER: orderedActors: %d", orderedActors.Num());
        i = 0;
        for (auto actor : orderedActors)
        {
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER:\t orderedActors[%d]: %s", i++, *actor->GetName());
        }
        AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER: Total Splines: %d", splines.Num());
        i = 0;
        for (USplineComponent* spline : splines)
        {
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER:\t splines[%d]: %s (%s). Owner: %s", i++, *spline->GetName(), *spline->GetClass()->GetName(), *spline->GetOwner()->GetName());
        }
        AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered END");
        return splines;
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, GetPipeSourceSplineOrdered, [](auto& scope, const AFGBuildEffectActor* self, const TArray<class AFGBuildablePipeBase*>& inPipes, TArray<AActor*>& orderedActors) {
        AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        int i = 0;
        AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered BEFORE: inPipes: %d", inPipes.Num());
        for (auto pipe : inPipes)
        {
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered BEFORE:\t inPipes[%d]: %s", i++, *pipe->GetName());
        }
        auto splines = scope(self, inPipes, orderedActors);
        AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER: orderedActors: %d", orderedActors.Num());
        i = 0;
        for (auto actor : orderedActors)
        {
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER:\t orderedActors[%d]: %s", i++, *actor->GetName());
        }
        AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER: Total Splines: %d", splines.Num());
        i = 0;
        for (USplineComponent* spline : splines)
        {
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER:\t splines[%d]: %s (%s). Owner: %s", i++, *spline->GetName(), *spline->GetClass()->GetName(), *spline->GetOwner()->GetName());
        }
        AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered END");
        return splines;
        });
}

void AutoLinkDebugging::RegisterBuildEffectTraceHooks()
{
    /* AFGBuildable */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, PlayBuildEffects, [](auto& scope, AFGBuildable* self, AActor* inInstigator) {
        AL_LOG("AFGBuildable::PlayBuildEffects START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, inInstigator);
        AL_LOG("AFGBuildable::PlayBuildEffects END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, ExecutePlayBuildEffects, [](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::ExecutePlayBuildEffects START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildable::ExecutePlayBuildEffects END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, OnBuildEffectFinished, [](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::OnBuildEffectFinished START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildable::OnBuildEffectFinished END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, PlayBuildEffectActor, [](auto& scope, AFGBuildable* self, AActor* inInstigator) {
        AL_LOG("AFGBuildable::PlayBuildEffectActor START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, inInstigator);
        AL_LOG("AFGBuildable::PlayBuildEffectActor END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, ExecutePlayBuildActorEffects, [](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::ExecutePlayBuildActorEffects START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildable::ExecutePlayBuildActorEffects END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, OnBuildEffectActorFinished, [](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::OnBuildEffectActorFinished START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildable::OnBuildEffectActorFinished END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, HandleBlueprintSpawnedBuildEffect, [](auto& scope, AFGBuildable* self, AFGBuildEffectActor* inBuildEffectActor) {
        AL_LOG("AFGBuildable::HandleBlueprintSpawnedBuildEffect START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, inBuildEffectActor);
        AL_LOG("AFGBuildable::HandleBlueprintSpawnedBuildEffect END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, SetIsPlayingBuildEffect, [](auto& scope, AFGBuildable* self, bool isPlaying) {
        AL_LOG("AFGBuildable::SetIsPlayingBuildEffect START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, isPlaying);
        AL_LOG("AFGBuildable::SetIsPlayingBuildEffect END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, SetIsPlayingBlueprintBuildEffect, [](auto& scope, AFGBuildable* self, bool isPlaying) {
        AL_LOG("AFGBuildable::SetIsPlayingBlueprintBuildEffect START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, isPlaying);
        AL_LOG("AFGBuildable::SetIsPlayingBlueprintBuildEffect END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, SetBuildEffectActor, [](auto& scope, AFGBuildable* self, AFGBuildEffectActor* BuildEffectActor) {
        AL_LOG("AFGBuildable::SetBuildEffectActor START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, BuildEffectActor);
        AL_LOG("AFGBuildable::SetBuildEffectActor END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, ShouldSkipBuildEffect, [](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::ShouldSkipBuildEffect START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto result = scope(self);
        AL_LOG("AFGBuildable::ShouldSkipBuildEffect END");
        return result;
        });

    /* AFGBuildableHologram */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableHologram, ConfigureBuildEffect,
        [](auto& scope, AFGBuildableHologram* self, AFGBuildable* buildable)
        {
            AL_LOG("AFGBuildableHologram::ConfigureBuildEffect START %s (%s). Buildable: %s", *self->GetName(), *self->GetClass()->GetName(), *buildable->GetName());
            scope(self, buildable);
            AL_LOG("AFGBuildableHologram::ConfigureBuildEffect END");
        });

    /* AFGBuildableSubsystem */

    SUBSCRIBE_METHOD_AFTER(AFGBuildableSubsystem::RequestBuildEffectActor, [](UObject* WorldContext, AFGBuildEffectActor*& BuildEffectActor, TSubclassOf< AFGBuildEffectActor > TemplateClass, FTransform Transform, AActor* instigator, bool bForceSolo) {
        AL_LOG("AFGBuildableSubsystem::RequestBuildEffectActor AFTER START bForceSolo: %d", bForceSolo);
        if (TemplateClass)
        {
            AL_LOG("AFGBuildableSubsystem::RequestBuildEffectActor AFTER START TemplateClass: %s", *TemplateClass->GetName());
        }
        if (instigator)
        {
            AL_LOG("AFGBuildableSubsystem::RequestBuildEffectActor AFTER START instigator: %s (%s)", *instigator->GetName(), *instigator->GetClass()->GetName());
        }
        DumpBuildEffectActor("AFGBuildableSubsystem::RequestBuildEffectActor RESULT", BuildEffectActor);
        AL_LOG("AFGBuildableSubsystem::RequestBuildEffectActor AFTER END");
        });

    /* AFGBuildEffectActor */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, AddAbstractDataEntry, [](auto& scope, AFGBuildEffectActor* self, TSubclassOf< AFGBuildable > buildableClass, const FRuntimeBuildableInstanceData& runtimeData, UAbstractInstanceDataObject* InstanceData, int32 Index) {
        AL_LOG("AFGBuildEffectActor::AddAbstractDataEntry START %s (%s). buildableClass: %s", *self->GetName(), *self->GetClass()->GetName(), *buildableClass->GetName());
        scope(self, buildableClass, runtimeData, InstanceData, Index);
        AL_LOG("AFGBuildEffectActor::AddAbstractDataEntry END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, RemoveAbstractDataEntry, [](auto& scope, AFGBuildEffectActor* self, TSubclassOf< AFGBuildable > buildableClass, int32 index) {
        AL_LOG("AFGBuildEffectActor::RemoveAbstractDataEntry START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        DumpBuildEffectActor("AFGBuildEffectActor::RemoveAbstractDataEntry BEFORE", self);
        scope(self, buildableClass, index);
        DumpBuildEffectActor("AFGBuildEffectActor::RemoveAbstractDataEntry AFTER", self);
        AL_LOG("AFGBuildEffectActor::RemoveAbstractDataEntry END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, SetActor, [](auto& scope, AFGBuildEffectActor* self, AActor* InSourceActor) {
        AL_LOG("AFGBuildEffectActor::SetActor START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, InSourceActor);
        AL_LOG("AFGBuildEffectActor::SetActor END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, SetActors, [](auto& scope, AFGBuildEffectActor* self, TArray<AActor*> InSourceActors) {
        AL_LOG("AFGBuildEffectActor::SetActors START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, InSourceActors);
        AL_LOG("AFGBuildEffectActor::SetActors END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, SetRecipe, [](auto& scope, AFGBuildEffectActor* self, TSubclassOf<UFGRecipe> inRecipe, AFGBuildable* buildable) {
        AL_LOG("AFGBuildEffectActor::SetRecipe START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        if (buildable)
        {
            AL_LOG("AFGBuildEffectActor::SetRecipe: buildable %s (%s)", *buildable->GetName(), *buildable->GetClass()->GetName());
        }
        scope(self, inRecipe, buildable);
        AL_LOG("AFGBuildEffectActor::SetRecipe END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, MarkAsBlueprintBuildEffect, [](auto& scope, AFGBuildEffectActor* self) {
        AL_LOG("AFGBuildEffectActor::MarkAsBlueprintBuildEffect START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        DumpBuildEffectActor("AFGBuildableSubsystem::MarkAsBlueprintBuildEffect RESULT", self);
        scope(self);
        AL_LOG("AFGBuildEffectActor::MarkAsBlueprintBuildEffect END");
        });

    //SUBSCRIBE_UOBJECT_METHOD_AFTER(AFGBuildEffectActor, GetBind, [](FBuildEffectEnded& result, AFGBuildEffectActor* self, UClass* actorClass) {
    //    AL_LOG("AFGBuildEffectActor::GetBind AFTER START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, SpawnCostEffectActor, [](auto& scope, AFGBuildEffectActor* self, const FTransform& SpawnLocation, FVector TargetLocation, float TargetExtent, TSubclassOf<UFGItemDescriptor> Item) {
    //    AL_LOG("AFGBuildEffectActor::SpawnCostEffectActor START %s (%s). Spawn At: %s. Target: %s", *self->GetName(), *self->GetClass()->GetName(), *SpawnLocation.ToString(), *TargetLocation.ToString());
    //    scope(self, SpawnLocation, TargetLocation, TargetExtent, Item);
    //    AL_LOG("AFGBuildEffectActor::SpawnCostEffectActor END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, Start, [](auto& scope, AFGBuildEffectActor* self) {
        AL_LOG("AFGBuildEffectActor::Start START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        DumpBuildEffectActor("AFGBuildEffectActor::Start BEFORE", self);
        scope(self);
        DumpBuildEffectActor("AFGBuildEffectActor::Start AFTER", self);
        AL_LOG("AFGBuildEffectActor::Start END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, Stop, [](auto& scope, AFGBuildEffectActor* self) {
        AL_LOG("AFGBuildEffectActor::Stop START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        DumpBuildEffectActor("AFGBuildEffectActor::Stop BEFORE", self);
        scope(self);
        DumpBuildEffectActor("AFGBuildEffectActor::Stop AFTER", self);
        AL_LOG("AFGBuildEffectActor::Stop END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, CreateVisuals, [](auto& scope, AFGBuildEffectActor* self) {
        AL_LOG("AFGBuildEffectActor::CreateVisuals START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        DumpBuildEffectActor("AFGBuildEffectActor::CreateVisuals BEFORE", self);
        scope(self);
        DumpBuildEffectActor("AFGBuildEffectActor::CreateVisuals AFTER", self);
        AL_LOG("AFGBuildEffectActor::CreateVisuals END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, ResolveMaterial, [](auto& scope, AFGBuildEffectActor* self, UMeshComponent* Mesh, const TArray<UMaterialInterface*>& Overrides) {
    //    AL_LOG("AFGBuildEffectActor::ResolveMaterial START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self, Mesh, Overrides);
    //    AL_LOG("AFGBuildEffectActor::ResolveMaterial END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, SetupThrowQueue, [](auto& scope, AFGBuildEffectActor* self) {
        AL_LOG("AFGBuildEffectActor::SetupThrowQueue START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        DumpBuildEffectActor("AFGBuildEffectActor::SetupThrowQueue RESULT", self);
        scope(self);
        AL_LOG("AFGBuildEffectActor::SetupThrowQueue END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, GetTotalSplineLength, [](auto& scope, const AFGBuildEffectActor* self) {
    //    AL_LOG("AFGBuildEffectActor::GetTotalSplineLength START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    AL_LOG("AFGBuildEffectActor::GetTotalSplineLength END");
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, GetTransformOnSplines, [](auto& scope, const AFGBuildEffectActor* self, bool bWorldSpace) {
    //    AL_LOG("AFGBuildEffectActor::GetTransformOnSplines START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    auto transform = scope(self, bWorldSpace);
    //    AL_LOG("AFGBuildEffectActor::GetTransformOnSplines END Transform: %s", *transform.ToString() );
    //    return transform;
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, UpdateCostQueue, [](auto& scope, AFGBuildEffectActor* self) {
    //    AL_LOG("AFGBuildEffectActor::UpdateCostQueue START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    AL_LOG("AFGBuildEffectActor::UpdateCostQueue END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, CalculateBuildEffectBounds, [](auto& scope, AFGBuildEffectActor* self) {
        AL_LOG("AFGBuildEffectActor::CalculateBuildEffectBounds START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        DumpBuildEffectActor("AFGBuildEffectActor::CalculateBuildEffectBounds BEFORE", self);
        scope(self);
        DumpBuildEffectActor("AFGBuildEffectActor::CalculateBuildEffectBounds AFTER", self);
        AL_LOG("AFGBuildEffectActor::CalculateBuildEffectBounds END");
        });
}

void AutoLinkDebugging::RegisterRailTraceHooks()
{
    /* UFGRailroadTrackConnectionComponent */

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, AddConnection, [](auto& scope, UFGRailroadTrackConnectionComponent* self, UFGRailroadTrackConnectionComponent* toComponent) {
        AL_LOG("UFGRailroadTrackConnectionComponent::AddConnection START %s on %s adding %s on %s", *self->GetName(), *self->GetOuter()->GetName(), *toComponent->GetName(), *toComponent->GetOuter()->GetName());
        scope(self, toComponent);
        AL_LOG("UFGRailroadTrackConnectionComponent::AddConnection END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, RemoveConnection, [](auto& scope, UFGRailroadTrackConnectionComponent* self, UFGRailroadTrackConnectionComponent* toComponent) {
        AL_LOG("UFGRailroadTrackConnectionComponent::RemoveConnection START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, toComponent);
        AL_LOG("UFGRailroadTrackConnectionComponent::RemoveConnection END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, IsOccupied, [](auto& scope, const UFGRailroadTrackConnectionComponent* self, float distance) {
        AL_LOG("UFGRailroadTrackConnectionComponent::IsOccupied START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto isOccupied = scope(self, distance);
        AL_LOG("UFGRailroadTrackConnectionComponent::IsOccupied END");
        return isOccupied;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, IsFacingSwitch, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::IsFacingSwitch START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::IsFacingSwitch END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, IsTrailingSwitch, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
    //    AL_LOG("UFGRailroadTrackConnectionComponent::IsTrailingSwitch START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    AL_LOG("UFGRailroadTrackConnectionComponent::IsTrailingSwitch END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, IsSwitchClear, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::IsSwitchClear START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::IsSwitchClear END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, SetSwitchPosition, [](auto& scope, UFGRailroadTrackConnectionComponent* self, int32 position) {
        AL_LOG("UFGRailroadTrackConnectionComponent::SetSwitchPosition START %s (%s) - position: %d", *self->GetName(), *self->GetClass()->GetName(), position);
        scope(self, position);
        AL_LOG("UFGRailroadTrackConnectionComponent::SetSwitchPosition END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, GetSwitchControl, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::GetSwitchControl START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto value = scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::GetSwitchControl END");
        return value;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, GetStation, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::GetStation START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto value = scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::GetStation END");
        return value;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, GetFacingSignal, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::GetFacingSignal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto value = scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::GetFacingSignal END");
        return value;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, GetTrailingSignal, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::GetTrailingSignal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto value = scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::GetTrailingSignal END");
        return value;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, GetSignalBlock, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::GetSignalBlock START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto value = scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::GetSignalBlock END");
        return value;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, GetOpposite, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::GetOpposite START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto value = scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::GetOpposite END");
        return value;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, GetNext, [](auto& scope, const UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::GetNext START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto value = scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::GetNext END");
        return value;
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, SetSwitchControl, [](auto& scope, UFGRailroadTrackConnectionComponent* self, AFGBuildableRailroadSwitchControl* control) {
        AL_LOG("UFGRailroadTrackConnectionComponent::SetSwitchControl START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, control);
        AL_LOG("UFGRailroadTrackConnectionComponent::SetSwitchControl END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, SetStation, [](auto& scope, UFGRailroadTrackConnectionComponent* self, AFGBuildableRailroadStation* station) {
        AL_LOG("UFGRailroadTrackConnectionComponent::SetStation START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, station);
        AL_LOG("UFGRailroadTrackConnectionComponent::SetStation END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, SetFacingSignal, [](auto& scope, UFGRailroadTrackConnectionComponent* self, AFGBuildableRailroadSignal* signal) {
        AL_LOG("UFGRailroadTrackConnectionComponent::SetFacingSignal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, signal);
        AL_LOG("UFGRailroadTrackConnectionComponent::SetFacingSignal END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, SetTrackPosition, [](auto& scope, UFGRailroadTrackConnectionComponent* self, const FRailroadTrackPosition& position) {
        DumpRailTrackPosition(FString::Printf(TEXT("UFGRailroadTrackConnectionComponent::SetTrackPosition START %s on %s"), *self->GetName(), *self->GetOuter()->GetName()), &position);
        scope(self, position);
        DumpRailConnection("UFGRailroadTrackConnectionComponent::SetTrackPosition END", self, false);
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, SortConnections, [](auto& scope, UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::SortConnections START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::SortConnections END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, AddConnectionInternal, [](auto& scope, UFGRailroadTrackConnectionComponent* self, UFGRailroadTrackConnectionComponent* toComponent) {
        AL_LOG("UFGRailroadTrackConnectionComponent::AddConnectionInternal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, toComponent);
        AL_LOG("UFGRailroadTrackConnectionComponent::AddConnectionInternal END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, RemoveConnectionInternal, [](auto& scope, UFGRailroadTrackConnectionComponent* self, UFGRailroadTrackConnectionComponent* toComponent) {
        AL_LOG("UFGRailroadTrackConnectionComponent::RemoveConnectionInternal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, toComponent);
        AL_LOG("UFGRailroadTrackConnectionComponent::RemoveConnectionInternal END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, OnConnectionsChangedInternal, [](auto& scope, UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::OnConnectionsChangedInternal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::OnConnectionsChangedInternal END");
        });

    SUBSCRIBE_UOBJECT_METHOD(UFGRailroadTrackConnectionComponent, ClampSwitchPosition, [](auto& scope, UFGRailroadTrackConnectionComponent* self) {
        AL_LOG("UFGRailroadTrackConnectionComponent::ClampSwitchPosition START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("UFGRailroadTrackConnectionComponent::ClampSwitchPosition END");
        });

    /* AFGBuildableRailroadSignal */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, BeginPlay, [](auto& scope, AFGBuildableRailroadSignal* self) {
        DumpRailSignal("AFGBuildableRailroadSignal::BeginPlay START", self);
        scope(self);
        DumpRailSignal("AFGBuildableRailroadSignal::BeginPlay END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, GetGuardedConnections, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::GetGuardedConnections START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::GetGuardedConnections END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, GetObservedConnections, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::GetObservedConnections START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::GetObservedConnections END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, HasValidConnections, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::HasValidConnections START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto val = scope(self);
        AL_LOG("AFGBuildableRailroadSignal::HasValidConnections END. Ret: %d", val);
        return val;
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, GetAspect, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::GetAspect START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::GetAspect END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, GetBlockValidation, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::GetBlockValidation START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::GetBlockValidation END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, HasObservedBlock, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::HasObservedBlock START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        auto val = scope(self);
        AL_LOG("AFGBuildableRailroadSignal::HasObservedBlock END Ret: %d", val);
        return val;
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, GetObservedBlock, [](auto& scope, AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::GetObservedBlock START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::GetObservedBlock END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, IsPathSignal, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::IsPathSignal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::IsPathSignal END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, IsBiDirectional, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::IsBiDirectional START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::IsBiDirectional END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, GetVisualState, [](auto& scope, const AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::GetVisualState START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::GetVisualState END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, OnAspectChanged, [](auto& scope, AFGBuildableRailroadSignal* self) {
    //    AL_LOG("AFGBuildableRailroadSignal::OnAspectChanged START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    DumpRailSignal("AFGBuildableRailroadSignal::OnAspectChanged END", self);
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, OnBlockValidationChanged, [](auto& scope, AFGBuildableRailroadSignal* self) {
    //    AL_LOG("AFGBuildableRailroadSignal::OnBlockValidationChanged START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    DumpRailSignal("AFGBuildableRailroadSignal::OnBlockValidationChanged END", self);
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, OnDirectionalityChanged, [](auto& scope, AFGBuildableRailroadSignal* self) {
    //    AL_LOG("AFGBuildableRailroadSignal::OnDirectionalityChanged START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    DumpRailSignal("AFGBuildableRailroadSignal::OnDirectionalityChanged END", self);
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, OnVisualStateChanged, [](auto& scope, AFGBuildableRailroadSignal* self) {
    //    AL_LOG("AFGBuildableRailroadSignal::OnVisualStateChanged START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    DumpRailSignal("AFGBuildableRailroadSignal::OnVisualStateChanged END", self);
    //    });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, OnDrawDebugVisualState, [](auto& scope, AFGBuildableRailroadSignal* self) {
    //    AL_LOG("AFGBuildableRailroadSignal::OnDrawDebugVisualState START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    AL_LOG("AFGBuildableRailroadSignal::OnDrawDebugVisualState END");
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, DisconnectSignal, [](auto& scope, AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::DisconnectSignal START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::DisconnectSignal END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, UpdateVisuals, [](auto& scope, AFGBuildableRailroadSignal* self) {
    //    AL_LOG("AFGBuildableRailroadSignal::UpdateVisuals START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
    //    scope(self);
    //    DumpRailSignal("AFGBuildableRailroadSignal::UpdateVisuals END", self);
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, ApplyVisualState, [](auto& scope, AFGBuildableRailroadSignal* self, int16 state) {
        AL_LOG("AFGBuildableRailroadSignal::ApplyVisualState START %s (%s), state %d", *self->GetName(), *self->GetClass()->GetName(), state);
        scope(self, state);
        AL_LOG("AFGBuildableRailroadSignal::ApplyVisualState END %s", *self->GetName());
        //DumpRailSignal("AFGBuildableRailroadSignal::ApplyVisualState END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, AddGuardedConnection, [](auto& scope, AFGBuildableRailroadSignal* self, UFGRailroadTrackConnectionComponent* connection) {
        AL_LOG("AFGBuildableRailroadSignal::AddGuardedConnection START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, connection);
        DumpRailSignal("AFGBuildableRailroadSignal::AddGuardedConnection END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, AddObservedConnection, [](auto& scope, AFGBuildableRailroadSignal* self, UFGRailroadTrackConnectionComponent* connection) {
        AL_LOG("AFGBuildableRailroadSignal::AddObservedConnection START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, connection);
        DumpRailSignal("AFGBuildableRailroadSignal::AddObservedConnection END", self);
        //AL_LOG("AFGBuildableRailroadSignal::AddObservedConnection END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, UpdateConnections, [](auto& scope, AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::UpdateConnections START %s", *self->GetName());
        scope(self);
        DumpRailSignal("AFGBuildableRailroadSignal::UpdateConnections END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, SetObservedBlock, [](auto& scope, AFGBuildableRailroadSignal* self, TWeakPtr< FFGRailroadSignalBlock > block) {
        AL_LOG("AFGBuildableRailroadSignal::SetObservedBlock START %s Block: %d", *self->GetName(), (block.IsValid() ? block.Pin().Get()->ID : -1 ));
        //DumpRailSignalBlock("AFGBuildableRailroadSignal::SetObservedBlock START", block.Pin().Get());
        scope(self, block);
        AL_LOG("AFGBuildableRailroadSignal::SetObservedBlock END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, OnBlockChanged, [](auto& scope, AFGBuildableRailroadSignal* self) {
    //    AL_LOG("AFGBuildableRailroadSignal::OnBlockChanged START %s", *self->GetName());
    //    scope(self);
    //    DumpRailSignal("AFGBuildableRailroadSignal::OnBlockChanged END", self);
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, UpdateDirectionality, [](auto& scope, AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::UpdateDirectionality START %s", *self->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::UpdateDirectionality END %s", *self->GetName());
        //DumpRailSignal("AFGBuildableRailroadSignal::UpdateDirectionality END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, UpdateAspect, [](auto& scope, AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::UpdateAspect START %s", *self->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::UpdateAspect END %s", *self->GetName());
        //DumpRailSignal("AFGBuildableRailroadSignal::UpdateAspect END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadSignal, UpdateBlockValidation, [](auto& scope, AFGBuildableRailroadSignal* self) {
        AL_LOG("AFGBuildableRailroadSignal::UpdateBlockValidation START %s", *self->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadSignal::UpdateBlockValidation END %s", *self->GetName());
        //DumpRailSignal("AFGBuildableRailroadSignal::UpdateBlockValidation END", self);
        });

    /* AFGBuildableRailroadTrack */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, BeginPlay, [](auto& scope, AFGBuildableRailroadTrack* self) {
        DumpRailTrack("AFGBuildableRailroadTrack::BeginPlay START", self, false);
        scope(self);
        DumpRailTrack("AFGBuildableRailroadTrack::BeginPlay END", self, false);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, IsOccupied, [](auto& scope, const AFGBuildableRailroadTrack* self) {
        DumpRailTrack("AFGBuildableRailroadTrack::IsOccupied START", self, false);
        scope(self);
        DumpRailTrack("AFGBuildableRailroadTrack::IsOccupied END", self, true);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, HasSignalBlock, [](auto& scope, const AFGBuildableRailroadTrack* self) {
        AL_LOG("AFGBuildableRailroadTrack::HasSignalBlock START %s", *self->GetName());
        auto val = scope(self);
        AL_LOG("AFGBuildableRailroadTrack::HasSignalBlock END Ret: %d", val);
        return val;
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, GetSignalBlock, [](auto& scope, const AFGBuildableRailroadTrack* self) {
        AL_LOG("AFGBuildableRailroadTrack::GetSignalBlock START %s", *self->GetName());
        scope(self);
        AL_LOG("AFGBuildableRailroadTrack::GetSignalBlock END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, UpdateOverlappingTracks, [](auto& scope, AFGBuildableRailroadTrack* self) {
        AL_LOG("AFGBuildableRailroadTrack::UpdateOverlappingTracks START");
        scope(self);
        AL_LOG("AFGBuildableRailroadTrack::UpdateOverlappingTracks END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, GetOverlappingTracks, [](auto& scope, AFGBuildableRailroadTrack* self) {
    //    DumpRailTrack("AFGBuildableRailroadTrack::GetOverlappingTracks START", self, true);
    //    scope(self);
    //    DumpRailTrack("AFGBuildableRailroadTrack::GetOverlappingTracks END", self, true);
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, AddOverlappingTrack, [](auto& scope, AFGBuildableRailroadTrack* self, AFGBuildableRailroadTrack* track) {
        DumpRailTrack("AFGBuildableRailroadTrack::AddOverlappingTrack START", self, true);
        scope(self, track);
        DumpRailTrack("AFGBuildableRailroadTrack::AddOverlappingTrack END", self, false);
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, PostSerializedFromBlueprint, [](auto& scope, AFGBuildableRailroadTrack* self, bool isBlueprintWorld) {
    //    DumpRailTrack(FString::Printf(TEXT("AFGBuildableRailroadTrack::PostSerializedFromBlueprint START isBlueprintWorld: %d"), isBlueprintWorld), self, true);
    //    scope(self, isBlueprintWorld);
    //    DumpRailTrack("AFGBuildableRailroadTrack::PostSerializedFromBlueprint END", self, false);
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, SetTrackGraphID, [](auto& scope, AFGBuildableRailroadTrack* self, int32 trackGraphID) {
        AL_LOG("AFGBuildableRailroadTrack::SetTrackGraphID START %s trackGraphID: %d", *self->GetName(), trackGraphID);
        scope(self, trackGraphID);
        AL_LOG("AFGBuildableRailroadTrack::SetTrackGraphID END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, SetSignalBlock, [](auto& scope, AFGBuildableRailroadTrack* self, TWeakPtr< FFGRailroadSignalBlock > block) {
        DumpRailTrack("AFGBuildableRailroadTrack::SetSignalBlock START", self, true);
        scope(self, block);
        DumpRailTrack("AFGBuildableRailroadTrack::SetSignalBlock END", self, false);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableRailroadTrack, SetupConnections, [](auto& scope, AFGBuildableRailroadTrack* self) {
        DumpRailTrack("AFGBuildableRailroadTrack::SetupConnections START", self, true);
        scope(self);
        DumpRailTrack("AFGBuildableRailroadTrack::SetupConnections END", self, false);
        });

    ///* AFGRailroadSignalHologram */

    //SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSignalHologram, ConfigureActor,
    //    [](auto& scope, const AFGRailroadSignalHologram* self, AFGBuildable* buildable)
    //    {
    //        auto signal = Cast<AFGBuildableRailroadSignal>(buildable);
    //        DumpRailSignal("AFGRailroadSignalHologram::ConfigureActor START", signal);
    //        scope(self, buildable);
    //        DumpRailSignal("AFGRailroadSignalHologram::ConfigureActor START", signal);
    //    });

    /* AFGRailroadSubsystem */

    SUBSCRIBE_METHOD(AFGRailroadSubsystem::MoveTrackPosition,
        [](auto& scope, struct FRailroadTrackPosition& position, float delta, float& out_movedDelta, float endStopDistance = 0.f)
        {
            DumpRailTrackPosition(TEXT("AFGRailroadSubsystem::MoveTrackPosition START"), &position);
            scope(position, delta, out_movedDelta, endStopDistance);
            DumpRailTrackPosition(TEXT("AFGRailroadSubsystem::MoveTrackPosition END"), &position);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, AddTrack,
        [](auto& scope, AFGRailroadSubsystem* self, AFGBuildableRailroadTrack* track)
        {
            AL_LOG("AFGRailroadSubsystem::AddTrack START %s (%s). track: %s", *self->GetName(), *self->GetClass()->GetName(), *track->GetName());
            scope(self, track);
            AL_LOG("AFGRailroadSubsystem::AddTrack END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, RemoveTrack,
        [](auto& scope, AFGRailroadSubsystem* self, AFGBuildableRailroadTrack* track)
        {
            AL_LOG("AFGRailroadSubsystem::RemoveTrack START %s (%s). track: %s", *self->GetName(), *self->GetClass()->GetName(), *track->GetName());
            scope(self, track);
            AL_LOG("AFGRailroadSubsystem::RemoveTrack END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, AddSignal,
        [](auto& scope, AFGRailroadSubsystem* self, AFGBuildableRailroadSignal* signal)
        {
            AL_LOG("AFGRailroadSubsystem::AddSignal START %s (%s). signal: %s", *self->GetName(), *self->GetClass()->GetName(), *signal->GetName());
            scope(self, signal);
            DumpRailSubsystem("AFGRailroadSubsystem::AddSignal END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, RemoveSignal,
        [](auto& scope, AFGRailroadSubsystem* self, AFGBuildableRailroadSignal* signal)
        {
            AL_LOG("AFGRailroadSubsystem::RemoveSignal START %s (%s). signal: %s", *self->GetName(), *self->GetClass()->GetName(), *signal->GetName());
            scope(self, signal);
            DumpRailSubsystem("AFGRailroadSubsystem::RemoveSignal END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, RebuildTrackGraph,
        [](auto& scope, AFGRailroadSubsystem* self, int32 graphID)
        {
            AL_LOG("AFGRailroadSubsystem::RebuildTrackGraph START %s (%s). graphID %d", *self->GetName(), *self->GetClass()->GetName(), graphID);
            scope(self, graphID);
            DumpRailSubsystem("AFGRailroadSubsystem::RebuildTrackGraph END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, RebuildSignalBlocks,
        [](auto& scope, AFGRailroadSubsystem* self, int32 graphID)
        {
            AL_LOG("AFGRailroadSubsystem::RebuildSignalBlocks START %s (%s). graphID %d", *self->GetName(), *self->GetClass()->GetName(), graphID);
            scope.Cancel();
            RebuildSignalBlocks(self, graphID);
            DumpRailSubsystem("AFGRailroadSubsystem::RebuildSignalBlocks END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, MergeTrackGraphs,
        [](auto& scope, AFGRailroadSubsystem* self, int32 first, int32 second)
        {
            AL_LOG("AFGRailroadSubsystem::MergeTrackGraphs START %s (%s). first %d, second %d", *self->GetName(), *self->GetClass()->GetName(), first, second);
            scope(self, first, second);
            DumpRailSubsystem("AFGRailroadSubsystem::MergeTrackGraphs END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, CreateTrackGraph,
        [](auto& scope, AFGRailroadSubsystem* self)
        {
            AL_LOG("AFGRailroadSubsystem::CreateTrackGraph START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
            scope(self);
            DumpRailSubsystem("AFGRailroadSubsystem::CreateTrackGraph END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, RemoveTrackGraph,
        [](auto& scope, AFGRailroadSubsystem* self, int32 graphID)
        {
            AL_LOG("AFGRailroadSubsystem::RemoveTrackGraph START %s (%s). graphID %d", *self->GetName(), *self->GetClass()->GetName(), graphID);
            scope(self, graphID);
            DumpRailSubsystem("AFGRailroadSubsystem::RemoveTrackGraph END", self);
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, AddTrackToGraph,
        [](auto& scope, AFGRailroadSubsystem* self, AFGBuildableRailroadTrack* track, int32 graphID)
        {
            AL_LOG("AFGRailroadSubsystem::AddTrackToGraph START %s (%s). track: %s, graphID %d", *self->GetName(), *self->GetClass()->GetName(), *track->GetName(), graphID);
            scope(self, track, graphID);
            AL_LOG("AFGRailroadSubsystem::AddTrackToGraph END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, RemoveTrackFromGraph,
        [](auto& scope, AFGRailroadSubsystem* self, AFGBuildableRailroadTrack* track)
        {
            AL_LOG("AFGRailroadSubsystem::RemoveTrackFromGraph START %s (%s). track: %s", *self->GetName(), *self->GetClass()->GetName(), *track->GetName());
            scope(self, track);
            AL_LOG("AFGRailroadSubsystem::RemoveTrackFromGraph END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, MarkGraphAsChanged,
        [](auto& scope, AFGRailroadSubsystem* self, int32 graphID)
        {
            AL_LOG("AFGRailroadSubsystem::MarkGraphAsChanged START %s (%s). graphID %d", *self->GetName(), *self->GetClass()->GetName(), graphID);
            scope(self, graphID);
            AL_LOG("AFGRailroadSubsystem::MarkGraphAsChanged END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGRailroadSubsystem, MarkGraphForFullRebuild,
        [](auto& scope, AFGRailroadSubsystem* self, int32 graphID)
        {
            AL_LOG("AFGRailroadSubsystem::MarkGraphForFullRebuild START %s (%s). graphID %d", *self->GetName(), *self->GetClass()->GetName(), graphID);
            scope(self, graphID);
            AL_LOG("AFGRailroadSubsystem::MarkGraphForFullRebuild END");
        });
}

void AutoLinkDebugging::RegisterPipeTraceHooks()
{
    /* AFGPipeNetwork */

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, SetPipeNetworkID, [](auto& scope, AFGPipeNetwork* self, int id) {
        AL_LOG("AFGPipeNetwork::SetPipeNetworkID START %s (%d). id: %d", *self->GetName(), self->GetPipeNetworkID(), id);
        scope(self, id);
        AL_LOG("AFGPipeNetwork::SetPipeNetworkID END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, AddFluidIntegrant, [](auto& scope, AFGPipeNetwork* self, class IFGFluidIntegrantInterface* fluidIntegrant) {
        AL_LOG("AFGPipeNetwork::AddFluidIntegrant START %s (%d) Integrant: %s", *self->GetName(), self->GetPipeNetworkID(), *GetFluidIntegrantName(fluidIntegrant));
        scope(self, fluidIntegrant);
        AL_LOG("AFGPipeNetwork::AddFluidIntegrant END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, RemoveFluidIntegrant, [](auto& scope, AFGPipeNetwork* self, class IFGFluidIntegrantInterface* fluidIntegrant) {
        AL_LOG("AFGPipeNetwork::RemoveFluidIntegrant START %s (%d) Integrant: %s", *self->GetName(), self->GetPipeNetworkID(), *GetFluidIntegrantName(fluidIntegrant));
        scope(self, fluidIntegrant);
        AL_LOG("AFGPipeNetwork::RemoveFluidIntegrant END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, MergeNetworks, [](auto& scope, AFGPipeNetwork* self, AFGPipeNetwork* network) {
        AL_LOG("AFGPipeNetwork::MergeNetworks START %s (%d), network: %s", *self->GetName(), self->GetPipeNetworkID(), *network->GetName());
        DumpPipeNetwork("AFGPipeNetwork::MergeNetworks BEFORE", self);
        DumpPipeNetwork("AFGPipeNetwork::MergeNetworks BEFORE", network);
        scope(self, network);
        DumpPipeNetwork("AFGPipeNetwork::MergeNetworks END", self);
        AL_LOG("AFGPipeNetwork::MergeNetworks END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, RemoveAllFluidIntegrants, [](auto& scope, AFGPipeNetwork* self) {
        AL_LOG("AFGPipeNetwork::RemoveAllFluidIntegrants START %s (%d)", *self->GetName(), self->GetPipeNetworkID());
        scope(self);
        DumpPipeNetwork("AFGPipeNetwork::RemoveAllFluidIntegrants END", self);
        AL_LOG("AFGPipeNetwork::RemoveAllFluidIntegrants END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, GetFirstFluidIntegrant, [](auto& scope, AFGPipeNetwork* self) {
        AL_LOG("AFGPipeNetwork::GetFirstFluidIntegrant START %s (%d)", *self->GetName(), self->GetPipeNetworkID());
        auto val = scope(self);
        DumpPipeNetwork("AFGPipeNetwork::GetFirstFluidIntegrant END", self);
        return val;
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, TryPropagateFluidDescriptorFrom, [](auto& scope, AFGPipeNetwork* self, AFGPipeNetwork* network) {
        AL_LOG("AFGPipeNetwork::TryPropagateFluidDescriptorFrom START %s (%d), network: %s", *self->GetName(), self->GetPipeNetworkID(), *network->GetName());
        scope(self, network);
        AL_LOG("AFGPipeNetwork::TryPropagateFluidDescriptorFrom END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeNetwork, MarkForFullRebuild, [](auto& scope, AFGPipeNetwork* self) {
        AL_LOG("AFGPipeNetwork::MarkForFullRebuild START %s", *self->GetName());
        scope(self);
        AL_LOG("AFGPipeNetwork::MarkForFullRebuild END");
        });

    /* AFGPipeSubsystem */

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, RegisterPipeNetwork, [](auto& scope, AFGPipeSubsystem* self, class AFGPipeNetwork* network) {
        AL_LOG("AFGPipeSubsystem::RegisterPipeNetwork START");
        DumpPipeNetwork("AFGPipeSubsystem::RegisterPipeNetwork", network);
        scope(self, network);
        AL_LOG("AFGPipeSubsystem::RegisterPipeNetwork END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, UnregisterPipeNetwork, [](auto& scope, AFGPipeSubsystem* self, class AFGPipeNetwork* network) {
        AL_LOG("AFGPipeSubsystem::UnregisterPipeNetwork START");
        DumpPipeNetwork("AFGPipeSubsystem::RegisterPipeNetwork", network);
        scope(self, network);
        AL_LOG("AFGPipeSubsystem::UnregisterPipeNetwork END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, TrySetNetworkFluidDescriptor, [](auto& scope, AFGPipeSubsystem* self, int32 networkID, TSubclassOf< class UFGItemDescriptor > fluidDescriptor) {
        AL_LOG("AFGPipeSubsystem::TrySetNetworkFluidDescriptor START %d", networkID);
        scope(self, networkID, fluidDescriptor);
        AL_LOG("AFGPipeSubsystem::TrySetNetworkFluidDescriptor END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, FlushIntegrant, [](auto& scope, AFGPipeSubsystem* self, AActor* integrantActor) {
        AL_LOG("AFGPipeSubsystem::FlushIntegrant START %s", *integrantActor->GetName());
        scope(self, integrantActor);
        AL_LOG("AFGPipeSubsystem::FlushIntegrant END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, FlushPipeNetwork, [](auto& scope, AFGPipeSubsystem* self, int32 networkID) {
        AL_LOG("AFGPipeSubsystem::FlushPipeNetwork START %d", networkID);
        scope(self, networkID);
        AL_LOG("AFGPipeSubsystem::FlushPipeNetwork END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, FlushPipeNetworkFromIntegrant, [](auto& scope, AFGPipeSubsystem* self, AActor* integrantActor) {
        AL_LOG("AFGPipeSubsystem::FlushPipeNetworkFromIntegrant START %s", *integrantActor->GetName());
        scope(self, integrantActor);
        AL_LOG("AFGPipeSubsystem::FlushPipeNetworkFromIntegrant END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, RegisterFluidIntegrant, [](auto& scope, AFGPipeSubsystem* self, IFGFluidIntegrantInterface* fluidIntegrant) {
        AL_LOG("AFGPipeSubsystem::RegisterFluidIntegrant START %s", *GetFluidIntegrantName(fluidIntegrant));
        scope(self, fluidIntegrant);
        AL_LOG("AFGPipeSubsystem::RegisterFluidIntegrant END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, UnregisterFluidIntegrant, [](auto& scope, AFGPipeSubsystem* self, IFGFluidIntegrantInterface* fluidIntegrant) {
        AL_LOG("AFGPipeSubsystem::UnregisterFluidIntegrant START %s", *GetFluidIntegrantName(fluidIntegrant));
        scope(self, fluidIntegrant);
        AL_LOG("AFGPipeSubsystem::UnregisterFluidIntegrant END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, RebuildPipeNetwork, [](auto& scope, AFGPipeSubsystem* self, int32 networkID) {
        AL_LOG("AFGPipeSubsystem::RebuildPipeNetwork START %d", networkID);
        auto network = self->FindPipeNetwork(networkID);
        DumpPipeNetwork("AFGPipeSubsystem::RebuildPipeNetwork START", network);
        scope(self, networkID);
        DumpPipeNetwork("AFGPipeSubsystem::RebuildPipeNetwork END", network);
        AL_LOG("AFGPipeSubsystem::RebuildPipeNetwork END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, MergePipeNetworks, [](auto& scope, AFGPipeSubsystem* self, int32 first, int32 second) {
        AL_LOG("AFGPipeSubsystem::MergePipeNetworks START first: %d, second: %d", first, second);
        scope(self, first, second);
        AL_LOG("AFGPipeSubsystem::MergePipeNetworks END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, RemoveFluidIntegrantFromNetwork, [](auto& scope, AFGPipeSubsystem* self, IFGFluidIntegrantInterface* fluidIntegrant) {
        AL_LOG("AFGPipeSubsystem::RemoveFluidIntegrantFromNetwork START %s", *GetFluidIntegrantName(fluidIntegrant));
        DumpFluidIntegrant("AFGPipeSubsystem::RemoveFluidIntegrantFromNetwork", fluidIntegrant);
        scope(self, fluidIntegrant);
        AL_LOG("AFGPipeSubsystem::RemoveFluidIntegrantFromNetwork END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGPipeSubsystem, AddFluidIntegrantToNetwork, [](auto& scope, AFGPipeSubsystem* self, IFGFluidIntegrantInterface* fluidIntegrant, int32 networkID) {
        AL_LOG("AFGPipeSubsystem::AddFluidIntegrantToNetwork START %s (%d)", *GetFluidIntegrantName(fluidIntegrant), networkID);
        scope(self, fluidIntegrant, networkID);
        AL_LOG("AFGPipeSubsystem::AddFluidIntegrantToNetwork END");
        });

    /* AFGBuildablePipelineAttachment */

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildablePipelineAttachment, BeginPlay, [](auto& scope, AFGBuildablePipelineAttachment* self) {
        AL_LOG("AFGBuildablePipelineAttachment::BeginPlay START %s", *self->GetName());
        AL_LOG("AFGBuildablePipelineAttachment::BeginPlay START %d connections", self->GetPipeConnections().Num());
        scope(self);
        AL_LOG("AFGBuildablePipelineAttachment::BeginPlay END %d connections", self->GetPipeConnections().Num());
        AL_LOG("AFGBuildablePipelineAttachment::BeginPlay END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildablePipelineAttachment, EndPlay, [](auto& scope, AFGBuildablePipelineAttachment* self, const EEndPlayReason::Type endPlayReason) {
        AL_LOG("AFGBuildablePipelineAttachment::EndPlay START %s", *self->GetName());
        scope(self, endPlayReason);
        AL_LOG("AFGBuildablePipelineAttachment::EndPlay END");
        });

    //SUBSCRIBE_UOBJECT_METHOD(AFGBuildablePipelineAttachment, GetFluidBox, [](auto& scope, AFGBuildablePipelineAttachment* self) {
    //    AL_LOG("AFGBuildablePipelineAttachment::GetFluidBox START %s", *self->GetName());
    //    auto val = scope(self);
    //    AL_LOG("AFGBuildablePipelineAttachment::GetFluidBox END");
    //    return val;
    //    });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildablePipelineAttachment, GetPipeConnections, [&](auto& scope, AFGBuildablePipelineAttachment* self) {
        AL_LOG("AFGBuildablePipelineAttachment::GetPipeConnections START");
        TArray<UFGPipeConnectionComponent*> conns = scope(self);
        AL_LOG("AFGBuildablePipelineAttachment::GetPipeConnections END");
        return conns;
        });
}

void AutoLinkDebugging::DumpConnection(FString prefix, UFGFactoryConnectionComponent* c, bool dumpConnected)
{
    EnsureColon(prefix);
    if (!c)
    {
        AL_LOG("%s Connection is null", *prefix);
        return;
    }
    AL_LOG("%s Connection is %s", *prefix, *c->GetFName().GetPlainNameString());

    auto nestedPrefix = GetNestedPrefix(prefix);

    AL_LOG("%s mDirection: %s", *nestedPrefix, *StaticEnum<EFactoryConnectionDirection>()->GetNameStringByValue((int64)c->mDirection));
    AL_LOG("%s mConnectorClearance: %f", *nestedPrefix, c->mConnectorClearance);

    if (dumpConnected)
    {
        DumpConnection(GetNestedPrefix(nestedPrefix).Append(" mConnectedComponent"), c->mConnectedComponent, false);
    }

    AL_LOG("%s mHasConnectedComponent: %d", *nestedPrefix, c->mHasConnectedComponent);
    if (c->mOuterBuildable)
    {
        AL_LOG("%s mOuterBuildable: %s", *nestedPrefix, *c->mOuterBuildable->GetName());
    }
    else
    {
        AL_LOG("%s mOuterBuildable: null", *nestedPrefix);
    }
}

void AutoLinkDebugging::DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor)
{
    EnsureColon(prefix);
    if (!conveyor)
    {
        AL_LOG("%s AFGBuildableConveyorBase is null", *prefix);
        return;
    }

    AL_LOG("%s Conveyor is %s", *prefix, *conveyor->GetName());
    auto ownerChainActor = conveyor->GetConveyorChainActor();
    if (ownerChainActor)
    {
        AL_LOG("%s Chain actor: %s", *prefix, *ownerChainActor->GetName());
    }
    else
    {
        AL_LOG("%s Chain actor: null", *prefix);
    }
    AL_LOG("%s Chain segment index: %d", *prefix, conveyor->mChainSegmentIndex);
    AL_LOG("%s Chain flags: %d", *prefix, conveyor->GetConveyorChainFlags());
    auto nextTickConveyor = conveyor->GetNextTickConveyor();
    if (nextTickConveyor)
    {
        AL_LOG("%s Next tick conveyor: %s", *prefix, *nextTickConveyor->GetName());
    }
    else
    {
        AL_LOG("%s Next tick conveyor: null", *prefix);
    }

    DumpConnection(prefix, conveyor->GetConnection0());
    DumpConnection(prefix, conveyor->GetConnection1());
}

void AutoLinkDebugging::DumpConveyorLift(FString prefix, AFGBuildableConveyorLift* conveyor)
{
    EnsureColon(prefix);
    if (!conveyor)
    {
        AL_LOG("%s AFGBuildableConveyorLift is null", *prefix);
        return;
    }

    DumpConveyor(prefix, conveyor);

    AL_LOG("%s mOpposingConnectionClearance[0]: %f", *prefix, conveyor->mOpposingConnectionClearance[0]);
    AL_LOG("%s mOpposingConnectionClearance[1]: %f", *prefix, conveyor->mOpposingConnectionClearance[1]);
}

void AutoLinkDebugging::DumpConnection(FString prefix, UFGPipeConnectionComponent* c)
{
    EnsureColon(prefix);
    if (!c)
    {
        AL_LOG("%s UFGPipeConnectionComponent is null", *prefix);
        return;
    }
    AL_LOG("%s UFGPipeConnectionComponent is %s at %x", *prefix, *c->GetName(), c);

    auto nestedPrefix = GetNestedPrefix(prefix);

    AL_LOG("%s mPipeNetworkID: %d", *nestedPrefix, c->mPipeNetworkID);
    AL_LOG("%s mPipeConnectionType: %d", *nestedPrefix, c->mPipeConnectionType);
    AL_LOG("%s mConnectorClearance: %d", *nestedPrefix, c->mConnectorClearance);

    AL_LOG("%s IsConnected: %d", *nestedPrefix, c->IsConnected());
    if (c->mConnectedComponent)
    {
        AL_LOG("%s mConnectedComponent: %s at %x", *nestedPrefix, *c->mConnectedComponent->GetName(), c->mConnectedComponent);
    }
    else
    {
        AL_LOG("%s mConnectedComponent: null", *nestedPrefix);
    }

    AL_LOG("%s HasFluidIntegrant: %d", *nestedPrefix, c->HasFluidIntegrant());
    AL_LOG("%s mFluidIntegrant: %s at %x", *nestedPrefix, *GetFluidIntegrantName( c->mFluidIntegrant ), c->mFluidIntegrant);
}

void AutoLinkDebugging::DumpFluidIntegrant(FString prefix, IFGFluidIntegrantInterface* f)
{
    EnsureColon(prefix);
    if (!f)
    {
        AL_LOG("%s IFGFluidIntegrantInterface is null", *prefix);
        return;
    }

    if (auto actor = Cast<AActor>(f))
    {
        AL_LOG("%s IFGFluidIntegrantInterface is %s (%s) at %x", *prefix, *actor->GetName(), *actor->GetClass()->GetName(), actor);
    }
    else
    {
        AL_LOG("%s IFGFluidIntegrantInterface at %x", *prefix, f);
    }

    for (auto c : f->GetPipeConnections())
    {
        DumpConnection(prefix, c);
    }
}

void AutoLinkDebugging::DumpConnection(FString prefix, UFGPipeConnectionComponentHyper* c)
{
    EnsureColon(prefix);
    if (!c)
    {
        AL_LOG("%s UFGPipeConnectionComponentHyper is null", *prefix);
        return;
    }
    AL_LOG("%s UFGPipeConnectionComponentHyper is %s at %x", *prefix, *c->GetName(), c);

    auto nestedPrefix = GetNestedPrefix(prefix);

    AL_LOG("%s mDisallowSnappingTo: %d", *nestedPrefix, c->mDisallowSnappingTo);
    AL_LOG("%s mPipeConnectionType: %d", *nestedPrefix, c->mPipeConnectionType);
    AL_LOG("%s mConnectorClearance: %d", *nestedPrefix, c->mConnectorClearance);

    AL_LOG("%s IsConnected: %d", *nestedPrefix, c->IsConnected());
    if (c->mConnectedComponent)
    {
        AL_LOG("%s mConnectedComponent: %s at %x", *nestedPrefix, *c->mConnectedComponent->GetName(), c->mConnectedComponent);
    }
    else
    {
        AL_LOG("%s mConnectedComponent: null", *nestedPrefix);
    }
}

void AutoLinkDebugging::DumpBuildEffectActor(FString prefix, const AFGBuildEffectActor* b)
{
    EnsureColon(prefix);
    if (!b)
    {
        AL_LOG("%s AFGBuildEffectActor is null", *prefix);
        return;
    }

    AL_LOG("%s AFGBuildEffectActor is %s at %x", *prefix, *b->GetName(), b);

    auto nestedPrefix = GetNestedPrefix(prefix);

    AL_LOG("%s mBounds: %s", *nestedPrefix, *b->mBounds.ToString());
    auto mBoundsSize = b->mBounds.GetSize();
    AL_LOG("%s mBounds Dimensions: X: %f, Y: %f, Z: %f", *nestedPrefix, mBoundsSize.X, mBoundsSize.Y, mBoundsSize.Z);
    AL_LOG("%s mActorBounds: %s", *nestedPrefix, *b->mActorBounds.ToString());
    auto mActorBoundsSize = b->mActorBounds.GetSize();
    AL_LOG("%s mActorBounds Dimensions: X: %f, Y: %f, Z: %f", *nestedPrefix, mActorBoundsSize.X, mActorBoundsSize.Y, mActorBoundsSize.Z);
    AL_LOG("%s mIsBlueprint: %d", *nestedPrefix, b->mIsBlueprint);
    AL_LOG("%s NumActors: %d", *nestedPrefix, b->NumActors);
    AL_LOG("%s mSourceActors: %d", *nestedPrefix, b->mSourceActors.Num());
    int i = 0;
    for (auto& pActor : b->mSourceActors)
    {
        AL_LOG("%s mSourceActor[%d]: %s (%s)", *GetNestedPrefix(nestedPrefix), i++, *pActor->GetName(), *pActor->GetClass()->GetName());
    }
}

void AutoLinkDebugging::DumpPipeNetwork(FString prefix, const AFGPipeNetwork* p)
{
    EnsureColon(prefix);
    if (!p)
    {
        AL_LOG("%s AFGPipeNetwork is null", *prefix);
        return;
    }

    AL_LOG("%s AFGPipeNetwork is %s with ID %d", *prefix, *p->GetName(), p->GetPipeNetworkID());
    auto nestedPrefix = GetNestedPrefix(prefix);
    AL_LOG("%s NumFluidIntegrants %d", *nestedPrefix, p->NumFluidIntegrants());

    int i = 0;
    for (auto in : p->mFluidIntegrants)
    {
        AL_LOG("%s mFluidIntegrants[%d]: %s", *GetNestedPrefix(nestedPrefix), i++, *GetFluidIntegrantName(in));
    }
}

void AutoLinkDebugging::DumpRailConnection(FString prefix, const UFGRailroadTrackConnectionComponent* c, bool shortDump)
{
    EnsureColon(prefix);
    if (!c)
    {
        AL_LOG("%s UFGRailroadTrackConnectionComponent is null", *prefix);
        return;
    }

    AL_LOG("%s UFGRailroadTrackConnectionComponent on %s", *prefix, *c->GetOuter()->GetName());
    auto nestedPrefix = GetNestedPrefix(prefix);

    auto connections = c->GetConnections();
    int i = 0;
    AL_LOG("%s GetConnections: %d items", *nestedPrefix, connections.Num());
    for (auto conn : connections)
    {
        AL_LOG("%s", *GetNestedPrefix(nestedPrefix).Appendf(TEXT(" GetConnections[%d] %s"), i++, *conn->GetOuter()->GetName()));
    }
    DumpRailTrackPosition(FString(nestedPrefix).Append(TEXT(" mTrackPosition")), &c->mTrackPosition);

    if (shortDump) return;

    AL_LOG("%s mSwitchPosition: %d", *nestedPrefix, c->mSwitchPosition);
    DumpRailSignal(FString(nestedPrefix).Append(TEXT(" mFacingSignal")), c->mFacingSignal);
    DumpRailSignal(FString(nestedPrefix).Append(TEXT(" mTrailingSignal")), c->mTrailingSignal);
}

void AutoLinkDebugging::DumpRailTrackPosition(FString prefix, const FRailroadTrackPosition* p)
{
    EnsureColon(prefix);
    if (!p)
    {
        AL_LOG("%s FRailroadTrackPosition is null", *prefix);
        return;
    }

    AL_LOG("%s FRailroadTrackPosition %p", *prefix, p);

    FString nestedPrefix = GetNestedPrefix(prefix);
    DumpRailTrack(FString(nestedPrefix).Append(TEXT(" Track")), p->Track.Get(), true);
    AL_LOG("%s Offset: %f", *nestedPrefix, p->Offset);
    AL_LOG("%s Forward: %f", *nestedPrefix, p->Forward);
}

void AutoLinkDebugging::DumpRailTrack(FString prefix, const AFGBuildableRailroadTrack* t, bool shortDump)
{
    EnsureColon(prefix);
    if (!t)
    {
        AL_LOG("%s AFGBuildableRailroadTrack is null", *prefix);
        return;
    }

    AL_LOG("%s AFGBuildableRailroadTrack is %s", *prefix, *t->GetName());

    FString nestedPrefix = GetNestedPrefix(prefix);
    AL_LOG("%s mSignalBlock: %p", *nestedPrefix, t->mSignalBlock.Pin().Get());
    AL_LOG("%s mSignalBlockID: %d", *nestedPrefix, t->mSignalBlockID);

    if (shortDump) return;

    AL_LOG("%s mConnections has %d elements", *nestedPrefix, t->mConnections.Num());

    int i = 0;
    for (auto c : t->mConnections)
    {
        DumpRailConnection(GetNestedPrefix(nestedPrefix).Appendf(TEXT(" mConnections[%d]"), i++), c, false);
    }
    AL_LOG("%s mIsOwnedByPlatform: %d", *nestedPrefix, t->mIsOwnedByPlatform);
    AL_LOG("%s mTrackGraphID: %d", *nestedPrefix, t->mTrackGraphID);
    AL_LOG("%s mLength: %f", *nestedPrefix, t->mLength);
    AL_LOG("%s mOverlappingTracks has %d elements", *nestedPrefix, t->mOverlappingTracks.Num());
    i = 0;
    for (auto o : t->mOverlappingTracks)
    {
        DumpRailTrack(GetNestedPrefix(nestedPrefix).Appendf(TEXT(" mOverlappingTracks[%d]"), i++), o, true);
    }
}

void AutoLinkDebugging::DumpRailTrackGraph(FString prefix, const FTrackGraph* g)
{
    EnsureColon(prefix);
    if (!g)
    {
        AL_LOG("%s FTrackGraph is null", *prefix);
        return;
    }

    AL_LOG("%s FTrackGraph at %p", *prefix, g);

    FString nestedPrefix = GetNestedPrefix(prefix);
    AL_LOG("%s NeedFullRebuild: %d", *nestedPrefix, g->NeedFullRebuild);
    AL_LOG("%s HasChanged: %d", *nestedPrefix, g->HasChanged);
    AL_LOG("%s SignalBlocks: %d items", *nestedPrefix, g->SignalBlocks.Num());
    int i = 0;
    for (auto s : g->SignalBlocks)
    {
        DumpRailSignalBlock(GetNestedPrefix(nestedPrefix).Appendf(TEXT(" SignalBlocks[%d]"), i++), s.Get());
    }
}

void AutoLinkDebugging::DumpRailSignal(FString prefix, const AFGBuildableRailroadSignal* s)
{
    EnsureColon(prefix);
    if (!s)
    {
        AL_LOG("%s AFGBuildableRailroadSignal is null", *prefix);
        return;
    }

    AL_LOG("%s AFGBuildableRailroadSignal is %s", *prefix, *s->GetName());
    FString nestedPrefix = GetNestedPrefix(prefix);
    DumpRailConnection(FString(nestedPrefix).Append(TEXT(" mOwningConnection")), s->mOwningConnection, true);

    AL_LOG("%s mGuardedConnections has %d elements", *nestedPrefix, s->mGuardedConnections.Num());
    int i = 0;
    for (auto c : s->mGuardedConnections)
    {
        DumpRailConnection(GetNestedPrefix(nestedPrefix).Appendf(TEXT(" mGuardedConnections[%d]"), i++), c, true);
    }

    AL_LOG("%s mObservedConnections has %d elements", *nestedPrefix, s->mObservedConnections.Num());
    i = 0;
    for (auto c : s->mObservedConnections)
    {
        DumpRailConnection(GetNestedPrefix(nestedPrefix).Appendf(TEXT(" mObservedConnections[%d]"), i++), c, true);
    }

    DumpRailSignalBlock(FString(nestedPrefix).Append(TEXT(" mObservedBlock")), s->mObservedBlock.Pin().Get());
    AL_LOG("%s mAspect: %d", *nestedPrefix, s->mAspect);
    AL_LOG("%s mBlockValidation: %d", *nestedPrefix, s->mBlockValidation);
    AL_LOG("%s mIsPathSignal: %d", *nestedPrefix, s->mIsPathSignal);
    AL_LOG("%s mIsBiDirectional: %d", *nestedPrefix, s->mIsBiDirectional);
    AL_LOG("%s mVisualState: %d", *nestedPrefix, s->mVisualState);
}

void AutoLinkDebugging::DumpRailSignalBlock(FString prefix, const FFGRailroadSignalBlock* b)
{
    EnsureColon(prefix);
    if (!b)
    {
        AL_LOG("%s FFGRailroadSignalBlock is null", *prefix);
        return;
    }

    AL_LOG("%s FFGRailroadSignalBlock ID: %d at %p", *prefix, b->ID, b);
}

void AutoLinkDebugging::DumpRailSubsystem(FString prefix, const AFGRailroadSubsystem* s)
{
    EnsureColon(prefix);
    if (!s)
    {
        AL_LOG("%s AFGRailroadSubsystem is null", *prefix);
        return;
    }

    AL_LOG("%s AFGRailroadSubsystem is %s", *prefix, *s->GetName());
    FString nestedPrefix = GetNestedPrefix(prefix);

    AL_LOG("%s mTrackGraphIDCounter: %d", *nestedPrefix, s->mTrackGraphIDCounter);
    AL_LOG("%s mTracks: %d items", *nestedPrefix, s->mTracks.Num());
    int i = 0;
    for (auto t : s->mTracks)
    {
        DumpRailTrack(GetNestedPrefix(nestedPrefix).Appendf(TEXT(" mTracks[%d]"), i++), t.Get(), false);
    }

    AL_LOG("%s mTrackGraphs: %d items", *nestedPrefix, s->mTrackGraphs.Num());
    for (auto& kvp : s->mTrackGraphs)
    {
        auto graphID = kvp.Key;
        auto& trackGraph = kvp.Value;
        DumpRailTrackGraph(GetNestedPrefix(nestedPrefix).Appendf(TEXT(" mTrackGraphs[%d]"), graphID), &trackGraph);
    }
}

void AutoLinkDebugging::RebuildSignalBlocks(AFGRailroadSubsystem* self, int32 graphID)
{
    FTrackGraph& graph = self->mTrackGraphs.FindChecked(graphID); // Checked for sanity

    AL_LOG("RebuildSignalBlocks, rebuilding signal blocks in graph '%i' with %i tracks and %i blocks.", graphID, graph.Tracks.Num(), graph.SignalBlocks.Num());

    // Remove the current graphs and rebuild everything from scratch. This will invalidate the weak pointers so no need to null the blocks on the tracks.
    graph.SignalBlocks.Empty();

    // Gather all the signals in the graph.
    TSet< AFGBuildableRailroadSignal* > signals;
    for (AFGBuildableRailroadTrack* track : graph.Tracks)
    {
        for (int32 i = 0; i < 2; ++i)
        {
            // Need to gather both here in case the track to or from the signal is missing.
            if (AFGBuildableRailroadSignal* signal = track->GetConnection(i)->GetFacingSignal())
            {
                signals.Add(signal);
            }
            if (AFGBuildableRailroadSignal* signal = track->GetConnection(i)->GetTrailingSignal())
            {
                signals.Add(signal);
            }
        }
    }

    AL_LOG("RebuildSignalBlocks: Found %d signals", signals.Num());

    // First update all the signals so their connections match up, needs to happen prior to calling GetObservedConnections or GetGuardedConnections, which the block rebuilding code depends on.
    for (auto signal : signals)
    {
        // First make sure the signal is up-to-date, with the cached connections.
        signal->UpdateConnections();
    }

    AL_LOG("RebuildSignalBlocks: ", signals.Num());

    // Now lets visit all the signals and fill the section behind them with a block.
    int32 ID = 0;
    for (auto signal : signals)
    {
        // Do we have any observed connections, otherwise there is no point of creating an empty block, skip.
        if (!signal->HasValidConnections())
        {
            DumpRailSignal("RebuildSignalBlocks: Does NOT have valid connections", signal);
            signal->SetObservedBlock(nullptr);

            continue;
        }

        // Has this signal already been visited from within a block, skip.
        if (signal->HasObservedBlock())
        {
            DumpRailSignal("RebuildSignalBlocks: Already has Observed block", signal);
            continue;
        }

        DumpRailSignal("RebuildSignalBlocks: Creating a new block for signal", signal);

        // We should create a new block for this signal.
        TSharedPtr< FFGRailroadSignalBlock > block = MakeShared< FFGRailroadSignalBlock >();
        block->ID = ID++;
        block->NoExitSignal = true; // At least until we find one.
        signal->SetObservedBlock(block);
        graph.SignalBlocks.Add(block);

        // Start by visiting the observed connections for the signal, and traverse from there.
        TArray< AFGBuildableRailroadTrack* > unvisited;

        // Helper to push the unvisited track including any overlapped tracks.
        auto PushUnvisited = [&unvisited](AFGBuildableRailroadTrack* unvisitedTrack)
            {
                unvisited.AddUnique(unvisitedTrack);
            };

        for (auto connection : signal->GetObservedConnections())
        {
            PushUnvisited(connection->GetTrack());
        }

        while (unvisited.Num() > 0)
        {
            auto visitedTrack = unvisited.Pop(false);

            DumpRailTrack("RebuildSignalBlocks: Visiting track", visitedTrack, true);

            if (!visitedTrack->HasSignalBlock())
            {
                AL_LOG("RebuildSignalBlocks: Track has no signal block")
                visitedTrack->SetSignalBlock(block);

                // Get all connected tracks, unless there is a signal separating them.
                for (int32 i = 0; i < 2; ++i)
                {
                    UFGRailroadTrackConnectionComponent* visitedConnection = visitedTrack->GetConnection(i);

                    DumpRailConnection(FString::Printf(TEXT("Visiting track connection %d"), i ), visitedConnection, true);

                    if (visitedConnection->GetStation())
                    {
                        block->ContainsStation = true;
                    }

                    auto exitSignal = visitedConnection->GetFacingSignal();
                    if (exitSignal)
                    {
                        block->NoExitSignal = false;
                    }
                    DumpRailSignal("Visiting track connection FacingSignal", exitSignal);

                    int j = 0;
                    for (UFGRailroadTrackConnectionComponent* unvisitedConnection : visitedConnection->GetConnections())
                    {
                        auto entrySignal = unvisitedConnection->GetFacingSignal();
                        DumpRailConnection(FString::Printf(TEXT("Visiting track connection -> GetConnections %d"), j), unvisitedConnection, true);
                        DumpRailSignal(FString::Printf(TEXT("Visiting track connection -> GetConnections %d FacingSignal"), j), entrySignal);

                        if (entrySignal || exitSignal)
                        {
                            // If there is a signal here pointing into our block, give it the same block.
                            if (entrySignal && entrySignal->HasValidConnections())
                            {
                                if (entrySignal->IsPathSignal() != signal->IsPathSignal())
                                {
                                    block->ContainsMixedEntrySignals = true;
                                }

                                entrySignal->SetObservedBlock(block);

                                // Check if this signal got the same block on both sides, then we have a loop.
                                for (auto connection : entrySignal->GetGuardedConnections())
                                {
                                    if (connection && connection->GetSignalBlock().HasSameObject(block.Get()))
                                    {
                                        block->ContainsLoop = true;
                                    }
                                }

                                // And lets visit any observed connections.
                                for (auto connection : entrySignal->GetObservedConnections())
                                {
                                    AL_LOG("Pushing observed connection %s on %s", *connection->GetName(), *connection->GetOuter()->GetName());
                                    PushUnvisited(connection->GetTrack());
                                }
                            }

                            // If we reach a signalled connection going out out of this block, skip, that will be visited by that signal later.
                            if (exitSignal)
                            {
                                if (exitSignal->GetObservedBlock().HasSameObject(block.Get()))
                                {
                                    block->ContainsLoop = true;
                                }
                            }
                        }
                        else
                        {
                            // No signals, continue traversing.
                            PushUnvisited(unvisitedConnection->GetTrack());
                        }
                        ++j;
                    }
                }

                // Lets also visit all overlapping tracks to the current one.
                for (auto overlappedTrack : visitedTrack->GetOverlappingTracks())
                {
                    PushUnvisited(overlappedTrack);
                }
            }
        }
    }

    // Let all the block update themselves.
    for (auto block : graph.SignalBlocks)
    {
        block->OnBlockChanged.Broadcast();

        if (block->NoExitSignal)
        {
            DumpRailSignalBlock("RebuildSignalBlocks found invalid block with no exit signals.", block.Get());
        }

        if (block->ContainsLoop)
        {
            DumpRailSignalBlock("RebuildSignalBlocks, found invalid block with loop in it.", block.Get());
        }

        if (block->ContainsMixedEntrySignals)
        {
            DumpRailSignalBlock("RebuildSignalBlocks, found invalid block with more than one type of entry signal.", block.Get());
        }

        if (block->ContainsStation)
        {
            DumpRailSignalBlock("RebuildSignalBlocks, found invalid block with one or more stations inside.", block.Get());
        }
    }
}
