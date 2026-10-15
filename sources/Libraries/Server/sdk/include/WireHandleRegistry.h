#pragma once

#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace dbc {
// Process-local object references in existing 32-bit packet/SQL slots. These are
// not memory addresses. Owners must erase references before reusing an object.
// Never recycle a handle during the lifetime of the process (avoids pool ABA).
class WireHandleRegistry {
public:
    template<class T> std::uint32_t Insert(T* pointer) {
        if (!pointer) return 0;
        const std::lock_guard<std::mutex> lock(m_mutex);
        auto old = m_byPointer.find(pointer);
        if (old != m_byPointer.end()) {
            if (m_byHandle.at(old->second).type != std::type_index(typeid(T)))
                throw std::logic_error("Wire handle exported with inconsistent pointer type");
            return old->second;
        }
        // endeMem and several legacy packet consumers use signed SQL INT/long.
        if (m_next > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()))
            throw std::overflow_error("Wire handle space exhausted; restart the server");
        const auto handle = m_next++;
        m_byHandle.emplace(handle, Entry{pointer, std::type_index(typeid(T))});
        try { m_byPointer.emplace(pointer, handle); }
        catch (...) { m_byHandle.erase(handle); throw; }
        return handle;
    }

    template<class T> T* Resolve(std::uint32_t handle) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        auto entry = m_byHandle.find(handle);
        if (entry == m_byHandle.end() || entry->second.type != std::type_index(typeid(T)))
            return nullptr;
        return static_cast<T*>(entry->second.pointer);
    }

    void Erase(const void* pointer) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        auto entry = m_byPointer.find(pointer);
        if (entry == m_byPointer.end()) return;
        m_byHandle.erase(entry->second);
        m_byPointer.erase(entry);
    }

    static WireHandleRegistry& Instance() {
        // Outlive static object destructors that invalidate their own references.
        static auto* registry = new WireHandleRegistry;
        return *registry;
    }
private:
    struct Entry { void* pointer; std::type_index type; };
    std::mutex m_mutex;
    std::uint32_t m_next = 1;
    std::unordered_map<const void*, std::uint32_t> m_byPointer;
    std::unordered_map<std::uint32_t, Entry> m_byHandle;
};
} // namespace dbc
