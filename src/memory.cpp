#include "memory.h"

#include "logger.h"

#include <cstdlib>

namespace remix::memory {
namespace {

struct PatternByte {
    u8 value = 0;
    bool wildcard = false;
};

std::vector<PatternByte> ParsePattern(const char* pattern) {
    std::vector<PatternByte> bytes;
    const char* cursor = pattern;
    while (*cursor) {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (!*cursor) {
            break;
        }
        if (*cursor == '?') {
            bytes.push_back({0, true});
            ++cursor;
            if (*cursor == '?') {
                ++cursor;
            }
            continue;
        }

        char token[3] = {cursor[0], cursor[1], '\0'};
        bytes.push_back({static_cast<u8>(strtoul(token, nullptr, 16)), false});
        cursor += 2;
    }
    return bytes;
}

uptr FindPatternInRange(u8* begin, size_t size, const std::vector<PatternByte>& pattern) {
    if (pattern.empty() || size < pattern.size()) {
        return 0;
    }

    const size_t last = size - pattern.size();
    for (size_t i = 0; i <= last; ++i) {
        bool matched = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (!pattern[j].wildcard && begin[i + j] != pattern[j].value) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return reinterpret_cast<uptr>(begin + i);
        }
    }
    return 0;
}

}  // namespace

bool Section::Contains(uptr address, size_t length) const {
    const uptr start = reinterpret_cast<uptr>(begin);
    const uptr end = start + size;
    return address >= start && address + length <= end;
}

bool Section::IsExecutable() const {
    return (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
}

bool Section::IsReadable() const {
    return (characteristics & IMAGE_SCN_MEM_READ) != 0;
}

bool Section::IsWritable() const {
    return (characteristics & IMAGE_SCN_MEM_WRITE) != 0;
}

const Section* Module::FindSection(const char* name) const {
    for (const auto& section : sections) {
        if (_stricmp(section.name.c_str(), name) == 0) {
            return &section;
        }
    }
    return nullptr;
}

bool Module::ContainsExecutable(uptr address) const {
    for (const auto& section : sections) {
        if (section.IsExecutable() && section.Contains(address)) {
            return true;
        }
    }
    return false;
}

bool LoadMainModule(Module* out) {
    if (!out) {
        return false;
    }

    HMODULE module = GetModuleHandleW(nullptr);
    if (!module) {
        return false;
    }

    auto* base = reinterpret_cast<u8*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    out->handle = module;
    out->base = base;
    out->size = nt->OptionalHeader.SizeOfImage;
    out->sections.clear();

    auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        char name[9]{};
        memcpy(name, section->Name, 8);
        out->sections.push_back({
            name,
            base + section->VirtualAddress,
            section->Misc.VirtualSize,
            section->Characteristics,
        });
    }

    log::Info("Loaded main module at 0x%p, size 0x%zX, sections %zu", base, out->size,
              out->sections.size());
    return true;
}

uptr FindPattern(const Module& module, const char* sectionName, const char* pattern) {
    const auto parsed = ParsePattern(pattern);
    if (parsed.empty()) {
        return 0;
    }

    if (sectionName && *sectionName) {
        const Section* section = module.FindSection(sectionName);
        if (!section) {
            return 0;
        }
        return FindPatternInRange(section->begin, section->size, parsed);
    }

    for (const auto& section : module.sections) {
        if (!section.IsExecutable()) {
            continue;
        }
        const uptr found = FindPatternInRange(section.begin, section.size, parsed);
        if (found) {
            return found;
        }
    }
    return 0;
}

bool InstallJump(uptr source, void* destination, size_t length) {
    if (!source || !destination || length < 5) {
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(source), length, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log::Error("VirtualProtect failed at 0x%p", reinterpret_cast<void*>(source));
        return false;
    }

    auto* bytes = reinterpret_cast<u8*>(source);
    bytes[0] = 0xE9;
    *reinterpret_cast<std::int32_t*>(bytes + 1) =
        static_cast<std::int32_t>(reinterpret_cast<uptr>(destination) - source - 5);
    for (size_t i = 5; i < length; ++i) {
        bytes[i] = 0x90;
    }

    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(source), length);
    DWORD ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(source), length, oldProtect, &ignored);
    return true;
}

}  // namespace remix::memory

