#pragma once

#include "common.h"

namespace remix::overlay {

void Start(HMODULE module);
void SetMenuState(bool visible, const std::vector<std::string>& lines, int selected,
                  const std::string& status, bool showStatus);

}  // namespace remix::overlay

