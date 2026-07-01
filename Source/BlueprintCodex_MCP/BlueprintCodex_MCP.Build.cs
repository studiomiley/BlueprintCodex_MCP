// Copyright 2026 Studio Miley. All Rights Reserved.
using UnrealBuildTool;

public class BlueprintCodex_MCP : ModuleRules
{
	public BlueprintCodex_MCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"UnrealEd",
			// NOTE: NO dependency on the main "BlueprintCodex" module. This companion must compile and
			// ship standalone (Epic builds each Fab plugin in isolation), so it cannot link the main
			// plugin's code. It acquires the main plugin's exporter at RUNTIME via the shared
			// IBlueprintCodexExportService modular feature (see IBlueprintCodexExportService.h) and routes
			// every export / discovery call through it - connecting only when both plugins are enabled.
			// Asset discovery for GetProjectIndex / SearchAssets.
			"AssetRegistry",
			// IPluginManager — enumerate enabled project / Game-Feature plugin content roots so
			// discovery scans beyond /Game (game features like /ShooterCore live in plugin roots).
			"Projects",
			// UE 5.8 Toolset Registry — supplies the UToolsetDefinition base class + the AICallable
			// tool framework that Epic's Unreal MCP server exposes. This whole plugin is 5.8-only
			// (ToolsetRegistry doesn't exist on 5.7), so it ships only in the 5.8 package and is not
			// enabled in 5.7 builds — no version guards are needed inside the module.
			"ToolsetRegistry",
			// The Unreal MCP server interface. The footer status light queries
			// IModelContextProtocolModule::Get()->GetServer()->IsServerRunning() so it reports "live"
			// only when the HTTP server is actually serving — not merely when our toolset is
			// registered. 5.8-only, like the rest of this module.
			"ModelContextProtocol",
		});

		// v1.2 consolidation: in the single-plugin build, the engine MCP plugins (ToolsetRegistry +
		// ModelContextProtocol) are Experimental and disabled by default, so the consolidated Codex
		// .uplugin marks them Optional rather than force-enabling them on every user. Delay-load their
		// DLLs (Win64-only directive — this whole module is Win64/5.8-only) so THIS module's DLL still
		// loads when they're absent; the StartupModule probe then skips registration and we stay inert.
		// Unlike Niagara/MetaSound/PCG (which import exported DATA symbols and so cannot be delay-loaded
		// on 5.8 — LNK1194), this module takes only FUNCTION imports from those modules
		// (UToolsetRegistry::Register/Unregister/IsRegistered, IModelContextProtocolModule::Get), which
		// the delay-load thunk mechanism can defer. No-op when the engine MCP plugins are enabled.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDelayLoadDLLs.AddRange(new string[]
			{
				"UnrealEditor-ToolsetRegistry.dll",
				"UnrealEditor-ModelContextProtocol.dll",
			});
		}
	}
}
