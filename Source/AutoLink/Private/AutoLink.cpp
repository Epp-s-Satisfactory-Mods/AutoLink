#include "AutoLink.h"

#include "AbstractInstanceManager.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildableConveyorLift.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionFactory.h"
#include "FGPipeSubsystem.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "InstanceData.h"
#include "Patching/NativeHookManager.h"

DEFINE_LOG_CATEGORY(LogAutoLink)

// The mod template does this but we have no text to localiz
#define LOCTEXT_NAMESPACE "FAutoLinkModule"

// Set this to 1 before building to actually log debug messages, 0 to turn them into no-ops at compile time
// I made this compile-time because I'm lazy, don't want to mess with log levels, and would prefer the shipping mod minimize perf impact.
#define LOG_DEBUG_INFO 0

#if LOG_DEBUG_INFO
#define AL_LOG(Verbosity, Format, ...) \
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

    AL_LOG(Verbose, TEXT("StartupModule: Hooking ConfigureComponents..."));

    auto defaultBuildableHologram = GetMutableDefault<AFGBuildableHologram>();
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableHologram::ConfigureComponents, defaultBuildableHologram, FAutoLinkModule::ConfigureComponentsHook);
}

void FAutoLinkModule::ConfigureComponentsHook(const AFGBuildableHologram* buildableHologram, AFGBuildable* buildable)
{
    AL_LOG(Verbose, TEXT("ConfigureComponentsHook: The hologram is %s and buildable is %s at %s"), *buildableHologram->GetName(), *buildable->GetName(), *buildable->GetActorLocation().ToString() );

    if (auto buildableConveyorAttachment = Cast<AFGBuildableConveyorAttachment>(buildable))
    {
        AL_LOG(Verbose, TEXT("ConfigureComponentsHook: It's a buildable conveyor attachment."));
        TInlineComponentArray<UFGFactoryConnectionComponent*> connectionComponents;
        buildableConveyorAttachment->GetComponents(connectionComponents);
        for (auto connectionComponent : connectionComponents)
        {
            AL_LOG(Verbose, TEXT("ConfigureComponentsHook: Examining input connection component: %s"), *connectionComponent->GetName());
            FAutoLinkModule::FindAndLinkCompatibleBeltConnection(connectionComponent);
        }
    }
    else if (auto buildableFactory = Cast<AFGBuildableFactory>(buildable))
    {
        AL_LOG(Verbose, TEXT("ConfigureComponentsHook: It's a buildable factory."));

        for (auto component : buildableFactory->GetComponents())
        {
            AL_LOG(Verbose, TEXT("ConfigureComponentsHook: Component: %s of type %s"), *component->GetName(), *component->GetClass()->GetFullName());

            if (auto connectionComponent = Cast<UFGFactoryConnectionComponent>(component))
            {
                AL_LOG(Verbose, TEXT("\tConfigureComponentsHook: It's a UFGFactoryConnectionComponent"));
                FAutoLinkModule::FindAndLinkCompatibleBeltConnection(connectionComponent);
            }
            else if (auto fluidIntegrant = Cast<IFGFluidIntegrantInterface>(component))
            {
                AL_LOG(Verbose, TEXT("\tConfigureComponentsHook: It's a IFGFluidIntegrantInterface."));
                for (auto pipeConnection : fluidIntegrant->GetPipeConnections())
                {
                    AL_LOG(Verbose, TEXT("\t\tConfigureComponentsHook: Has pipe connection: %s"),*pipeConnection->GetName());
                    FAutoLinkModule::FindAndLinkCompatiblePipeConnection(pipeConnection, fluidIntegrant );
                }
            }
        }
    }
}

void FAutoLinkModule::FindAndLinkCompatibleBeltConnection( UFGFactoryConnectionComponent* connectionComponent)
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
    auto world = connectionComponent->GetWorld();

    AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: connector at: %s, searchEnd is at: %s, connectorLocation+normal: %s"), *connectorLocation.ToString(), *searchEnd.ToString(), *(connectorLocation + connectionComponent->GetConnectorNormal()).ToString());

    TArray< FHitResult > hitResults;
    world->LineTraceMultiByObjectType(
        hitResults,
        connectorLocation,
        searchEnd,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects));

    TArray< FactoryConnectionCandidate > candidates;
    for (const FHitResult& result : hitResults)
    {
        AActor* actor = result.GetActor();
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

        // We only find and register conveyors as possible candidates, but conveyor belts and conveyor lifts are each a little different
        if (auto buildableConveyor = Cast<AFGBuildableConveyorBase>(actor))
        {
            const float CONVEYOR_LIFT_MIN_DISTANCE = 100.f;
            auto minConnectorDistance = 0.0f; // Conveyor lifts have some natural clearance
            auto maxConnectorDistance = 0.0f; // Conveyor belts have to be touching the connector; if it's a conveyor lift, we'll change this accordingly in a bit
            auto conveyorLift = Cast<AFGBuildableConveyorLift>(actor);

            switch (connectionDirection)
            {
            case EFactoryConnectionDirection::FCD_INPUT:
                if (conveyorLift)
                {
                    minConnectorDistance = CONVEYOR_LIFT_MIN_DISTANCE;
                    maxConnectorDistance =
                        buildableConveyor->mConnection1->GetConnectorClearance() + // mConnection1 is always the output connection
                        conveyorLift->mOpposingConnectionClearance[1]; // If the conveyor bellows extended to snap on construction, the clearance through which they snapped will be here
                }
                candidates.Add( FactoryConnectionCandidate(buildableConveyor->mConnection1, minConnectorDistance, maxConnectorDistance) );
                break;
            case EFactoryConnectionDirection::FCD_OUTPUT:
                if (conveyorLift)
                {
                    minConnectorDistance = CONVEYOR_LIFT_MIN_DISTANCE;
                    maxConnectorDistance =
                        buildableConveyor->mConnection0->GetConnectorClearance() + // mConnection0 is always the input connection
                        conveyorLift->mOpposingConnectionClearance[0]; // If the conveyor bellows extended to snap on construction, the clearance through which they snapped will be here
                }
                candidates.Add( FactoryConnectionCandidate(buildableConveyor->mConnection0, minConnectorDistance, maxConnectorDistance) );
                break;
            }
        }
    }

    float closestDistanceSq = FLT_MAX;
    UFGFactoryConnectionComponent* compatibleConnectionComponent = nullptr;
    for (auto& candidate : candidates)
    {
        auto otherConnection = candidate.ConnectionComponent;
        auto minConnectorDistance = candidate.MinConnectorDistance;
        auto maxConnectorDistance = candidate.MaxConnectorDistance;

        AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection: Examining connection: %s (%s). Max distance: %f"), *otherConnection->GetName(), *otherConnection->GetClass()->GetName(), maxConnectorDistance);

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

        // If we're within 1 cm, then we can just take this one as the best one. There could be multiple this close but that's extremely rare and just choosing one seems fine.
        // This makes sense and prevents the dot product math below, plus avoids some cases where things are barely overlapping and giving false dot product negatives.
        if (distanceSq < 1)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tFound one that's so close, we'll just take it as the final result: %s"), *otherLocation.ToCompactString());
            closestDistanceSq = distanceSq;
            compatibleConnectionComponent = otherConnection;
            break;
        }

        // Determine if both connectors are in front of each other (meaning they're facing each other).
        // If we don't do this check, the connectors might be facing away or physically past each other.
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
            AL_LOG(Verbose, TEXT("FindAndLinkCompatibleBeltConnection:\tFound a new closest one at: %s"), *otherLocation.ToCompactString());
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

void FAutoLinkModule::FindAndLinkCompatiblePipeConnection(UFGPipeConnectionComponent* connectionComponent, IFGFluidIntegrantInterface* owningFluidIntegrant)
{
    if (connectionComponent->IsConnected())
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection: Exiting because the connection component is already connected"));
        return;
    }

    auto connectorLocation = connectionComponent->GetConnectorLocation();
    auto searchEnd = connectorLocation + connectionComponent->GetConnectorNormal();
    auto world = connectionComponent->GetWorld();

    AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection: connector at: %s, searchEnd is at: %s, connectorLocation+normal: %s"), *connectorLocation.ToString(), *searchEnd.ToString(), *(connectorLocation + connectionComponent->GetConnectorNormal()).ToString());

    TArray< FHitResult > hitResults;
    world->LineTraceMultiByObjectType(
        hitResults,
        connectorLocation,
        searchEnd,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects));

    TArray< UFGPipeConnectionComponent* > candidates;
    for (const FHitResult& result : hitResults)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection: Examining result: %s (%s)"), *result.GetActor()->GetName(), *result.GetActor()->GetClass()->GetName());

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

        if (auto fluidInterface = Cast<IFGFluidIntegrantInterface>(actor))
        {
            candidates.Append(fluidInterface->GetPipeConnections());
        }
    }

    float closestDistanceSq = FLT_MAX;
    UFGPipeConnectionComponent* compatibleConnectionComponent = nullptr;
    for (auto otherConnection : candidates)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection:Examining connection: %s (%s). Connection type %d"), *otherConnection->GetName(), *otherConnection->GetClass()->GetName(), otherConnection->GetPipeConnectionType());

        if (!IsValid(otherConnection))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection:\tNot valid!"));
            continue;
        }

        if (otherConnection->IsConnected())
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection:\tAlready connected!"));
            continue;
        }

        if (!otherConnection->CanConnectTo(connectionComponent))
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection:\tCannot be connected to this!"));
            continue;
        }

        const FVector connectorNormal = connectionComponent->GetConnectorNormal();

        // Determine if the connections components are aligned
        const FVector crossProduct = FVector::CrossProduct(connectorNormal, otherConnection->GetConnectorNormal());
        auto isCollinear = crossProduct.IsNearlyZero(.01);
        if (!isCollinear)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection:\tOther connection normal is not collinear with this connector normal! The parts are not aligned!"), );
            continue;
        }

        // For belt connectors, we do dot product things to ensure they are facing each other because conveyor lifts can be further away,
        // meaning they could also theoretically be on the wrong side (e.g. clipping through the factory). Pipes have to be virtually touching
        // for us to link them so we just make double sure of that here.

        const FVector otherLocation = otherConnection->GetConnectorLocation();
        const float distanceSq = FVector::DistSquared(otherLocation, connectorLocation);
        if (distanceSq > 1)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection:\tPipe is too far away to be auto-linked!"), );
            continue;
        }

        if (distanceSq < closestDistanceSq)
        {
            AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection:\tFound a new closest one at: %s"), *otherLocation.ToCompactString());
            closestDistanceSq = distanceSq;
            compatibleConnectionComponent = otherConnection;
        }
    }

    if (!compatibleConnectionComponent)
    {
        AL_LOG(Verbose, TEXT("FindAndLinkCompatiblePipeConnection: No compatible overlapping connection found"));
        return;
    }

    compatibleConnectionComponent->SetConnection(connectionComponent);
    auto pipeSubsystem = AFGPipeSubsystem::GetPipeSubsystem(world);
    pipeSubsystem->RegisterFluidIntegrant(owningFluidIntegrant);
}

#undef AL_LOG
#undef LOCTEXT_NAMESPACE

IMPLEMENT_GAME_MODULE(FAutoLinkModule, AutoLink)