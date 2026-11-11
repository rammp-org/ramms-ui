// RammsJsonLibrary.cpp

#include "RammsJsonLibrary.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogRammsJson, Log, All);

bool URammsJsonLibrary::JsonStringToStruct(const FString& JsonString, int32& OutStruct)
{
	// Blueprints call execJsonStringToStruct; this body only satisfies the declaration.
	check(0);
	return false;
}

bool URammsJsonLibrary::JsonStringToStructNative(const FString& JsonString, const UScriptStruct* Struct, void* OutStruct)
{
	if (!Struct || !OutStruct)
	{
		return false;
	}

	TSharedPtr<FJsonObject>			Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		UE_LOG(LogRammsJson, Warning, TEXT("Json String To Struct (%s): not a JSON object: %s"), *Struct->GetName(), *JsonString.Left(256));
		return false;
	}

	FText Reason;
	if (!FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), Struct, OutStruct, 0, 0, false, &Reason))
	{
		UE_LOG(LogRammsJson, Warning, TEXT("Json String To Struct (%s): doesn't fit: %s"), *Struct->GetName(), *Reason.ToString());
		return false;
	}

	UE_LOG(LogRammsJson, Verbose, TEXT("Json String To Struct (%s): %s"), *Struct->GetName(), *JsonString.Left(256));
	return true;
}
