#include "AutoLinkRootInstanceModule.h"

#include "AutoLinkDebugging.h"
#include "AutoLinkDebugSettings.h"
#include "AutoLinkLinuxHooking.h"
#include "AutoLinkLogCategory.h"
#include "AutoLinkLogMacros.h"
#include "AutoLinkWindowsHooking.h"

#include "AbstractInstanceManager.h"
#include "BlueprintHookManager.h"
#include "FGBlueprintHologram.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildableConveyorBelt.h"
#include "FGBuildableConveyorLift.h"
#include "FGBuildableDecor.h"
#include "FGBuildableFactoryBuilding.h"
#include "FGBuildablePipeHyper.h"
#include "FGBuildablePipeline.h"
#include "FGBuildablePipelineAttachment.h"
#include "FGBuildablePipelineJunction.h"
#include "FGBuildablePoleBase.h"
#include "FGBuildablePowerPole.h"
#include "FGBuildableRailroadTrack.h"
#include "FGBuildableStorage.h"
#include "FGBuildableSubsystem.h"
#include "FGCentralStorageContainer.h"
#include "FGConveyorAttachmentHologram.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeHyperAttachmentHologram.h"
#include "FGPipelineAttachmentHologram.h"
#include "FGPipeSubsystem.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "Hologram/FGBuildableHologram.h"
#include "InstanceData.h"
#include "Patching/NativeHookManager.h"

UAutoLinkRootInstanceModule::UAutoLinkRootInstanceModule()
{
}

UAutoLinkRootInstanceModule::~UAutoLinkRootInstanceModule()
{
}

void UAutoLinkRootInstanceModule::DispatchLifecycleEvent(ELifecyclePhase phase)
{
    AL_LOG("UAutoLinkRootInstanceModule::DispatchLifecycleEvent: Phase %d", phase);

    if (phase != ELifecyclePhase::INITIALIZATION)
    {
        Super::DispatchLifecycleEvent(phase);
        return;
    }

    if (WITH_EDITOR)
    {
        AL_LOG("StartupModule: Not hooking anything because WITH_EDITOR is true!");
        return;
    }

    if (AL_DEBUG_ENABLED)
    {
        AutoLinkDebugging::RegisterDebugHooks();

        if(!AL_DEBUG_ENABLE_MOD)
        {
            Super::DispatchLifecycleEvent(phase);
            return;
        }
    }

    AL_LOG("UAutoLinkRootInstanceModule: Hooking Mod Functions...");

    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBlueprintHologram::Construct, GetMutableDefault<AFGBlueprintHologram>(),
        [](AActor* returnValue, AFGBlueprintHologram* hologram, TArray< AActor* >& out_children, FNetConstructionID NetConstructionID)
        {
            AL_LOG("AFGBlueprintHologram::Construct AFTER: The hologram is %s", *hologram->GetName());

            for (auto child : out_children)
            {
                AL_LOG("AFGBlueprintHologram::Construct AFTER: Child %s (%s) at %s",
                    *child->GetName(),
                    *child->GetClass()->GetName(),
                    *child->GetActorLocation().ToString());

                if (auto buildable = Cast<AFGBuildable>(child))
                {
                    FindAndLinkForBuildable(buildable);
                }
            }

            AL_LOG("AFGBlueprintHologram::Construct AFTER: Return value %s (%s) at %s",
                *returnValue->GetName(),
                *returnValue->GetClass()->GetName(),
                *returnValue->GetActorLocation().ToString());
        });

#define SHORT_CIRCUIT_TYPE( T )\
    if (hologram->IsA(T::StaticClass())){ return;}

    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableHologram::ConfigureComponents, GetMutableDefault<AFGBuildableHologram>(),
        [](const AFGBuildableHologram* hologram, AFGBuildable* buildable)
        {
            // Short-circuit in this (the parent) ConfigureComponents for cases that are handled in child classes
            SHORT_CIRCUIT_TYPE(AFGConveyorAttachmentHologram);
            SHORT_CIRCUIT_TYPE(AFGPipeHyperAttachmentHologram);
            SHORT_CIRCUIT_TYPE(AFGPipelineAttachmentHologram);
            AL_LOG("AFGBuildableHologram::ConfigureComponents: The hologram is %s and buildable is %s at %s", *hologram->GetName(), *buildable->GetName(), *buildable->GetActorLocation().ToString());

            FindAndLinkForBuildable(buildable);
        });
#undef SHORT_CIRCUIT_TYPE

#define SUBSCRIBE_CONFIGURE_COMPONENTS_BASE( T, PRE_CALL )\
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(\
        T::ConfigureComponents,\
        GetMutableDefault<T>(),\
        [](const T* hologram, AFGBuildable* buildable)\
        {\
            AL_LOG(#T "::ConfigureComponents: The hologram is %s and buildable is %s at %s", *hologram->GetName(), *buildable->GetName(), *buildable->GetActorLocation().ToString());\
            PRE_CALL;\
            FindAndLinkForBuildable(buildable);\
        });

#define SUBSCRIBE_CONFIGURE_COMPONENTS( T ) SUBSCRIBE_CONFIGURE_COMPONENTS_BASE( T, {} )

    // Note that we don't subscribe to the overridden versions on:
    //  - AFGConveyorBeltHologram
    //  - AFGConveyorLiftHologram
    //  - AFGPipelineHologram
    //  - AFGRailroadTrackHologram
    //  - AFGRoadHologram
    //  - AFGTrainPlatformHologram
    // Because they either don't have auto-linkable connections or handle their own snapping/connecting just fine
    SUBSCRIBE_CONFIGURE_COMPONENTS(AFGConveyorAttachmentHologram);
    SUBSCRIBE_CONFIGURE_COMPONENTS(AFGPipeHyperAttachmentHologram);
    SUBSCRIBE_CONFIGURE_COMPONENTS_BASE(AFGPipelineAttachmentHologram,
        {
            if (auto junction = Cast<AFGBuildablePipelineJunction>(buildable))
            {
                if (junction->GetPipeConnections().Num() == 0)
                {
                    AL_LOG("AFGPipelineAttachmentHologram::ReceiveConfigureComponents: AFGBuildablePipelineJunction has no pipe connections yet. Adding them with GetComponents.");
                    junction->GetComponents<UFGPipeConnectionComponent>(junction->mPipeConnections);
                }
            }
        });

#undef SUBSCRIBE_CONFIGURE_COMPONENTS

#if PLATFORM_WINDOWS
    AutoLinkWindowsHooking::RegisterWindowsOnlyHooks();
#elif PLATFORM_LINUX
    AutoLinkLinuxHooking::RegisterLinuxOnlyHooks();
#endif

    Super::DispatchLifecycleEvent(phase);
}

bool UAutoLinkRootInstanceModule::ShouldTryToAutoLink(AFGBuildable* buildable)
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

void UAutoLinkRootInstanceModule::FindAndLinkForBuildable(AFGBuildable* buildable)
{
    if (!ShouldTryToAutoLink(buildable))
    {
        AL_LOG("FindAndLinkForBuildable: Buildable %s of type %s is not linkable!", *buildable->GetName(), *buildable->GetClass()->GetName());
        return;
    }

    AL_LOG("FindAndLinkForBuildable: Buildable is %s of type %s", *buildable->GetName(), *buildable->GetClass()->GetName());

    // Belt connections
    {
        TInlineComponentArray<UFGFactoryConnectionComponent*> openConnections;
        FindOpenBeltConnections(openConnections, buildable);
        AL_LOG("FindAndLinkForBuildable: Found %d open belt connections", openConnections.Num());
        for (auto connection : openConnections)
        {
            FindAndLinkCompatibleBeltConnection(connection);
        }
    }

    // Railroad connections
    {
        TInlineComponentArray<UFGRailroadTrackConnectionComponent*> openConnections;
        FindOpenRailroadConnections(openConnections, buildable);
        AL_LOG("FindAndLinkForBuildable: Found %d open railroad connections", openConnections.Num());
        for (auto connection : openConnections)
        {
            FindAndLinkCompatibleRailroadConnection(connection);
        }
    }

    // Pipe connections
    {
        // The base game has no way to directly link pipe junctions to pipe junctions. To preserve this,
        // we do not allow pipe junctions to autolink to pipe junctions, though they seem to work.
        TArray<UClass*> incompatibleFluidClasses = TArray<UClass*>();
        if (buildable->IsA(AFGBuildablePipelineJunction::StaticClass()))
        {
            incompatibleFluidClasses.Add(AFGBuildablePipelineJunction::StaticClass());
        }

        TInlineComponentArray<TPair<UFGPipeConnectionComponent*, IFGFluidIntegrantInterface*>> openConnectionsAndIntegrants;
        FindOpenFluidConnections(openConnectionsAndIntegrants, buildable);
        AL_LOG("FindAndLinkForBuildable: Found %d open fluid connections", openConnectionsAndIntegrants.Num());
        TSet< IFGFluidIntegrantInterface* > integrantsToRegister;
        for (auto& connectionAndIntegrant : openConnectionsAndIntegrants)
        {
            auto connection = connectionAndIntegrant.Key;
            auto integrant = connectionAndIntegrant.Value;

            if (!connection->HasFluidIntegrant())
            {
                connection->SetFluidIntegrant(integrant);
            }

            if (FindAndLinkCompatibleFluidConnection(connection, incompatibleFluidClasses))
            {
                AL_LOG("FindAndLinkForBuildable: Saving fluid integrant to register for %s (%s)", *connection->GetName(), *connection->GetClass()->GetName());
                integrantsToRegister.Add(integrant);
            }
        }

        if (integrantsToRegister.Num() > 0)
        {
            AL_LOG("FindAndLinkForBuildable: Found connections have a total of %d integrants to register", integrantsToRegister.Num());
            auto pipeSubsystem = AFGPipeSubsystem::GetPipeSubsystem(buildable->GetWorld());
            for (auto integrant : integrantsToRegister)
            {
                AL_LOG("FindAndLinkForBuildable: Registering fluid integrant %s", *AutoLinkDebugging::GetFluidIntegrantName(integrant));
                pipeSubsystem->RegisterFluidIntegrant(integrant);
            }
        }
    }

    // Hypertube connections
    {
        TInlineComponentArray<UFGPipeConnectionComponentHyper*> openConnections;
        FindOpenHyperConnections(openConnections, buildable);
        AL_LOG("FindAndLinkForBuildable: Found %d open hyper connections", openConnections.Num());
        for (auto connection : openConnections)
        {
            FindAndLinkCompatibleHyperConnection(connection);
        }
    }
}

void UAutoLinkRootInstanceModule::AddIfCandidate(TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections, UFGFactoryConnectionComponent* connection)
{
    if (!connection)
    {
        AL_LOG("\tAddIfCandidate: UFGFactoryConnectionComponent is null");
        return;
    }

    if (connection->IsConnected())
    {
        AL_LOG("\tAddIfCandidate: UFGFactoryConnectionComponent %s (%s) is already connected to %s", *connection->GetName(), *connection->GetClass()->GetName(), *connection->GetConnection()->GetName());
        return;
    }

    switch (connection->GetDirection())
    {
        // We only support specific input and output connections because it's really not clear what the others mean
    case EFactoryConnectionDirection::FCD_INPUT:
    case EFactoryConnectionDirection::FCD_OUTPUT:
        break;
    default:
        // If not a direction we support, just exit
        AL_LOG("\tAddIfCandidate: Connection direction is %d (we only support input or output)", connection->GetDirection());
        return;
    }

    openConnections.Add(connection);
}

void UAutoLinkRootInstanceModule::FindOpenBeltConnections(TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections, AFGBuildable* buildable)
{
    // Start with special cases where we know how to get the connections without a full scan
    if (auto conveyorBase = Cast<AFGBuildableConveyorBase>(buildable))
    {
        AL_LOG("FindOpenBeltConnections: AFGBuildableConveyorBase %s", *conveyorBase->GetName());
        AddIfCandidate(openConnections, conveyorBase->GetConnection0());
        AddIfCandidate(openConnections, conveyorBase->GetConnection1());
    }
    else if (auto conveyorPole = Cast<AFGBuildablePoleBase>(buildable))
    {
        AL_LOG("FindOpenBeltConnections: AFGBuildablePoleBase so skipping");
        // Don't try to link to/from conveyor poles. They're cosmetic and we don't need to do the extra scanning work.
        return;
    }
    else if (auto factory = Cast<AFGBuildableFactory>(buildable))
    {
        AL_LOG("FindOpenBeltConnections: AFGBuildableFactory %s", *factory->GetName());
        for (auto connectionComponent : factory->GetConnectionComponents())
        {
            AL_LOG("FindOpenBeltConnections:\tFound UFGFactoryConnectionComponent");
            AddIfCandidate(openConnections, connectionComponent);
        }
    }
    else
    {
        AL_LOG("FindOpenBeltConnections: AFGBuildable is %s (%s)", *buildable->GetName(), *buildable->GetClass()->GetName());
        TInlineComponentArray<UFGFactoryConnectionComponent*> factoryConnections;
        buildable->GetComponents(factoryConnections);
        for (auto connectionComponent : factoryConnections)
        {
            AL_LOG("FindOpenBeltConnections:\tFound UFGFactoryConnectionComponent %s", *connectionComponent->GetName());
            AddIfCandidate(openConnections, connectionComponent);
        }
    }
}

bool UAutoLinkRootInstanceModule::IsCandidate( UFGPipeConnectionComponent* connection)
{
    if (!connection)
    {
        AL_LOG("\tAddIfCandidate: UFGPipeConnectionComponent is null");
        return false;
    }

    if (connection->IsConnected())
    {
        AL_LOG("\tAddIfCandidate: UFGPipeConnectionComponent %s (%s) is already connected to %s", *connection->GetName(), *connection->GetClass()->GetName(), *connection->GetConnection()->GetName());
        return false;
    }

    switch (connection->GetPipeConnectionType())
    {
    case EPipeConnectionType::PCT_CONSUMER:
    case EPipeConnectionType::PCT_PRODUCER:
    case EPipeConnectionType::PCT_ANY:
        break;
    default:
        AL_LOG("\tAddIfCandidate: UFGPipeConnectionComponent connection type is %d, which doesn't actually connect to entities in the world.", connection->GetPipeConnectionType());
        return false;
    }

    return true;
}

void UAutoLinkRootInstanceModule::AddIfCandidate(
    TInlineComponentArray<TPair<UFGPipeConnectionComponent*, IFGFluidIntegrantInterface*>>& openConnectionsAndIntegrants,
    UFGPipeConnectionComponent* connection,
    IFGFluidIntegrantInterface* owningFluidIntegrant)
{
    if (IsCandidate(connection))
    {
        openConnectionsAndIntegrants.Add(
            TPair<UFGPipeConnectionComponent*, IFGFluidIntegrantInterface*>(connection, owningFluidIntegrant));
    }
}

void UAutoLinkRootInstanceModule::FindOpenFluidConnections(
    TInlineComponentArray<TPair<UFGPipeConnectionComponent*,IFGFluidIntegrantInterface*>>& openConnectionsAndIntegrants,
    AFGBuildable* buildable)
{
    // Start with special cases where we know to get the connections without a full scan
    if (auto pipeline = Cast<AFGBuildablePipeline>(buildable))
    {
        AL_LOG("FindOpenFluidConnections: AFGBuildablePipeline %s", *pipeline->GetName());

        AddIfCandidate(openConnectionsAndIntegrants, pipeline->GetPipeConnection0(), pipeline);
        AddIfCandidate(openConnectionsAndIntegrants, pipeline->GetPipeConnection1(), pipeline);
    }
    else if (auto fluidIntegrant = Cast<IFGFluidIntegrantInterface>(buildable))
    {
        auto pipeConnections = fluidIntegrant->GetPipeConnections();
        AL_LOG("FindOpenFluidConnections: IFGFluidIntegrantInterface %s has %d total pipe connections", *buildable->GetName(), pipeConnections.Num());

        for (auto connection : pipeConnections)
        {
            AddIfCandidate(openConnectionsAndIntegrants, connection, fluidIntegrant);
        }
    }
    else
    {
        AL_LOG("FindOpenFluidConnections: AFGBuildable is %s (%s)", *buildable->GetName(), *buildable->GetClass()->GetName());
        for (auto component : buildable->GetComponents())
        {
            if (auto integrant = Cast<IFGFluidIntegrantInterface>(component))
            {
                auto pipeConnections = integrant->GetPipeConnections();
                AL_LOG("FindOpenFluidConnections:\tHas an IFGFluidIntegrantInterface component %s of type %s with %d total pipe connections", *component->GetName(), *component->GetClass()->GetName(), pipeConnections.Num());
                for (auto connection : pipeConnections)
                {
                    AddIfCandidate(openConnectionsAndIntegrants, connection, integrant);
                }
            }
        }
    }
}

void UAutoLinkRootInstanceModule::AddIfCandidate(TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections, UFGPipeConnectionComponentHyper* connection)
{
    if (!connection)
    {
        AL_LOG("\tAddIfCandidate: UFGPipeConnectionComponentHyper is null");
        return;
    }

    if (connection->IsConnected())
    {
        AL_LOG("\tAddIfCandidate: UFGPipeConnectionComponentHyper %s (%s) is already connected to %s", *connection->GetName(), *connection->GetClass()->GetName(), *connection->GetConnection()->GetName());
        return;
    }

    switch (connection->GetPipeConnectionType())
    {
    case EPipeConnectionType::PCT_CONSUMER:
    case EPipeConnectionType::PCT_PRODUCER:
    case EPipeConnectionType::PCT_ANY:
        break;
    default:
        AL_LOG("\tAddIfCandidate: UFGPipeConnectionComponentHyper connection type is %d, which doesn't actually connect to entities in the world.", connection->GetPipeConnectionType());
        return;
    }

    openConnections.Add(connection);
}

void UAutoLinkRootInstanceModule::FindOpenHyperConnections(TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections, AFGBuildable* buildable)
{
    // Start with special cases where we know to get the connections without a full scan
    if (auto buildablePipe = Cast<AFGBuildablePipeHyper>(buildable))
    {
        AL_LOG("FindOpenHyperConnections: AFGBuildablePipeHyper %s", *buildablePipe->GetName());
        AddIfCandidate(openConnections, Cast<UFGPipeConnectionComponentHyper>(buildablePipe->GetConnection0()));
        AddIfCandidate(openConnections, Cast<UFGPipeConnectionComponentHyper>(buildablePipe->GetConnection1()));
    }
    else
    {
        AL_LOG("FindOpenHyperConnections: AFGBuildable is %s (%s)", *buildable->GetName(), *buildable->GetClass()->GetName());
        TInlineComponentArray<UFGPipeConnectionComponentHyper*> connections;
        buildable->GetComponents(connections);
        for (auto connectionComponent : connections)
        {
            AL_LOG("\tFindOpenHyperConnections: Found UFGPipeConnectionComponentHyper");
            AddIfCandidate(openConnections, connectionComponent);
        }
    }
}

void UAutoLinkRootInstanceModule::AddIfCandidate(TInlineComponentArray<UFGRailroadTrackConnectionComponent*>& openConnections, UFGRailroadTrackConnectionComponent* connection)
{
    if (!connection)
    {
        AL_LOG("\tAddIfCandidate: UFGRailroadTrackConnectionComponent is null");
        return;
    }

    // Railroads can have multiple connections, so they're always candidates

    openConnections.Add(connection);
}

void UAutoLinkRootInstanceModule::FindOpenRailroadConnections(TInlineComponentArray<UFGRailroadTrackConnectionComponent*>& openConnections, AFGBuildable* buildable)
{
    // Start with special cases where we know to get the connections without a full scan
    if (auto railroad = Cast<AFGBuildableRailroadTrack>(buildable))
    {
        AL_LOG("FindOpenRailroadConnections: AFGBuildableRailroadTrack %s", *railroad->GetName());
        AddIfCandidate(openConnections, railroad->GetConnection(0));
        AddIfCandidate(openConnections, railroad->GetConnection(1));
    }
    else
    {
        AL_LOG("FindOpenRailroadConnections: AFGBuildable is %s (%s)", *buildable->GetName(), *buildable->GetClass()->GetName());
        TInlineComponentArray<UFGRailroadTrackConnectionComponent*> connections;
        buildable->GetComponents(connections);
        for (auto connectionComponent : connections)
        {
            AL_LOG("\tFindOpenRailroadConnections: Found UFGRailroadTrackConnectionComponent");
            AddIfCandidate(openConnections, connectionComponent);
        }
    }
}

void UAutoLinkRootInstanceModule::HitScan(
    TArray<AActor*>& actors,
    UWorld* world,
    FVector scanStart,
    FVector scanEnd,
    AActor* ignoreActor)
{
    AL_LOG("HitScan: Scanning from %s to %s", *scanStart.ToString(), *scanEnd.ToString());

    TArray< FHitResult > hitResults;
    auto collisionQueryParams = FCollisionQueryParams();
    collisionQueryParams.AddIgnoredActor(ignoreActor);
    world->LineTraceMultiByObjectType(
        hitResults,
        scanStart,
        scanEnd,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects),
        collisionQueryParams);

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

        AL_LOG("HitScan: Hit actor %s of type %s at %s. Actor is at %s",
            *actor->GetName(),
            *actor->GetClass()->GetName(),
            *result.Location.ToString(),
            *actor->GetActorLocation().ToString());

        actors.AddUnique(actor);
    }
}

void UAutoLinkRootInstanceModule::OverlapScan(
    TArray<AActor*>& actors,
    UWorld* world,
    FVector scanStart,
    float radius,
    AActor* ignoreActor)
{
    AL_LOG("OverlapScan: Scanning from %s with radius %f", *scanStart.ToString(), radius);

    TArray< FOverlapResult > overlapResults;
    auto collisionQueryParams = FCollisionQueryParams();
    collisionQueryParams.AddIgnoredActor(ignoreActor);
    world->OverlapMultiByObjectType(
        overlapResults,
        scanStart,
        FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects),
        FCollisionShape::MakeSphere(radius),
        collisionQueryParams);

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

        AL_LOG("OverlapScan: Hit actor %s of type %s. Actor is at %s (%f units away from the search start)",
            *actor->GetName(),
            *actor->GetClass()->GetName(),
            *actor->GetActorLocation().ToString(),
            (actor->GetActorLocation() - scanStart).Length());

        actors.AddUnique(actor);
    }
}

void UAutoLinkRootInstanceModule::FindAndLinkCompatibleBeltConnection(UFGFactoryConnectionComponent* connectionComponent)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG("FindAndLinkCompatibleBeltConnection: Exiting because the connection component is already connected");
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
        searchDistance = 20.0;
    }
    else if ((connectionConveyorLift = Cast<AFGBuildableConveyorLift>(outerBuildable)) != nullptr)
    {
        // If this is a conveyor lift, then there could be another conveyor lift facing it.
        // Conveyor lifts can directly connect with their connectors at 400 units away,
        // so we have that distance plus a bit to ensure we hit the buildable
        searchDistance = 420.0;
    }
    else
    {
        // If this is a normal factory/buildable, it could still be aligned with a fully-extended
        // conveyor lift and we still pad a bit to ensure an appropriate hit
        searchDistance = 320.0;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    auto searchEnd = connectorLocation + (connectionComponent->GetConnectorNormal() * searchDistance);
    auto connectionDirection = connectionComponent->GetDirection();

    AL_LOG("FindAndLinkCompatibleBeltConnection: Connector at: %s, direction is: %d",
        *connectorLocation.ToString(),
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
    for (auto hitActor : hitActors)
    {
        if (auto hitConveyor = Cast<AFGBuildableConveyorBase>(hitActor))
        {
            // We always consider conveyors as candidates and we can get their candidate connection faster than searching all their components
            AL_LOG("FindAndLinkCompatibleBeltConnection: Hit result is conveyor %s of type %s", *hitConveyor->GetName(), *hitConveyor->GetClass()->GetName());
            auto candidateConnection = connectionDirection == EFactoryConnectionDirection::FCD_INPUT
                ? hitConveyor->GetConnection1()
                : hitConveyor->GetConnection0();

            candidates.Add(candidateConnection);
            continue;
        }

        // If we are NOT a conveyor (either a belt or a lift), then we shouldn't consider non-conveyors for attachment.  Autolinking between attachments
        // or attachments and buildings causes crashes in the base game's multithreaded code.  We just checked whether this candidate is a conveyor so
        // nothing else can be a valid candidate unless we are a conveyor.
        if (!connectionConveyorBelt && !connectionConveyorLift)
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection: NOT considering hit result actor %s of type %s because Connector is not on a conveyor", *hitActor->GetName(), *hitActor->GetClass()->GetName());
            continue;
        }

        if (auto buildable = Cast<AFGBuildable>(hitActor))
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection: Examining buildable %s of type %s", *buildable->GetName(), *buildable->GetClass()->GetName());
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
            AL_LOG("FindAndLinkCompatibleBeltConnection: Ignoring hit result actor %s of type %s", *hitActor->GetName(), *hitActor->GetClass()->GetName());
        }
    }

    float closestDistance = FLT_MAX;
    UFGFactoryConnectionComponent* compatibleConnectionComponent = nullptr;
    for (auto& candidateConnection : candidates)
    {
        AL_LOG("FindAndLinkCompatibleBeltConnection: Examining connection: %s on %s.",
            *candidateConnection->GetName(),
            *candidateConnection->GetOuterBuildable()->GetName());

        if (!IsValid(candidateConnection))
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tNot valid!");
            continue;
        }

        if (candidateConnection->IsConnected())
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tAlready connected!");
            continue;
        }

        if (!candidateConnection->CanConnectTo(connectionComponent))
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tCannot be connected to this!");
            continue;
        }

        // Now that the quickest checks are done, check distance
        // These are offsets where negative is behind the connector (against the normal) and positive is in front (with the normal).
        auto minConnectorOffset = 0.0f;
        auto maxConnectorOffset = 0.0f;

        auto candidateOuterBuildable = candidateConnection->GetOuterBuildable();
        auto candidateConveyorBelt = Cast<AFGBuildableConveyorBelt>(candidateOuterBuildable);
        auto candidateConveyorLift = Cast<AFGBuildableConveyorLift>(candidateOuterBuildable);

        if (connectionConveyorBelt || candidateConveyorBelt)
        {
            // If there's a belt involved, leave the offsets at 0, since belts have to be up against the connector
            // (unless a storage container or dimensional depot are involved, but we handle those later).
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tBelt involved. Leaving offsets at 0 for now.");
        }
        else if (connectionConveyorLift || candidateConveyorLift)
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tAt least one conveyor lift is involved.");

            minConnectorOffset = 100.0; // Prevent the lift(s) from clipping in ways the game doesn't normally allow

            // Two conveyor lifts can directly connect 400 units away. There are ways to make it look like two
            // fully-extended lifts are attached at 300 units each but the game will shrink the bellows to 200
            // units each on reload, even if we AutoLink them - the bellows only seem to extend into buildings.
            // If it's just one conveyor lift, then we're either scanning to/from a building and those connectors
            // can be at-most 300 units away as the bellows will fully extend for them.
            maxConnectorOffset = connectionConveyorLift && candidateConveyorLift ? 400.0 : 300.0;
        }

        // Dimensional depot input connectors are 10 units deeper than storage container input connectors. This misalignment
        // means replacing a storage container with a dimensional depot (or vice versa) doesn't naturally autolink, which is
        // confusing since it's intuitive to swap one for the other at different points. Since there's a glow around the depot
        // connector that hides any small gap, we can compensate with offset tolerances and it won't look weird, whether the
        // belt ends in a dim depot glow or extends into the storage container an extra 10 units.
        if (connectionDirection == EFactoryConnectionDirection::FCD_INPUT && (candidateConveyorBelt || candidateConveyorLift))
        {
            if (outerBuildable->IsA(AFGCentralStorageContainer::StaticClass()))
            {
                maxConnectorOffset += 10.0f;
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tDimensional depot to conveyor. Setting max connector offset to %f to handle alignment issues.", maxConnectorOffset);
            }
            else if (outerBuildable->IsA(AFGBuildableStorage::StaticClass()))
            {
                minConnectorOffset -= 10.0f;
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tStorage container to conveyor. Settings min connector offset to %f to handle alignment issues.", minConnectorOffset);
            }
        }
        else if (connectionDirection == EFactoryConnectionDirection::FCD_OUTPUT && (connectionConveyorBelt || connectionConveyorLift))
        {
            if (candidateOuterBuildable->IsA(AFGCentralStorageContainer::StaticClass()))
            {
                maxConnectorOffset += 10.0f;
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tConveyor to dimensional depot. Setting max connector offset to %f to handle alignment issues.", maxConnectorOffset);
            }
            else if (candidateOuterBuildable->IsA(AFGBuildableStorage::StaticClass()))
            {
                minConnectorOffset -= 10.0f;
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tConveyor to storage container. Settings min connector offset to %f to handle alignment issues.", minConnectorOffset);
            }
        }

        AL_LOG("FindAndLinkCompatibleBeltConnection:\tFinal values. Min offset: %f. Max offset: %f", minConnectorOffset, maxConnectorOffset);

        if (minConnectorOffset > maxConnectorOffset)
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tMin offset %f is greater than Max offset %f? Would be great to get a reproduction of how this could happen... skipping this candidate", minConnectorOffset, maxConnectorOffset);
            continue;
        }

        const FVector candidateConnectorLocation = candidateConnection->GetConnectorLocation();
        FVector fromCandidateToConnectorVector = connectorLocation - candidateConnectorLocation; // This gives the vector from the candidate connection to the main connector

        AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnector Location %s, Candidate Location: %s, Candidate to Connector Vector: %s",
            *connectorLocation.ToString(),
            *candidateConnectorLocation.ToString(),
            *fromCandidateToConnectorVector.ToString());

        const FVector connectorNormal = connectionComponent->GetConnectorNormal();
        const FVector candidateConnectorNormal = candidateConnection->GetConnectorNormal();

        AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnector normal: %s, Candidate normal: %s",
            *connectorNormal.ToString(),
            *candidateConnectorNormal.ToString());

        double fromCandidateToConnectorDistance;

        const float CompareTolerance = .1;
        if (fromCandidateToConnectorVector.IsNearlyZero(CompareTolerance))
        {
            if (minConnectorOffset > 0 || maxConnectorOffset < 0)
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnectors are touching but this is not allowed per the connector offsets!");
                continue;
            }

            // If the connectors are touching and 0 is an allowed distance, then check whether they are facing each other.
            if (!FVector::PointsAreNear(connectorNormal, -candidateConnectorNormal, CompareTolerance))
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnectors are touching but not pointed in opposite directions!");
                continue;
            }

            fromCandidateToConnectorDistance = 0;
        }
        else
        {
            FVector fromCandidateToConnectorNormal;
            fromCandidateToConnectorVector.ToDirectionAndLength(fromCandidateToConnectorNormal, fromCandidateToConnectorDistance);

            AL_LOG("FindAndLinkCompatibleBeltConnection:\tCandidate Distance: %f, Candidate to connector normal: %s",
                fromCandidateToConnectorDistance,
                *fromCandidateToConnectorNormal.ToString());

            auto areAlignedInOppositeDirections =
                (FVector::PointsAreNear(candidateConnectorNormal, fromCandidateToConnectorNormal, CompareTolerance) &&
                 FVector::PointsAreNear(connectorNormal, -fromCandidateToConnectorNormal, CompareTolerance))
                ||
                (FVector::PointsAreNear(candidateConnectorNormal, -fromCandidateToConnectorNormal, CompareTolerance) &&
                 FVector::PointsAreNear(connectorNormal, fromCandidateToConnectorNormal, CompareTolerance));

            if (!areAlignedInOppositeDirections)
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnectors are not aligned and/or are not pointed in opposite directions!");
                continue;
            }

            // Pad a bit to compensate for floating point precision
            auto minOffsetPoint = connectorLocation + (minConnectorOffset * connectorNormal);
            auto maxOffsetPoint = connectorLocation + (maxConnectorOffset * connectorNormal);
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tMin offset point: %s, Max offset point: %s", *minOffsetPoint.ToString(), *maxOffsetPoint.ToString());

#define IS_IN_RANGE( VALUE, MIN, MAX, TOLERANCE ) (VALUE >= (MIN - TOLERANCE) && VALUE <= (MAX + TOLERANCE))
#define IS_BETWEEN( VALUE, FIRST, SECOND, TOLERANCE) \
        ((FIRST <= SECOND) ? IS_IN_RANGE(VALUE, FIRST, SECOND, TOLERANCE) : IS_IN_RANGE(VALUE, SECOND, FIRST, TOLERANCE))

            if (!IS_BETWEEN(candidateConnectorLocation.X, minOffsetPoint.X, maxOffsetPoint.X, CompareTolerance) ||
                !IS_BETWEEN(candidateConnectorLocation.Y, minOffsetPoint.Y, maxOffsetPoint.Y, CompareTolerance) ||
                !IS_BETWEEN(candidateConnectorLocation.Z, minOffsetPoint.Z, maxOffsetPoint.Z, CompareTolerance))
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tCandidate is not an allowed distance from the connector! Candidate location: %s", *candidateConnectorLocation.ToString());
                continue;
            }

#undef IS_IN_RANGE
#undef IS_BETWEEN
        }

        if (fromCandidateToConnectorDistance < closestDistance)
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tFound a new closest one (%f) at: %s", fromCandidateToConnectorDistance, *candidateConnectorLocation.ToString());
            closestDistance = fromCandidateToConnectorDistance;
            compatibleConnectionComponent = candidateConnection;

            if (closestDistance < 1)
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tFound extremely close candidate (%f units away). Taking it as the best.", closestDistance);
                break;
            }
        }
    }

    if (!compatibleConnectionComponent)
    {
        AL_LOG("FindAndLinkCompatibleBeltConnection: No compatible connection found");
        return;
    }

    auto connectionConveyor = Cast<AFGBuildableConveyorBase>(outerBuildable);
    auto otherConnectionConveyor = Cast<AFGBuildableConveyorBase>(compatibleConnectionComponent->GetOuterBuildable());
    if (connectionConveyor && otherConnectionConveyor)
    {
        AL_LOG("FindAndLinkCompatibleBeltConnection: Removing, setting connection, and then re-adding conveyor so it will build the correct chain actor");
        auto buildableSybsystem = AFGBuildableSubsystem::Get(connectionComponent->GetWorld());
        buildableSybsystem->RemoveConveyor(connectionConveyor);
        connectionComponent->SetConnection(compatibleConnectionComponent);
        buildableSybsystem->AddConveyor(connectionConveyor);
    }
    else
    {
        AL_LOG("FindAndLinkCompatibleBeltConnection: Connecting the components!");
        connectionComponent->SetConnection(compatibleConnectionComponent);
    }
}

void UAutoLinkRootInstanceModule::FindAndLinkCompatibleRailroadConnection(UFGRailroadTrackConnectionComponent* connectionComponent)
{
    // Note that we do NOT short-circuit if it's already connected because railroads can have multiple connections

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    // Railroad connectors seem to be lower than the railroad hitboxes, so we need to adjust our search start up to ensure we actually hit adjacent railroads
    auto searchStart = connectorLocation + (FVector::UpVector * 10);
    // Search a small extra distance from the connector. Though we will limit connections to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchRadius = 20.0f;

    AL_LOG("FindAndLinkCompatibleRailroadConnection: Connector at: %s", *connectorLocation.ToString());

    // Curved rails don't always seem to be hit by linear hitscan out of the connector normal so we do a radius search here to be sure we're getting good candidates.
    auto connectionOwner = connectionComponent->GetOwner();
    TArray< AActor* > hitActors;
    OverlapScan(
        hitActors,
        connectionComponent->GetWorld(),
        searchStart,
        searchRadius,
        connectionOwner);

    TArray< UFGRailroadTrackConnectionComponent* > candidates;
    for (auto actor : hitActors)
    {
        if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection: Examining hit result actor %s of type %s", *buildable->GetName(), *buildable->GetClass()->GetName());
            TInlineComponentArray<UFGRailroadTrackConnectionComponent*> openConnections;
            FindOpenRailroadConnections(openConnections, buildable);

            for (auto openConnection : openConnections)
            {
                candidates.Add(openConnection);
            }
        }
        else
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection: Ignoring hit result actor %s of type %s", *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    TArray< UFGRailroadTrackConnectionComponent* > compatibleConnections;
    for (auto candidateConnection : candidates)
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection: Examining connection: %s on %s at %s (%f units away)",
            *candidateConnection->GetName(),
            *candidateConnection->GetOwner()->GetName(),
            *candidateConnection->GetConnectorLocation().ToString(),
            FVector::Distance(connectorLocation, candidateConnection->GetConnectorLocation()));

        if (!IsValid(candidateConnection))
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tNot valid!");
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
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tAlready connected to this candidate!");
            continue;
        }

        if (compatibleConnections.Contains(candidateConnection))
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tThis connection is already slated for linking!");
            continue;
        }

        const FVector candidateLocation = candidateConnection->GetConnectorLocation();
        const FVector fromCandidateToConnectorVector = connectorLocation - candidateLocation; // This gives the vector from the candidate connector to the main connector
        const float distanceSq = fromCandidateToConnectorVector.SquaredLength();
        if (distanceSq > 1) // Anything more than a 1 cm away is too far (and most are even much closer, based on my tests)
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tCandidate connection is too far. Distance SQ: %f!", distanceSq);
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
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tCandidate connection normal is not collinear with this connector normal! The parts are not aligned! Cross product is %s", *crossProduct.ToString());
            continue;
        }

        // Determine if both connectors are facing in opposite directions. Rail junctions are an example where overlap might mean they're facing the same direction.
        // If a dot product is positive, then the vectors are less than 90 degrees apart.
        // So this dot product should be negative, meaning the connector normal is pointing AGAINST the candidate normal vector
        const double connectorDotProduct = connectorNormal.Dot(candidateConnectorNormal);
        if (connectorDotProduct >= 0)
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tThe connectors are not facing in opposite directions! connectorDotProduct: %.8f", connectorDotProduct);
            continue;
        }

        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tFound one that's extremely close, so we're saving it for linking. Location: %s", *candidateLocation.ToString());
        compatibleConnections.AddUnique(candidateConnection);

        // We don't break here - keep linking because railroads can be connected to multiple other railoads at junctions.
    }

    // At the time this runs, the game has already created graph IDs and calculated overlapping tracks while thinking they are
    // not connected (if they're connected, the code will not treat them as overlapping).  If the tracks are curved very tightly,
    // they can ever-so-slightly overlap and get tracked as overlapping, which can confuse the game into thinking there are rail
    // signal loops if we just simply connect them. Also, regardless of overlap, if you JUST connect the tracks and don't force
    // the game to recalculate graphs and signal blocks here, that creates edge cases that messes up other rail signals.
    // 
    // Side note: all of these get fixed when the game gets reloaded and all rail stuff is calculated from scratch, which is comforting
    // but obviously terrible user experience.
    // 
    // We can cover all but one known scenario by removing the current track, connecting it, then re-adding it and forcing recalculation.
    // This also updates the overlapping calculations of the current track to NOT include the candidate.  But we have to iterate over
    // the compatible connections and ensure they are each individually not overlapping and know they are not overlapping.

    if (compatibleConnections.Num() > 0)
    {
        auto connectionTrack = connectionComponent->GetTrack();

        auto subsystem = AFGRailroadSubsystem::Get(connectionComponent->GetWorld());
        subsystem->RemoveTrack(connectionTrack);

        for (auto compatibleConnection : compatibleConnections)
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tLinking to connection %s on %s", *compatibleConnection->GetName(), *compatibleConnection->GetTrack()->GetName());
            connectionComponent->AddConnection(compatibleConnection);
        }

        subsystem->AddTrack(connectionTrack);

        for (auto compatibleConnection : compatibleConnections)
        {
            auto compatibleTrack = compatibleConnection->GetTrack();
            if (compatibleTrack->mOverlappingTracks.Contains(connectionTrack))
            {
                AL_LOG("FindAndLinkCompatibleRailroadConnection:\tTrack %s still thinks it's overlapping the base track. Updating it.", *compatibleTrack->GetName());
                compatibleTrack->UpdateOverlappingTracks();
            }
        }
    }
}

bool UAutoLinkRootInstanceModule::FindAndLinkCompatibleFluidConnection(
    UFGPipeConnectionComponent* connectionComponent,
    const TArray<UClass*>& incompatibleClasses)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG("FindAndLinkCompatibleFluidConnection: Exiting because the connection component is already connected");
        return false;
    }

    auto searchStart = connectionComponent->GetConnectorLocation();
    // Search a small extra distance out from the connector. Though we will limit pipes to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchRadius = 50.0f;

    AL_LOG("FindAndLinkCompatibleFluidConnection: Connection: %s (%s) with connection type %d",
        *connectionComponent->GetName(),
        *connectionComponent->GetClass()->GetName(),
        connectionComponent->GetPipeConnectionType());

    TArray< AActor* > hitActors;
    OverlapScan(
        hitActors,
        connectionComponent->GetWorld(),
        searchStart,
        searchRadius,
        connectionComponent->GetOwner());

    TArray< UFGPipeConnectionComponentBase* > candidates;
    for (auto actor : hitActors)
    {
        if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG( "FindAndLinkCompatibleFluidConnection: Examining hit result actor %s of type %s", *buildable->GetName(), *buildable->GetClass()->GetName());

            auto actorIsCompatible = true;
            for(auto incompatibleClass : incompatibleClasses)
            {
                if (buildable->IsA(incompatibleClass))
                {
                    AL_LOG("FindAndLinkCompatibleFluidConnection: Skipping hit result because it is an instance of incompatible class %s", *incompatibleClass->GetName());
                    actorIsCompatible = false;
                    break;
                }
            }

            if (!actorIsCompatible) continue;

            TInlineComponentArray<TPair<UFGPipeConnectionComponent*, IFGFluidIntegrantInterface*>> openConnectionsAndIntegrants;
            FindOpenFluidConnections(openConnectionsAndIntegrants, buildable);

            for (auto& openConnectionAndIntegrant : openConnectionsAndIntegrants)
            {
                candidates.Add(openConnectionAndIntegrant.Key);
            }
        }
        else
        {
            AL_LOG("FindAndLinkCompatibleFluidConnection: Ignoring hit result actor %s of type %s", *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    return ConnectBestPipeCandidate(connectionComponent, candidates);
}

bool UAutoLinkRootInstanceModule::FindAndLinkCompatibleHyperConnection(UFGPipeConnectionComponentHyper* connectionComponent)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG("FindAndLinkCompatibleHyperConnection: Exiting because the connection component is already connected");
        return false;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    // Search a small extra distance straight out from the connector. Though we will limit pipes to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchEnd = connectorLocation + (connectionComponent->GetConnectorNormal() * 10);

    AL_LOG("FindAndLinkCompatibleHyperConnection: Connector at: %s, searchEnd is at: %s", *connectorLocation.ToString(), *searchEnd.ToString());

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
            AL_LOG("FindAndLinkCompatibleHyperConnection: Examining hit result actor %s of type %s", *buildable->GetName(), *buildable->GetClass()->GetName());
            TInlineComponentArray<UFGPipeConnectionComponentHyper*> openConnections;
            FindOpenHyperConnections(openConnections, buildable);

            for (auto openConnection : openConnections)
            {
                candidates.Add(openConnection);
            }
        }
        else
        {
            AL_LOG("FindAndLinkCompatibleHyperConnection: Ignoring hit result actor %s of type %s", *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    return ConnectBestPipeCandidate(connectionComponent, candidates);
}

bool UAutoLinkRootInstanceModule::ConnectBestPipeCandidate(UFGPipeConnectionComponentBase* connectionComponent, TArray<UFGPipeConnectionComponentBase*>& candidates)
{
    auto connectorLocation = connectionComponent->GetConnectorLocation();
    auto connectionFluidComponent = Cast<UFGPipeConnectionComponent>(connectionComponent);
    for (auto candidateConnection : candidates)
    {
        AL_LOG("ConnectBestPipeCandidate: Examining connection candidate: %s (%s) at %s (%f units away). Connection type %d",
            *candidateConnection->GetName(),
            *candidateConnection->GetClass()->GetName(),
            *candidateConnection->GetConnectorLocation().ToString(),
            FVector::Distance(connectorLocation, candidateConnection->GetConnectorLocation()),
            candidateConnection->GetPipeConnectionType());

        if (!IsValid(candidateConnection))
        {
            AL_LOG("ConnectBestPipeCandidate:\tNot valid!");
            continue;
        }

        // Determine if the connections components are aligned
        const FVector connectorNormal = connectionComponent->GetConnectorNormal();
        const FVector crossProduct = FVector::CrossProduct(connectorNormal, candidateConnection->GetConnectorNormal());
        auto isCollinear = crossProduct.IsNearlyZero(.01);
        if (!isCollinear)
        {
            AL_LOG("ConnectBestPipeCandidate:\tOther connection normal is not collinear with this connector normal! The parts are not aligned!");
            continue;
        }

        // Pipes have to be virtually touching for us to link them so we just make double sure of that here.
        const FVector otherLocation = candidateConnection->GetConnectorLocation();
        const float distanceSq = FVector::DistSquared(otherLocation, connectorLocation);
        if (distanceSq > 1) // Anything more than a 1 cm away is too far (and most are even much closer, based on my tests)
        {
            AL_LOG("ConnectBestPipeCandidate:\tConnection is too far away to be auto-linked!");
            continue;
        }

        AL_LOG("ConnectBestPipeCandidate:\tFound one that's extremely close; taking it as the best result. Location: %s", *otherLocation.ToString());

        candidateConnection->SetConnection(connectionComponent);
        return true;
    }

    AL_LOG("ConnectBestPipeCandidate: No compatible connection found");
    return false;
}
