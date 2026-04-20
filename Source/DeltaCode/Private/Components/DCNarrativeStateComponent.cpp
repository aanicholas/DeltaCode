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
#include "Components/DCNarrativeStateComponent.h"

UDCNarrativeStateComponent::UDCNarrativeStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDCNarrativeStateComponent::AddTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid() || Tags.HasTag(Tag))
	{
		return;
	}
	Tags.AddTag(Tag);

	FGameplayTagContainer Added;
	Added.AddTag(Tag);
	OnTagsChanged.Broadcast(Added, FGameplayTagContainer());
}

void UDCNarrativeStateComponent::AddTags(const FGameplayTagContainer& InTags)
{
	FGameplayTagContainer Added;
	for (const FGameplayTag& Tag : InTags)
	{
		if (Tag.IsValid() && !Tags.HasTag(Tag))
		{
			Tags.AddTag(Tag);
			Added.AddTag(Tag);
		}
	}
	if (!Added.IsEmpty())
	{
		OnTagsChanged.Broadcast(Added, FGameplayTagContainer());
	}
}

void UDCNarrativeStateComponent::RemoveTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid() || !Tags.HasTag(Tag))
	{
		return;
	}
	Tags.RemoveTag(Tag);

	FGameplayTagContainer Removed;
	Removed.AddTag(Tag);
	OnTagsChanged.Broadcast(FGameplayTagContainer(), Removed);
}

void UDCNarrativeStateComponent::RemoveTags(const FGameplayTagContainer& InTags)
{
	FGameplayTagContainer Removed;
	for (const FGameplayTag& Tag : InTags)
	{
		if (Tag.IsValid() && Tags.HasTag(Tag))
		{
			Tags.RemoveTag(Tag);
			Removed.AddTag(Tag);
		}
	}
	if (!Removed.IsEmpty())
	{
		OnTagsChanged.Broadcast(FGameplayTagContainer(), Removed);
	}
}

void UDCNarrativeStateComponent::SetTagsFromSave(const FGameplayTagContainer& InTags)
{
	const FGameplayTagContainer Old = Tags;
	Tags = InTags;

	FGameplayTagContainer Added;
	FGameplayTagContainer Removed;
	for (const FGameplayTag& Tag : Tags)
	{
		if (!Old.HasTag(Tag)) { Added.AddTag(Tag); }
	}
	for (const FGameplayTag& Tag : Old)
	{
		if (!Tags.HasTag(Tag)) { Removed.AddTag(Tag); }
	}
	if (!Added.IsEmpty() || !Removed.IsEmpty())
	{
		OnTagsChanged.Broadcast(Added, Removed);
	}
}
