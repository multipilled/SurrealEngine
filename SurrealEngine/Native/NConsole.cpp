
#include "Precomp.h"
#include "NConsole.h"
#include "VM/NativeFunc.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/Resources/UFont.h"

void NConsole::RegisterFunctions()
{
	RegisterVMNativeFunc_2("Console", "ConsoleCommand", &NConsole::ConsoleCommand, 0);
	RegisterVMNativeFunc_1("Console", "SaveTimeDemo", &NConsole::SaveTimeDemo, 0);
	if (engine->LaunchInfo.IsBrotherBear())
	{
		RegisterVMNativeFunc_3("Console", "CreateNativeFont", &NConsole::CreateNativeFont_BB, 0);
	}
}

void NConsole::ConsoleCommand(UObject* Self, const std::string& S, BitfieldBool& ReturnValue)
{
	std::string result = engine->ConsoleCommand(Self, S, ReturnValue);
	if (!result.empty())
	{
		// XXX: This depends on how Engine.Console.Message is declared
		// might break for other games
		Array<ExpressionValue> exprs = {};
		if (engine->LaunchInfo.ue1Version > 219)
			exprs.push_back(ExpressionValue::ObjectValue(nullptr));
		
		exprs.push_back(ExpressionValue::StringValue(result));
		exprs.push_back(ExpressionValue::NameValue("Console"));

		CallEvent(Self, "Message", exprs);
	}
}

void NConsole::SaveTimeDemo(UObject* Self, const std::string& S)
{
	LogUnimplemented("Console.SaveTimeDemo(" + S + ")");
}

void NConsole::CreateNativeFont_BB(UObject* Self, const std::string& FontName, int Height, UObject*& ReturnValue)
{
	// The engine cannot rasterize operating system fonts yet. Hand back the stock canvas font whose
	// glyph height is closest to the requested height so callers still get something they can draw with.
	LogUnimplemented("Console.CreateNativeFont(" + FontName + ", " + std::to_string(Height) + ")");

	UFont* bestFont = nullptr;
	if (engine->canvas)
	{
		int bestDiff = 0;
		for (UFont* font : { engine->canvas->SmallFont(), engine->canvas->MedFont(), engine->canvas->BigFont(), engine->canvas->LargeFont() })
		{
			if (!font)
				continue;
			int diff = std::abs(font->GetGlyph('A').VSize - std::abs(Height));
			if (!bestFont || diff < bestDiff)
			{
				bestFont = font;
				bestDiff = diff;
			}
		}
	}
	ReturnValue = bestFont;
}
