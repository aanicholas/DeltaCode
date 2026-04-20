/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * This program is free software: you can redistribute
 * it and/or modify it under the terms of the GNU
 * General Public License as published by the Free
 * Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#include "UI/DeltaCodeStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FDeltaCodeStyle::StyleInstance = nullptr;

void FDeltaCodeStyle::Initialize()
{
	if (StyleInstance.IsValid())
	{
		return;
	}

	StyleInstance = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	const FString PluginContentDir = IPluginManager::Get().FindPlugin(TEXT("DeltaCode"))->GetBaseDir() / TEXT("Resources");
	StyleInstance->SetContentRoot(PluginContentDir);

#define IMAGE_BRUSH(RelativePath, Size) \
	FSlateImageBrush(StyleInstance->RootToContentDir(RelativePath, TEXT(".png")), Size)

	StyleInstance->Set("DeltaCode.OpenGeneratorPanel",
		new IMAGE_BRUSH("Icon40", FVector2D(40.0f, 40.0f)));

#undef IMAGE_BRUSH

	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FDeltaCodeStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

FName FDeltaCodeStyle::GetStyleSetName()
{
	static FName Name(TEXT("DeltaCodeStyle"));
	return Name;
}

const ISlateStyle& FDeltaCodeStyle::Get()
{
	return *StyleInstance;
}
