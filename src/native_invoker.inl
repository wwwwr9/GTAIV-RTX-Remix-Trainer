#pragma once

namespace remix::native {
namespace detail {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct ScrNativeCallContext {
    void* returnValue;
    u32 argCount;
    void* args;
    u32 dataCount;
    Vec3* originalData[4];
    Vec4 temporaryData[4];
};

class NativeContext final : public ScrNativeCallContext {
public:
    NativeContext() {
        static_assert(sizeof(void*) == 4, "GTA IV CE is a 32-bit process; build this project as Win32.");
        memset(stack_, 0, sizeof(stack_));
        returnValue = stack_;
        args = stack_;
        argCount = 0;
        dataCount = 0;
        memset(originalData, 0, sizeof(originalData));
        memset(temporaryData, 0, sizeof(temporaryData));
    }

    template <typename T>
    void Push(T value) {
        static_assert(sizeof(T) <= sizeof(u32), "GTA IV native arguments must fit in 32 bits.");
        if (argCount >= kMaxArgs) {
            return;
        }
        u32 raw = 0;
        memcpy(&raw, &value, sizeof(T));
        memcpy(stack_ + (argCount * sizeof(u32)), &raw, sizeof(raw));
        ++argCount;
    }

    template <typename T>
    T Result() {
        while (dataCount > 0) {
            --dataCount;
            Vec3* out = originalData[dataCount];
            const Vec4* temp = &temporaryData[dataCount];
            if (out) {
                out->x = temp->x;
                out->y = temp->y;
                out->z = temp->z;
            }
        }

        T value{};
        memcpy(&value, stack_, std::min(sizeof(T), sizeof(u32)));
        return value;
    }

private:
    static constexpr u32 kMaxArgs = 16;
    u8 stack_[kMaxArgs * sizeof(u32)]{};
};

using NativeHandler = void(__cdecl*)(ScrNativeCallContext*);

NativeHandler GetHandler(const char* name);
bool SafeCall(NativeHandler handler, NativeContext* context, const char* name);

inline void PushArgs(NativeContext&) {}

template <typename First, typename... Rest>
void PushArgs(NativeContext& context, First first, Rest... rest) {
    context.Push(first);
    PushArgs(context, rest...);
}

}  // namespace detail

template <typename R, typename... Args>
R Invoke(const char* name, Args... args) {
    detail::NativeHandler handler = detail::GetHandler(name);
    if (!handler) {
        return R{};
    }

    detail::NativeContext context;
    detail::PushArgs(context, args...);
    if (!detail::SafeCall(handler, &context, name)) {
        return R{};
    }
    return context.Result<R>();
}

template <typename... Args>
void InvokeVoid(const char* name, Args... args) {
    detail::NativeHandler handler = detail::GetHandler(name);
    if (!handler) {
        return;
    }

    detail::NativeContext context;
    detail::PushArgs(context, args...);
    detail::SafeCall(handler, &context, name);
}

}  // namespace remix::native

