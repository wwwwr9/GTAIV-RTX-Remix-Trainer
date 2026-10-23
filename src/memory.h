#pragma once

#include "common.h"

namespace remix::memory {

struct Section {
    std::string name;
    u8* begin = nullptr;
    size_t size = 0;
    DWORD characteristics = 0;

    bool Contains(uptr address, size_t length = 1) const;
    bool IsExecutable() const;
    bool IsReadable() const;
    bool IsWritable() const;
};

struct Module {
    HMODULE handle = nullptr;
    u8* base = nullptr;
    size_t size = 0;
    std::vector<Section> sections;

    const Section* FindSection(const char* name) const;
    bool ContainsExecutable(uptr address) const;
};

bool LoadMainModule(Module* out);
uptr FindPattern(const Module& module, const char* sectionName, const char* pattern);
bool InstallJump(uptr source, void* destination, size_t length);

}  // namespace remix::memory

