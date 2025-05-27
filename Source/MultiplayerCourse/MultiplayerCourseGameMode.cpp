// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerCourseGameMode.h"
#include "MultiplayerCourseCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMultiplayerCourseGameMode::AMultiplayerCourseGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

void AMultiplayerCourseGameMode::HostLANGame()
{
	GetWorld()->ServerTravel("/Game/ThirdPerson/Maps/ThirdPersonMap?listen"); //our map path and load it
	//?listen means that it will start as a ListenServer
}

void AMultiplayerCourseGameMode::JoinLANGame()
{
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (PC) {
		PC->ClientTravel("10.0.0.18",TRAVEL_Absolute); //getting our local ipv4
	}
}
