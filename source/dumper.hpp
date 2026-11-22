#pragma once
#include "common.hpp"

#include <array>
#include <span>
#include <functional>
#include <algorithm>
#include <ranges>
#include <cstring>
#include <new>

#include "MemLabelId.hpp"
#include "Object.hpp"
#include "GenerateTypeTreeTransfer.hpp"
#include "platform_impl.hpp"
#include "dumper.hpp"
#include "executable.hpp"
#include "binary_output.hpp"

struct IDumper
{
    virtual void Run() = 0;
};

template<Revision R, Variant V, typename TPlatformImpl>
    requires IsPlatformImpl<R, V, TPlatformImpl>
class Dumper : public IDumper
{
    using Object = ::Object<R, V>;
    using TypeTree = ::TypeTree<R, V>;
    using TypeTreeShareableData = ::TypeTreeShareableData<R, V>;
    using TransferInstructionFlags = ::TransferInstructionFlags<R, V>;
    using MemLabelId = ::MemLabelId<R, V>;
    using GenerateTypeTreeTransfer = ::GenerateTypeTreeTransfer<R, V>;
    using RuntimeTypeArray = ::RuntimeTypeArray<R, V>;
    using RTTI = ::RTTI<R, V>;

    using DumpedTypeTreeWriter = ::DumpedTypeTreeWriter<R, V>;

    MemLabelId MakeLabel()
    {
        MemLabelId label = {};

        if constexpr (requires { label.m_RootReferenceWithSalt; })
        {
            // The engine's own MemLabelId for type trees, taken from the static
            // it passes to TypeTree::TypeTree(MemLabelId const&):
            // { identifier = 0x55, salt = 0, root = -1 }.
            label.identifier = 0x55;
            label.m_RootReferenceWithSalt.m_Salt = 0;
            label.m_RootReferenceWithSalt.m_RootReferenceIndex = 0xFFFFFFFF;
        }
        else
        {
            label.identifier = 0x55;
        }

        return label;
    }

    // Fallback for builds where the engine vftable could not be located: the
    // engine sometimes calls a virtual on the transfer object (Mesh::Transfer in
    // 1.5.2 / 1.6.0) and ignores the result, so a table of no-ops is harmless -
    // and far better than calling through our own C++ vtable.
    static void TransferVirtualNoOp() {}

    static void *const *FallbackTransferVtable()
    {
        static void *const kEntries[] = {
            reinterpret_cast<void *>(&TransferVirtualNoOp),
            reinterpret_cast<void *>(&TransferVirtualNoOp),
            reinterpret_cast<void *>(&TransferVirtualNoOp),
            reinterpret_cast<void *>(&TransferVirtualNoOp),
        };
        return kEntries;
    }

    void RedirectTransfer(Object *object, GenerateTypeTreeTransfer &transfer)
    {
        object->VirtualRedirectTransfer(transfer);
    }

    bool IsValidPointer(void const *ptr, const size_t size, const uint8_t expectedProtection = ExecutableSection::kSectionProtectionRead)
    {
        return std::ranges::any_of(PlatformImpl.GetExecutableSections(), [&](const ExecutableSection &section)
        {
            return section.IsValidPointer(ptr, size, expectedProtection);
        });
    }

    bool IsValidClassName(char const *className)
    {
        if (!IsValidPointer(className, 1))
            return false;

        for (size_t i = 0; i < 256; i++)
        {
            if (!IsValidPointer(className + i, 1))
                return false;

            const auto c = static_cast<unsigned char>(className[i]);
            if (c == 0)
                return i > 0;

            if (c < 0x20 || c > 0x7E)
                return false;
        }

        return false;
    }

    bool StringEquals(char const *p, const std::string_view other)
    {
        if (!IsValidPointer(p, other.size() + 1))
            return false;

        if (p[other.size()] != '\0')
            return false;

        return std::string_view(p, other.size()) == other;
    }

    bool IsValidRuntimeTypeArray(RuntimeTypeArray const *pArray)
    {
        if (!IsValidPointer(pArray, sizeof(RuntimeTypeArray)))
            return false;

        if constexpr (R >= Revision::V2022_3_0_0)
        {
            if (pArray->Count < 2 || pArray->Count > pArray->Types.size())
                return false;

            for (int i = 0; i < 2; i++)
            {
                if (!IsValidPointer(pArray->Types[i], sizeof(RTTI)))
                {
                    return false;
                }

                if (pArray->Types[i]->factory && !IsValidPointer(reinterpret_cast<void const *>(pArray->Types[i]->factory), 1))
                {
                    return false;
                }
            }

            if (pArray->Types[0]->persistentTypeID != 0)
                return false;

            if (pArray->Types[1]->base != pArray->Types[0])
                return false;

            if (!StringEquals(pArray->Types[0]->className, "Object"))
                return false;

            for (int i = 0; i < pArray->Count; i++)
            {
                auto const *pRTTI = pArray->Types[i];

                if (!IsValidPointer(pRTTI, sizeof(RTTI)))
                    return false;

                if (!IsValidClassName(pRTTI->className))
                    return false;

                if (pRTTI->factory != nullptr
                    && !IsValidPointer(reinterpret_cast<void const *>(pRTTI->factory), 1, ExecutableSection::kSectionProtectionExecute))
                {
                    return false;
                }

                if (pRTTI->base != nullptr && !IsValidPointer(pRTTI->base, sizeof(RTTI)))
                    return false;
            }

            return true;
        }

        return false;
    }

    RuntimeTypeArray const *GetRuntimeTypeArray()
    {
        PlatformImpl.DebugLog("Retrieving RuntimeTypeArray");

        for (const auto &section : PlatformImpl.GetExecutableSections() | std::views::filter([](const ExecutableSection &x)
        {
            if ((x.Protection & ExecutableSection::kSectionProtectionRead) == 0)
                return false;

            if (x.Data.size() < sizeof(RuntimeTypeArray))
                return false;

            return true;
        }))
        {
            for (size_t i = 0; i < section.Data.size() - sizeof(RuntimeTypeArray); i += sizeof(uintptr_t))
            {
                auto pArray = reinterpret_cast<RuntimeTypeArray const *>(section.Data.data() + i);

                if (!IsValidRuntimeTypeArray(pArray))
                    continue;

                return pArray;
            }
        }

        PlatformImpl.DebugLog("Failed to find RuntimeTypeArray");

        return nullptr;
    }

    char const *GetCommonStringBuffer()
    {
        PlatformImpl.DebugLog("Retrieving common string buffer");

        static constexpr auto kCommonStringBufferPattern = std::span("AABB\0AnimationClip");
        const auto searcher = std::boyer_moore_horspool_searcher(std::cbegin(kCommonStringBufferPattern), std::cend(kCommonStringBufferPattern));

        for (const auto &section : PlatformImpl.GetExecutableSections())
        {
            if ((section.Protection & ExecutableSection::kSectionProtectionRead) == 0)
                continue;

            const auto region = section.Data;
            if (const auto result = std::search(std::cbegin(region), std::cend(region), searcher);
                result != std::cend(region))
            {
                return region.data() + std::distance(std::cbegin(region), result);
            }
        }

        PlatformImpl.DebugLog("Failed to find common string buffer");
        return nullptr;
    }
public:
    void Run() override
    {
        {
            char banner[96];
            std::snprintf(banner, sizeof(banner),
                "Dumper started (transfer vftable=0x%llX)",
                static_cast<unsigned long long>(typetree_ripper::g_TransferVftable));
            PlatformImpl.DebugLog(banner);
        }

        if constexpr (R >= Revision::V2022_3_0_0)
        {
            auto pArray = GetRuntimeTypeArray();
            auto pTable = GetCommonStringBuffer();

            const auto dumpTypes = [&](const TransferInstructionFlags &flags, const std::string_view outputName)
            {
                for (int i = 0; i < pArray->Count; i++)
                {
                    PlatformImpl.DebugLog((std::string("Processing type ") + pArray->Types[i]->className).c_str());

                    RTTI *pRTTI = pArray->Types[i];

                    const auto label = MakeLabel();
                    TypeTreeShareableData data(label);
                    TypeTree tree(&data, label);

                    Object * object = nullptr;
                    if (!pRTTI->isAbstract && pRTTI->factory)
                    {
                        object = pRTTI->factory(label, kCreateObjectDefault);
                        // "THX_Proxy" (what is this) can't be created, so it returns nothing.
                        // but it also is not abstract, curious!
                        if (object)
                        {
                            GenerateTypeTreeTransfer transfer(tree, flags, object, pRTTI->size);

                            *reinterpret_cast<void **>(&transfer) =
                                typetree_ripper::g_TransferVftable != 0
                                    ? reinterpret_cast<void *>(typetree_ripper::g_TransferVftable)
                                    : const_cast<void **>(FallbackTransferVtable());

                            RedirectTransfer(object, transfer);

                            char state[256];
                            std::snprintf(state, sizeof(state), "  tree: nodes=%zu strings=%zu",
                                static_cast<size_t>(tree.Nodes().size()),
                                static_cast<size_t>(tree.StringsBuffer().size()));
                            PlatformImpl.DebugLog(state);
                        }
                    }

                    Writer.Add(pRTTI, tree, flags, pTable, object);
                }

                PlatformImpl.DebugLog("Dumped types, now writing to file");

                auto outputStream = PlatformImpl.CreateOutputFile(outputName.data());
                Writer.Write(outputStream);
            };

            // Always dump release types
            dumpTypes(TransferInstructionFlags::kSerializeGameRelease, "release.ttbin");

            // If we are in an editor, also dump the editor types
            if constexpr (V == Variant::Editor)
            {
                dumpTypes(TransferInstructionFlags::kNone, "editor.ttbin");
            }
        }
    }

    static Dumper Instance;
private:
    TPlatformImpl PlatformImpl{};
    DumpedTypeTreeWriter Writer{};
};

template<Revision R, Variant V, typename TPlatformImpl>
    requires IsPlatformImpl<R, V, TPlatformImpl>
Dumper<R, V, TPlatformImpl> Dumper<R, V, TPlatformImpl>::Instance;

template<Variant V, template<Revision, Variant> typename TPlatformImpl>
class DumperArray
{
    // Layer of indirection added so that we can static_assert on the platform implementation concept
    template<Revision R>
    struct DumperInstantiationHelper
    {
        static_assert(IsPlatformImpl<R, V, TPlatformImpl<R, V>>, "Dumper platform implementation is ill-formed");
        using DumperType = Dumper<R, V, TPlatformImpl<R, V>>;
    };

public:
    static constexpr auto Instances = []<size_t... I>(std::index_sequence<I...>)
    {
        return std::array<IDumper *, sizeof...(I)>{ &DumperInstantiationHelper<static_cast<Revision>(I)>::DumperType::Instance... };
    }(std::make_index_sequence<std::to_underlying(Revision::Count)>{});
};

template<template<Revision, Variant> typename TPlatformImpl>
static constexpr auto DumperVariantInstances = []<size_t... I>(std::index_sequence<I...>)
{
    return std::array<const std::array<IDumper *, std::to_underlying(Revision::Count)>, sizeof...(I)>{ DumperArray<static_cast<Variant>(I), TPlatformImpl>::Instances... };
}(std::make_index_sequence<std::to_underlying(Variant::Count)>{});

template<template<Revision, Variant> typename TPlatformImpl>
void RunDumper(const Revision revision, const Variant variant)
{
    const auto intRevision = std::to_underlying(revision);
    const auto intVariant = std::to_underlying(variant);

    return DumperVariantInstances<TPlatformImpl>[intVariant][intRevision]->Run();
}
