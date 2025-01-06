#include "AutoLink.h"

// The mod template does this but we have no text to localize
#define LOCTEXT_NAMESPACE "FAutoLinkModule"

void FAutoLinkModule::StartupModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_GAME_MODULE(FAutoLinkModule, AutoLink)