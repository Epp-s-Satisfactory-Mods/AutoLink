#include "AutoLink.h"

#include "AbstractInstanceManager.h"
#include "FGBlueprintHologram.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildableConveyorLift.h"
#include "FGBuildablePipeHyper.h"
#include "FGBuildablePipeline.h"
#include "FGBuildableRailroadTrack.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionFactory.h"
#include "FGPipeSubsystem.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "Hologram/FGBuildableHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "InstanceData.h"
#include "Patching/NativeHookManager.h"

DEFINE_LOG_CATEGORY(LogAutoLink)

// The mod template does this but we have no text to localize
#define LOCTEXT_NAMESPACE "FAutoLinkModule"

// When we're building to ship, set this to 0 to no-op logging and minimize performance impact. Would prefer to do this through
// build defines based on whether we're building for development or shipping but at the moment alpakit always builds shipping.
#define AL_LOG_DEBUG_TEXT 1

#if AL_LOG_DEBUG_TEXT
#define AL_LOG(Verbosity, Format, ...)\
    UE_LOG( LogAutoLink, Verbosity, Format, ##__VA_ARGS__ )
#else
#define AL_LOG(Verbosity, Format, ...)
#endif

void FAutoLinkModule::StartupModule()
{
    if (WITH_EDITOR)
    {
        AL_LOG(Verbose, TEXT("StartupModule: Not hooking anything because WITH_EDITOR is true!"));
        return;
    }

    AL_LOG(Verbose, TEXT("StartupModule: Hooking Functions..."));

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
        [](const AFGBuildableHologram* buildableHologram, AFGBuildable* buildable)
        {
            AL_LOG(Verbose, TEXT("ConfigureComponentsHook: The hologram is %s and buildable is %s at %s"), *buildableHologram->GetName(), *buildable->GetName(), *buildable->GetActorLocation().ToString());
            FindAndLinkForBuildable(buildable);
        });
}

void FAutoLinkModule::FindAndLinkForBuildable(AFGBuildable* buildable)
{
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
    // Start with special cases where we know to get the connections without a full scan
    if (auto conveyorBase = Cast<AFGBuildableConveyorBase>(buildable))
    {
        AL_LOG(Verbose, TEXT("FindOpenBeltConnections: AFGBuildableConveyorBase"));
        AddIfOpen(openConnections, conveyorBase->GetConnection0());
        AddIfOpen(openConnections, conveyorBase->GetConnection1());
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
            AL_LOG(Verbose, TEXT("\tFindOpenBeltConnections: Found UFGFactoryConnectionComponent"));
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

    if (connection->IsConnected())
    {
        AL_LOG(Verbose, TEXT("\tAddIfOpen: UFGRailroadTrackConnectionComponent %s (%s) is already connected to %s"), *connection->GetName(), *connection->GetClass()->GetName(), *connection->GetConnection()->GetName());
        return;
    }

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
        // Don't know what to do with EFactoryConnectionDirection::FCD_ANY or other values and I suspect they're not necessary. Can come back and handle them later if that's wrong.
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Exiting because the connection direction is: %d"), connectionDirection);
        return;
    }

    // UFGFactoryConnectionComponent::FindCompatibleOverlappingConnection doesn't doesn't do the trick here; it doesn't detect conveyor lifts so we have
    // to fall back to doing the search ourselves.

    const float MAX_FACTORY_CONNECTOR_SEARCH_DISTANCE = 301.0f; // One more than the furthest away a compatible connector can be (in this case, a conveyor lift)
    auto connectorLocation = connectionComponent->GetConnectorLocation();
    auto searchEnd = connectorLocation + (connectionComponent->GetConnectorNormal() * MAX_FACTORY_CONNECTOR_SEARCH_DISTANCE);

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Connector at: %s, searchEnd is at: %s, direction is: %d"), *connectorLocation.ToString(), *searchEnd.ToString(), connectionDirection);

    TArray< AActor* > hitActors;
    HitScan(
        hitActors,
        connectionComponent->GetWorld(),
        connectorLocation,
        searchEnd,
        connectionComponent->GetOwner());

    // 12 is a random guess of the max possible candidates we could ever really see
    TArray<FactoryConnectionCandidate, TInlineAllocator<12>> candidates;
    for (auto actor : hitActors)
    {
        if (auto buildableConveyor = Cast<AFGBuildableConveyorBase>(actor))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Examining buildable conveyor base %s of type %s"), *buildableConveyor->GetName(), *buildableConveyor->GetClass()->GetName());
            // Conveyor belts or conveyor lift
            const float CONVEYOR_LIFT_MIN_DISTANCE = 100.f; // Conveyor lifts have some natural clearance
            auto minConnectorDistance = 0.0f;
            auto maxConnectorDistance = 0.0f; // Conveyor belts have to be touching the connector; if it's a conveyor lift, we'll change this accordingly in a bit
            auto conveyorLift = Cast<AFGBuildableConveyorLift>(actor);

            switch (connectionDirection)
            {
            case EFactoryConnectionDirection::FCD_INPUT:
                if (conveyorLift)
                {
                    minConnectorDistance = CONVEYOR_LIFT_MIN_DISTANCE;
                    maxConnectorDistance =
                        buildableConveyor->GetConnection1()->GetConnectorClearance() + // Connection1 is always the output connection
                        conveyorLift->mOpposingConnectionClearance[1]; // If the conveyor bellows extended to snap on construction, the clearance through which they snapped will be here
                }
                candidates.Add(FactoryConnectionCandidate(buildableConveyor->GetConnection1(), minConnectorDistance, maxConnectorDistance));
                break;
            case EFactoryConnectionDirection::FCD_OUTPUT:
                if (conveyorLift)
                {
                    minConnectorDistance = CONVEYOR_LIFT_MIN_DISTANCE;
                    maxConnectorDistance =
                        buildableConveyor->GetConnection0()->GetConnectorClearance() + // Connection0 is always the input connection
                        conveyorLift->mOpposingConnectionClearance[0]; // If the conveyor bellows extended to snap on construction, the clearance through which they snapped will be here
                }
                candidates.Add(FactoryConnectionCandidate(buildableConveyor->GetConnection0(), minConnectorDistance, maxConnectorDistance));
                break;
            }
        }
        else if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Examining buildable %s of type %s"), *buildable->GetName(), *buildable->GetClass()->GetName());
            TInlineComponentArray<UFGFactoryConnectionComponent*> openConnections;
            FindOpenBeltConnections(openConnections, buildable);

            for (auto openConnection : openConnections)
            {
                candidates.Add(FactoryConnectionCandidate(openConnection, 0, 0));
            }
        }
        else
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Ignoring hit result actor %s of type %s"), *actor->GetName(), *actor->GetClass()->GetName());
        }
    }

    float closestDistanceSq = FLT_MAX;
    UFGFactoryConnectionComponent* compatibleConnectionComponent = nullptr;
    for (auto& candidate : candidates)
    {
        auto otherConnection = candidate.ConnectionComponent;
        auto minConnectorDistance = candidate.MinConnectorDistance;
        auto maxConnectorDistance = candidate.MaxConnectorDistance;

        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Examining connection: %s (%s). Min distance %f. Max distance: %f"),
            *otherConnection->GetName(),
            *otherConnection->GetClass()->GetName(),
            minConnectorDistance,
            maxConnectorDistance);

        if (!IsValid(otherConnection))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tNot valid!"));
            continue;
        }

        if (otherConnection == compatibleConnectionComponent)
        {
            // Sometimes the resolving the hit scan yields multiple hits on the same component; we can short-circuit that here.
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tRedundant! This is already the best candidate."));
            continue;
        }

        if (otherConnection->IsConnected())
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tAlready connected!"));
            continue;
        }

        if (!otherConnection->CanConnectTo(connectionComponent))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tCannot be connected to this!"));
            continue;
        }

        const FVector otherLocation = otherConnection->GetConnectorLocation();
        const FVector fromOtherToConnectorVector = connectorLocation - otherLocation; // This gives the vector from the other connection to the main connector
        const float distanceSq = fromOtherToConnectorVector.SquaredLength();
        const float minDistanceSq = minConnectorDistance == 0.0f ? 0.0f : ((minConnectorDistance * minConnectorDistance) - 1); // Give a little padding for floating points
        const float maxDistanceSq = (maxConnectorDistance * maxConnectorDistance) + 1; // A little padding for floating points
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tMin Distance: %f, Max Distance: %f, Distance: %f!"), minConnectorDistance, maxConnectorDistance, FMath::Sqrt( distanceSq ) );
        if (distanceSq < minDistanceSq || distanceSq > maxDistanceSq)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tOther connection is too close or too far. Min Distance SQ: %f, Max Distance SQ: %f, Distance SQ: %f!"), minDistanceSq, maxDistanceSq, distanceSq);
            continue;
        }

        const FVector connectorNormal = connectionComponent->GetConnectorNormal();
        const FVector otherConnectorNormal = otherConnection->GetConnectorNormal();

        // Determine if the connection components are aligned. If the cross product is 0, they are on the same line
        const FVector crossProduct = FVector::CrossProduct(connectorNormal, otherConnectorNormal);
        auto isCollinear = crossProduct.IsNearlyZero(.01);
        if (!isCollinear)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tOther connection normal is not collinear with this connector normal! The parts are not aligned!"), );
            continue;
        }

        // If we're within 1 cm, then we can just take this one as the best one. This isn't possible with conveyor lifts but is with belt. There could be multiple this
        // close but that's extremely rare and just choosing one seems fine.
        // This prevents the dot product math below, plus avoids some cases where things are barely overlapping and giving false dot product negatives.
        if (distanceSq < 1)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tFound one that's less than 1 cm away; take it as the best result. Location: %s"), *otherLocation.ToString());
            closestDistanceSq = distanceSq;
            compatibleConnectionComponent = otherConnection;
            break;
        }

        // Determine if both connectors are in front of each other (meaning they're facing each other).
        // If we don't do this check, the connectors might be facing away or overlapping but past each other.
        // 
        // If a dot product is positive, then the vectors are less than 90 degrees apart.
        // So this dot product should be negative, meaning the connector normal is pointing AGAINST the vector from the candidate to the connector
        const double connectorNormalDotProduct = connectorNormal.Dot(fromOtherToConnectorVector);
        // And this dot product should be positive, meaning the other connector normal is pointing WITH the vector from the candidate to the connector
        const double otherNormalDotProduct = otherConnectorNormal.Dot(fromOtherToConnectorVector);
        if (connectorNormalDotProduct > 0 || otherNormalDotProduct < 0)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tThe connectors are not each facing each other! connectorDotVec: %f, otherDotVec: %f"), connectorNormalDotProduct, otherNormalDotProduct);
            continue;
        }

        if (distanceSq < closestDistanceSq)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tFound a new closest one at: %s"), *otherLocation.ToString());
            closestDistanceSq = distanceSq;
            compatibleConnectionComponent = otherConnection;
        }
    }

    if (!compatibleConnectionComponent)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: No compatible connection found"));
        return;
    }

    connectionComponent->SetConnection(compatibleConnectionComponent);
}

void FAutoLinkModule::FindAndLinkCompatibleRailroadConnection(UFGRailroadTrackConnectionComponent* connectionComponent)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Exiting because the connection component is already connected"));
        return;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    // Railroad connectors seem to be lower than the railroad hitboxes, so we need to adjust our search start up to ensure we actually hit adjacent railroads
    auto searchStart = connectorLocation + (FVector::UpVector * 10);
    // Search a small extra distance straight out from the connector. Though we will limit connections to 1 cm away, sometimes the hit box for the containing actor is a bit further
    auto searchEnd = connectorLocation + (connectionComponent->GetConnectorNormal() * 10);

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Connector at: %s, searchStart is %s, searchEnd is at: %s"),
        *connectorLocation.ToString(),
        *searchStart.ToString(),
        *searchEnd.ToString());

    TArray< AActor* > hitActors;
    HitScan(
        hitActors,
        connectionComponent->GetWorld(),
        searchStart,
        searchEnd,
        connectionComponent->GetOwner());

    TArray< UFGRailroadTrackConnectionComponent* > candidates;
    for (auto actor : hitActors)
    {
        if (auto buildable = Cast<AFGBuildable>(actor))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Examining buildable %s of type %s"), *buildable->GetName(), *buildable->GetClass()->GetName());
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

    for (auto otherConnection : candidates)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection: Examining connection: %s (%s) at %s (%f units away)"),
            *otherConnection->GetName(),
            *otherConnection->GetClass()->GetName(),
            *otherConnection->GetConnectorLocation().ToString(),
            FVector::Distance(connectorLocation, otherConnection->GetConnectorLocation()));

        if (!IsValid(otherConnection))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tNot valid!"));
            continue;
        }

        if (otherConnection->IsConnected())
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tAlready connected!"));
            continue;
        }

        const FVector connectorNormal = connectionComponent->GetConnectorNormal();

        // Determine if the connections components are aligned
        const FVector crossProduct = FVector::CrossProduct(connectorNormal, otherConnection->GetConnectorNormal());
        auto isCollinear = crossProduct.IsNearlyZero(.01);
        if (!isCollinear)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tOther connection normal is not collinear with this connector normal! The parts are not aligned!"), );
            continue;
        }

        // For belt connectors, we do dot product things to ensure they are facing each other because conveyor lifts can be further away,
        // meaning they could also theoretically be on the wrong side (e.g. clipping through the factory). These have to be virtually touching
        // for us to link them so we just make double sure of that here.
        const FVector otherLocation = otherConnection->GetConnectorLocation();
        const float distanceSq = FVector::DistSquared(otherLocation, connectorLocation);

        if (distanceSq > 1) // Anything more than a 1 cm away is too far (and most are even much closer, based on my tests)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tConnection is too far away to be auto-linked!"), );
            continue;
        }

        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleRailroadConnection:\tFound one that's extremely close; taking it as the best result. Location: %s"), *otherLocation.ToString());
        otherConnection->AddConnection(connectionComponent);
        break;
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
#undef AL_LOG_DEBUG_TEXT
#undef LOCTEXT_NAMESPACE

IMPLEMENT_GAME_MODULE(FAutoLinkModule, AutoLink)