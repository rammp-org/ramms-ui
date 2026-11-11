// RammsJsonLibrary.h
//
// JSON helpers for Blueprints. JsonBlueprintUtilities can read one field of a JSON object into a
// struct ("Get Field"), but not a whole object; the bridge sends whole objects (the InteractionPanel
// JSON is one), so this fills a struct from the object itself.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RammsJsonLibrary.generated.h"

UCLASS()
class RAMMSUI_API URammsJsonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fill a struct from a JSON object string: each key fills the field of the same name (a Blueprint
	 * struct's fields go by the names shown in the editor), nested objects and arrays included. Keys
	 * with no field are ignored and fields with no key keep their values.
	 * @return false, with a warning in the log, when the string isn't a JSON object or doesn't fit the struct.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Ramms|Json",
		meta = (DisplayName = "Json String To Struct", CustomStructureParam = "OutStruct"))
	static UPARAM(DisplayName = "Success") bool JsonStringToStruct(const FString& JsonString, int32& OutStruct);

	/** The native version: fill OutStruct, of type Struct, from JsonString. */
	static bool JsonStringToStructNative(const FString& JsonString, const UScriptStruct* Struct, void* OutStruct);

	DECLARE_FUNCTION(execJsonStringToStruct)
	{
		P_GET_PROPERTY(FStrProperty, JsonString);
		Stack.StepCompiledIn<FStructProperty>(nullptr);
		const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
		void*				   StructPtr = Stack.MostRecentPropertyAddress;
		P_FINISH;

		bool bSuccess = false;
		P_NATIVE_BEGIN;
		bSuccess = StructProperty && StructPtr && JsonStringToStructNative(JsonString, StructProperty->Struct, StructPtr);
		P_NATIVE_END;
		*static_cast<bool*>(RESULT_PARAM) = bSuccess;
	}
};
