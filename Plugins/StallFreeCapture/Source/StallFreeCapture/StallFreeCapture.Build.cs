// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.IO;
using UnrealBuildTool;

public class StallFreeCapture : ModuleRules
{
	public StallFreeCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Projects",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		string SfcCoreDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "sfc_core"));
		PublicSystemIncludePaths.Add(Path.Combine(SfcCoreDir, "include"));

		// Unreal Development/Shipping use the release CRT. Only the Debug configuration uses the debug library.
		string Preset = Target.Configuration == UnrealTargetConfiguration.Debug ? "debug" : "release";
		string LibFileName = Target.Platform == UnrealTargetPlatform.Win64 ? "sfc_core.lib" : "libsfc_core.a";
		string BuildDir = Path.Combine(SfcCoreDir, "build", Preset);

		if (!Directory.Exists(BuildDir))
		{
			RunCMake(SfcCoreDir, "cmake", "--preset", Preset);
		}

		RunCMake(SfcCoreDir, "cmake", "--build", "--preset", Preset);

		string LibPath = FindLibrary(BuildDir, LibFileName);
		if (LibPath == null)
		{
			throw new System.InvalidOperationException(
				"sfc_core library was not produced in " + BuildDir + ". From ThirdParty/sfc_core, run: cmake --preset " + Preset + " && cmake --build --preset " + Preset);
		}

		PublicAdditionalLibraries.Add(LibPath);
	}

	private static string FindLibrary(string BuildDir, string LibFileName)
	{
		if (!Directory.Exists(BuildDir))
		{
			return null;
		}

		foreach (string Candidate in Directory.GetFiles(BuildDir, LibFileName, SearchOption.AllDirectories))
		{
			if (Candidate.Contains("_deps"))
			{
				continue;
			}
			return Candidate;
		}

		return null;
	}

	private static void RunCMake(string WorkingDirectory, params string[] Arguments)
	{
		ProcessStartInfo StartInfo = new ProcessStartInfo
		{
			FileName = Arguments[0],
			WorkingDirectory = WorkingDirectory,
			UseShellExecute = false,
		};
		for (int Index = 1; Index < Arguments.Length; Index++)
		{
			StartInfo.ArgumentList.Add(Arguments[Index]);
		}

		using (Process Process = Process.Start(StartInfo))
		{
			if (Process == null || !Process.WaitForExit(600000) || Process.ExitCode != 0)
			{
				throw new System.InvalidOperationException("Failed to build sfc_core with: " + string.Join(" ", Arguments));
			}
		}
	}
}
