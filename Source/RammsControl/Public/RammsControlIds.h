// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The shared control vocabulary: well-known Ids and group names that
 * contributors advertise and input sources drive.
 *
 * This exists so an input source never has to know which controller class is
 * on the robot. Keyboard teleop asking for Drive::Forward works against the
 * differential drive today and a holonomic drive tomorrow, with no change in
 * the teleop, because both advertise the same Id. That is the same property
 * that lets the UI build itself from DescribeControlSurface.
 *
 * It lives in RammsControl rather than RammsCore because RammsControl owns the
 * control types and is already a dependency of everything that speaks them --
 * the controllers, the robot's surface component, and the input plugins.
 *
 * Functions rather than namespace-scope FName constants: an FName built during
 * static initialisation runs before the name table is reliably up, and these
 * are cheap enough that the difference never shows.
 *
 * Not every control can be a constant. Anything there can be more than one of
 * is named per instance (a robot may carry several 5-bar linkages, each
 * advertising "linkage.<component>.height"), so those are discovered from the
 * surface by Group and Kind instead. Group names for that are here too.
 */
namespace RammsControlIds
{
	/** Base motion, in the robot's local frame. A drive controller advertises the
	 *  subset it can actually do: differential drive has no Strafe. */
	namespace Drive
	{
		/** Continuous, normalized -1..1. Forward positive. */
		inline FName Forward()
		{
			return FName(TEXT("drive.forward"));
		}
		/** Continuous, normalized -1..1. Left positive (yaw rate). */
		inline FName Turn()
		{
			return FName(TEXT("drive.turn"));
		}
		/** Continuous, normalized -1..1. Left positive. Holonomic bases only. */
		inline FName Strafe()
		{
			return FName(TEXT("drive.strafe"));
		}
		/** Which drive mode is live, when a robot carries more than one. */
		inline FName Mode()
		{
			return FName(TEXT("drive.mode"));
		}
	} // namespace Drive

	/**
	 * A 5-bar linkage positions its endpoint in a plane, so it offers two controls
	 * per linkage rather than one. Ids are per instance -- a robot may carry
	 * several -- so they are built from the component name, and the suffixes are
	 * here so an input source can tell the two apart without rebuilding the Id.
	 */
	namespace Linkage
	{
		inline const TCHAR* HeightSuffix()
		{
			return TEXT(".height");
		}
		inline const TCHAR* TranslationSuffix()
		{
			return TEXT(".translation");
		}

		/** "linkage.<component>.height" -- endpoint height, cm. */
		inline FName Height(const FString& Component)
		{
			return FName(*FString::Printf(TEXT("linkage.%s%s"), *Component, HeightSuffix()));
		}

		/** "linkage.<component>.translation" -- endpoint fore/aft, cm. */
		inline FName Translation(const FString& Component)
		{
			return FName(*FString::Printf(TEXT("linkage.%s%s"), *Component, TranslationSuffix()));
		}

		/**
		 * Rate controls: normalised -1..1, integrated into the endpoint target
		 * while held, the way a drive stick works. They command a DELTA and say
		 * nothing about where the endpoint is -- the position controls above do
		 * that. Paired, so the UI renders them as a single joystick.
		 */
		inline const TCHAR* JogUpSuffix()
		{
			return TEXT(".jog_up");
		}
		inline const TCHAR* JogForwardSuffix()
		{
			return TEXT(".jog_forward");
		}

		inline FName JogUp(const FString& Component)
		{
			return FName(*FString::Printf(TEXT("linkage.%s%s"), *Component, JogUpSuffix()));
		}
		inline FName JogForward(const FString& Component)
		{
			return FName(*FString::Printf(TEXT("linkage.%s%s"), *Component, JogForwardSuffix()));
		}
	} // namespace Linkage

	/** Group names, for discovering per-instance controls off the surface. */
	namespace Groups
	{
		inline FName Drive()
		{
			return FName(TEXT("Drive"));
		}
		inline FName Linkage()
		{
			return FName(TEXT("Linkage"));
		}
		inline FName Motors()
		{
			return FName(TEXT("Motors"));
		}
	} // namespace Groups
} // namespace RammsControlIds
