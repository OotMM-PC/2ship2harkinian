#pragma once

#include <nlohmann/json.hpp>
#include <string>

void OotmmCheckLedger_Record(const std::string& checkId);
void OotmmCheckLedger_Forget(const std::string& checkId);
bool OotmmCheckLedger_Contains(const std::string& checkId);
void OotmmCheckLedger_Embed(nlohmann::json& save);
void OotmmCheckLedger_Init();
