// Copyright 2026 Studio Miley. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * Blueprint Codex — MCP sub-plugin (UE 5.8+). Hosts UBlueprintCodexToolset, a UToolsetDefinition
 * whose AICallable static functions expose the main plugin's exporters to Epic's in-editor Unreal
 * MCP. The Toolset Registry does NOT auto-discover toolsets, so StartupModule explicitly calls
 * UToolsetRegistry::RegisterToolsetClass (ShutdownModule unregisters) — see the .cpp for the
 * loading-phase rationale. StartupModule also feeds the main plugin's live/off MCP status light
 * via FBlueprintCodexMcpStatus.
 */
class FBlueprintCodex_MCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
