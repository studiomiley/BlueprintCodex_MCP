// Copyright 2026 Studio Miley. All Rights Reserved.

#include "BlueprintCodexToolset.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(BlueprintCodexToolset)

// This module compiles and ships STANDALONE — it has NO dependency on the main BlueprintCodex module
// (Epic builds each Fab plugin in isolation). It reaches the main plugin's exporter at RUNTIME through
// the shared IBlueprintCodexExportService modular feature; see IBlueprintCodexExportService.h. Format
// and Mode are passed as the int32 mirrors declared there (no main-plugin enum types here).
#include "IBlueprintCodexExportService.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "Interfaces/IPluginManager.h"  // enumerate project / Game-Feature plugin content roots

namespace
{
	// Mirror of FBlueprintExporter::EBlueprintCodexExportMode + EBlueprintCodexOutputFormat as plain
	// ints, so this module carries no main-plugin enum types. The values MUST match the main plugin's
	// enums and the int32 contract documented in IBlueprintCodexExportService.h.
	enum : int32 { ModeStructure = 0, ModeFlow = 1, ModeRaw = 2, ModeJson = 3, ModeSummary = 4, ModeFlowJson = 5, ModeSummaryJson = 6 };
	constexpr int32 FormatText = 0;  // EBlueprintCodexOutputFormat::Text

	// Returned by every tool when the main Blueprint Codex plugin isn't installed/enabled (service null).
	const TCHAR* MainPluginUnavailableMessage()
	{
		return TEXT("ERROR: Blueprint Codex is not available. Enable the Blueprint Codex plugin: this MCP companion routes through its exporters and cannot run without it.");
	}

	// Resolve any UObject asset from either a full object path (/Game/X/Foo.Foo) or a bare package
	// path (/Game/X/Foo) — MCP clients commonly pass the latter. Loads UObject (not UBlueprint) so
	// the whole supported-type surface (Materials, Tables, Sound Cues, sub-plugin types, …) resolves.
	UObject* LoadAssetByPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty()) { return nullptr; }
		if (UObject* Obj = LoadObject<UObject>(nullptr, *AssetPath))
		{
			return Obj;
		}
		if (!AssetPath.Contains(TEXT(".")))
		{
			const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
			return LoadObject<UObject>(nullptr, *ObjectPath);
		}
		return nullptr;
	}

	IAssetRegistry& GetAssetRegistry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	}

	// Build the set of in-scope content roots for project-wide discovery: /Game plus every enabled
	// PROJECT plugin (including Game Feature plugins like /ShooterCore) that can contain content.
	// Engine and engine-plugin content is intentionally excluded — this matches Epic find_assets'
	// "project + project-plugins" scope and keeps discovery on the user's gameplay content, not
	// /Engine noise. Game-Feature-structured projects (Lyra, many shooters) keep most gameplay in
	// these plugin roots, so /Game-only discovery would silently miss the bulk of the project.
	TArray<FName> GetProjectContentRoots()
	{
		TArray<FName> Roots;
		Roots.Add(FName(TEXT("/Game")));
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			if (Plugin->GetLoadedFrom() != EPluginLoadedFrom::Project) { continue; }  // skip engine / engine-plugin content
			FString Mount = Plugin->GetMountedAssetPath();   // e.g. "/ShooterCore/"
			Mount.RemoveFromEnd(TEXT("/"));
			if (!Mount.IsEmpty() && Mount != TEXT("/Game"))
			{
				Roots.AddUnique(FName(*Mount));
			}
		}
		return Roots;
	}

	// Tally the ~tokens (≈ chars / 4 — the project's convention; cf. FBlueprintExportResult::ApproxTokens)
	// of text we hand back to an MCP client, into the main module's session counter that the Codex
	// window header surfaces. Routed through the runtime export service; a no-op if the main plugin is
	// absent. Pass-through, so a content return reads `return RecordServed(...)`.
	FString RecordServed(FString Served)
	{
		if (IBlueprintCodexExportService* Service = IBlueprintCodexExportService::Get())
		{
			Service->AddTokensServed(Served.Len() / 4);
		}
		return Served;
	}

	// Shared describe path: acquire the runtime export service, load the asset, build the requested view
	// via the service (the single main-plugin dispatcher behind it), and return either the content or a
	// clear human-readable message. No service → the main plugin isn't enabled. A load failure yields an
	// "asset not found" message; a genuinely unsupported (type × mode) returns the dispatcher's own
	// OutContent message (e.g. "Flow output is not available for this asset type (UDataTable).") so the
	// agent learns which mode to use instead.
	FString DescribeAsset(const FString& AssetPath, int32 Mode)
	{
		IBlueprintCodexExportService* Service = IBlueprintCodexExportService::Get();
		if (!Service)
		{
			return MainPluginUnavailableMessage();
		}

		UObject* Asset = LoadAssetByPath(AssetPath);
		if (!Asset)
		{
			return FString::Printf(
				TEXT("ERROR: no asset found at '%s'. Use SearchAssets or GetProjectIndex to find a valid asset path."),
				*AssetPath);
		}

		FString Out;
		const bool bOk = Service->BuildAssetText(Asset, FormatText, Mode, Out);
		if (!bOk)
		{
			// BuildAssetText already populated Out with a clear "not available for this type" line.
			return Out;
		}
		if (Out.IsEmpty())
		{
			return FString::Printf(TEXT("(no content for '%s')"), *Asset->GetName());
		}
		return RecordServed(MoveTemp(Out));
	}

	// === Surgical projection helpers (post-filter BuildAssetText output; no builder changes) ===

	// Count of newline-separated lines in S (0 for empty).
	int32 CountLines(const FString& S)
	{
		if (S.IsEmpty()) { return 0; }
		int32 Num = 1;
		for (const TCHAR C : S) { if (C == TEXT('\n')) { ++Num; } }
		return Num;
	}

	// "// --- Variables ---" -> "Variables".
	FString ExtractSectionName(const FString& TrimmedHeader)
	{
		FString S = TrimmedHeader;
		S.RemoveFromStart(TEXT("//"));
		S.TrimStartAndEndInline();      // "--- Variables ---"
		S.RemoveFromStart(TEXT("---"));
		S.RemoveFromEnd(TEXT("---"));
		S.TrimStartAndEndInline();      // "Variables"
		return S;
	}

	struct FParsedSection { FString Name; FString Text; /* includes the "// --- Name ---" header line */ };

	// Split a Structure text into its "// --- <Name> ---" sections. OutPreamble is everything before
	// the first header (the "// Blueprint: ..." line); each section's Text includes its header line.
	void ParseSections(const FString& Full, FString& OutPreamble, TArray<FParsedSection>& OutSections)
	{
		TArray<FString> Lines;
		Full.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		int32 CurIdx = INDEX_NONE;
		for (const FString& Line : Lines)
		{
			FString T = Line;
			T.TrimStartAndEndInline();
			if (T.StartsWith(TEXT("// ---")))
			{
				OutSections.Add(FParsedSection{ ExtractSectionName(T), Line });
				CurIdx = OutSections.Num() - 1;
			}
			else if (CurIdx != INDEX_NONE)
			{
				OutSections[CurIdx].Text += TEXT("\n") + Line;
			}
			else
			{
				if (!OutPreamble.IsEmpty()) { OutPreamble += TEXT("\n"); }
				OutPreamble += Line;
			}
		}
	}

	// Load + build the full text for a mode WITHOUT recording served tokens (the caller records the
	// sliced result). Returns true with OutText on success; false with OutMessage set to a "main plugin
	// unavailable" / "asset not found" error or the dispatcher's "not available for this type" line.
	bool BuildFullTextForSlice(
		const FString& AssetPath,
		int32 Mode,
		FString& OutText,
		FString& OutMessage)
	{
		IBlueprintCodexExportService* Service = IBlueprintCodexExportService::Get();
		if (!Service)
		{
			OutMessage = MainPluginUnavailableMessage();
			return false;
		}

		UObject* Asset = LoadAssetByPath(AssetPath);
		if (!Asset)
		{
			OutMessage = FString::Printf(
				TEXT("ERROR: no asset found at '%s'. Use SearchAssets or GetProjectIndex to find a valid asset path."),
				*AssetPath);
			return false;
		}
		FString Full;
		if (!Service->BuildAssetText(Asset, FormatText, Mode, Full))
		{
			OutMessage = Full;  // dispatcher's "not available for this type" line
			return false;
		}
		if (Full.IsEmpty())
		{
			OutMessage = FString::Printf(TEXT("(no content for '%s')"), *Asset->GetName());
			return false;
		}
		OutText = MoveTemp(Full);
		return true;
	}
}

FString UBlueprintCodexToolset::GetProjectIndex(const FString& PathFilter, int32 Limit)
{
	IBlueprintCodexExportService* Service = IBlueprintCodexExportService::Get();
	if (!Service)
	{
		return MainPluginUnavailableMessage();
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;

	// Empty PathFilter → scan ALL project content roots (/Game + enabled project / Game-Feature
	// plugin content like /ShooterCore). A specific PathFilter scopes to that single root/subpath.
	FString Scope;
	if (PathFilter.IsEmpty())
	{
		Filter.PackagePaths = GetProjectContentRoots();
		Scope = TEXT("project + plugin content");
	}
	else
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
		Scope = PathFilter;
	}

	TArray<FAssetData> Assets;
	GetAssetRegistry().GetAssets(Filter, Assets);

	TArray<FString> Lines;
	for (const FAssetData& A : Assets)
	{
		if (Service->IsExportableAsset(A))
		{
			Lines.Add(FString::Printf(TEXT("%s  (%s)"),
				*A.GetObjectPathString(), *A.AssetClassPath.GetAssetName().ToString()));
		}
	}

	if (Lines.Num() == 0)
	{
		return FString::Printf(TEXT("(no exportable assets under %s)"), *Scope);
	}

	Lines.Sort();
	const int32 Total = Lines.Num();
	const int32 Shown = FMath::Min(Total, FMath::Max(1, Limit));
	if (Shown < Total)
	{
		Lines.SetNum(Shown);
	}

	const FString Header = (Shown < Total)
		? FString::Printf(TEXT("# Blueprint Codex: %d asset(s) under %s (showing %d)\n"), Total, *Scope, Shown)
		: FString::Printf(TEXT("# Blueprint Codex: %d asset(s) under %s\n"), Total, *Scope);
	return RecordServed(Header + FString::Join(Lines, TEXT("\n")));
}

FString UBlueprintCodexToolset::SearchAssets(const FString& Query, int32 Limit)
{
	if (Query.IsEmpty())
	{
		return TEXT("ERROR: empty search query.");
	}

	IBlueprintCodexExportService* Service = IBlueprintCodexExportService::Get();
	if (!Service)
	{
		return MainPluginUnavailableMessage();
	}

	FARFilter Filter;
	Filter.PackagePaths = GetProjectContentRoots();   // /Game + enabled project / Game-Feature plugin content
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	GetAssetRegistry().GetAssets(Filter, Assets);

	TArray<FString> Matches;
	for (const FAssetData& A : Assets)
	{
		if (!Service->IsExportableAsset(A)) { continue; }
		if (A.AssetName.ToString().Contains(Query, ESearchCase::IgnoreCase))
		{
			Matches.Add(FString::Printf(TEXT("%s  (%s)"),
				*A.GetObjectPathString(), *A.AssetClassPath.GetAssetName().ToString()));
		}
	}

	if (Matches.Num() == 0)
	{
		return FString::Printf(TEXT("(no exportable assets matching '%s')"), *Query);
	}

	Matches.Sort();
	const int32 Total = Matches.Num();
	const int32 Shown = FMath::Min(Total, FMath::Max(1, Limit));
	if (Shown < Total)
	{
		Matches.SetNum(Shown);
	}

	const FString Header = (Shown < Total)
		? FString::Printf(TEXT("# %d match(es) for '%s' (showing %d)\n"), Total, *Query, Shown)
		: FString::Printf(TEXT("# %d match(es) for '%s'\n"), Total, *Query);
	return RecordServed(Header + FString::Join(Matches, TEXT("\n")));
}

FString UBlueprintCodexToolset::GetAssetStructure(const FString& AssetPath)
{
	return DescribeAsset(AssetPath, ModeStructure);
}

FString UBlueprintCodexToolset::GetAssetFlow(const FString& AssetPath)
{
	return DescribeAsset(AssetPath, ModeFlow);
}

FString UBlueprintCodexToolset::GetAssetRaw(const FString& AssetPath)
{
	return DescribeAsset(AssetPath, ModeRaw);
}

FString UBlueprintCodexToolset::GetAssetJson(const FString& AssetPath)
{
	return DescribeAsset(AssetPath, ModeJson);
}

FString UBlueprintCodexToolset::GetAssetFlowJson(const FString& AssetPath)
{
	return DescribeAsset(AssetPath, ModeFlowJson);
}

FString UBlueprintCodexToolset::GetAssetSummary(const FString& AssetPath)
{
	return DescribeAsset(AssetPath, ModeSummary);
}

FString UBlueprintCodexToolset::GetAssetSummaryJson(const FString& AssetPath)
{
	return DescribeAsset(AssetPath, ModeSummaryJson);
}

FString UBlueprintCodexToolset::GetAssetSection(const FString& AssetPath, const FString& Sections)
{
	FString Full, Message;
	if (!BuildFullTextForSlice(AssetPath, ModeStructure, Full, Message))
	{
		return Message;
	}

	FString Preamble;
	TArray<FParsedSection> Parsed;
	ParseSections(Full, Preamble, Parsed);
	if (Parsed.Num() == 0)
	{
		// Defensive: no section headers found — hand back the whole Structure rather than nothing.
		return RecordServed(MoveTemp(Full));
	}

	const FString Req = Sections.TrimStartAndEnd();
	if (Req.IsEmpty())
	{
		// Outline mode: list the section names + body line counts (a cheap "what's in here").
		FString Out = Preamble + TEXT("\n\n// --- Section Outline (pass a name to GetAssetSection to fetch it) ---");
		for (const FParsedSection& S : Parsed)
		{
			Out += FString::Printf(TEXT("\n//   %s  (%d line(s))"), *S.Name, FMath::Max(0, CountLines(S.Text) - 1));
		}
		return RecordServed(MoveTemp(Out));
	}

	TArray<FString> Keys;
	Req.ParseIntoArray(Keys, TEXT(","));   // culls empties by default

	FString Body;
	TArray<FString> MatchedNames;
	for (const FParsedSection& S : Parsed)
	{
		for (const FString& Key : Keys)
		{
			const FString K = Key.TrimStartAndEnd();
			if (!K.IsEmpty() && S.Name.Contains(K, ESearchCase::IgnoreCase))
			{
				Body += TEXT("\n") + S.Text;
				MatchedNames.Add(S.Name);
				break;
			}
		}
	}

	if (MatchedNames.Num() == 0)
	{
		TArray<FString> Names;
		for (const FParsedSection& S : Parsed) { Names.Add(S.Name); }
		return FString::Printf(
			TEXT("No Structure section matching '%s' for this asset. Available sections: %s"),
			*Sections, *FString::Join(Names, TEXT(", ")));
	}

	return RecordServed(Preamble + Body);
}

FString UBlueprintCodexToolset::GetGraphFlow(const FString& AssetPath, const FString& GraphName)
{
	const FString Query = GraphName.TrimStartAndEnd();
	if (Query.IsEmpty())
	{
		return TEXT("ERROR: empty graph name. Pass an event/function/macro name (see the Functions / Events sections of GetAssetStructure or GetAssetSection).");
	}

	FString Full, Message;
	if (!BuildFullTextForSlice(AssetPath, ModeFlow, Full, Message))
	{
		return Message;
	}

	// Parse the Flow text into entries. Sections are "// --- <Name> ---" headers; within a section,
	// entries (events / functions / macros / construction script) are separated by lone "//" lines.
	TArray<FString> Lines;
	Full.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);

	struct FEntry { FString Section; FString Title; FString Text; };
	TArray<FEntry> Entries;
	FString Preamble;
	FString CurSection;
	FString CurTitle;
	FString CurText;
	bool bSeenSection = false;
	bool bInEntry = false;

	auto Flush = [&]()
	{
		if (bInEntry && !CurText.TrimStartAndEnd().IsEmpty())
		{
			Entries.Add(FEntry{ CurSection, CurTitle, CurText });
		}
		bInEntry = false;
		CurTitle.Empty();
		CurText.Empty();
	};

	for (const FString& Line : Lines)
	{
		FString T = Line;
		T.TrimStartAndEndInline();

		if (T.StartsWith(TEXT("// ---")))   // section header
		{
			Flush();
			CurSection = ExtractSectionName(T);
			bSeenSection = true;
			continue;
		}
		if (!bSeenSection)                  // preamble (the "// Blueprint: ..." line)
		{
			if (!Preamble.IsEmpty()) { Preamble += TEXT("\n"); }
			Preamble += Line;
			continue;
		}
		if (T == TEXT("//"))                // entry separator
		{
			Flush();
			continue;
		}
		if (!bInEntry)
		{
			if (T.IsEmpty()) { continue; }  // skip blank lines between entries
			bInEntry = true;
			CurTitle = T;
			CurText = Line;
		}
		else
		{
			CurText += TEXT("\n") + Line;
		}
	}
	Flush();

	TArray<FString> Picked;
	for (const FEntry& E : Entries)
	{
		if (E.Title.Contains(Query, ESearchCase::IgnoreCase))
		{
			Picked.Add(FString::Printf(TEXT("// [%s]\n%s"), *E.Section, *E.Text));
		}
	}

	if (Picked.Num() == 0)
	{
		TArray<FString> Titles;
		for (const FEntry& E : Entries) { Titles.Add(E.Title); }
		const FString TitleList = Titles.Num() ? FString::Join(Titles, TEXT("\n  ")) : FString(TEXT("(none)"));
		return FString::Printf(
			TEXT("No graph/event/function in '%s' matching '%s'. Available entries:\n  %s"),
			*AssetPath, *GraphName, *TitleList);
	}

	FString Out = Preamble;
	for (const FString& P : Picked)
	{
		Out += TEXT("\n\n") + P;
	}
	return RecordServed(MoveTemp(Out));
}
