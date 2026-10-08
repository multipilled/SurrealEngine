#pragma once

#include "Packages/Core/UObject.h"

class NConsole
{
public:
	static void RegisterFunctions();

	static void ConsoleCommand(UObject* Self, const std::string& S, BitfieldBool& ReturnValue);
	static void SaveTimeDemo(UObject* Self, const std::string& S);
	static void CreateNativeFont_BB(UObject* Self, const std::string& FontName, int Height, UObject*& ReturnValue);
};
