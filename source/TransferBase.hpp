#pragma once
#include "common.hpp"
#include "TypeTree.hpp"

template<Revision R, Variant V>
class TransferBase;

DECLARE_REVISION(TransferBase, Revision::V2022_3_0_0);
DECLARE_REVISION_VARIANT(TransferBase, Revision::V2022_3_0_0, Variant::Editor);
DECLARE_REVISION_VARIANT(TransferBase, Revision::V2022_3_2_0, Variant::Editor);
DECLARE_REVISION_VARIANT(TransferBase, Revision::V2022_3_61_1, Variant::Editor);

//
// 4.x
//

template<Revision R, Variant V>
class TransferBase
{
protected:
    TransferInstructionFlags<R, V> m_Flags = {};
    void *m_UserData = nullptr;
};

//
// 2022.3
//

DEFINE_REVISION(class, TransferBase, Revision::V2022_3_0_0)
{
protected:
    TransferInstructionFlags<R, V> m_Flags = {};
    void *m_UserData = nullptr;
    void *m_ManagedReferencesTransferState = nullptr;
    void *m_GetManagedReferencesRegistry = nullptr;
    void *m_CustomLSOIResolver = nullptr;
    char const *m_ResourceImagePathPrefix = nullptr;
    bool m_MissingManagedTypes = false;
};

DEFINE_REVISION_VARIANT(class, TransferBase, Revision::V2022_3_0_0, Variant::Editor)
{
protected:
    TransferInstructionFlags<R, Variant::Editor> m_Flags = {};
    void *m_UserData = nullptr;
    void *m_ManagedReferencesTransferState = nullptr;
    void *m_GetManagedReferencesRegistry = nullptr;
    void *m_CustomLSOIResolver = nullptr;
    char const *m_ResourceImagePathPrefix = nullptr;

    // +0x38: a core::string_with_label, verified against Tuanjie 2022.3.61t15
    // (TransferBase ctor at 0x7FF77C80C470). Layout:
    //   +0x00..0x24  inline string storage (24-byte small-string buffer,
    //                "remaining capacity" byte at +0x18, small-string flag at +0x20)
    //   +0x24        MemLabelId used when the string has to grow
    //
    // AnimationClip's ACLClip::CompatibleWithOldVersionTransfer compares this
    // string against build versions such as "2022.3.55t4"/"2022.3.48t3".
    // Leaving it uninitialised made the engine read a garbage length from what
    // used to be our padding and call core::string::grow() with a
    // multi-terabyte value.
    char m_VersionString[0x24] = {};
    MemLabelId<R, Variant::Editor> m_VersionStringLabel;

    // +0x68, +0x70: zero-initialised in the engine's constructor as well.
    void *m_Unknown68 = nullptr;
    void *m_Unknown70 = nullptr;
    uint8_t m_Unknown78 = 0;

public:
    TransferBase()
    {
        // Small-string form of an empty core::string.
        m_VersionString[0] = '\0';
        m_VersionString[0x18] = 0x18;
        m_VersionString[0x20] = 1;

        // The engine fills this in from SetCurrentMemoryOwner, which resolves
        // to the static default label { identifier = 0x4B, salt = 0, root = -1 }.
        m_VersionStringLabel.identifier = 0x4B;
        if constexpr (requires { m_VersionStringLabel.m_RootReferenceWithSalt; })
        {
            m_VersionStringLabel.m_RootReferenceWithSalt.m_Salt = 0;
            m_VersionStringLabel.m_RootReferenceWithSalt.m_RootReferenceIndex = 0xFFFFFFFF;
        }
    }

public:
    // Probe/diagnostic hook: fill the version string the engine's
    // "compatible with old version" checks compare against.
    void SetVersionString(char const *text)
    {
        std::memset(m_VersionString, 0, sizeof(m_VersionString));

        const auto textLength = std::strlen(text);
        const auto capacity = sizeof(m_VersionString) - 1;
        const auto length = textLength < capacity ? textLength : capacity;

        std::memcpy(m_VersionString, text, length);
        m_VersionString[0x18] = static_cast<char>(0x18 - length);
        m_VersionString[0x20] = 1;
    }

    virtual ~TransferBase() = default;
};

// The engine's editor TransferBase for 2022.3 is 0x80 bytes (see layout comment).
static_assert(sizeof(TransferBase<Revision::V2022_3_0_0, Variant::Editor>) == 0x80,
    "TransferBase must match the engine layout for 2022.3");

//
// 2022.3.2 (Tuanjie 1.0.x / early builds)
//
// This era has no version string; the transfer object is 0x50 bytes in total,
// which is why GenerateTypeTreeTransfer's m_TypeTree ends up at +0x50.
// None of the base fields past m_Flags are read by BeginTransfer or
// LogUnalignedTransfer, so they are kept as zeroed storage.
//

//
// 2022.3.61 (Tuanjie 1.6.x)
//
// 1.6 went back to the 0x80 TransferBase after the 0x50 one that 1.0-1.2
// used, so the modern body is re-established here; declarations are ranges
// and cannot return to an older shape on their own.
//

DEFINE_REVISION_VARIANT(class, TransferBase, Revision::V2022_3_61_1, Variant::Editor)
{
protected:
    TransferInstructionFlags<R, Variant::Editor> m_Flags = {};
    void *m_UserData = nullptr;
    void *m_ManagedReferencesTransferState = nullptr;
    void *m_GetManagedReferencesRegistry = nullptr;
    void *m_CustomLSOIResolver = nullptr;
    char const *m_ResourceImagePathPrefix = nullptr;

    // +0x38: a core::string_with_label, verified against Tuanjie 2022.3.61t15
    // (TransferBase ctor at 0x7FF77C80C470). Layout:
    //   +0x00..0x24  inline string storage (24-byte small-string buffer,
    //                "remaining capacity" byte at +0x18, small-string flag at +0x20)
    //   +0x24        MemLabelId used when the string has to grow
    //
    // AnimationClip's ACLClip::CompatibleWithOldVersionTransfer compares this
    // string against build versions such as "2022.3.55t4"/"2022.3.48t3".
    // Leaving it uninitialised made the engine read a garbage length from what
    // used to be our padding and call core::string::grow() with a
    // multi-terabyte value.
    char m_VersionString[0x24] = {};
    MemLabelId<R, Variant::Editor> m_VersionStringLabel;

    // +0x68, +0x70: zero-initialised in the engine's constructor as well.
    void *m_Unknown68 = nullptr;
    void *m_Unknown70 = nullptr;
    uint8_t m_Unknown78 = 0;

public:
    TransferBase()
    {
        // Small-string form of an empty core::string.
        m_VersionString[0] = '\0';
        m_VersionString[0x18] = 0x18;
        m_VersionString[0x20] = 1;

        // The engine fills this in from SetCurrentMemoryOwner, which resolves
        // to the static default label { identifier = 0x4B, salt = 0, root = -1 }.
        m_VersionStringLabel.identifier = 0x4B;
        if constexpr (requires { m_VersionStringLabel.m_RootReferenceWithSalt; })
        {
            m_VersionStringLabel.m_RootReferenceWithSalt.m_Salt = 0;
            m_VersionStringLabel.m_RootReferenceWithSalt.m_RootReferenceIndex = 0xFFFFFFFF;
        }
    }

public:
    // Probe/diagnostic hook: fill the version string the engine's
    // "compatible with old version" checks compare against.
    void SetVersionString(char const *text)
    {
        std::memset(m_VersionString, 0, sizeof(m_VersionString));

        const auto textLength = std::strlen(text);
        const auto capacity = sizeof(m_VersionString) - 1;
        const auto length = textLength < capacity ? textLength : capacity;

        std::memcpy(m_VersionString, text, length);
        m_VersionString[0x18] = static_cast<char>(0x18 - length);
        m_VersionString[0x20] = 1;
    }

    virtual ~TransferBase() = default;
};

static_assert(sizeof(TransferBase<Revision::V2022_3_61_1, Variant::Editor>) == 0x80,
    "Tuanjie 1.6 TransferBase is the 0x80 layout");

DEFINE_REVISION_VARIANT(class, TransferBase, Revision::V2022_3_2_0, Variant::Editor)
{
protected:
    TransferInstructionFlags<R, Variant::Editor> m_Flags = {};
    uint8_t m_Reserved[0x50 - 0x10] = {};

public:
    virtual ~TransferBase() = default;
};

static_assert(sizeof(TransferBase<Revision::V2022_3_2_0, Variant::Editor>) == 0x50,
    "early Tuanjie TransferBase must be 0x50 bytes");
