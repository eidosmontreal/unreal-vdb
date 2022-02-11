// Copyright 2022 Eidos-Montreal / Eidos-Sherbrooke

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VolumeTrackEditor.h"

#if WITH_EDITOR
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "SequencerSectionPainter.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "MovieSceneVolumeTrack.h"
#include "MovieSceneVolumeSection.h"
#include "CommonMovieSceneTools.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"
#include "ISectionLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "MovieSceneTimeHelpers.h"
#include "Fonts/FontMeasure.h"
#include "SequencerTimeSliderController.h"
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/SlateIconFinder.h"
#include "LevelSequence.h"
#include "VolumeSequencerCommon.h"

#define LOCTEXT_NAMESPACE "FVolumeTrackEditor"


static UActorComponent* TryExtractVolumeComponentFromObjectGuid(FVolumeTrackHandlerBase* TrackHandler, const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr)
{
	UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(Guid) : nullptr;

	UActorComponent* ActorComponent = TrackHandler->TryExtractVolumeComponent(BoundObject);
	if (ActorComponent == nullptr)
	{
		return nullptr;
	}

	TObjectPtr<UObject> Volume = TrackHandler->GetVolume(ActorComponent);
	if (Volume == nullptr)
	{
		return nullptr;
	}

	return ActorComponent;
}

FVolumeSection::FVolumeSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: Section(*CastChecked<UMovieSceneVolumeSection>(&InSection))
	, Sequencer(InSequencer)
	, InitialFirstLoopStartOffsetDuringResize(0)
	, InitialStartTimeDuringResize(0)
{ }

UMovieSceneSection* FVolumeSection::GetSectionObject()
{
	return &Section;
}

FText FVolumeSection::GetSectionTitle() const
{
	if (Section.Params.Volume != nullptr)
	{
		return FText::FromString(Section.Params.Volume->GetName());
	
	}
	return LOCTEXT("NoVolumeSection", "No Volumetric Animation");
}

float FVolumeSection::GetSectionHeight() const
{
	return (float)20;
}

int32 FVolumeSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	int32 LayerId = Painter.PaintSectionBackground();

	static const FSlateBrush* GenericDivider = FEditorStyle::GetBrush("Sequencer.GenericDivider");

	if (!Section.HasStartFrame() || !Section.HasEndFrame())
	{
		return LayerId;
	}

	FFrameRate TickResolution = TimeToPixelConverter.GetTickResolution();

	// Add lines where the animation starts and ends/loops
	const float AnimPlayRate = FMath::IsNearlyZero(Section.Params.PlayRate) ? 1.0f : Section.Params.PlayRate;
	const float Duration = Section.Params.GetSequenceLength();
	const float SeqLength = Duration - TickResolution.AsSeconds(Section.Params.StartFrameOffset + Section.Params.EndFrameOffset) / AnimPlayRate;
	const float FirstLoopSeqLength = SeqLength - TickResolution.AsSeconds(Section.Params.FirstLoopStartFrameOffset) / AnimPlayRate;

	if (!FMath::IsNearlyZero(SeqLength, KINDA_SMALL_NUMBER) && SeqLength > 0)
	{
		float MaxOffset = Section.GetRange().Size<FFrameTime>() / TickResolution;
		float OffsetTime = FirstLoopSeqLength;
		float StartTime = Section.GetInclusiveStartFrame() / TickResolution;

		while (OffsetTime < MaxOffset)
		{
			float OffsetPixel = TimeToPixelConverter.SecondsToPixel(StartTime + OffsetTime) - TimeToPixelConverter.SecondsToPixel(StartTime);

			FSlateDrawElement::MakeBox(
				Painter.DrawElements,
				LayerId,
				Painter.SectionGeometry.MakeChild(
					FVector2D(2.f, Painter.SectionGeometry.Size.Y - 2.f),
					FSlateLayoutTransform(FVector2D(OffsetPixel, 1.f))
				).ToPaintGeometry(),
				GenericDivider,
				DrawEffects
			);

			OffsetTime += SeqLength;
		}
	}

	TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();
	if (Painter.bIsSelected && SequencerPtr.IsValid())
	{
		FFrameTime CurrentTime = SequencerPtr->GetLocalTime().Time;
		if (Section.GetRange().Contains(CurrentTime.FrameNumber) && Section.Params.Volume != nullptr)
		{
			const float Time = TimeToPixelConverter.FrameToPixel(CurrentTime);

			// Draw the current time next to the scrub handle
			const float AnimTime = Section.MapTimeToAnimation(Duration, CurrentTime, TickResolution);
			int32 FrameTime = Section.Params.GetTrackHandler()->GetFrameAtTime(Section.Params.Volume, AnimTime);
			FString FrameString = FString::FromInt(FrameTime);

			const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
			const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

			// Flip the text position if getting near the end of the view range
			static const float TextOffsetPx = 10.f;
			bool  bDrawLeft = (Painter.SectionGeometry.Size.X - Time) < (TextSize.X + 22.f) - TextOffsetPx;
			float TextPosition = bDrawLeft ? Time - TextSize.X - TextOffsetPx : Time + TextOffsetPx;
			//handle mirrored labels
			const float MajorTickHeight = 9.0f;
			FVector2D TextOffset(TextPosition, Painter.SectionGeometry.Size.Y - (MajorTickHeight + TextSize.Y));

			const FLinearColor DrawColor = FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());
			const FVector2D BoxPadding = FVector2D(4.0f, 2.0f);
			// draw time string

			FSlateDrawElement::MakeBox(
				Painter.DrawElements,
				LayerId + 5,
				Painter.SectionGeometry.ToPaintGeometry(TextOffset - BoxPadding, TextSize + 2.0f * BoxPadding),
				FEditorStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.5f)
			);

			FSlateDrawElement::MakeText(
				Painter.DrawElements,
				LayerId + 6,
				Painter.SectionGeometry.ToPaintGeometry(TextOffset, TextSize),
				FrameString,
				SmallLayoutFont,
				DrawEffects,
				DrawColor
			);

		}
	}

	return LayerId;
}

void FVolumeSection::BeginResizeSection()
{
	InitialFirstLoopStartOffsetDuringResize = Section.Params.FirstLoopStartFrameOffset;
	InitialStartTimeDuringResize = Section.HasStartFrame() ? Section.GetInclusiveStartFrame() : 0;
}

void FVolumeSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	// Adjust the start offset when resizing from the beginning
	if (ResizeMode == SSRM_LeadingEdge)
	{
		FFrameRate FrameRate = Section.GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber StartOffset = FrameRate.AsFrameNumber((ResizeTime - InitialStartTimeDuringResize) / FrameRate * Section.Params.PlayRate);

		StartOffset += InitialFirstLoopStartOffsetDuringResize;

		if (StartOffset < 0)
		{
			// Ensure start offset is not less than 0 and adjust ResizeTime
			ResizeTime = ResizeTime - StartOffset;

			StartOffset = FFrameNumber(0);
		}
		else
		{
			// If the start offset exceeds the length of one loop, trim it back.
			const FFrameNumber SeqLength = FrameRate.AsFrameNumber(Section.Params.GetSequenceLength()) - Section.Params.StartFrameOffset - Section.Params.EndFrameOffset;
			StartOffset = StartOffset % SeqLength;
		}

		Section.Params.FirstLoopStartFrameOffset = StartOffset;
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FVolumeSection::BeginSlipSection()
{
	BeginResizeSection();
}

void FVolumeSection::SlipSection(FFrameNumber SlipTime)
{
	FFrameRate FrameRate = Section.GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber StartOffset = FrameRate.AsFrameNumber((SlipTime - InitialStartTimeDuringResize) / FrameRate * Section.Params.PlayRate);

	StartOffset += InitialFirstLoopStartOffsetDuringResize;

	if (StartOffset < 0)
	{
		// Ensure start offset is not less than 0 and adjust ResizeTime
		SlipTime = SlipTime - StartOffset;

		StartOffset = FFrameNumber(0);
	}
	else
	{
		// If the start offset exceeds the length of one loop, trim it back.
		const FFrameNumber SeqLength = FrameRate.AsFrameNumber(Section.Params.GetSequenceLength()) - Section.Params.StartFrameOffset - Section.Params.EndFrameOffset;
		StartOffset = StartOffset % SeqLength;
	}

	Section.Params.FirstLoopStartFrameOffset = StartOffset;

	ISequencerSection::SlipSection(SlipTime);
}

void FVolumeSection::BeginDilateSection()
{
	Section.PreviousPlayRate = Section.Params.PlayRate; //make sure to cache the play rate
}

void FVolumeSection::DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor)
{
	Section.Params.PlayRate = Section.PreviousPlayRate / DilationFactor;
	Section.SetRange(NewRange);
}

FVolumeTrackEditor::FVolumeTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{ }

TSharedRef<ISequencerTrackEditor> FVolumeTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FVolumeTrackEditor(InSequencer));
}

bool FVolumeTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return InSequence && InSequence->IsA(ULevelSequence::StaticClass());
}

bool FVolumeTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneVolumeTrack::StaticClass();
}

TSharedRef<ISequencerSection> FVolumeTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));

	return MakeShareable(new FVolumeSection(SectionObject, GetSequencer()));
}

void FVolumeTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	auto AddMenuEntryFromHandlerFunc = [this](FVolumeTrackHandlerBase* InTrackHandler, FMenuBuilder& InMenuBuilder, const TArray<FGuid>& InObjectBindings)
	{
		UMovieSceneTrack* Track = nullptr;
		TAttribute<FText> MenuLabel = InTrackHandler->GetLabelText();
		TAttribute<FText> MenuToolTip = InTrackHandler->GetToolTipText();
		
		InMenuBuilder.AddMenuEntry(MenuLabel,
			MenuToolTip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FVolumeTrackEditor::BuildVolumeTrack, InObjectBindings, Track, InTrackHandler))
		);
	};

	FVolumeTrackHandlersScoped ScopedTH = GetVolumeTrackHandlers();
	if (ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		for (FVolumeTrackHandlerBase* TrackHandler : ScopedTH.TrackHandlers)
		{
			if (TryExtractVolumeComponentFromObjectGuid(TrackHandler, ObjectBindings[0], GetSequencer()) != nullptr)
			{
				AddMenuEntryFromHandlerFunc(TrackHandler, MenuBuilder, ObjectBindings);
			}
		}
	}

	for (FVolumeTrackHandlerBase* TrackHandler : ScopedTH.TrackHandlers)
	{
		if (TrackHandler->IsSupportedVolumeComponentClass(ObjectClass))
		{
			if (TryExtractVolumeComponentFromObjectGuid(TrackHandler, ObjectBindings[0], GetSequencer()) != nullptr)
			{
				AddMenuEntryFromHandlerFunc(TrackHandler, MenuBuilder, ObjectBindings);
			}			
		}
	}
}

void FVolumeTrackEditor::BuildVolumeTrack(TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track, FVolumeTrackHandlerBase* TrackHandler)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (SequencerPtr.IsValid())
	{
		const FScopedTransaction Transaction(LOCTEXT("AddVolume_Transaction", "Add Volumetric Animation"));

		for (FGuid ObjectBinding : ObjectBindings)
		{
			if (ObjectBinding.IsValid())
			{
				UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding);
				UActorComponent* ActorComp = TryExtractVolumeComponentFromObjectGuid(TrackHandler, ObjectBinding, GetSequencer());
				if (ActorComp != nullptr)
				{
					AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FVolumeTrackEditor::AddKeyInternal, Object, ActorComp, Track, TrackHandler));
				}
			}
		}
	}
}

FKeyPropertyResult FVolumeTrackEditor::AddKeyInternal(FFrameNumber KeyTime, UObject* Object, UActorComponent* VolAnimComp, UMovieSceneTrack* Track, FVolumeTrackHandlerBase* TrackHandler)
{
	FKeyPropertyResult KeyPropertyResult;

	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
	FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
	if (ObjectHandle.IsValid())
	{
		if (!Track)
		{
			Track = AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectHandle, UMovieSceneVolumeTrack::StaticClass(), NAME_None);
			KeyPropertyResult.bTrackCreated = true;
		}

		if (ensure(Track))
		{
			Track->Modify();

			UMovieSceneSection* NewSection = Cast<UMovieSceneVolumeTrack>(Track)->AddNewAnimation(KeyTime, VolAnimComp, TrackHandler);
			KeyPropertyResult.bTrackModified = true;
			KeyPropertyResult.SectionsCreated.Add(NewSection);

			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();
		}
	}

	return KeyPropertyResult;
}

TSharedPtr<SWidget> FVolumeTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	FVolumeTrackHandlersScoped ScopedTH = GetVolumeTrackHandlers();
	for (FVolumeTrackHandlerBase* TrackHandler : ScopedTH.TrackHandlers)
	{
		if (TryExtractVolumeComponentFromObjectGuid(TrackHandler, ObjectBinding, GetSequencer()) != nullptr)
		{
			TWeakPtr<ISequencer> WeakSequencer = GetSequencer();

			auto SubMenuCallback = [=]() -> TSharedRef<SWidget>
			{
				FMenuBuilder MenuBuilder(true, nullptr);

				TArray<FGuid> ObjectBindings;
				ObjectBindings.Add(ObjectBinding);

				BuildVolumeTrack(ObjectBindings, Track, TrackHandler);

				return MenuBuilder.MakeWidget();
			};

			FText HoverText = TrackHandler->GetHoverText();

			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					FSequencerUtilities::MakeAddButton(HoverText, FOnGetContent::CreateLambda(SubMenuCallback), Params.NodeIsHovered, GetSequencer())
				];
		}
	}

	return TSharedPtr<SWidget>();
}

const FSlateBrush* FVolumeTrackEditor::GetIconBrush() const
{
	return FSlateIconFinder::FindIconForClass(nullptr).GetIcon();
}

#undef LOCTEXT_NAMESPACE

#endif	// #if WITH_EDITOR