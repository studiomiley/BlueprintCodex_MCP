// Copyright 2026 Studio Miley. All Rights Reserved.

#include "BlueprintCodex_MCP.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "BlueprintCodexToolset.h"

// Status seam: we push the live/off predicate the main Codex window reads THROUGH the shared runtime
// export service (IBlueprintCodexExportService) rather than calling the main module directly - this
// module has NO compile dependency on the main plugin (see IBlueprintCodexExportService.h). Plus the
// Unreal MCP server interface, so "live" reflects the HTTP server actually running, not just
// registration.
#include "IBlueprintCodexExportService.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintCodexMCP, Log, All);

#define LOCTEXT_NAMESPACE "FBlueprintCodex_MCPModule"

// The UE 5.8 Toolset Registry does NOT auto-discover toolsets — each toolset module must EXPLICITLY
// register its UToolsetDefinition subclass via UToolsetRegistry::RegisterToolsetClass (verified
// against the engine: Epic's own EditorToolset module does exactly this). Registration must run
// after the registry subsystem is initialised, so this module loads at PostEngineInit (see
// BlueprintCodex_MCP.uplugin) — matching EditorToolset; registering from the earlier Default phase
// silently no-ops ("subsystem unavailable").
void FBlueprintCodex_MCPModule::StartupModule()
{
	// ToolsetRegistry + ModelContextProtocol are Experimental engine plugins this companion enables as
	// dependencies. The .Build.cs still delay-loads their DLLs (Win64) so this module's DLL loads even
	// if they're somehow absent — bail here, before touching any of their symbols, so we stay inert
	// instead of crashing on an unresolved delay-load thunk.
	if (!FModuleManager::Get().LoadModule(TEXT("ToolsetRegistry")) ||
		!FModuleManager::Get().LoadModule(TEXT("ModelContextProtocol")))
	{
		return;
	}

	UToolsetRegistry::RegisterToolsetClass(UBlueprintCodexToolset::StaticClass());

	// Feed the main plugin's header status light THROUGH the runtime export service (acquired by name;
	// null if the main Blueprint Codex plugin isn't installed/enabled, in which case there is no window
	// to light and we simply skip). "Live" is the honest predicate: our toolset is registered with the
	// Toolset Registry AND the Unreal MCP HTTP server is actually running, so a connected MCP client
	// could reach our tools right now. Registered-but-server-stopped reads off. Evaluated lazily on each
	// query (the window's paint path), so it tracks server start/stop live. The predicate is captureless,
	// so it binds to the plain bool(*)() the service expects.
	if (IBlueprintCodexExportService* ExportService = IBlueprintCodexExportService::Get())
	{
		ExportService->SetMcpStatusProvider([]() -> bool
		{
			if (!UToolsetRegistry::IsToolsetClassRegistered(UBlueprintCodexToolset::StaticClass()))
			{
				return false;
			}
			if (IModelContextProtocolModule* Mcp = IModelContextProtocolModule::Get())
			{
				if (FModelContextProtocolServer* Server = Mcp->GetServer())
				{
					return Server->IsServerRunning();
				}
			}
			return false;
		});
		UE_LOG(LogBlueprintCodexMCP, Display,
			TEXT("Connected to the Blueprint Codex export service; MCP toolset registered."));
	}
	else
	{
		UE_LOG(LogBlueprintCodexMCP, Warning,
			TEXT("Blueprint Codex plugin not found. The MCP toolset is registered, but its tools will report 'enable Blueprint Codex' until that plugin is enabled."));
	}
}

void FBlueprintCodex_MCPModule::ShutdownModule()
{
	// Clear the seam first so the main module never calls a predicate that closes over this
	// (now-unloading) module's symbols. Routed through the runtime export service; if the main plugin is
	// gone there is nothing to clear. This touches only the service + our own symbols, so it is always
	// safe — even when the optional engine MCP plugins were never present.
	if (IBlueprintCodexExportService* ExportService = IBlueprintCodexExportService::Get())
	{
		ExportService->SetMcpStatusProvider(nullptr);
	}

	// Unregister only if ToolsetRegistry is still loaded: it is an Optional dependency (may have been
	// absent all along, in which case StartupModule bailed and we never registered), and at shutdown
	// engine plugins unload before project plugins. Touching it otherwise hits unresolved delay-loaded
	// symbols.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("ToolsetRegistry")))
	{
		UToolsetRegistry::UnregisterToolsetClass(UBlueprintCodexToolset::StaticClass());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintCodex_MCPModule, BlueprintCodex_MCP)
