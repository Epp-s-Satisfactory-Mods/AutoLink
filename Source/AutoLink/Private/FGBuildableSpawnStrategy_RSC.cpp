#include "FGBuildableSpawnStrategy_RSC.h"

#include "FGBuildableRailroadSwitchControl.h"
#include "FGPlayerController.h"

UFGBuildableSpawnStrategy_RSC::UFGBuildableSpawnStrategy_RSC()
    : mControlledConnections()
{
}

bool UFGBuildableSpawnStrategy_RSC::IsCompatibleWith(AFGBuildable* buildable) const
{
    if (!buildable) return false;

    return buildable->IsA(AFGBuildableRailroadSwitchControl::StaticClass());
}

void UFGBuildableSpawnStrategy_RSC::PreSpawnBuildable(AFGBuildable* buildable)
{
    auto switchControl = Cast<AFGBuildableRailroadSwitchControl>(buildable);
    fgcheckf(switchControl, TEXT("Buildable is not a switch control"));

    auto firstPlayerController = Cast<AFGPlayerController>(switchControl->GetWorld()->GetFirstPlayerController());

    switchControl->SetBuiltWithRecipe(this->mBuiltWithRecipe);

    for (auto connection : this->mControlledConnections)
    {
        switchControl->AddControlledConnection(connection);
    }

    UFGBuildableSpawnStrategy::PreSpawnBuildable(buildable);

    switchControl->PlayBuildEffects(firstPlayerController->GetControlledCharacter());
}
