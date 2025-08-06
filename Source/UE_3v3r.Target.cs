// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE_3v3rTarget : TargetRules
{
	public UE_3v3rTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		//DefaultBuildSettings = BuildSettingsVersion.V2;
		//IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UE_3v3r");
	}
}
