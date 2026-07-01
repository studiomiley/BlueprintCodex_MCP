// Copyright 2026 Studio Miley. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "BlueprintCodexToolset.generated.h"

/**
 * Blueprint Codex MCP toolset (UE 5.8+). Each AICallable static function is surfaced to an MCP
 * client by Epic's Unreal MCP server (via the Toolset Registry), letting an AI agent pull a project
 * index and the readable Structure / Flow / Raw / JSON text of ANY supported asset on demand: the
 * same output the file export and clipboard produce, reusing FBlueprintExporter::BuildAssetText
 * under the hood (no new export logic).
 *
 * Supported asset types match the Blueprint Codex export pipeline: Blueprints (incl. Widget / Anim
 * / Interface / Function+Macro Library / Control Rig), Behavior Trees, Enums, Data & Curve Tables,
 * Data Assets, the Material family (Material / Instance / Function / Function Instance / Parameter
 * Collection), Environment Queries (EQS), Sound Cues, the Anim/Sound/Curve families, and (when the
 * optional sub-plugins are installed) State Tree, PCG, Niagara, and MetaSound assets.
 *
 * Tools return plain FString (text or newline-separated listings); the doc comment on each function
 * becomes the tool's description in the generated MCP schema.
 */
UCLASS(BlueprintType, Hidden)
class UBlueprintCodexToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Lists the project's exportable assets, one "path  (type)" per line. The trailing type label
	 *  (e.g. Blueprint, DataTable, Material, SoundCue) tells you which output modes apply. Pass an
	 *  EMPTY PathFilter to scan ALL project content: /Game plus enabled project / Game-Feature
	 *  plugin content roots (e.g. /ShooterCore, /ShooterExplorer). Or pass a specific root/subpath
	 *  (e.g. /Game/Characters or /ShooterCore) to scope to it. Call this (or SearchAssets) FIRST to
	 *  discover what assets exist and their types before requesting their Summary / Structure / Flow /
	 *  Raw / JSON. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetProjectIndex(const FString& PathFilter, int32 Limit = 500);

	/** Searches ALL project content (/Game plus enabled project / Game-Feature plugin content roots,
	 *  e.g. /ShooterCore) for exportable assets whose name contains Query (case-insensitive), and
	 *  returns matching "path  (type)" lines. Use to locate an asset (and learn its type) before
	 *  calling GetAssetSummary / GetAssetStructure / GetAssetFlow / GetAssetRaw / GetAssetJson. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString SearchAssets(const FString& Query, int32 Limit = 50);

	/** Returns the readable Structure text of the asset at the given path. Structure works for EVERY
	 *  supported asset type: for Blueprints it lists variables, components, functions, events,
	 *  interfaces, references and the native C++ parent; for other types it lists the type-specific
	 *  authored data (table rows, material parameters/graph, EQS options, sound-cue node tree, etc.).
	 *  This is the primary "describe this asset" tool. AssetPath is an Unreal object path such as
	 *  /Game/Path/Foo.Foo (or /Game/Path/Foo). */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetStructure(const FString& AssetPath);

	/** Returns the execution / dataflow Flow text of the asset: an indented tree with display
	 *  names. Available for Blueprints (event graphs, functions, macros), Materials & Material
	 *  Functions (expression graph), and (when their sub-plugins are installed) Niagara scripts
	 *  and PCG / MetaSound graphs. Other asset types have no flow graph; for those this returns a
	 *  short "not available for this type" message (use GetAssetStructure instead). */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetFlow(const FString& AssetPath);

	/** Returns the Raw text of the asset: its complete native Unreal T3D record (every node, pin and property), made deterministic for Blueprints by normalising pin GUIDs, so it is complete in content but a deterministically rewritten near-verbatim record rather than a byte-for-byte copy of the editor's own copy/paste output. Available for Blueprints,
	 *  Materials & Material Functions, and (when their sub-plugins are installed) Niagara and
	 *  MetaSound assets. Other asset types return a short "not available for this type" message. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetRaw(const FString& AssetPath);

	/** Returns the machine-readable JSON serialization of the asset's STRUCTURE (variables,
	 *  components, functions, references, class defaults, …). Available for Blueprints, Data Tables,
	 *  Curve Tables, Environment Queries (EQS), Sound Cues, Materials and the sub-plugin types. For
	 *  the execution-graph JSON use GetAssetFlowJson instead. Other asset types return a short
	 *  "not available for this type" message. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetJson(const FString& AssetPath);

	/** Returns the machine-readable JSON of the asset's execution / dataflow Flow graph: a
	 *  {nodes, connections} topology (node ids / classes / titles / pins + typed exec & data edges),
	 *  the machine companion to GetAssetFlow's readable tree. Available for Blueprints, Materials and
	 *  Material Functions (the types with a flow-JSON builder). Other asset types return a short
	 *  "not available for this type" message (use GetAssetJson for their structure JSON). */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetFlowJson(const FString& AssetPath);

	/** Returns a condensed, high-signal digest of the asset: what it is, its immediate + native
	 *  parent class, the public surface (events / functions / variables / dispatchers), key asset
	 *  references and headline counts. Meant to be read INSTEAD of the full Structure when you only
	 *  need to know what an asset is and how to use it. Works for EVERY asset type (full digest for
	 *  Blueprints, a generic identity+references core for others). This is the cheapest "what is this
	 *  asset" tool; reach for it before GetAssetStructure when you just need orientation. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetSummary(const FString& AssetPath);

	/** Returns the machine-readable JSON form of GetAssetSummary's digest: the same fields
	 *  (identity, parent, interfaces, flags, public surface, key references, counts) as structured
	 *  data. Works for every asset type. Use when a tool/agent wants the digest as data rather than
	 *  prose. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetSummaryJson(const FString& AssetPath);

	/** Returns ONLY the requested section(s) of an asset's Structure: a surgical, low-token
	 *  alternative to GetAssetStructure for when you need just part of it (e.g. just the variables,
	 *  or just the function signatures), so you don't pull the whole asset to answer a narrow
	 *  question. Sections is a comma-separated list of section names, matched case-insensitively as
	 *  substrings against the "// --- <Name> ---" headers Structure emits, e.g. "Variables",
	 *  "Functions", "Components", "Implemented Interfaces", "Asset References", "Events". Pass an
	 *  EMPTY string to get just the asset's section OUTLINE (the list of available section names with
	 *  line counts), the cheapest way to see what an asset contains before fetching a section. Works
	 *  for every asset type that has a Structure view. AssetPath is an Unreal object path as in
	 *  GetAssetStructure. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetAssetSection(const FString& AssetPath, const FString& Sections);

	/** Returns the Flow (execution / dataflow tree) of a SINGLE named graph (one event, function or
	 *  macro) instead of the whole asset's Flow. Use it after GetAssetStructure / GetAssetSection has
	 *  told you the event/function names, to pull just the one you care about at a fraction of the
	 *  tokens of the full GetAssetFlow. GraphName is matched case-insensitively (substring) against the
	 *  entry titles, e.g. "Update Jetpack", "Tick", "CheckForSurfaceOrVelocity". If nothing matches,
	 *  the available entry titles are listed so you can retry. AssetPath is an Unreal object path as in
	 *  GetAssetFlow. */
	UFUNCTION(meta = (AICallable), Category = "Blueprint Codex")
	static FString GetGraphFlow(const FString& AssetPath, const FString& GraphName);
};
