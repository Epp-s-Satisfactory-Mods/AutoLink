
#include "AutoLinkDebugging.h"
#include "AutoLinkLogMacros.h"

#include "AbstractInstanceManager.h"
#include "FGBuildGunBuild.h"
#include "Patching/NativeHookManager.h"
// When we're building to ship, set this to 0 to no-op logging and debugging functions to minimize performance impact. Would prefer to do
// this through build defines based on whether we're building for development or shipping but at the moment alpakit always builds shipping.

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
    if (c->mFluidIntegrant)
    {
        if (auto actor = Cast<AActor>(c->mFluidIntegrant))
        {
            AL_LOG("%s:\t\t mFluidIntegrant: %s (%s) at %x", *prefix, *actor->GetName(), *actor->GetClass()->GetName(), actor);
        }
        else
        {
            AL_LOG("%s:\t\t mFluidIntegrant: %x", *prefix, c->mFluidIntegrant);
        }

        int i = 0;
        for (auto con : c->mFluidIntegrant->GetPipeConnections())
        {
            if (con)
            {
                AL_LOG("%s:\t\t\t mFluidIntegrant connection %d: %s at %x", *prefix, i, *con->GetName(), con);
            }
            else
            {
                AL_LOG("%s:\t\t\t mFluidIntegrant connetion %d: null", *prefix, i);
            }
            ++i;
        }
    }
    else
    {
        AL_LOG("%s:\t\t mFluidIntegrant: null", *prefix);
    }
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
