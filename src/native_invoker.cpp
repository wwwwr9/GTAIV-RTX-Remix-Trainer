#include "native_invoker.h"

#include "logger.h"

namespace remix::native {
namespace {

std::unordered_map<u32, detail::NativeHandler> g_handlers;
bool g_ready = false;

struct NativeSpec {
    const char* name;
    u32 hash;
    bool required;
};

constexpr NativeSpec kNatives[] = {
    {"GET_PLAYER_ID", 0x62E319C6, true},
    {"GET_PLAYER_CHAR", 0x511454A9, true},
    {"GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS", 0x737F24F9, true},
    {"GET_CHAR_HEADING", 0x057A3AC7, true},
    {"REQUEST_MODEL", 0x502B5185, true},
    {"HAS_MODEL_LOADED", 0x4E61480A, true},
    {"CREATE_CAR", 0x2F1D6843, true},
    {"SET_CAR_HEADING", 0x75E40528, true},
    {"SET_CAR_ON_GROUND_PROPERLY", 0x0E717E98, true},
    {"MARK_MODEL_AS_NO_LONGER_NEEDED", 0x00FA0E33, true},
    {"FORCE_WEATHER", 0x7EFB5077, true},
    {"FORCE_WEATHER_NOW", 0x63737D31, true},
    {"RELEASE_WEATHER", 0x3A115D9D, true},
    {"GET_CURRENT_WEATHER", 0x27E421EA, true},
    {"SET_TIME_OF_DAY", 0x52100540, true},
    {"FORCE_TIME_OF_DAY", 0x0B9B5070, true},
    {"RELEASE_TIME_OF_DAY", 0x2AD2206E, true},
    {"GET_TIME_OF_DAY", 0x384B3876, true},
};

u32 NativeHashFromName(const char* name) {
    for (const auto& spec : kNatives) {
        if (_stricmp(spec.name, name) == 0) {
            return spec.hash;
        }
    }
    return 0;
}

bool IsDataSectionForNativeScan(const memory::Section& section) {
    if (!section.IsReadable() || section.IsExecutable()) {
        return false;
    }
    return section.name == ".rdata" || section.name == ".data" || section.name == "data" ||
           section.name == ".textbss" || section.IsWritable();
}

detail::NativeHandler TryReadHandler(const memory::Module& module, uptr address) {
    uptr candidate = 0;
    __try {
        candidate = *reinterpret_cast<const u32*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }

    if (!module.ContainsExecutable(candidate)) {
        return nullptr;
    }

    const auto* bytes = reinterpret_cast<const u8*>(candidate);
    if (bytes[0] == 0x00 || bytes[0] == 0xCC) {
        return nullptr;
    }
    return reinterpret_cast<detail::NativeHandler>(candidate);
}

detail::NativeHandler TryReadHandlerRelative(const memory::Module& module, uptr hashAddress,
                                             int relativeOffset) {
    const uptr address = hashAddress + relativeOffset;
    return TryReadHandler(module, address);
}

detail::NativeHandler ResolveNativeByHashInRegistrationCode(const memory::Module& module, u32 hash) {
    for (const auto& section : module.sections) {
        if (!section.IsExecutable() || section.size < 16) {
            continue;
        }

        const size_t scanSize = section.size - 10;
        for (size_t offset = 0; offset <= scanSize; ++offset) {
            const auto* cursor = section.begin + offset;

            // GTA IV registers natives as: push handler; push native_hash; call RegisterNative.
            if (cursor[0] != 0x68 || *reinterpret_cast<const u32*>(cursor + 1) != hash ||
                cursor[5] != 0xE8) {
                continue;
            }

            if (offset < 5 || cursor[-5] != 0x68) {
                continue;
            }

            const uptr handlerAddress = *reinterpret_cast<const u32*>(cursor - 4);
            if (!module.ContainsExecutable(handlerAddress)) {
                continue;
            }

            auto handler = reinterpret_cast<detail::NativeHandler>(handlerAddress);
            log::Info("Native hash 0x%08X matched registration code %s+0x%zX -> handler 0x%p",
                      hash, section.name.c_str(), offset, reinterpret_cast<void*>(handler));
            return handler;
        }
    }
    return nullptr;
}

constexpr int kHandlerRelativeOffsets[] = {
    -0x20,  // Common RAGE native registration layout: next, handlers[], count, hashes[].
    -0x24,
    -0x1C,
    -0x28,
    0x04,
    -0x04,
    0x08,
    -0x08,
    0x0C,
    -0x0C,
    0x10,
    -0x10,
};

detail::NativeHandler ResolveNativeByHashInRange(const memory::Module& module, u32 hash, u8* begin,
                                                 size_t size, const char* rangeName,
                                                 uptr rangeBase, bool* sawHash) {
    if (!begin || size < sizeof(u32)) {
        return nullptr;
    }

    const size_t scanSize = size - sizeof(u32);
    for (size_t offset = 0; offset <= scanSize; offset += sizeof(u32)) {
        const auto* cursor = begin + offset;
        __try {
            if (*reinterpret_cast<const u32*>(cursor) != hash) {
                continue;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }

        if (sawHash) {
            *sawHash = true;
        }
        const uptr hashAddress = reinterpret_cast<uptr>(cursor);

        for (const int relative : kHandlerRelativeOffsets) {
            if (auto handler = TryReadHandlerRelative(module, hashAddress, relative)) {
                log::Info("Native hash 0x%08X matched in %s 0x%p+0x%zX, handler rel %+d -> 0x%p",
                          hash, rangeName, reinterpret_cast<void*>(rangeBase), offset, relative,
                          reinterpret_cast<void*>(handler));
                return handler;
            }
        }

        for (int relative = -0x200; relative <= 0x200; relative += 4) {
            bool alreadyTried = false;
            for (const int known : kHandlerRelativeOffsets) {
                if (relative == known) {
                    alreadyTried = true;
                    break;
                }
            }
            if (alreadyTried) {
                continue;
            }
            if (auto handler = TryReadHandlerRelative(module, hashAddress, relative)) {
                log::Info("Native hash 0x%08X matched in %s 0x%p+0x%zX, fallback handler rel %+d -> 0x%p",
                          hash, rangeName, reinterpret_cast<void*>(rangeBase), offset, relative,
                          reinterpret_cast<void*>(handler));
                return handler;
            }
        }
    }
    return nullptr;
}

detail::NativeHandler ResolveNativeByHash(const memory::Module& module, u32 hash) {
    if (auto handler = ResolveNativeByHashInRegistrationCode(module, hash)) {
        return handler;
    }

    bool sawHash = false;
    for (const auto& section : module.sections) {
        if (!IsDataSectionForNativeScan(section) || section.size < sizeof(u32) * 2) {
            continue;
        }

        if (auto handler = ResolveNativeByHashInRange(module, hash, section.begin, section.size,
                                                      section.name.c_str(),
                                                      reinterpret_cast<uptr>(section.begin),
                                                      &sawHash)) {
            return handler;
        }
    }

    if (sawHash) {
        log::Error("Native hash 0x%08X was found, but no executable handler pointer was near it", hash);
    } else {
        log::Error("Native hash 0x%08X was not found in scanned GTAIV.exe data sections", hash);
    }
    return nullptr;
}

}  // namespace

u32 Joaat(const char* text) {
    u32 hash = 0;
    if (!text) {
        return hash;
    }

    while (*text) {
        unsigned char c = static_cast<unsigned char>(*text++);
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<unsigned char>(c + ('a' - 'A'));
        }
        hash += c;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

bool Initialize(const memory::Module& module) {
    g_handlers.clear();
    g_ready = false;

    size_t requiredMissing = 0;
    for (const auto& spec : kNatives) {
        detail::NativeHandler handler = ResolveNativeByHash(module, spec.hash);
        if (!handler) {
            log::Error("Native not resolved: %s (hash 0x%08X)%s", spec.name, spec.hash,
                       spec.required ? " [required]" : "");
            if (spec.required) {
                ++requiredMissing;
            }
            continue;
        }

        g_handlers[spec.hash] = handler;
        log::Info("Native resolved: %-36s hash 0x%08X -> 0x%p", spec.name, spec.hash,
                  reinterpret_cast<void*>(handler));
    }

    if (requiredMissing != 0) {
        log::Error("Native resolver missing %zu required native(s); trainer will stay inactive",
                   requiredMissing);
        return false;
    }

    g_ready = true;
    return true;
}

bool IsReady() {
    return g_ready;
}

namespace detail {

NativeHandler GetHandler(const char* name) {
    if (!g_ready || !name) {
        return nullptr;
    }
    const u32 hash = NativeHashFromName(name);
    if (!hash) {
        return nullptr;
    }
    const auto it = g_handlers.find(hash);
    if (it == g_handlers.end()) {
        return nullptr;
    }
    return it->second;
}

bool SafeCall(NativeHandler handler, NativeContext* context, const char* name) {
    __try {
        handler(context);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log::Error("SEH caught while invoking native %s", name ? name : "<null>");
        return false;
    }
}

}  // namespace detail

}  // namespace remix::native
