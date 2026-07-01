# Blueprint Codex MCP

Free companion plugin for **Blueprint Codex** that exposes its asset exporters to Epic's in-editor Unreal MCP as AI-callable tools. With both plugins enabled, a connected AI agent can pull the readable, deterministic text of any supported asset on demand, without exporting files by hand.

Blueprint Codex MCP does no reading of its own. It registers Blueprint Codex's read tools inside Epic's Unreal MCP and routes every request through the main plugin's export pipeline. Enable it alongside Blueprint Codex when you want live AI access to your project in the editor.

## Requirements

- **Blueprint Codex** (the main plugin, sold separately). Blueprint Codex MCP does nothing without it; the tools still load but report that Blueprint Codex is needed.
- **Unreal Engine 5.8, Windows only.** Epic's Model Context Protocol plugin is not yet available on macOS or on 5.7.
- **Epic's Model Context Protocol and Toolset Registry plugins** (both Experimental in 5.8). Enabling Blueprint Codex MCP enables both for you.

Editor-only, and disabled by default.

## Installation

Install and enable **Blueprint Codex** (the main plugin) first. Then add Blueprint Codex MCP one of two ways, so that it lives at `<YourProject>/Plugins/BlueprintCodex_MCP`, and follow **Setup** below.

**Precompiled (no compiler needed).** Download the latest build from this repository's **Releases** page and copy the `BlueprintCodex_MCP` folder into your project's `Plugins` folder. Each release is built for a specific Unreal Engine 5.8 version; if your engine version differs, use the source option instead.

**From source.** Clone this repository (or download the code) into `<YourProject>/Plugins/BlueprintCodex_MCP`. With Visual Studio (or the Build Tools) installed, the editor compiles the plugin on first load.

## What it does

The plugin registers Blueprint Codex's read tools inside Epic's Unreal MCP, so a connected agent can discover your project and read any supported asset the way Blueprint Codex exports it: with real display names, only the values an asset actually authors, and its full structure. Coverage matches Blueprint Codex (30+ supported asset types and variants).

It surfaces detail that reflection-based reads leave out of reach, such as the keys, values, and tangents inside a curve, an animation state machine's states and transition conditions, and a MetaSound graph. This was verified live on Epic's Lyra sample.

## Tools

Each tool is an AICallable function surfaced through Epic's Unreal MCP:

- **GetProjectIndex**: list the project's exportable assets (path and type) under a content path, or across the whole project.
- **SearchAssets**: find exportable assets whose name matches a query.
- **GetAssetStructure**: the readable Structure of an asset (class info, variables, components, functions, references, and per-type sections).
- **GetAssetFlow**: the execution and dataflow tree of a Blueprint, Material, or supported graph, with display names.
- **GetAssetRaw**: the asset's native Unreal text (T3D) record, with Blueprint pin GUIDs normalised for deterministic diffs.
- **GetAssetJson**: the machine-readable JSON of an asset's Structure.
- **GetAssetFlowJson**: the machine-readable JSON of an asset's Flow graph (nodes and connections).
- **GetAssetSummary**: a condensed digest of an asset (identity, public surface, key references, counts).
- **GetAssetSummaryJson**: the JSON form of the Summary.
- **GetAssetSection**: only the requested section(s) of an asset's Structure, for when you need just part of it.
- **GetGraphFlow**: the Flow of a single named graph (one event, function, or macro) instead of the whole asset.

## Setup

1. Enable **Blueprint Codex**, then enable **Blueprint Codex MCP** (which switches on Epic's Model Context Protocol and Toolset Registry plugins). Restart the editor if prompted.
2. Start Epic's MCP server. It does not start automatically: turn on its **Auto Start Server** setting, or run the console command `ModelContextProtocol.StartServer`.
3. Connect your AI agent to the server (the connection is governed by your choice of tool).
4. The status light in the **Tools &rarr; Blueprint Codex** window goes live once an agent can reach your project.

## Status indicator

The Blueprint Codex window shows whether the MCP server is live, so you can tell at a glance if an agent can reach your project. A counter also shows a rough estimate of how much text Blueprint Codex has served to connected agents during the editor session (a cumulative estimate of about four characters per token, reset each time you reopen the editor).

## Read-only and local

Like Blueprint Codex itself, the companion only ever reads your assets. It never edits, resaves, or migrates anything. It returns only the specific asset text an agent requests, and that text travels over Epic's Model Context Protocol server to whichever AI service you connect, so that path is governed entirely by your choice of tool.

## Engine and platform

- Unreal Engine 5.8, Windows (Win64) only.
- Editor-only; not included in packaged builds.
- Disabled by default.

Blueprint Codex itself runs on Unreal Engine 5.7 and 5.8, on Windows and macOS. Only this MCP companion is limited to 5.8 and Windows, because Epic's Model Context Protocol plugin is not yet available elsewhere.
