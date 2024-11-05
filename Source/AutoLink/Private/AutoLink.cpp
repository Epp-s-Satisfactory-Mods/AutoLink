#include "AutoLink.h"

#include "AbstractInstanceManager.h"
#include "FGBlueprintHologram.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildableConveyorBelt.h"
#include "FGBuildableConveyorLift.h"
#include "FGBuildableDecor.h"
#include "FGBuildableFactoryBuilding.h"
#include "FGBuildablePipeHyper.h"
#include "FGBuildablePipeline.h"
#include "FGBuildablePoleBase.h"
#include "FGBuildablePowerPole.h"
#include "FGBuildableRailroadTrack.h"
#include "FGBuildableSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeSubsystem.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "Hologram/FGBuildableHologram.h"
#include "InstanceData.h"
#include "Patching/NativeHookManager.h"

#if AL_DEBUGGING
#include "FGBuildGunBuild.h"
#endif

DEFINE_LOG_CATEGORY(LogAutoLink)

// The mod template does this but we have no text to localize
#define LOCTEXT_NAMESPACE "FAutoLinkModule"

#if AL_DEBUGGING
#define AL_LOG(Verbosity, Format, ...)\
    UE_LOG( LogAutoLink, Verbosity, Format, ##__VA_ARGS__ )
#else
#define AL_LOG(Verbosity, Format, ...)
#endif

#if AL_DEBUGGING
void FAutoLinkModule::DumpConnection(FString prefix, UFGFactoryConnectionComponent* c)
{
    if (!c)
    {
        AL_LOG(Verbose, TEXT("%s:\t Connection is null"), *prefix);
        return;
    }
    AL_LOG(Verbose, TEXT("%s:\t Connection is %s"), *prefix, *c->GetFName().GetPlainNameString());

    AL_LOG(Verbose, TEXT("%s:\t\t mConnector: %d"), *prefix, c->mConnector);
    AL_LOG(Verbose, TEXT("%s:\t\t mDirection: %d"), *prefix, c->mDirection);
    AL_LOG(Verbose, TEXT("%s:\t\t mConnectorClearance: %f"), *prefix, c->mConnectorClearance);
    if (c->mConnectedComponent)
    {
        AL_LOG(Verbose, TEXT("%s:\t\t mConnectedComponent: %s on %s"), *prefix, *c->mConnectedComponent->GetFName().GetPlainNameString(), *c->mConnectedComponent->GetOuterBuildable()->GetName());
    }
    else
    {
        AL_LOG(Verbose, TEXT("%s:\t\t mConnectedComponent: null"), *prefix);
    }
    AL_LOG(Verbose, TEXT("%s:\t\t mHasConnectedComponent: %d"), *prefix, c->mHasConnectedComponent);
    if (c->mConnectionInventory)
    {
        AL_LOG(Verbose, TEXT("%s:\t\t mConnectionInventory: %s"), *prefix, *c->mConnectionInventory->GetName());
    }
    else
    {
        AL_LOG(Verbose, TEXT("%s:\t\t mConnectionInventory: null"), *prefix);
    }
    AL_LOG(Verbose, TEXT("%s:\t\t mInventoryAccessIndex: %d"), *prefix, c->mInventoryAccessIndex);
    if (c->mOuterBuildable)
    {
        AL_LOG(Verbose, TEXT("%s:\t\t mOuterBuildable: %s"), *prefix, *c->mOuterBuildable->GetName());
    }
    else
    {
        AL_LOG(Verbose, TEXT("%s:\t\t mOuterBuildable: null"), *prefix);
    }
    AL_LOG(Verbose, TEXT("%s:\t\t mForwardPeekAndGrabToBuildable: %d"), *prefix, c->mForwardPeekAndGrabToBuildable);
}

void FAutoLinkModule::DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor)
{
    if (!conveyor)
    {
        AL_LOG(Verbose, TEXT("%s: Conveyor is null"), *prefix);
        return;
    }

    AL_LOG(Verbose, TEXT("%s: Conveyor is %s"), *prefix, *conveyor->GetName());
    auto ownerChainActor = conveyor->GetConveyorChainActor();
    if (ownerChainActor)
    {
        AL_LOG(Verbose, TEXT("%s: Chain actor: %s"), *prefix, *ownerChainActor->GetName());
    }
    else
    {
        AL_LOG(Verbose, TEXT("%s: Chain actor: null"), *prefix);
    }
    AL_LOG(Verbose, TEXT("%s: Chain segment index: %d"), *prefix, conveyor->mChainSegmentIndex);
    AL_LOG(Verbose, TEXT("%s: Chain flags: %d"), *prefix, conveyor->GetConveyorChainFlags());
    auto nextTickConveyor = conveyor->GetNextTickConveyor();
    if (nextTickConveyor)
    {
        AL_LOG(Verbose, TEXT("%s: Next tick conveyor: %s"), *prefix, *nextTickConveyor->GetName());
    }
    else
    {
        AL_LOG(Verbose, TEXT("%s: Next tick conveyor: null"), *prefix);
    }

    DumpConnection(prefix, conveyor->GetConnection0());
    DumpConnection(prefix, conveyor->GetConnection1());
}
#endif //AL_DEBUGGING

void FAutoLinkModule::StartupModule()
{
    if (WITH_EDITOR)
    {
        AL_LOG(Verbose, TEXT("StartupModule: Not hooking anything because WITH_EDITOR is true!"));
        return;
    }

    AL_LOG(Verbose, TEXT("StartupModule: Hooking Functions..."));

#if AL_DEBUGGING
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

            AL_LOG(Verbose, TEXT("UFGBuildGunState::OnRecipeSampled. Actor is %s (%s)."), *actor->GetName(), *actor->GetClass()->GetName());

            if (auto connectionOwner = Cast<AFGBuildableConveyorBase>(actor))
            {
                DumpConveyor(TEXT("UFGBuildGunState::OnRecipeSampled"), connectionOwner);
            }
            else
            {
                TInlineComponentArray<UFGFactoryConnectionComponent*> factoryConnections;
                actor->GetComponents(factoryConnections);
                for (auto connectionComponent : factoryConnections)
                {
                    DumpConnection(TEXT("UFGBuildGunState::OnRecipeSampled"), connectionComponent);
                }
            }

            scope(buildGunState, recipe);
        });
#endif

    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBlueprintHologram::Construct, GetMutableDefault<AFGBlueprintHologram>(),
        [](AActor* returnValue, AFGBlueprintHologram* hologram, TArray< AActor* >& out_children, FNetConstructionID NetConstructionID)
        {
            AL_LOG(Verbose, TEXT("AFGBlueprintHologram::Construct: The hologram is %s"), *hologram->GetName());

            for (auto child : out_children)
            {
                AL_LOG(Verbose, TEXT("AFGBlueprintHologram::Construct: Child %s (%s) at %s"),
                    *child->GetName(),
                    *child->GetClass()->GetName(),
                    *child->GetActorLocation().ToString());

                if (auto buildable = Cast<AFGBuildable>(child))
                {
                    FindAndLinkForBuildable(buildable);
                }
            }

            AL_LOG(Verbose, TEXT("AFGBlueprintHologram::Construct: Return value %s (%s) at %s"),
                *returnValue->GetName(),
                *returnValue->GetClass()->GetName(),
                *returnValue->GetActorLocation().ToString());
        });

    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableHologram::ConfigureComponents, GetMutableDefault<AFGBuildableHologram>(),
        [](const AFGBuildableHologram* hologram, AFGBuildable* buildable)
        {
            AL_LOG(Verbose, TEXT("AFGBuildableHologram::ConfigureComponents: The hologram is %s and buildable is %s at %s"), *hologram->GetName(), *buildable->GetName(), *buildable->GetActorLocation().ToString());
            FindAndLinkForBuildable(buildable);
        });
}

bool FAutoLinkModule::ShouldTryToAutoLink(AFGBuildable* buildable)
{
    // This function exists for performace, so we can short-circuit when objects will never have relevant connections.
    // This saves us from having to scan all the components on every object built. This doesn't matter much for individual
    // buildings but big blueprints can have a lot of objects that all get scanned in one frame. In testing, I built
    // some oversized blueprint rails with lots of foundations that definitely dropped the framerate significantly for
    // a beat and anything we can do to help alleviate the hit of big blueprints is gonna be a win in the cases that
    // users will notice.

    // This function was created by manually poking around the header files and trying to target the things that are
    // obviously not connectable and are built often or I suspect are likely to be used in blueprints. Note that we don't
    // link electricity since it's not something that can be auto-linked without core gameplay changes like building wires
    // for users, so when we it meets the above criteria and we are sure it doesn't have a conveyor, pipeline, hypertube,
    // or railroad connection, we will not attempt to auto-link.

    // Note that things like blueprint designers are intentionally omitted from these checks; even if they don't have
    // connections, they are very rarely constructed and so adding a check here that every buildable will have to do is
    // likely a poor tradeoff - we can eat the components scan a few times per game on designers to avoid an extra type
    // check in every single conveyor built, for example.

    // ...I wrote all that and then iterated until there are only a few things below, but if you have suggestions, let me
    // know on GitHub.

    // Base class for inert things like foundations, walls, etc
    if (buildable->IsA<AFGBuildableFactoryBuilding>())
    {
        return false;
    }

    // Conveyor and pipe supports
    if (buildable->IsA<AFGBuildablePoleBase>())
    {
        return false;
    }

    // All power poles
    if (buildable->IsA<AFGBuildablePowerPole>())
    {
        return false;
    }

    // If we don't know for certain, then we assume we have to scan it
    return true;
}

void FAutoLinkModule::FindAndLinkForBuildable(AFGBuildable* buildable)
{
    if (!ShouldTryToAutoLink(buildable))
    {
        AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Buildable %s of type %s is not linkable!"), *buildable->GetName(), *buildable->GetClass()->GetName());
        return;
    }

    AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Buildable is %s of type %s"), *buildable->GetName(), *buildable->GetClass()->GetName());

    // Belt connections
    {
        TInlineComponentArray<UFGFactoryConnectionComponent*> openConnections;
        FindOpenBeltConnections(openConnections, buildable);
        AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Found %d open belt connections"), openConnections.Num());
        for (auto connection : openConnections)
        {
            FindAndLinkCompatibleBeltConnection(connection);
        }
    }

    // Railroad connections
    {
        TInlineComponentArray<UFGRailroadTrackConnectionComponent*> openConnections;
        FindOpenRailroadConnections(openConnections, buildable);
        AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Found %d open railroad connections"), openConnections.Num());
        for (auto connection : openConnections)
        {
            FindAndLinkCompatibleRailroadConnection(connection);
        }
    }

    // Pipe connections
    {
        TInlineComponentArray<UFGPipeConnectionComponent*> openConnections;
        FindOpenFluidConnections(openConnections, buildable);
        AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Found %d open fluid connections"), openConnections.Num());
        TSet< IFGFluidIntegrantInterface* > integrantsToRegister;
        for (auto connection : openConnections)
        {
            if (FindAndLinkCompatibleFluidConnection(connection))
            {
                if (connection->HasFluidIntegrant())
                {
                    AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Connection %s (%s) has a fluid integrant"), *connection->GetName(), *connection->GetClass()->GetName());
                    integrantsToRegister.Add(connection->GetFluidIntegrant());
                }
                else
                {
                    AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Connection %s (%s) has NO fluid integrant"), *connection->GetName(), *connection->GetClass()->GetName());
                }
            }
        }

        if (integrantsToRegister.Num() > 0)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Found connections have a total of %d integrants to register"), integrantsToRegister.Num());
            auto pipeSubsystem = AFGPipeSubsystem::GetPipeSubsystem(buildable->GetWorld());
            for (auto integrant : integrantsToRegister)
            {
                pipeSubsystem->RegisterFluidIntegrant(integrant);
            }
        }
    }

    // Hypertube connections
    {
        TInlineComponentArray<UFGPipeConnectionComponentHyper*> openConnections;
        FindOpenHyperConnections(openConnections, buildable);
        AL_LOG(Verbose, TEXT("FindAndLinkForBuildable: Found %d open hyper connections"), openConnections.Num());
        for (auto connection : openConnections)
        {
            FindAndLinkCompatibleHyperConnection(connection);
        }
    }
}

void FAutoLinkModule::AddIfOpen(TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections, UFGFactoryConnectionComponent* connection)
{
    if (!connection)
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGFactoryConnectionComponent is null"));
        return;
    }

    if (connection->IsConnected())
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGFactoryConnectionComponent %s (%s) is already connected to %s"), *connection->GetName(), *connection->GetClass()->GetName(), *connection->GetConnection()->GetName());
        return;
    }

    openConnections.Add(connection);
}

void FAutoLinkModule::FindOpenBeltConnections(TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections, AFGBuildable* buildable)
{
    // Start with special cases where we know how to get the connections without a full scan
    if (auto conveyorBase = Cast<AFGBuildableConveyorBase>(buildable))
    {
        AL_LOG(Verbose, TEXT("FindOpenBeltConnections: AFGBuildableConveyorBase"));
        AddIfOpen(openConnections, conveyorBase->GetConnection0());
        AddIfOpen(openConnections, conveyorBase->GetConnection1());
    }
    else if (auto conveyorPole = Cast<AFGBuildablePoleBase>(buildable))
    {
        AL_LOG(Verbose, TEXT("FindOpenBeltConnections: AFGBuildablePoleBase so skipping"));
        // Don't try to link to/from conveyor poles. They're cosmetic and we don't need to do the extra scanning work.
        return;
    }
    // For whatever reason, AFGBuildableFactory::GetConnectionComponents can return null connections and none of the actual connections,
    // so it falls into the default searching case here
    else
    {
        AL_LOG(Verbose, TEXT("FindOpenBeltConnections: AFGBuildable is %s (%s)"), *buildable->GetName(), *buildable->GetClass()->GetName());
        TInlineComponentArray<UFGFactoryConnectionComponent*> factoryConnections;
        buildable->GetComponents(factoryConnections);
        for (auto connectionComponent : factoryConnections)
        {
            AL_LOG(Verbose, TEXT("FindOpenBeltConnections:\tFound UFGFactoryConnectionComponent"));
            AddIfOpen(openConnections, connectionComponent);
        }
    }
}

void FAutoLinkModule::AddIfOpen(
    TInlineComponentArray<UFGPipeConnectionComponent*>& openConnections,
    UFGPipeConnectionComponent* connection,
    IFGFluidIntegrantInterface* owningFluidIntegrant)
{
    if (!connection)
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGPipeConnectionComponent is null"));
        return;
    }

    if (connection->IsConnected())
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGPipeConnectionComponent %s (%s) is already connected to %s"), *connection->GetName(), *connection->GetClass()->GetName(), *connection->GetConnection()->GetName());
        return;
    }

    if (!connection->HasFluidIntegrant())
    {
        // This often isn't set when it it logically should be. Tests haven't shown any harm in setting it and
        // this passes it up to the caller so it can do fluid integrant management if something needs to be linked.
        connection->SetFluidIntegrant(owningFluidIntegrant);
    }

    openConnections.Add(connection);
}

void FAutoLinkModule::FindOpenFluidConnections(
    TInlineComponentArray<UFGPipeConnectionComponent*>& openConnections,
    AFGBuildable* buildable )
{
    // Start with special cases where we know to get the connections without a full scan
    if (auto pipeline = Cast<AFGBuildablePipeline>(buildable))
    {
        AL_LOG(Verbose, TEXT("FindOpenFluidConnections: AFGBuildablePipeline"));

        AddIfOpen(openConnections, pipeline->GetPipeConnection0(), pipeline);
        AddIfOpen(openConnections, pipeline->GetPipeConnection1(), pipeline);
    }
    else if (auto fluidIntegrant = Cast<IFGFluidIntegrantInterface>(buildable))
    {
        AL_LOG(Verbose, TEXT("FindOpenFluidConnections: IFGFluidIntegrantInterface"));
        for (auto connection : fluidIntegrant->GetPipeConnections())
        {
            AddIfOpen(openConnections, connection, fluidIntegrant);
        }
    }
    else
    {
        AL_LOG(Verbose, TEXT("FindOpenFluidConnections: AFGBuildable is %s (%s)"), *buildable->GetName(), *buildable->GetClass()->GetName());
        for (auto component : buildable->GetComponents())
        {
            if (auto integrant = Cast<IFGFluidIntegrantInterface>(component))
            {
                AL_LOG(Verbose, TEXT("FindOpenFluidConnections:\tHas an IFGFluidIntegrantInterface component %s of type %s"), *component->GetName(), *component->GetClass()->GetName());
                for (auto connection : integrant->GetPipeConnections())
                {
                    AddIfOpen(openConnections, connection, integrant);
                }
            }
        }
    }
}

void FAutoLinkModule::AddIfOpen(TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections, UFGPipeConnectionComponentHyper* connection)
{
    if (!connection)
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGPipeConnectionComponentHyper is null"));
        return;
    }

    if (connection->IsConnected())
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGPipeConnectionComponentHyper %s (%s) is already connected to %s"), *connection->GetName(), *connection->GetClass()->GetName(), *connection->GetConnection()->GetName());
        return;
    }

    openConnections.Add(connection);
}

void FAutoLinkModule::FindOpenHyperConnections(TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections, AFGBuildable* buildable)
{
    // Start with special cases where we know to get the connections without a full scan
    if (auto buildablePipe = Cast<AFGBuildablePipeHyper>(buildable))
    {
        AL_LOG(Verbose, TEXT("FindOpenHyperConnections: AFGBuildablePipeHyper"));
        AddIfOpen(openConnections, Cast<UFGPipeConnectionComponentHyper>(buildablePipe->GetConnection0()));
        AddIfOpen(openConnections, Cast<UFGPipeConnectionComponentHyper>(buildablePipe->GetConnection1()));
    }
    else
    {
        AL_LOG(Verbose, TEXT("FindOpenHyperConnections: AFGBuildable is %s (%s)"), *buildable->GetName(), *buildable->GetClass()->GetName());
        TInlineComponentArray<UFGPipeConnectionComponentHyper*> connections;
        buildable->GetComponents(connections);
        for (auto connectionComponent : connections)
        {
            AL_LOG(Verbose, TEXT("\tFindOpenHyperConnections: Found UFGPipeConnectionComponentHyper"));
            AddIfOpen(openConnections, connectionComponent);
        }
    }
}

void FAutoLinkModule::AddIfOpen(TInlineComponentArray<UFGRailroadTrackConnectionComponent*>& openConnections, UFGRailroadTrackConnectionComponent* connection)
{
    if (!connection)
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGRailroadTrackConnectionComponent is null"));
        return;
    }

    // Railroads can have multiple connections, so they're always "open"

    openConnections.Add(connection);
}

void FAutoLinkModule::FindOpenRailroadConnections(TInlineComponentArray<UFGRailroadTrackConnectionComponent*>& openConnections, AFGBuildable* buildable)
{
    // Start with special cases where we know to get the connections without a full scan
    if (auto railroad = Cast<AFGBuildableRailroadTrack>(buildable))
    {
        AL_LOG(Verbose, TEXT("FindOpenRailroadConnections: AFGBuildableRailroadTrack"));
        AddIfOpen(openConnections, railroad->GetConnection(0));
        AddIfOpen(openConnections, railroad->GetConnection(1));
    }
    else
    {
        AL_LOG(Verbose, TEXT("FindOpenRailroadConnections: AFGBuildable is %s (%s)"), *buildable->GetName(), *buildable->GetClass()->GetName());
        TInlineComponentArray<UFGRailroadTrackConnectionComponent*> connections;
        buildable->GetComponents(connections);
        for (auto connectionComponent : connections)
        {
            AL_LOG(Verbose, TEXT("\tFindOpenRailroadConnections: Found UFGRailroadTrackConnectionComponent"));
            AddIfOpen(openConnections, connectionComponent);
        }
    }
}

void FAutoLinkModule::HitScan(
    TArray<AActor*>& actors,
    UWorld* world,
    FVector scanStart,
    FVector scanEnd,
    AActor* ignoreActor )
{
    TArray< FHitResult > hitResults;
    world->LineTraceMultiByObjectType(
        hitResults,
        scanStart,
        scanEnd,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects));

    for (const FHitResult& result : hitResults)
    {
        auto actor = result.GetActor();
        if (actor && actor->IsA(AAbstractInstanceManager::StaticClass()))
        {
            if (auto manager = AAbstractInstanceManager::GetInstanceManager(actor))
            {
                FInstanceHandle handle;
                if (manager->ResolveHit(result, handle))
                {
                    actor = handle.GetOwner();
                }
            }
        }

        if (actor == ignoreActor)
        {
            AL_LOG(Verbose, TEXT("HitScan: Per ignoreActor, IGNORING hit actor %s of type %s at %s"), *actor->GetName(), *actor->GetClass()->GetName(), *result.Location.ToString());
            continue;
        }

        AL_LOG(Verbose, TEXT("HitScan: Hit actor %s of type %s at %s"), *actor->GetName(), *actor->GetClass()->GetName(), *result.Location.ToString());

        actors.Add(actor);
    }
}

void FAutoLinkModule::FindAndLinkCompatibleBeltConnection(UFGFactoryConnectionComponent* connectionComponent)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Exiting because the connection component is already connected"));
        return;
    }

    auto connectionDirection = connectionComponent->GetDirection();
    switch(connectionDirection)
    {
    case EFactoryConnectionDirection::FCD_INPUT:
    case EFactoryConnectionDirection::FCD_OUTPUT:
        break;
    default:
        // EFactoryConnectionDirection::FCD_SNAP_ONLY is apparently used for conveyor poles and we don't need to link those since they're cosmetic.
        // EFactoryConnectionDirection::FCD_ANY or others, I don't know where they'd be used and I suspect they're unnecessary. Can come back and
        // handle them later if that's wrong.
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Exiting because the connection direction is: %d"), connectionDirection);
        return;
    }

    // Belts need to be right up against the connectors, but conveyor lifts have some other cases that we will address
    float searchDistance;
    auto outerBuildable = connectionComponent->GetOuterBuildable();
    auto connectionConveyorBelt = Cast<AFGBuildableConveyorBelt>(outerBuildable);
    AFGBuildableConveyorLift* connectionConveyorLift = nullptr;
    if (connectionConveyorBelt)
    {
        // If this is a belt then it in needs to be against the connector, but search
        // a little bit outward to be sure we hit any buildable containing a connector
        searchDistance = 10.0;
    }
    else if ((connectionConveyorLift = Cast<AFGBuildableConveyorLift>(outerBuildable)) != nullptr)
    {
        // If this is a conveyor lift, then there could be another conveyor lift facing it
        // A single conveyor lift can have its bellows extended to 300 units away, so we have
        // to search twice that distance plus a bit to ensure we hit the buildable
        searchDistance = 610.0;
    }
    else
    {
        // If this is a normal factory/buildable, it could still be aligned with a fully-extended
        // conveyor lift and we still pad a bit to ensure an appropriate hit
        searchDistance = 310.0;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    auto searchEnd = connectorLocation + (connectionComponent->GetConnectorNormal() * searchDistance);

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Connector at: %s, searchEnd is at: %s, searchDistance is %f, direction is: %d"),
        *connectorLocation.ToString(),
        *searchEnd.ToString(),
        searchDistance,
        connectionDirection);

    TArray< AActor* > hitActors;
    HitScan(
        hitActors,
        connectionComponent->GetWorld(),
        connectorLocation,
        searchEnd,
        outerBuildable);

    // 12 is a random guess of the max possible candidates we could ever really see
    TArray<UFGFactoryConnectionComponent*, TInlineAllocator<12>> candidates;
    for (auto actor : hitActors)
    {
        if (auto hitConveyor = Cast<AFGBuildableConveyorBase>(actor))
        {
            // Special case for when we can get at the connections without scanning all the components
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Examining conveyor %s of type %s"), *hitConveyor->GetName(), *hitConveyor->GetClass()->GetName());
            auto candidateConnection = connectionDirection == EFactoryConnectionDirection::FCD_INPUT
                ? hitConveyor->GetConnection1()
                : hitConveyor->GetConnection0();

            candidates.Add(candidateConnection);
        }
        if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Examining buildable %s of type %s"), *buildable->GetName(), *buildable->GetClass()->GetName());
            TInlineComponentArray<UFGFactoryConnectionComponent*> openConnections;
            FindOpenBeltConnections(openConnections, buildable);

            for (auto openConnection : openConnections)
            {
                candidates.Add(openConnection);
            }
        }
        else
        {
            // This shouldn't really happen but if it does, I'd like a message in the log while testing
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Ignoring hit result actor %s of type %s"), *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    float closestDistanceSq = FLT_MAX;
    UFGFactoryConnectionComponent* compatibleConnectionComponent = nullptr;
    for (auto& candidateConnection : candidates)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Examining connection: %s on %s."),
            *candidateConnection->GetName(),
            *candidateConnection->GetOuterBuildable()->GetName());

        if (!IsValid(candidateConnection))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tNot valid!"));
            continue;
        }

        if (candidateConnection == compatibleConnectionComponent)
        {
            // Sometimes the resolving the hit scan yields multiple hits on the same component; we can short-circuit that here.
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tRedundant! This is already the best candidate."));
            continue;
        }

        if (candidateConnection->IsConnected())
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tAlready connected!"));
            continue;
        }

        if (!candidateConnection->CanConnectTo(connectionComponent))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tCannot be connected to this!"));
            continue;
        }

        // Now that the quickest checks are done, check distances
        auto minConnectorDistance = 0.0f;
        auto maxConnectorDistance = 0.0f;

        auto candidateOuterBuildable = candidateConnection->GetOuterBuildable();
        if (auto candidateConveyorBelt = Cast<AFGBuildableConveyorBelt>(candidateOuterBuildable))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tCandidate connection is on conveyor belt %s"), *candidateConveyorBelt->GetName());
            // If the candidate is a belt, leave the distances at 0, since the belt has to be up against the connector.
        }
        else if (auto candidateConveyorLift = Cast<AFGBuildableConveyorLift>(candidateOuterBuildable))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tCandidate connection is on conveyor lift %s"), *candidateConveyorLift->GetName());
            if (connectionConveyorLift)
            {
                // Scanning from a lift and hit a lift, so max distance will depend on the bellows of both lifts
                minConnectorDistance = 100.0; // Prevent conveyor lifts from smashing into and protruding past each other
                // At the very least, they can be their combined connector clearances away. May be further based on bellows
                maxConnectorDistance = connectionComponent->GetConnectorClearance() + candidateConnection->GetConnectorClearance();

                switch (connectionDirection)
                {
                    // If the conveyor bellows extended to snap on construction, add the clearances through which they snapped
                case EFactoryConnectionDirection::FCD_INPUT:
                    maxConnectorDistance += connectionConveyorLift->mOpposingConnectionClearance[0];
                    maxConnectorDistance += candidateConveyorLift->mOpposingConnectionClearance[1];
                    break;
                case EFactoryConnectionDirection::FCD_OUTPUT:
                    maxConnectorDistance += connectionConveyorLift->mOpposingConnectionClearance[1];
                    maxConnectorDistance += candidateConveyorLift->mOpposingConnectionClearance[0];
                    break;
                }
            }
            else
            {
                // Scanning from a buildable and hit a lift, so max distance will depend on the bellows of the hit lift
                minConnectorDistance = 100.0; // Prevent the conveyor lift from smashing into the buildable too extremely much...
                maxConnectorDistance = candidateConnection->GetConnectorClearance();
                switch (connectionDirection)
                {
                    // If the conveyor bellows extended to snap on construction, add the clearances through which they snapped
                case EFactoryConnectionDirection::FCD_INPUT:
                    maxConnectorDistance += candidateConveyorLift->mOpposingConnectionClearance[1];
                    break;
                case EFactoryConnectionDirection::FCD_OUTPUT:
                    maxConnectorDistance += candidateConveyorLift->mOpposingConnectionClearance[0];
                    break;
                }
            }
        }
        else if (connectionConveyorLift)
        {
            // Scanning from a lift and hit a buildable, so max distance will depend on the bellows of the lift
            minConnectorDistance = 100.0; // Prevent the conveyor lift from smashing into the buildable too extremely much...
            maxConnectorDistance = connectionComponent->GetConnectorClearance();
            switch (connectionDirection)
            {
                // If the conveyor bellows extended to snap on construction, add the clearances through which they snapped
            case EFactoryConnectionDirection::FCD_INPUT:
                maxConnectorDistance += connectionConveyorLift->mOpposingConnectionClearance[0];
                break;
            case EFactoryConnectionDirection::FCD_OUTPUT:
                maxConnectorDistance += connectionConveyorLift->mOpposingConnectionClearance[1];
                break;
            }
        }

        const FVector candidateLocation = candidateConnection->GetConnectorLocation();
        const FVector fromCandidateToConnectorVector = connectorLocation - candidateLocation; // This gives the vector from the candidate connection to the main connector
        const float distanceSq = fromCandidateToConnectorVector.SquaredLength();
        const float minDistanceSq = minConnectorDistance == 0.0f ? 0.0f : ((minConnectorDistance * minConnectorDistance) - 1); // Give a little padding for floating points
        const float maxDistanceSq = (maxConnectorDistance * maxConnectorDistance) + 1; // A little padding for floating points
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tMin Distance: %f, Max Distance: %f, Distance: %f!"), minConnectorDistance, maxConnectorDistance, FMath::Sqrt( distanceSq ) );
        if (distanceSq < minDistanceSq || distanceSq > maxDistanceSq)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tOther connection is too close or too far. Min Distance SQ: %f, Max Distance SQ: %f, Distance SQ: %f!"), minDistanceSq, maxDistanceSq, distanceSq);
            continue;
        }

        const FVector connectorNormal = connectionComponent->GetConnectorNormal();
        const FVector candidateConnectorNormal = candidateConnection->GetConnectorNormal();

        // Determine if the connection components are aligned. If the cross product is 0, they are on the same line
        const FVector crossProduct = FVector::CrossProduct(connectorNormal, candidateConnectorNormal);
        auto isCollinear = crossProduct.IsNearlyZero(.01);
        if (!isCollinear)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tOther connection normal is not collinear with this connector normal! The parts are not aligned!"), );
            continue;
        }

        if (distanceSq < 1)
        {
            // If we're within 1 cm and they're collinear, this is a belt-to-something connection that's the ideal distance.
            // Now we just need to make sure they're facing each other so we know they're not overlapping in the same direction.

            // If a dot product is positive, then the vectors are less than 90 degrees apart.
            // So this dot product should be negative, meaning the connector normal is pointing AGAINST the candidate normal vector
            const double connectorDotProduct = connectorNormal.Dot(candidateConnectorNormal);
            if (connectorDotProduct >= 0)
            {
                AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tThe connectors are not facing in opposite directions! connectorDotProduct: %.8f"), connectorDotProduct);
                continue;
            }

            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tFound one that's less than 1 cm away; take it as the best result. Location: %s"), *candidateLocation.ToString());
            closestDistanceSq = distanceSq;
            compatibleConnectionComponent = candidateConnection;
            break;
        }

        // We're further than 1 cm but within an allowed distance (i.e. this is two conveyor lifts that are lined up nicely).
        // Determine if both connectors are in front of each other (meaning they're facing in opposite directions). If we don't do
        // this check, the connectors might be facing away or overlapping but past each other; this latter case is why we check the
        // normal vectors against the vector linking the two connectors.  If we just did their normal vectors, they could be on the
        // wrong sides of each other and still give a "facing in opposite directions" result.
        // 
        // If a dot product is positive, then the vectors are less than 90 degrees apart.
        // So this dot product should be negative, meaning the connector normal is pointing AGAINST the vector from the candidate to the connector
        const double connectorNormalDotProduct = connectorNormal.Dot(fromCandidateToConnectorVector);
        // And this dot product should be positive, meaning the candidate connector normal is pointing WITH the vector from the candidate to the connector
        const double candidateNormalDotProduct = candidateConnectorNormal.Dot(fromCandidateToConnectorVector);
        if (connectorNormalDotProduct > 0 || candidateNormalDotProduct < 0)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tThe connectors are not each facing each other! connectorDotVec: %f, candidateDotVec: %f"), connectorNormalDotProduct, candidateNormalDotProduct);
            continue;
        }

        if (distanceSq < closestDistanceSq)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tFound a new closest one at: %s"), *candidateLocation.ToString());
            closestDistanceSq = distanceSq;
            compatibleConnectionComponent = candidateConnection;
        }
    }

    if (!compatibleConnectionComponent)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: No compatible connection found"));
        return;
    }

    auto connectionConveyor = Cast<AFGBuildableConveyorBase>(outerBuildable);
    auto otherConnectionConveyor = Cast<AFGBuildableConveyorBase>(compatibleConnectionComponent->GetOuterBuildable());
    if (connectionConveyor && otherConnectionConveyor)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Removing, setting connection, and then re-adding conveyor so it will build the correct chain actor"));
        auto buildableSybsystem = AFGBuildableSubsystem::Get(connectionComponent->GetWorld());
        buildableSybsystem->RemoveConveyor(connectionConveyor);
        connectionComponent->SetConnection(compatibleConnectionComponent);
        buildableSybsystem->AddConveyor(connectionConveyor);
    }
    else
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Connecting the components!"));
        connectionComponent->SetConnection(compatibleConnectionComponent);
    }
}

void FAutoLinkModule::FindAndLinkCompatibleRailroadConnection(UFGRailroadTrackConnectionComponent* connectionComponent)
{
    auto connectorLocation = connectionComponent->GetConnectorLocation();
    // Railroad connectors seem to be lower than the railroad hitboxes, so we need to adjust our search start up to ensure we actually hit adjacent railroads
    auto searchStart = connectorLocation + (FVector::UpVector * 10);
    // Search a small extra distance from the connector. Though we will limit connections to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchRadius = 10.0f;

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Connector at: %s, searchStart is %s, searchRadius is %f"),
        *connectorLocation.ToString(),
        *searchStart.ToString(),
        searchRadius);

    // Curved rails don't always seem to be hit by the normal linear hitscan out of the connector normal
    // so we do a radius search here to be sure we're getting good candidates.
    auto connectionOwner = connectionComponent->GetOwner();
    TArray< FOverlapResult > overlapResults;
    connectionComponent->GetWorld()->OverlapMultiByObjectType(
        overlapResults,
        searchStart,
        FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects),
        FCollisionShape::MakeSphere(searchRadius));

    TArray< AActor* > hitActors;
    for (const FOverlapResult& result : overlapResults)
    {
        auto actor = result.GetActor();
        if (actor && actor->IsA(AAbstractInstanceManager::StaticClass()))
        {
            if (auto manager = AAbstractInstanceManager::GetInstanceManager(actor))
            {
                FInstanceHandle handle;
                if (manager->ResolveOverlap(result, handle))
                {
                    actor = handle.GetOwner();
                }
            }
        }

        if (actor == connectionOwner)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: IGNORING connection owner %s of type %s"), *actor->GetName(), *actor->GetClass()->GetName());
            continue;
        }

        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Hit actor %s of type %s"), *actor->GetName(), *actor->GetClass()->GetName());

        hitActors.Add(actor);
    }

    TArray< UFGRailroadTrackConnectionComponent* > candidates;
    for (auto actor : hitActors)
    {
        if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Examining hit result actor %s of type %s"), *buildable->GetName(), *buildable->GetClass()->GetName());
            TInlineComponentArray<UFGRailroadTrackConnectionComponent*> openConnections;
            FindOpenRailroadConnections(openConnections, buildable);

            for (auto openConnection : openConnections)
            {
                candidates.Add(openConnection);
            }
        }
        else
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Ignoring hit result actor %s of type %s"), *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    for (auto candidateConnection : candidates)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Examining connection: %s on %s at %s (%f units away)"),
            *candidateConnection->GetName(),
            *candidateConnection->GetOwner()->GetName(),
            *candidateConnection->GetConnectorLocation().ToString(),
            FVector::Distance(connectorLocation, candidateConnection->GetConnectorLocation()));

        if (!IsValid(candidateConnection))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tNot valid!"));
            continue;
        }

        auto alreadyConnectedToThis = false;
        for (auto otherSubConnection : candidateConnection->GetConnections())
        {
            if (connectionComponent == otherSubConnection)
            {
                alreadyConnectedToThis = true;
                break;
            }
        }

        if (alreadyConnectedToThis)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tAlready connected to this!"));
            continue;
        }

        const FVector candidateLocation = candidateConnection->GetConnectorLocation();
        const FVector fromCandidateToConnectorVector = connectorLocation - candidateLocation; // This gives the vector from the candidate connector to the main connector
        const float distanceSq = fromCandidateToConnectorVector.SquaredLength();
        if (distanceSq > 1) // Anything more than a 1 cm away is too far (and most are even much closer, based on my tests)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tCandidate connection is too far. Distance SQ: %f!"), distanceSq);
            continue;
        }

        const FVector connectorNormal = connectionComponent->GetConnectorNormal();
        const FVector candidateConnectorNormal = candidateConnection->GetConnectorNormal();

        // Determine if the connections components are aligned
        const FVector crossProduct = FVector::CrossProduct(connectorNormal, candidateConnectorNormal);
        // We allow slightly unaligned normal vetors to account for curved rails.
        auto isCollinear = FMath::IsNearlyZero(crossProduct.X, .01) && FMath::IsNearlyZero(crossProduct.Y, .01) && FMath::IsNearlyZero(crossProduct.Z, 1.0);
        if (!isCollinear)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tCandidate connection normal is not collinear with this connector normal! The parts are not aligned! Cross product is %s"), *crossProduct.ToString());
            continue;
        }

        // Determine if both connectors are facing in opposite directions. If we don't do this check, the connectors might be facing in the same direction,
        // particularly with rail junctions that overlap a lot.
        // If a dot product is positive, then the vectors are less than 90 degrees apart.
        // So this dot product should be negative, meaning the connector normal is pointing AGAINST the candidate normal vector
        const double connectorDotProduct = connectorNormal.Dot(candidateConnectorNormal);
        if (connectorDotProduct >= 0)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tThe connectors are not facing in opposite directions! connectorDotProduct: %.8f"), connectorDotProduct);
            continue;
        }

        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tFound one that's extremely close; so we're linking it. Location: %s"), *candidateLocation.ToString());
        connectionComponent->AddConnection(candidateConnection);
        // We don't break here - keep linking because railroads can be connected to multiple other railoads at junctions.
    }

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: No compatible connection found"));
}

bool FAutoLinkModule::FindAndLinkCompatibleFluidConnection(UFGPipeConnectionComponent* connectionComponent)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleFluidConnection: Exiting because the connection component is already connected"));
        return false;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    // Search a small extra distance straight out from the connector. Though we will limit pipes to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchEnd = connectorLocation + (connectionComponent->GetConnectorNormal() * 10);

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleFluidConnection: Connector at: %s, searchEnd is at: %s"), *connectorLocation.ToString(), *searchEnd.ToString());

    TArray< AActor* > hitActors;
    HitScan(
        hitActors,
        connectionComponent->GetWorld(),
        connectorLocation,
        searchEnd,
        connectionComponent->GetOwner());

    TArray< UFGPipeConnectionComponentBase* > candidates;
    for (auto actor : hitActors)
    {
        if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleFluidConnection: Examining buildable %s of type %s"), *buildable->GetName(), *buildable->GetClass()->GetName());
            TInlineComponentArray<UFGPipeConnectionComponent*> openConnections;
            FindOpenFluidConnections(openConnections, buildable);

            for (auto openConnection : openConnections)
            {
                candidates.Add(openConnection);
            }
        }
        else
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleFluidConnection: Ignoring hit result actor %s of type %s"), *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    return ConnectBestPipeCandidate(connectionComponent, candidates);
}

bool FAutoLinkModule::FindAndLinkCompatibleHyperConnection(UFGPipeConnectionComponentHyper* connectionComponent)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleHyperConnection: Exiting because the connection component is already connected"));
        return false;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    // Search a small extra distance straight out from the connector. Though we will limit pipes to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchEnd = connectorLocation + (connectionComponent->GetConnectorNormal() * 10);

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleHyperConnection: Connector at: %s, searchEnd is at: %s"), *connectorLocation.ToString(), *searchEnd.ToString());

    TArray< AActor* > hitActors;
    HitScan(
        hitActors,
        connectionComponent->GetWorld(),
        connectorLocation,
        searchEnd,
        connectionComponent->GetOwner());

    TArray< UFGPipeConnectionComponentBase* > candidates;
    for (auto actor : hitActors)
    {
        if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleHyperConnection: Examining buildable %s of type %s"), *buildable->GetName(), *buildable->GetClass()->GetName());
            TInlineComponentArray<UFGPipeConnectionComponentHyper*> openConnections;
            FindOpenHyperConnections(openConnections, buildable);

            for (auto openConnection : openConnections)
            {
                candidates.Add(openConnection);
            }
        }
        else
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleHyperConnection: Ignoring hit result actor %s of type %s"), *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    return ConnectBestPipeCandidate(connectionComponent, candidates);
}

bool FAutoLinkModule::ConnectBestPipeCandidate(UFGPipeConnectionComponentBase* connectionComponent, TArray<UFGPipeConnectionComponentBase*>& candidates)
{
    auto connectorLocation = connectionComponent->GetConnectorLocation();
    for (auto otherConnection : candidates)
    {
        AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate: Examining connection: %s (%s) at %s (%f units away). Connection type %d"),
            *otherConnection->GetName(),
            *otherConnection->GetClass()->GetName(),
            *otherConnection->GetConnectorLocation().ToString(),
            FVector::Distance(connectorLocation, otherConnection->GetConnectorLocation()),
            otherConnection->GetPipeConnectionType());

        if (!IsValid(otherConnection))
        {
            AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate:\tNot valid!"));
            continue;
        }

        if (otherConnection->IsConnected())
        {
            AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate:\tAlready connected!"));
            continue;
        }

        if (!otherConnection->CanConnectTo(connectionComponent))
        {
            AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate:\tCannot be connected to this!"));
            continue;
        }

        const FVector connectorNormal = connectionComponent->GetConnectorNormal();

        // Determine if the connections components are aligned
        const FVector crossProduct = FVector::CrossProduct(connectorNormal, otherConnection->GetConnectorNormal());
        auto isCollinear = crossProduct.IsNearlyZero(.01);
        if (!isCollinear)
        {
            AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate:\tOther connection normal is not collinear with this connector normal! The parts are not aligned!"), );
            continue;
        }

        // For belt connectors, we do dot product things to ensure they are facing each other because conveyor lifts can be further away,
        // meaning they could also theoretically be on the wrong side (e.g. clipping through the factory). Pipes have to be virtually touching
        // for us to link them so we just make double sure of that here.
        const FVector otherLocation = otherConnection->GetConnectorLocation();
        const float distanceSq = FVector::DistSquared(otherLocation, connectorLocation);

        if (distanceSq > 1) // Anything more than a 1 cm away is too far (and most are even much closer, based on my tests)
        {
            AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate:\tConnection is too far away to be auto-linked!"), );
            continue;
        }

        AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate:\tFound one that's extremely close; taking it as the best result. Location: %s"), *otherLocation.ToString());
        otherConnection->SetConnection(connectionComponent);
        return true;
    }

    AL_LOG(Verbose, TEXT("ConnectBestPipeCandidate: No compatible connection found"));
    return false;
}

#undef AL_LOG
#undef LOCTEXT_NAMESPACE

IMPLEMENT_GAME_MODULE(FAutoLinkModule, AutoLink)