// Copyright Epic Games, Inc. All Rights Reserved.

#include "RammsTouchLinux.h"

#if PLATFORM_LINUX

	#include "CoreGlobals.h"
	#include "HAL/IConsoleManager.h"
	#include "Misc/CoreDelegates.h"
	#include "Misc/ScopeLock.h"

THIRD_PARTY_INCLUDES_START
	#include <SDL3/SDL.h>
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogRammsTouch, Log, All);

namespace RammsTouchLinux
{
	namespace
	{
		int32					GCancelAsUp = 1;
		FAutoConsoleVariableRef CVarCancelAsUp(
			TEXT("ramms.Touch.CancelAsUp"),
			GCancelAsUp,
			TEXT("Linux: deliver a touch SDL cancels as a finger-up. UE 5.8 ignores SDL's cancel, which leaves the ")
				TEXT("touch down for good. 1 = on (default), 0 = off."));

		int32					GTrace = 0;
		FAutoConsoleVariableRef CVarTrace(
			TEXT("ramms.Touch.Trace"),
			GTrace,
			TEXT("Linux: log each finger as SDL delivers it, with its timing: 1 = a line at down, first move and up; ")
				TEXT("2 = every event as well. 0 = off (default)."));

		/** Events of one finger closer together than this were delivered
		 *  together (a backlog let go at once), not as the finger moved. */
		constexpr uint64 BurstNs = 1000000;

		struct FFingerTrace
		{
			uint64 DownNs = 0;
			uint64 FirstMoveNs = 0;
			uint64 LastNs = 0;
			uint64 MaxGapNs = 0;
			uint64 DownFrame = 0;
			int32  Moves = 0;
			int32  BurstEvents = 0;
		};

		// SDL can push events from other threads; fingers come from the event
		// pump on the game thread, but the trace is cheap to keep safe.
		FCriticalSection						  TraceLock;
		TMap<TPair<uint64, uint64>, FFingerTrace> Fingers;

		SDL_EventFilter PreviousFilter = nullptr;
		void*			PreviousUserData = nullptr;
		bool			bInstalled = false;
		FDelegateHandle PostEngineInitHandle;

		double Ms(uint64 Ns)
		{
			return static_cast<double>(Ns) / 1.0e6;
		}

		void TraceFinger(const SDL_Event& Event)
		{
			const SDL_TouchFingerEvent& Finger = Event.tfinger;
			const uint64				Now = static_cast<uint64>(Event.common.timestamp);
			const uint64				Id = static_cast<uint64>(Finger.fingerID);
			const TPair<uint64, uint64> Key(static_cast<uint64>(Finger.touchID), Id);
			FScopeLock					Lock(&TraceLock);

			if (Event.type == SDL_EVENT_FINGER_DOWN)
			{
				FFingerTrace& Trace = Fingers.FindOrAdd(Key);
				Trace = FFingerTrace();
				Trace.DownNs = Trace.LastNs = Now;
				Trace.DownFrame = GFrameCounter;
				UE_LOG(LogRammsTouch, Log, TEXT("finger %llu down at (%.3f, %.3f), frame %llu; %d down"),
					Id, Finger.x, Finger.y, GFrameCounter, Fingers.Num());
				return;
			}

			FFingerTrace* Trace = Fingers.Find(Key);
			if (!Trace)
			{
				UE_LOG(LogRammsTouch, Log, TEXT("finger %llu: event %u without a down"), Id, static_cast<uint32>(Event.type));
				return;
			}
			const uint64 Gap = Now - Trace->LastNs;
			Trace->MaxGapNs = FMath::Max(Trace->MaxGapNs, Gap);
			if (Gap < BurstNs)
			{
				++Trace->BurstEvents;
			}
			Trace->LastNs = Now;

			if (Event.type == SDL_EVENT_FINGER_MOTION)
			{
				if (++Trace->Moves == 1)
				{
					Trace->FirstMoveNs = Now;
					UE_LOG(LogRammsTouch, Log, TEXT("finger %llu first move %.1f ms after its down (%llu frames later)"),
						Id, Ms(Now - Trace->DownNs), GFrameCounter - Trace->DownFrame);
				}
				else if (GTrace >= 2)
				{
					UE_LOG(LogRammsTouch, Log, TEXT("finger %llu move to (%.3f, %.3f), %.1f ms after the last"),
						Id, Finger.x, Finger.y, Ms(Gap));
				}
				return;
			}

			UE_LOG(LogRammsTouch, Log,
				TEXT("finger %llu %s after %.1f ms: %d moves, the first at +%.1f ms; longest gap %.1f ms; %d events in bursts"),
				Id, Event.type == SDL_EVENT_FINGER_CANCELED ? TEXT("cancelled") : TEXT("up"), Ms(Now - Trace->DownNs), Trace->Moves,
				Trace->Moves > 0 ? Ms(Trace->FirstMoveNs - Trace->DownNs) : 0.0, Ms(Trace->MaxGapNs), Trace->BurstEvents);
			Fingers.Remove(Key);
		}

		void TraceMouseButton(const SDL_Event& Event)
		{
			const SDL_MouseButtonEvent& Button = Event.button;
			UE_LOG(LogRammsTouch, Log, TEXT("mouse %u button %d %s at (%.0f, %.0f)%s"), static_cast<uint32>(Button.which),
				static_cast<int32>(Button.button), Button.down ? TEXT("down") : TEXT("up"), Button.x, Button.y,
				Button.which == SDL_TOUCH_MOUSEID ? TEXT(", made from a touch") : TEXT(""));
		}

		bool SDLCALL FilterEvent(void* UserData, SDL_Event* Event)
		{
			switch (Event->type)
			{
				case SDL_EVENT_FINGER_CANCELED:
					if (GTrace)
					{
						TraceFinger(*Event);
					}
					if (GCancelAsUp)
					{
						// Same tfinger payload; UE ends the touch on an up.
						UE_LOG(LogRammsTouch, Log, TEXT("SDL cancelled finger %llu; delivered as a finger-up so the touch ends"),
							static_cast<uint64>(Event->tfinger.fingerID));
						Event->type = SDL_EVENT_FINGER_UP;
					}
					break;
				case SDL_EVENT_FINGER_DOWN:
				case SDL_EVENT_FINGER_UP:
				case SDL_EVENT_FINGER_MOTION:
					if (GTrace)
					{
						TraceFinger(*Event);
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
				case SDL_EVENT_MOUSE_BUTTON_UP:
					if (GTrace)
					{
						TraceMouseButton(*Event);
					}
					break;
				default:
					break;
			}
			return PreviousFilter ? PreviousFilter(PreviousUserData, Event) : true;
		}

		void Install()
		{
			if (bInstalled)
			{
				return;
			}
			if (!SDL_WasInit(SDL_INIT_EVENTS))
			{
				UE_LOG(LogRammsTouch, Log, TEXT("SDL events are not running; the touch filter is not installed"));
				return;
			}
			// Chained, should anything else have set one.
			if (!SDL_GetEventFilter(&PreviousFilter, &PreviousUserData))
			{
				PreviousFilter = nullptr;
				PreviousUserData = nullptr;
			}
			SDL_SetEventFilter(&FilterEvent, nullptr);
			bInstalled = true;
			UE_LOG(LogRammsTouch, Log, TEXT("SDL touch filter installed (ramms.Touch.CancelAsUp %d, ramms.Touch.Trace %d)"),
				GCancelAsUp, GTrace);
		}
	} // namespace

	void Startup()
	{
		if (SDL_WasInit(SDL_INIT_EVENTS))
		{
			Install();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([]() { Install(); });
		}
	}

	void Shutdown()
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		if (!bInstalled)
		{
			return;
		}
		SDL_EventFilter Current = nullptr;
		void*			CurrentUserData = nullptr;
		if (SDL_GetEventFilter(&Current, &CurrentUserData) && Current == &FilterEvent)
		{
			SDL_SetEventFilter(PreviousFilter, PreviousUserData);
		}
		bInstalled = false;
	}
} // namespace RammsTouchLinux

#else

namespace RammsTouchLinux
{
	void Startup() {}
	void Shutdown() {}
} // namespace RammsTouchLinux

#endif
