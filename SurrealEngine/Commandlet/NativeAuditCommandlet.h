#pragma once

#include "Commandlet/Commandlet.h"

class NativeAuditCommandlet : public Commandlet
{
public:
	NativeAuditCommandlet();

	void OnCommand(DebuggerApp* console, const std::string& args) override;
	void OnPrintHelp(DebuggerApp* console) override;
};
