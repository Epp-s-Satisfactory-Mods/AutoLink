#include "AutoLinkRootInstanceModule.h"

#include "AutoLinkDebugging.h"
#include "AutoLinkDebugSettings.h"
#include "AutoLinkLogCategory.h"
#include "AutoLinkLogMacros.h"

#include "AbstractInstanceManager.h"
#include "BlueprintHookManager.h"
#include "FGBlueprintHologram.h"
#include "FGBuildableConveyorAttachment.h"
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
#include "FGBuildableRailroadSwitchControl.h"
#include "FGBuildableRailroadTrack.h"
#include "FGBuildableSpawnStrategy_RSC.h"
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
#include "Tests/FGTestBlueprintFunctionLibrary.h"

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

    AL_LOG("UAutoLinkRootInstanceModule: Loading blueprint types...");

    this->RailroadSwitchControlBlueprintClass.LoadSynchronous();
    this->RailroadSwitchControlRecipeBlueprintClass.LoadSynchronous();

    RailRoadSwitchControlClass = this->RailroadSwitchControlBlueprintClass.Get();
    RailRoadSwitchControlRecipeClass = this->RailroadSwitchControlRecipeBlueprintClass.Get();

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

        // Don't register fluid integrants if we're inside a blueprint designer
        if ( !buildable->GetBlueprintDesigner() && integrantsToRegister.Num() > 0)
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

    auto numConnections = connection->GetConnections().Num();
    if ( numConnections >= MAX_CONNECTIONS_PER_RAIL_CONNECTOR)
    {
        AL_LOG("\tAddIfCandidate: UFGRailroadTrackConnectionComponent is full with %d connections", numConnections);
        return;
    }

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
            if (connectionConveyorLift || candidateConveyorLift)
            {
                // If it's a belt to conveyor lift, we allow the same distance that the lifts can extend to connect when building them normally
                maxConnectorOffset = 200.0;
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tBelt to conveyor lift. Setting max offset to %f.", maxConnectorOffset);
            }
            else
            {
                // If a belt to a non-lift, leave the offsets at 0, since belts have to be up against the connector
                // (unless a storage container or dimensional depot are involved, but we handle those later).
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tBelt to non-lift. Leaving offsets at 0 for now.");
            }
        }
        else if (connectionConveyorLift || candidateConveyorLift)
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tAt least one conveyor lift is involved.");

            if (candidateOuterBuildable->IsA(AFGBuildableConveyorAttachment::StaticClass()) || candidateOuterBuildable->IsA(AFGBuildableConveyorAttachment::StaticClass()))
            {
                minConnectorOffset = 0; // Lifts are allowed to clip all the way into splitters/mergers
            }
            else
            {
                minConnectorOffset = 100.0; // But they cannot clip all the way into other factory buildings
            }

            // Two conveyor lifts can directly connect 400 units away. There are ways to make it look like two
            // fully-extended lifts are attached at 300 units each (600 total) but the game will shrink the
            // bellows to 200 units each on reload, even if we AutoLink them - the bellows only seem to extend
            // into buildings. If it's just one conveyor lift, then we're either scanning to/from a building and
            // those connectors can be at-most 300 units away as the bellows will fully extend for them.
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
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tStorage container to conveyor. Setting min connector offset to %f to handle alignment issues.", minConnectorOffset);
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

        AL_LOG("FindAndLinkCompatibleBeltConnection:\tFinal offsets . Min offset: %f. Max offset: %f", minConnectorOffset, maxConnectorOffset);

        if (minConnectorOffset > maxConnectorOffset)
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tMin offset %f is greater than Max offset %f? Would be great to get a reproduction of how this could happen... skipping this candidate", minConnectorOffset, maxConnectorOffset);
            continue;
        }

        const FVector candidateConnectorLocation = candidateConnection->GetConnectorLocation();
        // This gives the vector from the candidate connection to the main connector, which is useful to know where the connectors are in space relative to each other.
        // With simple vector operations on just the connector normals, they could theoretically point "against" each other but be on opposite sides of the map. Using
        // the vector between the connector and the candidate lets us determine their distance and whether the connectors are overlapping.
        FVector fromCandidateToConnectorVector = connectorLocation - candidateConnectorLocation; 

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
        if (fromCandidateToConnectorVector.IsNearlyZero(1)) // Treat a distance within 1 cm as touching
        {
            if (minConnectorOffset > 0 || maxConnectorOffset < 0)
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnectors are touching but this is not allowed per the connector offset limits!");
                continue;
            }

            AL_LOG("FindAndLinkCompatibleBeltConnection:\tUnitVectorsArePointingInOppositeDirections: %d", UnitVectorsArePointingInOppositeDirections(connectorNormal, candidateConnectorNormal, .015));

            // If the connectors are touching and 0 is an allowed distance, then check whether they are facing each other.
            if (!FVector::PointsAreNear(connectorNormal, -candidateConnectorNormal, .1)) // Allow a little floating point precision error
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnectors are touching but not pointed in opposite directions!");
                continue;
            }

            fromCandidateToConnectorDistance = 0;
        }
        else if (minConnectorOffset == 0 && maxConnectorOffset == 0)
        {
            AL_LOG("FindAndLinkCompatibleBeltConnection:\tConnectors are not touching but min and max offset are both 0!");
            continue;
        }
        else
        {
            // Connectors are some distance from each other and min/max offsets allow some distance - now we figure out if they match
            FVector fromCandidateToConnectorNormal;
            fromCandidateToConnectorVector.ToDirectionAndLength(fromCandidateToConnectorNormal, fromCandidateToConnectorDistance);

            AL_LOG("FindAndLinkCompatibleBeltConnection:\tCandidate Distance: %f, Candidate to connector normal: %s",
                fromCandidateToConnectorDistance,
                *fromCandidateToConnectorNormal.ToString());

            const int ConnectorOffsetPadding = 1; // Padding to allow for floating point issues and ever-so-slightly angled connectors
            const double CosineTolerance = .001; // Equates to 2.563 degrees of tolerance, which is around the limit of the angle at which conveyor lifts will naturally snap to connections, from in-game testing

            if (UnitVectorsArePointingInOppositeDirections(connectorNormal, fromCandidateToConnectorNormal, CosineTolerance) // Connector normal points at candidate
                &&
                UnitVectorsArePointingInOppositeDirections(candidateConnectorNormal, -fromCandidateToConnectorNormal, CosineTolerance)) // Candidate normal points at connector
            {
                // They are aligned, pointing at each other, and the candidate is at a positive offset, so check against allowed positive offsets

                if (maxConnectorOffset <= 0 || fromCandidateToConnectorDistance > maxConnectorOffset + ConnectorOffsetPadding)
                {
                    AL_LOG("FindAndLinkCompatibleBeltConnection:\tCandidate offset (%f) is outside of the max connector offset (%f)!", fromCandidateToConnectorDistance, maxConnectorOffset);
                    continue;
                }
                else if (minConnectorOffset >= 0 && fromCandidateToConnectorDistance < minConnectorOffset - ConnectorOffsetPadding)
                {
                    AL_LOG("FindAndLinkCompatibleBeltConnection:\tCandidate offset (%f) is outside of the min connector offset (%f)!", fromCandidateToConnectorDistance, minConnectorOffset);
                    continue;
                }
            }
            else if (
                UnitVectorsArePointingInOppositeDirections(connectorNormal, -fromCandidateToConnectorNormal, CosineTolerance) // Connector normal points away from candidate
                &&
                UnitVectorsArePointingInOppositeDirections(candidateConnectorNormal, fromCandidateToConnectorNormal, CosineTolerance)) // Candidate normal points away from connector
            {
                auto negativeCandidateDistance = -fromCandidateToConnectorDistance;

                // They are aligned, pointing away from each other, and the candidate is at a negative offset, so check against allowed negative offsets
                if (minConnectorOffset >= 0 || negativeCandidateDistance < minConnectorOffset - ConnectorOffsetPadding)
                {
                    AL_LOG("FindAndLinkCompatibleBeltConnection:\tCandidate offset (%f) is outside of the min connector offset (%f)!", negativeCandidateDistance, minConnectorOffset);
                    continue;
                }
                else if (maxConnectorOffset <= 0 && negativeCandidateDistance > maxConnectorOffset + ConnectorOffsetPadding)
                {
                    AL_LOG("FindAndLinkCompatibleBeltConnection:\tCandidate offset (%f) is outside of the max connector offset (%f)!", negativeCandidateDistance, maxConnectorOffset);
                    continue;
                }
            }
            else
            {
                AL_LOG("FindAndLinkCompatibleBeltConnection:\tThe connectors are not collinear!");
                continue;
            }
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
    auto numStartingConnections = connectionComponent->GetConnections().Num();
    if (numStartingConnections >= MAX_CONNECTIONS_PER_RAIL_CONNECTOR)
    {
        AL_LOG("FindAndLinkCompatibleBeltConnection: Exiting because the connection component is already full");
        return;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    // Railroad connectors seem to be lower than the railroad hitboxes, so we need to adjust our search start up to ensure we actually hit adjacent railroads
    auto searchStart = connectorLocation;
    // Search a small extra distance from the connector. Though we will limit connections to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchRadius = 30.0f;

    AL_LOG("FindAndLinkCompatibleRailroadConnection: Connector at: %s. Currently has %d connections. Already has a switch control: %d", *connectorLocation.ToString(), numStartingConnections, connectionComponent->GetSwitchControl() != nullptr);

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

            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tFound %d open railroad connections on hit result actor", openConnections.Num());
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

    auto numCompatibleConnections = 0;
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

        // Don't need to explicitly check for the candidate being full because that's done in AddIfCandidate

        if (candidateConnection->GetConnections().Contains(connectionComponent))
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
        // We allow slightly unaligned normal vectors to account for curved rails.
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

        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tThis is a compatible connection! Saving it for linking. Location: %s", *candidateLocation.ToString());
        compatibleConnections.AddUnique(candidateConnection);
        ++numCompatibleConnections;

        if (numStartingConnections + numCompatibleConnections >= MAX_CONNECTIONS_PER_RAIL_CONNECTOR)
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tThe connector started with %d existing connections and we've found %d to link, which will fill it up. Breaking out of search loop and linking what we have.", numStartingConnections, numCompatibleConnections);
            break;
        }

        // The connection isn't full yet, so keep searching for compatible connections.
    }

    if (numCompatibleConnections == 0)
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tNo compatible connections found!",
            numStartingConnections,
            numCompatibleConnections,
            MAX_CONNECTIONS_PER_RAIL_CONNECTOR);
        return;
    }

    if (numStartingConnections + numCompatibleConnections > MAX_CONNECTIONS_PER_RAIL_CONNECTOR)
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tThe connector started with %d connections and we saved %d more to connect, which sums to more than the allowed %d. This really shouldn't happen - there's a bug somewhere! Aborting!",
            numStartingConnections,
            numCompatibleConnections,
            MAX_CONNECTIONS_PER_RAIL_CONNECTOR);
        return;
    }

    AL_LOG("FindAndLinkCompatibleRailroadConnection:\tFound a total of %d compatible connections to attempt to link", numCompatibleConnections);

    // Linking multiple connections to the same opposing connection creates a switch in the graph so trains can choose
    // which route to take. The game doesn't allow creating a switch at an existing switch (even going in the opposite
    // direction), so we can't allow it either. Figure out what switches would be generated by autolinking so we can
    // create the proper switch control or, if necessary, abort linking altogether.
    // Note that only one switch should ever be created at a time by linking a connection - any more than that means
    // we are trying to create overlapping switches.

    UFGRailroadTrackConnectionComponent* connectionNeedingSwitchControl = nullptr;

    auto willNeedSwitchControlForConnection =
        // Switch controls shouldn't exist on a connection unless it already has more than 1 connection but I received
        // a crash report where a modded blueprint had a connector with 1 connection AND a switch control, so we'll
        // be extra sure not to exacerbate invalid states by assuming anything with a switch control counts as a switch.
        !connectionComponent->GetSwitchControl()
        &&
        (
            // If we're adding 2-3 connections, we had 0 or 1 before (because the max is 3) and we're creating a new switch
            numCompatibleConnections > 1 ||
            // If we had 1 connection and we're adding any, then we're making a new switch
            (numStartingConnections == 1 && numCompatibleConnections > 0)
        );
    if (willNeedSwitchControlForConnection)
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tLinking will result in a switch control created for this connection");
        connectionNeedingSwitchControl = connectionComponent;
    }
    else
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tLinking will NOT result in a switch control created for this connection");
    }

    // We know whether we are creating a switch for the scanning connection. Now we have to figure out if it would create
    // a new switch for any of the compatible connections.

    // True if the scanning connection already has a switch
    bool foundExistingSwitch = numStartingConnections > 1 || connectionComponent->GetSwitchControl();
    for (auto compatibleConnection : compatibleConnections)
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tChecking whether compatible connection %s on %s already has a switch control", *compatibleConnection->GetName(), *compatibleConnection->GetTrack()->GetName());
        int numConnections = compatibleConnection->GetConnections().Num();
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\t\tIt has %d connections already", numConnections);

        // If it already has connections before linking, it has or will create a switch
        if (numConnections > 0)
        {
            // If it has or creates a switch and one of the other connection has or creates a switch, we're creating overlapping switches
            if (foundExistingSwitch || connectionNeedingSwitchControl)
            {
                AL_LOG("FindAndLinkCompatibleRailroadConnection:\tCompatible connection %s on %s either has or needs a switch but connection %s on %s will need one to complete linking. This is trying to create overlapping switches, so we abort all linking!",
                    *compatibleConnection->GetName(),
                    *compatibleConnection->GetTrack()->GetName(),
                    *connectionNeedingSwitchControl->GetName(),
                    *connectionNeedingSwitchControl->GetTrack()->GetName());
                return;
            }

            if (numConnections > 1 || compatibleConnection->GetSwitchControl())
            {
                // If there are multiple connections or a switch control already on this, then we have an existing switch
                AL_LOG("FindAndLinkCompatibleRailroadConnection:\t\tCompatible connection already has a switch or switch control - we shouldn't need to create one");
                foundExistingSwitch = true;
            }
            else
            {
                // If there was just one connection and no switch control, then linking will make a new switch and this will need a switch control
                AL_LOG("FindAndLinkCompatibleRailroadConnection:\t\tCompatible connection will need a switch after linking");
                connectionNeedingSwitchControl = compatibleConnection;
            }
        }
    }

    // At the time this runs, the game has already created graph IDs and calculated overlapping tracks while thinking they are
    // not connected (if they're connected, the code will not treat them as overlapping).  If the tracks are curved very tightly,
    // they can ever-so-slightly overlap and get tracked as overlapping, which can confuse the game into thinking there are rail
    // signal loops if we just simply connect them. Also, regardless of overlap, if you JUST connect the tracks and don't force
    // the game to fully recalculate graphs and signal blocks here, that creates edge cases that messes up other rail signals.
    // 
    // Side note: all of these get fixed when the game gets reloaded and all rail stuff is calculated from scratch, which is comforting
    // but obviously not what we want.
    // 
    // We can fix the current track entirely by removing it from the subsystem, connecting it to appropriate candidates, then re-adding it
    // and forcing recalculation. This queues graph rebuilds and updates the overlapping calculations of the current track to NOT include
    // any candidates. To ensure the overlapping tracks are correct on the compatible connection tracks, we just explicitly update their
    // overlapping tracks after the connection is made.

    auto connectionTrack = connectionComponent->GetTrack();

    auto railSubsystem = AFGRailroadSubsystem::Get(connectionComponent->GetWorld());
    railSubsystem->RemoveTrack(connectionTrack);

    for (auto compatibleConnection : compatibleConnections)
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tLinking to connection %s on %s", *compatibleConnection->GetName(), *compatibleConnection->GetTrack()->GetName());
        connectionComponent->AddConnection(compatibleConnection);
    }

    railSubsystem->AddTrack(connectionTrack);

    for (auto compatibleConnection : compatibleConnections)
    {
        auto linkedTrack = compatibleConnection->GetTrack();
        if (linkedTrack->mOverlappingTracks.Contains(connectionTrack))
        {
            AL_LOG("FindAndLinkCompatibleRailroadConnection:\tTrack %s still thinks it's overlapping the base track. Updating it.", *linkedTrack->GetName());
            linkedTrack->UpdateOverlappingTracks();
        }
    }

    // Now that tracks are connected and updated, we can create a switch control if any connection needs it
    if (connectionNeedingSwitchControl)
    {
        AL_LOG("FindAndLinkCompatibleRailroadConnection:\tCreating a switch control for connection %s on %s", *connectionNeedingSwitchControl->GetName(), *connectionNeedingSwitchControl->GetTrack()->GetName());

        auto strat = NewObject<UFGBuildableSpawnStrategy_RSC>();
        strat->mPlayBuildEffect = true;
        strat->mBuiltWithRecipe = RailRoadSwitchControlRecipeClass;
        strat->mControlledConnection = connectionNeedingSwitchControl;

        auto switchControl = Cast<AFGBuildableRailroadSwitchControl>(UFGTestBlueprintFunctionLibrary::SpawnBuildableFromClass(
            RailRoadSwitchControlClass,
            connectionNeedingSwitchControl->GetComponentTransform(),
            connectionNeedingSwitchControl->GetWorld(),
            strat));
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

bool UAutoLinkRootInstanceModule::UnitVectorsArePointingInOppositeDirections(FVector firstUnitVector, FVector secondUnitVector, double cosineTolerance)
{
    auto dotProduct = firstUnitVector.Dot(secondUnitVector);

    // Because they are unit vectors, the dot product is exactly the cosine of the angle between the vectors. For them to be in opposite directions,
    // that should be -1, but we have a tolerance for floating point error and to allow slight angles when needed

    auto result = dotProduct <= (cosineTolerance - 1);
    AL_LOG("UnitVectorsArePointingInOppositeDirections: Dot product of %s and %s is %f. Cosine tolerance is %f. Angle between them is: %f. Result: %d", *firstUnitVector.ToString(), *secondUnitVector.ToString(), dotProduct, cosineTolerance, FMath::RadiansToDegrees(acos(dotProduct)), result);
    return result;
}
