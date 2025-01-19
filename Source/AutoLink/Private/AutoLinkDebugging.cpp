
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
#include "FGBuildableSubsystem.h"
#include "FGBuildEffectActor.h"
#include "FGBuildGun.h"
#include "FGBuildGunBuild.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeSubsystem.h"
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
            if (auto conveyor = Cast<AFGBuildableConveyorBase>(actor))
            {
                DumpConveyor(TEXT("UFGBuildGunState::OnRecipeSampled"), conveyor);
                dumpedAtLeastOnce = true;
            }

            if (auto integrant = Cast<IFGFluidIntegrantInterface>(actor))
            {
                DumpFluidIntegrant(TEXT("UFGBuildGunState::OnRecipeSampled"), integrant);
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
            }

            TInlineComponentArray<UFGPipeConnectionComponentHyper*> hyperConnections;
            actor->GetComponents(hyperConnections);
            for (auto connectionComponent : hyperConnections)
            {
                DumpConnection(TEXT("UFGBuildGunState::OnRecipeSampled"), connectionComponent);
            }

            AL_LOG("UFGBuildGunState::OnRecipeSampled. Actor %s (%s) at %x dumped.", *actor->GetName(), *actor->GetClass()->GetName(), actor);

            scope(buildGunState, recipe);
        });

    if (AL_REGISTER_DEBUG_TRACE_HOOKS)
    {
        RegisterDebugTraceHooks();
    }
}

void AutoLinkDebugging::RegisterDebugTraceHooks()
{
    if (!AL_DEBUG_ENABLE_MOD)
    {
        RegisterDebugTraceForDisabledModFunctions();
    }

    if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
    {
        RegisterBuildEffectTraceHooks();
    }

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

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildableHologram, ConfigureBuildEffect,
        [](auto& scope, AFGBuildableHologram* self, AFGBuildable* buildable)
        {
            AL_LOG("AFGBuildableHologram::ConfigureBuildEffect START %s (%s). Buildable: %s", *self->GetName(), *self->GetClass()->GetName(), *buildable->GetName());
            scope(self, buildable);
            AL_LOG("AFGBuildableHologram::ConfigureBuildEffect END");
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, BeginPlay, [&](auto& scope, AFGBuildable* self) {
        AL_LOG("AFGBuildable::BeginPlay START %s", *self->GetName());
        if (auto b = Cast<AFGBuildablePipelineAttachment>(self))
        {
            AL_LOG("AFGBuildable::BeginPlay START %d connections", b->GetPipeConnections().Num());
        }

        scope(self);

        if (auto b = Cast<AFGBuildablePipelineAttachment>(self))
        {
            AL_LOG("AFGBuildable::BeginPlay END %d connections", b->GetPipeConnections().Num());
        }
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
        AL_LOG("AFGBuildable::PostSerializedFromBlueprint START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
        scope(self, isBlueprintWorld);
        AL_LOG("AFGBuildable::PostSerializedFromBlueprint END");
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

void AutoLinkDebugging::DumpConnection(FString prefix, UFGFactoryConnectionComponent* c)
{
    if (!c)
    {
        AL_LOG("%s:\t Connection is null", *prefix);
        return;
    }
    AL_LOG("%s:\t Connection is %s", *prefix, *c->GetFName().GetPlainNameString());

    AL_LOG("%s:\t\t mConnector: %d", *prefix, c->mConnector);
    AL_LOG("%s:\t\t mDirection: %d", *prefix, c->mDirection);
    AL_LOG("%s:\t\t mConnectorClearance: %f", *prefix, c->mConnectorClearance);
    if (c->mConnectedComponent)
    {
        AL_LOG("%s:\t\t mConnectedComponent: %s on %s", *prefix, *c->mConnectedComponent->GetFName().GetPlainNameString(), *c->mConnectedComponent->GetOuterBuildable()->GetName());
    }
    else
    {
        AL_LOG("%s:\t\t mConnectedComponent: null", *prefix);
    }
    AL_LOG("%s:\t\t mHasConnectedComponent: %d", *prefix, c->mHasConnectedComponent);
    if (c->mConnectionInventory)
    {
        AL_LOG("%s:\t\t mConnectionInventory: %s", *prefix, *c->mConnectionInventory->GetName());
    }
    else
    {
        AL_LOG("%s:\t\t mConnectionInventory: null", *prefix);
    }
    AL_LOG("%s:\t\t mInventoryAccessIndex: %d", *prefix, c->mInventoryAccessIndex);
    if (c->mOuterBuildable)
    {
        AL_LOG("%s:\t\t mOuterBuildable: %s", *prefix, *c->mOuterBuildable->GetName());
    }
    else
    {
        AL_LOG("%s:\t\t mOuterBuildable: null", *prefix);
    }
    AL_LOG("%s:\t\t mForwardPeekAndGrabToBuildable: %d", *prefix, c->mForwardPeekAndGrabToBuildable);
}

void AutoLinkDebugging::DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor)
{
    if (!conveyor)
    {
        AL_LOG("%s: Conveyor is null", *prefix);
        return;
    }

    AL_LOG("%s: Conveyor is %s", *prefix, *conveyor->GetName());
    auto ownerChainActor = conveyor->GetConveyorChainActor();
    if (ownerChainActor)
    {
        AL_LOG("%s: Chain actor: %s", *prefix, *ownerChainActor->GetName());
    }
    else
    {
        AL_LOG("%s: Chain actor: null", *prefix);
    }
    AL_LOG("%s: Chain segment index: %d", *prefix, conveyor->mChainSegmentIndex);
    AL_LOG("%s: Chain flags: %d", *prefix, conveyor->GetConveyorChainFlags());
    auto nextTickConveyor = conveyor->GetNextTickConveyor();
    if (nextTickConveyor)
    {
        AL_LOG("%s: Next tick conveyor: %s", *prefix, *nextTickConveyor->GetName());
    }
    else
    {
        AL_LOG("%s: Next tick conveyor: null", *prefix);
    }

    DumpConnection(prefix, conveyor->GetConnection0());
    DumpConnection(prefix, conveyor->GetConnection1());
}

void AutoLinkDebugging::DumpConnection(FString prefix, UFGPipeConnectionComponent* c)
{
    if (!c)
    {
        AL_LOG("%s:\t UFGPipeConnectionComponent is null", *prefix);
        return;
    }
    AL_LOG("%s:\t UFGPipeConnectionComponent is %s at %x", *prefix, *c->GetName(), c);

    AL_LOG("%s:\t\t mPipeNetworkID: %d", *prefix, c->mPipeNetworkID);
    AL_LOG("%s:\t\t mPipeConnectionType: %d", *prefix, c->mPipeConnectionType);
    AL_LOG("%s:\t\t mConnectorClearance: %d", *prefix, c->mConnectorClearance);

    AL_LOG("%s:\t\t IsConnected: %d", *prefix, c->IsConnected());
    if (c->mConnectedComponent)
    {
        AL_LOG("%s:\t\t mConnectedComponent: %s at %x", *prefix, *c->mConnectedComponent->GetName(), c->mConnectedComponent);
    }
    else
    {
        AL_LOG("%s:\t\t mConnectedComponent: null", *prefix);
    }

    AL_LOG("%s:\t\t HasFluidIntegrant: %d", *prefix, c->HasFluidIntegrant());
    AL_LOG("%s:\t\t mFluidIntegrant: %s at %x", *prefix, *GetFluidIntegrantName( c->mFluidIntegrant ), c->mFluidIntegrant);
}

void AutoLinkDebugging::DumpFluidIntegrant(FString prefix, IFGFluidIntegrantInterface* f)
{
    if (!f)
    {
        AL_LOG("%s: IFGFluidIntegrantInterface is null", *prefix);
        return;
    }

    if (auto actor = Cast<AActor>(f))
    {
        AL_LOG("%s:\t IFGFluidIntegrantInterface is %s (%s) at %x", *prefix, *actor->GetName(), *actor->GetClass()->GetName(), actor);
    }
    else
    {
        AL_LOG("%s:\t IFGFluidIntegrantInterface at %x", *prefix, f);
    }

    for (auto c : f->GetPipeConnections())
    {
        DumpConnection(prefix, c);
    }
}

void AutoLinkDebugging::DumpConnection(FString prefix, UFGPipeConnectionComponentHyper* c)
{
    if (!c)
    {
        AL_LOG("%s:\t UFGPipeConnectionComponentHyper is null", *prefix);
        return;
    }
    AL_LOG("%s:\t UFGPipeConnectionComponentHyper is %s at %x", *prefix, *c->GetName(), c);

    AL_LOG("%s:\t\t mDisallowSnappingTo: %d", *prefix, c->mDisallowSnappingTo);
    AL_LOG("%s:\t\t mPipeConnectionType: %d", *prefix, c->mPipeConnectionType);
    AL_LOG("%s:\t\t mConnectorClearance: %d", *prefix, c->mConnectorClearance);

    AL_LOG("%s:\t\t IsConnected: %d", *prefix, c->IsConnected());
    if (c->mConnectedComponent)
    {
        AL_LOG("%s:\t\t mConnectedComponent: %s at %x", *prefix, *c->mConnectedComponent->GetName(), c->mConnectedComponent);
    }
    else
    {
        AL_LOG("%s:\t\t mConnectedComponent: null", *prefix);
    }
}

void AutoLinkDebugging::DumpBuildEffectActor(FString prefix, const AFGBuildEffectActor* b)
{
    if (!b)
    {
        AL_LOG("%s:\t AFGBuildEffectActor is null", *prefix);
        return;
    }

    AL_LOG("%s:\t AFGBuildEffectActor is %s at %x", *prefix, *b->GetName(), b);

    AL_LOG("%s:\t mBounds: %s", *prefix, *b->mBounds.ToString());
    auto mBoundsSize = b->mBounds.GetSize();
    AL_LOG("%s:\t mBounds Dimensions: X: %f, Y: %f, Z: %f", *prefix, mBoundsSize.X, mBoundsSize.Y, mBoundsSize.Z);
    AL_LOG("%s:\t mActorBounds: %s", *prefix, *b->mActorBounds.ToString());
    auto mActorBoundsSize = b->mActorBounds.GetSize();
    AL_LOG("%s:\t mActorBounds Dimensions: X: %f, Y: %f, Z: %f", *prefix, mActorBoundsSize.X, mActorBoundsSize.Y, mActorBoundsSize.Z);
    AL_LOG("%s:\t mIsBlueprint: %d", *prefix, b->mIsBlueprint);
    AL_LOG("%s:\t NumActors: %d", *prefix, b->NumActors);
    AL_LOG("%s:\t mSourceActors: %d", *prefix, b->mSourceActors.Num());
    int i = 0;
    for (auto& pActor : b->mSourceActors)
    {
        AL_LOG("%s:\t\t mSourceActor[%d]: %s (%s)", *prefix, i++, *pActor->GetName(), *pActor->GetClass()->GetName());
    }
}

void AutoLinkDebugging::DumpPipeNetwork(FString prefix, const AFGPipeNetwork* p)
{
    if (!p)
    {
        AL_LOG("%s: AFGPipeNetwork is null", *prefix);
        return;
    }

    AL_LOG("%s:\t AFGPipeNetwork is %s with ID %d", *prefix, *p->GetName(), p->GetPipeNetworkID());
    AL_LOG("%s:\t NumFluidIntegrants %d", *prefix, p->NumFluidIntegrants());

    int i = 0;
    for (auto in : p->mFluidIntegrants)
    {
        AL_LOG("%s:\t\t mFluidIntegrants[%d]: %s", *prefix, i++, *GetFluidIntegrantName(in));
    }
}
