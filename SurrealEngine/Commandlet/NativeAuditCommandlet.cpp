
#include "Precomp.h"
#include "NativeAuditCommandlet.h"
#include "DebuggerApp.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/Properties/UProperty.h"
#include "VM/NativeFunc.h"

NativeAuditCommandlet::NativeAuditCommandlet()
{
	SetLongFormName("nativeaudit");
	SetShortDescription("List native functions the game declares that have no handler or a different argument count");
}

static std::string DescribeFunction(UFunction* func)
{
	std::string owner = func->NativeStruct ? func->NativeStruct->Name.ToString() : (func->StructParent ? func->StructParent->Name.ToString() : "?");
	std::string sig = owner + "." + func->Name.ToString() + "(";
	bool first = true;
	std::string ret;
	for (UProperty* prop : func->Properties)
	{
		if (!AnyFlags(prop->PropFlags, PropertyFlags::Parm))
			continue;
		std::string type = prop->Class ? prop->Class->Name.ToString() : "?";
		if (AnyFlags(prop->PropFlags, PropertyFlags::ReturnParm))
		{
			ret = " -> " + type;
			continue;
		}
		if (!first)
			sig += ", ";
		first = false;
		if (AnyFlags(prop->PropFlags, PropertyFlags::OptionalParm))
			sig += "optional ";
		if (AnyFlags(prop->PropFlags, PropertyFlags::OutParm))
			sig += "out ";
		sig += type + " " + prop->Name.ToString();
	}
	return sig + ")" + ret;
}

void NativeAuditCommandlet::OnCommand(DebuggerApp* console, const std::string& args)
{
	int missing = 0, mismatched = 0, ok = 0;
	for (const NameString& pkgname : engine->packages->GetPackageNames())
	{
		Package* package = nullptr;
		try
		{
			package = engine->packages->GetPackage(pkgname);
		}
		catch (...)
		{
			continue;
		}

		for (UFunction* func : package->GetAllObjects<UFunction>())
		{
			try
			{
				func->LoadNow();
			}
			catch (const std::exception& e)
			{
				console->WriteOutput("LOADFAIL " + func->Name.ToString() + ": " + e.what() + NewLine());
				continue;
			}

			if (!AnyFlags(func->FuncFlags, FunctionFlags::Native))
				continue;

			int scriptArgs = 0;
			for (UProperty* prop : func->Properties)
			{
				if (AnyFlags(prop->PropFlags, PropertyFlags::Parm))
					scriptArgs++;
			}

			bool found = false;
			int registeredArgs = -1;
			if (func->NativeFuncIndex != 0)
			{
				found = (size_t)func->NativeFuncIndex < NativeFunctions::NativeByIndex.size() && NativeFunctions::NativeByIndex[func->NativeFuncIndex];
				auto it = NativeFunctions::ArgCountByIndex.find(func->NativeFuncIndex);
				if (it != NativeFunctions::ArgCountByIndex.end())
					registeredArgs = it->second;
			}
			else
			{
				NameString className = func->NativeStruct ? func->NativeStruct->Name : NameString();
				auto key = std::make_pair(func->Name, className);
				found = NativeFunctions::NativeByName.find(key) != NativeFunctions::NativeByName.end() && NativeFunctions::NativeByName[key];
				auto it = NativeFunctions::ArgCountByName.find(key);
				if (it != NativeFunctions::ArgCountByName.end())
					registeredArgs = it->second;
			}

			std::string line = std::to_string(func->NativeFuncIndex) + "\t" + DescribeFunction(func);
			if (!found)
			{
				console->WriteOutput("MISSING\t" + line + NewLine());
				missing++;
			}
			else if (registeredArgs != scriptArgs)
			{
				console->WriteOutput("ARGS\t" + line + "\tscript=" + std::to_string(scriptArgs) + " handler=" + std::to_string(registeredArgs) + NewLine());
				mismatched++;
			}
			else
			{
				ok++;
			}
		}
	}
	console->WriteOutput("Native audit: " + std::to_string(ok) + " ok, " + std::to_string(missing) + " missing, " + std::to_string(mismatched) + " argument count mismatches" + NewLine());
}

void NativeAuditCommandlet::OnPrintHelp(DebuggerApp* console)
{
}
