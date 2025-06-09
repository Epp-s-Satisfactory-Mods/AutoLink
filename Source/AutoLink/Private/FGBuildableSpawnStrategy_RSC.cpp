#include "FGBuildableSpawnStrategy_RSC.h"

#include "FGBuildableRailroadSwitchControl.h"
#include "FGPlayerController.h"

UFGBuildableSpawnStrategy_RSC::UFGBuildableSpawnStrategy_RSC()
    : mControlledConnection(nullptr)
{
}

bool UFGBuildableSpawnStrategy_RSC::IsCompatibleWith(AFGBuildable* buildable) const
{
    if (!buildable) return false;

    return buildable->IsA(AFGBuildableRailroadSwitchControl::StaticClass());
}

void UFGBuildableSpawnStrategy_RSC::PreSpawnBuildable(AFGBuildable* buildable)
{
    fgcheckf(IsValid(this->mControlledConnection), TEXT("Target railroad connection is not valid"));
    fgcheckf(!this->mControlledConnection->GetSwitchControl(), TEXT("Target railroad connection already has a switch control"));

    auto switchControl = Cast<AFGBuildableRailroadSwitchControl>(buildable);
    fgcheckf(switchControl, TEXT("Buildable is not a switch control"));

    auto firstPlayerController = Cast<AFGPlayerController>(switchControl->GetWorld()->GetFirstPlayerController());
    //switchControl->mBuildEffectInstignator = firstPlayerController->GetControlledCharacter();

    //switchControl->bForceLegacyBuildEffect = true;
    switchControl->SetBuiltWithRecipe(this->mControlledConnection->GetTrack()->GetBuiltWithRecipe());
    switchControl->AddControlledConnection(this->mControlledConnection);
    this->mControlledConnection->SetSwitchControl(switchControl);

    UFGBuildableSpawnStrategy::PreSpawnBuildable(buildable);

    switchControl->PlayBuildEffects(firstPlayerController->GetControlledCharacter());


}
