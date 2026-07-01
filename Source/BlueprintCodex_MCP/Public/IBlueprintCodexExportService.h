// Copyright 2026 Studio Miley. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"

struct FAssetData;

/**
 * Cross-plugin export-service contract between the main Blueprint Codex plugin and the optional
 * Blueprint Codex - MCP companion. The main plugin implements this and registers it as a modular
 * feature under FeatureName(); the MCP companion acquires it at runtime via Get() and routes every
 * export call through it.
 *
 * NEITHER plugin compile-depends on the other: each ships its own identical copy of this header and
 * they rendezvous through the engine's IModularFeatures registry by name. That is what lets the MCP
 * companion build and ship standalone (Epic compiles each Fab plugin in isolation) yet still drive
 * the full Codex export pipeline once both plugins are installed and enabled. With the main plugin
 * absent or disabled, Get() returns nullptr and the MCP tools report a clear "enable Blueprint Codex"
 * message instead of failing to load.
 *
 * IMPORTANT: the two copies of this header (Plugins/BlueprintCodex/.../Public and
 * Plugins/BlueprintCodex_MCP/.../Public) MUST stay ABI-identical - same methods, same order, same
 * signatures - because each side compiles against its own copy and they meet through a vtable at
 * runtime. Format and Mode are passed as int32 (NOT the main plugin's enums) so the contract carries
 * no main-plugin types:
 *   OutputFormat: Text=0, Markdown=1
 *                 (mirrors EBlueprintCodexOutputFormat)
 *   ExportMode:   Structure=0, Flow=1, Raw=2, Json=3, Summary=4, FlowJson=5, SummaryJson=6
 *                 (mirrors FBlueprintExporter::EBlueprintCodexExportMode)
 */
class IBlueprintCodexExportService : public IModularFeature
{
public:
	virtual ~IBlueprintCodexExportService() = default;

	/** Modular-feature name both plugins agree on. */
	static FName FeatureName()
	{
		static const FName Name(TEXT("BlueprintCodexExportService"));
		return Name;
	}

	/** The registered service, or nullptr if the main Blueprint Codex plugin is absent or disabled. */
	static IBlueprintCodexExportService* Get()
	{
		IModularFeatures& Features = IModularFeatures::Get();
		const FName Name = FeatureName();
		if (Features.GetModularFeatureImplementationCount(Name) > 0)
		{
			return static_cast<IBlueprintCodexExportService*>(Features.GetModularFeatureImplementation(Name, 0));
		}
		return nullptr;
	}

	/** Build a view's text for an asset. Returns true and fills OutContent on success; returns false
	 *  with OutContent set to a human-readable "not available for this type" message on an
	 *  unsupported (type x mode). OutputFormat / ExportMode are the int32 mirrors documented above. */
	virtual bool BuildAssetText(UObject* Asset, int32 OutputFormat, int32 ExportMode, FString& OutContent) = 0;

	/** Is this asset one Blueprint Codex can export (the discovery predicate)? */
	virtual bool IsExportableAsset(const FAssetData& Asset) = 0;

	/** Tally served ~tokens into the main module's session counter (drives the window status light). */
	virtual void AddTokensServed(int64 EstimatedTokens) = 0;

	/** Install (or clear, with nullptr) the MCP liveness predicate the main window reads. A plain
	 *  captureless function pointer, so the main module never dispatches a destructor thunk into an
	 *  already-unloaded MCP DLL at process exit. */
	virtual void SetMcpStatusProvider(bool (*Predicate)()) = 0;
};
