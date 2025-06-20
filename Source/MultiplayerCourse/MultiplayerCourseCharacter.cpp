// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerCourseCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StaticMeshActor.h"
#include <Kismet/GameplayStatics.h>

//////////////////////////////////////////////////////////////////////////
// AMultiplayerCourseCharacter

AMultiplayerCourseCharacter::AMultiplayerCourseCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AMultiplayerCourseCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMultiplayerCourseCharacter::ClientRPCFunction_Implementation()
{
	if (ParticleSystem) {
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ParticleSystem, GetActorLocation(),
			FRotator::ZeroRotator, true, EPSCPoolMethod::AutoRelease);
	}
}

void AMultiplayerCourseCharacter::ServerRPCTest_Implementation(int intParam)
{
	if (HasAuthority()) {
#if 0 
		//#if is a preprocessor directive to deactive part of the code, you could use 0 to disable it and 1 to enable it
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("ServerRPCTest_Implementation"));
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("intParam: %d"), intParam));
#endif
		if (!SphereMesh) return;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this; //multiplayerCourseCharacter will be the owner of the StaticMesh
		AStaticMeshActor* StaticMeshActor = GetWorld()->SpawnActor< AStaticMeshActor>(SpawnParameters); //setting ower at spawn
		//Or we could set the owner after spawn it:
		//StaticMeshActor->SetOwner(this);
		if (StaticMeshActor)
		{
			StaticMeshActor->SetReplicates(true);
			StaticMeshActor->SetReplicateMovement(true);
			StaticMeshActor->SetMobility(EComponentMobility::Movable);
			FVector SpawnLocation = (GetActorLocation() + GetActorRotation().Vector() * 100.0f) + (GetActorUpVector() * 50.0f); //a vector 100 cm in front of the player
			StaticMeshActor->SetActorLocation(SpawnLocation);
			UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
			if (StaticMeshComponent)
			{
				StaticMeshComponent->SetIsReplicated(true);
				StaticMeshComponent->SetSimulatePhysics(true);
				StaticMeshComponent->SetStaticMesh(SphereMesh);
			}

		}
	}
}
//yes, it needs to return a bool and be named "_Validate"
//If returns true - validates; False - gives an invalid and it won't execute the RPC and it will disconnect the client!
bool AMultiplayerCourseCharacter::ServerRPCTest_Validate(int intParam) {
	return intParam >= 0 && intParam <= 100;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMultiplayerCourseCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {

		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMultiplayerCourseCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMultiplayerCourseCharacter::Look);

	}

}

void AMultiplayerCourseCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMultiplayerCourseCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}




