// RammsBridgeDetectionParse.cpp

#include "RammsBridgeDetectionParse.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace RammsBridgeDetectionParse
{
	static const TCHAR* const VectorKeys[] = { TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("W") };

	bool ReadNumbers(const FJsonObject& Object, const TCHAR* Field, int32 Count, double* Out)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(Field);
		if (!Value.IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		const TSharedPtr<FJsonObject>*		  Fields = nullptr;
		if (Value->TryGetArray(Array))
		{
			if (Array->Num() < Count)
			{
				return false;
			}
			for (int32 i = 0; i < Count; ++i)
			{
				if (!(*Array)[i].IsValid() || !(*Array)[i]->TryGetNumber(Out[i]))
				{
					return false;
				}
			}
			return true;
		}
		if (Value->TryGetObject(Fields))
		{
			for (int32 i = 0; i < Count; ++i)
			{
				if (!(*Fields)->TryGetNumberField(VectorKeys[i], Out[i]))
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}

	bool IsNormalized(const FVector4& Box, const FVector2D& Centroid)
	{
		const auto IsUnit = [](double Value) { return Value >= -0.001 && Value <= 1.001; };
		return IsUnit(Box.X) && IsUnit(Box.Y) && IsUnit(Box.Z) && IsUnit(Box.W) && IsUnit(Centroid.X) && IsUnit(Centroid.Y);
	}

	int32 Parse(const FString& DetectedObjectsJson, TFunctionRef<bool(const FString& Source)> IsMine,
		const FVector2D& PixelDivisor, FResult& Out)
	{
		TSharedPtr<FJsonObject>			Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DetectedObjectsJson);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return -1;
		}

		Out.Boxes.Reset();
		Out.Sources.Reset();
		Out.PushSource.Empty();
		Out.SelectedInstanceId.Empty();
		Root->TryGetStringField(TEXT("stream_id"), Out.PushSource);
		Root->TryGetStringField(TEXT("selected_instance_id"), Out.SelectedInstanceId);

		// No "objects" at all is the same as an empty list: the push says nothing is there.
		const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
		if (!Root->TryGetArrayField(TEXT("objects"), Objects))
		{
			return 0;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Objects)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Object))
			{
				continue;
			}
			FString Source;
			if (!(*Object)->TryGetStringField(TEXT("source"), Source) || Source.IsEmpty())
			{
				Source = Out.PushSource;
			}
			if (!IsMine(Source))
			{
				continue;
			}

			// x_min, y_min, x_max, y_max; all zero means no box, then the centroid alone places it. A
			// centroid of exactly (0, 0) with no box is skipped: the orchestrator sends that when it has
			// no box for an object (its "centroid" is [0, 0] when "box" is unset), not an object at the corner
			double	   BoxValues[4] = { 0.0, 0.0, 0.0, 0.0 };
			double	   CentroidValues[2] = { 0.0, 0.0 };
			const bool bHasBox = ReadNumbers(**Object, TEXT("box"), 4, BoxValues)
				&& (BoxValues[0] != BoxValues[2] || BoxValues[1] != BoxValues[3]);
			bool bHasCentroid = ReadNumbers(**Object, TEXT("centroid"), 2, CentroidValues);
			if (!bHasBox && (!bHasCentroid || (CentroidValues[0] == 0.0 && CentroidValues[1] == 0.0)))
			{
				continue;
			}
			if (!bHasCentroid)
			{
				CentroidValues[0] = (BoxValues[0] + BoxValues[2]) * 0.5;
				CentroidValues[1] = (BoxValues[1] + BoxValues[3]) * 0.5;
			}

			const FVector4	Box(BoxValues[0], BoxValues[1], BoxValues[2], BoxValues[3]);
			const FVector2D Centroid(CentroidValues[0], CentroidValues[1]);
			const FVector4	ScaleBox = bHasBox ? Box : FVector4(Centroid.X, Centroid.Y, Centroid.X, Centroid.Y);
			const FVector2D Scale = IsNormalized(ScaleBox, Centroid) ? FVector2D(1.0, 1.0) : PixelDivisor;

			FString InstanceId, PerceptionClass;
			(*Object)->TryGetStringField(TEXT("instance_id"), InstanceId);
			(*Object)->TryGetStringField(TEXT("perception_class"), PerceptionClass);

			Out.Sources.Add(Source);
			FRammsBoundingBox& Detection = Out.Boxes.AddDefaulted_GetRef();
			Detection.ID = InstanceId.IsEmpty() ? NAME_None : FName(*InstanceId);
			Detection.Label = FText::FromString(PerceptionClass.IsEmpty() ? InstanceId : PerceptionClass);
			Detection.Shape = ERammsDetectionShape::Rect;
			Detection.Centroid = Centroid / Scale;
			Detection.bHasCentroidOverride = true;
			if (bHasBox)
			{
				const FVector2D Min(FMath::Min(Box.X, Box.Z), FMath::Min(Box.Y, Box.W));
				const FVector2D Max(FMath::Max(Box.X, Box.Z), FMath::Max(Box.Y, Box.W));
				Detection.Position = Min / Scale;
				Detection.Size = (Max - Min) / Scale;
			}
			else
			{
				Detection.Position = Detection.Centroid;
				Detection.Size = FVector2D::ZeroVector;
			}
		}
		return Out.Boxes.Num();
	}
} // namespace RammsBridgeDetectionParse
