#pragma once
#include <array>
#include <optional>
#include <sstream>
#include <tuple>
#include <utility>

#define FOR_EACH_VARIANT(X) \
    X(Editor) \
    X(Runtime) \
    X(RuntimeDev)

enum class Variant
{
#define DEFINE_VARIANT_ENUM_ENTRY(Name) Name,
    FOR_EACH_VARIANT(DEFINE_VARIANT_ENUM_ENTRY)
#undef DEFINE_VARIANT_ENUM_ENTRY

    Count
};

template<typename StringViewType>
    requires std::convertible_to<StringViewType, std::string_view> || std::convertible_to<StringViewType, std::wstring_view>
std::optional<Variant> VariantStringToVariant(const StringViewType name)
{
    const auto convertString = [name](const auto names) -> std::optional<Variant>
    {
        for (const auto &[variantName, variant] : names)
        {
            if (name == variantName)
            {
                return variant;
            }
        }

        return std::nullopt;
    };

    if constexpr (std::convertible_to<StringViewType, std::string_view>)
    {
        constexpr std::array kVariantNames = {
#define DEFINE_VARIANT_NAME_A_ENTRY(Name) std::make_tuple(#Name, Variant::Name),  
            FOR_EACH_VARIANT(DEFINE_VARIANT_NAME_A_ENTRY)
#undef DEFINE_VARIANT_NAME_A_ENTRY
        };

        return convertString(kVariantNames);
    }
    else
    {
        constexpr std::array kVariantNames = {
#define DEFINE_VARIANT_NAME_W_ENTRY(Name) std::make_tuple(L""#Name, Variant::Name),  
            FOR_EACH_VARIANT(DEFINE_VARIANT_NAME_W_ENTRY)
#undef DEFINE_VARIANT_NAME_W_ENTRY
        };

        return convertString(kVariantNames);
    }
}

// Windows paths are case insensitive but the engine names we compare against are
// not: launching "tuanjie.exe" yields exactly that in the module path, so an
// exact comparison against "Tuanjie.exe" fails and DetectVariant() aborts.
inline bool EqualsIgnoreCase(const std::string_view left, const std::string_view right)
{
    if (left.size() != right.size())
        return false;

    for (size_t i = 0; i < left.size(); i++)
    {
        const auto lower = [](const char c)
        {
            return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        };

        if (lower(left[i]) != lower(right[i]))
            return false;
    }

    return true;
}

inline std::optional<Variant> ExecutableNameToVariant(const std::string_view name)
{
    constexpr std::array kExecutableVariants = {
        std::make_tuple("Unity.exe", Variant::Editor),
        std::make_tuple("Unity.com", Variant::Editor),
        std::make_tuple("Tuanjie.exe", Variant::Editor),
    };

    for (const auto& [executableName, variant] : kExecutableVariants)
    {
        if (EqualsIgnoreCase(name, executableName))
        {
            return variant;
        }
    }

    return std::nullopt;
}

inline std::string_view VariantToString(const Variant variant)
{
    constexpr std::array kVariantNames = {
#define DEFINE_VARIANT_STRING_ENTRY(Name) #Name,  
            FOR_EACH_VARIANT(DEFINE_VARIANT_STRING_ENTRY)
#undef DEFINE_VARIANT_STRING_ENTRY
    };

    return kVariantNames[std::to_underlying(variant)];
}

#undef FOR_EACH_VARIANT

#define FOR_EACH_REVISION(X) \
    X(0, 0, 0, 0) \
    X(2022, 3, 0, 0) \
    X(2022, 3, 2, 0) \
    X(2022, 3, 2, 8) \
    X(2022, 3, 2, 17) \
    X(2022, 3, 27, 0) \
    X(2022, 3, 61, 1) \
    X(2022, 3, 61, 2) \
    X(2022, 3, 62, 0) \
    X(2022, 3, 62, 8) \
    X(2023, 1, 0, 0) \
    X(6000, 5, 0, 0)

enum class Revision
{
#define DEFINE_REVISION_ENUM_ENTRY(major, minor, patch, build) V##major##_##minor##_##patch##_##build,
    FOR_EACH_REVISION(DEFINE_REVISION_ENUM_ENTRY)
#undef DEFINE_REVISION_ENUM_ENTRY

    Count,
};

// Full layout identity, including the Tuanjie "tN" counter.
using RevisionVersion = std::tuple<uint16_t, uint8_t, uint8_t, uint16_t>;

// What the .ttbin header records: the 4th element is a layout discriminator, not
// part of the asset version number.
using RevisionRelease = std::tuple<uint16_t, uint8_t, uint8_t>;

template <typename StringType, typename StringStreamType, auto Delimiter>
RevisionVersion ParseVersionString(const StringType &value)
{
    // spanstream doesn't exist in the android ndk :(
    StringStreamType versionParser(value);

    StringType versionComponent;
    std::getline(versionParser, versionComponent, Delimiter);
    const auto major = static_cast<uint16_t>(std::stoi(versionComponent));

    std::getline(versionParser, versionComponent, Delimiter);
    const auto minor = static_cast<uint8_t>(std::stoi(versionComponent));

    std::getline(versionParser, versionComponent, Delimiter);

    // As the passed in version might have a suffix, like 2023.5.0f1, we need to find where the patch number ends
    // This parses the "0f1" part of the version

    const auto patchEndIndex = [versionComponent]
    {
        for (size_t i = 0; i < versionComponent.size(); i++)
        {
            if (versionComponent[i] - '0' > 9)
                return i;
        }

        return versionComponent.size();
    }();

    const auto patch = static_cast<uint8_t>(std::stoi(versionComponent.substr(0, patchEndIndex)));

    // Only Tuanjie's "t" counter separates builds that share major.minor.patch
    // (2022.3.2t5 vs 2022.3.2t8). Unity's "f"/"p"/"b" suffixes are release
    // counters of the *same* layout, so they stay at build == 0 - otherwise
    // 2022.3.61f1 and 2022.3.61f6 would split into different buckets.
    uint16_t build = 0;

    if (patchEndIndex < versionComponent.size() && versionComponent[patchEndIndex] == 't')
    {
        auto buildEndIndex = patchEndIndex + 1;

        while (buildEndIndex < versionComponent.size() && versionComponent[buildEndIndex] - '0' <= 9)
            buildEndIndex++;

        if (buildEndIndex > patchEndIndex + 1)
        {
            build = static_cast<uint16_t>(std::stoi(
                versionComponent.substr(patchEndIndex + 1, buildEndIndex - patchEndIndex - 1)));
        }
    }

    return std::make_tuple(major, minor, patch, build);
}

inline std::optional<Revision> VersionToRevision(const uint16_t major, const uint8_t minor, const uint8_t patch,
    const uint16_t build = 0)
{
    constexpr std::array kRevisionVersions = {
#define DEFINE_REVISION_VERSION_ENTRY(major, minor, patch, build) std::make_tuple(major, minor, patch, build, Revision::V##major##_##minor##_##patch##_##build), 
        FOR_EACH_REVISION(DEFINE_REVISION_VERSION_ENTRY)
#undef DEFINE_REVISION_VERSION_ENTRY
    };

    std::optional<Revision> versionRevision;

    for (const auto &[revisionMajor, revisionMinor, revisionPatch, revisionBuild, revision] : kRevisionVersions)
    {
        if (std::tie(revisionMajor, revisionMinor, revisionPatch, revisionBuild)
            <= std::tie(major, minor, patch, build))
        {
            versionRevision = revision;
        }
    }

    return versionRevision;
}

template<typename StringType>
    requires std::convertible_to<StringType, std::string> || std::convertible_to<StringType, std::wstring>
std::optional<Revision> VersionStringToRevision(const StringType& value)
{
    RevisionVersion versionComponents;
    
    if constexpr (std::convertible_to<StringType, std::string>)
    {
        versionComponents = ParseVersionString<std::string, std::istringstream, '.'>(value);
    }
    else
    {
        versionComponents = ParseVersionString<std::wstring, std::basic_istringstream<wchar_t>, L'.'>(value);
    }

    const auto& [major, minor, patch, build] = versionComponents;
    return VersionToRevision(major, minor, patch, build);
}

inline RevisionRelease RevisionToVersion(const Revision revision)
{
    constexpr std::array kRevisionVersions = {
#define DEFINE_REVISION_VERSION_TUPLE_ENTRY(major, minor, patch, build) std::make_tuple<uint16_t, uint8_t, uint8_t>(major, minor, patch), 
        FOR_EACH_REVISION(DEFINE_REVISION_VERSION_TUPLE_ENTRY)
#undef DEFINE_REVISION_VERSION_TUPLE_ENTRY
    };

    return kRevisionVersions[std::to_underlying(revision)];
}

#undef FOR_EACH_REVISION

namespace details
{
    template<template<Revision, Variant> typename T, Revision R, Variant V>
    struct HasExplicitRevision : std::false_type {};

    template<template<Revision, Variant> typename T, Revision R, Variant V>
    consteval Revision GetNextExplicitRevisionRecursive()
    {
        if constexpr (R >= Revision::Count)
        {
            return Revision::Count;
        }
        else if constexpr (HasExplicitRevision<T, R, V>::value)
        {
            return R;
        }
        else
        {
            return GetNextExplicitRevisionRecursive<T, static_cast<Revision>(static_cast<int>(R) + 1), V>();
        }
    }

    template<template<Revision, Variant> typename T, Revision R, Variant V>
    consteval Revision GetNextExplicitRevision()
    {
        return GetNextExplicitRevisionRecursive<T, static_cast<Revision>(static_cast<int>(R) + 1), V>();
    }

    template<template<Revision, Variant> typename T, Revision R, Variant V, Revision Min>
    concept ExplicitRevision = R >= Min && R < GetNextExplicitRevision<T, Min, V>();
}

#define DECLARE_REVISION(Type, Rev) \
    template<Variant V> struct ::details::HasExplicitRevision<Type, Rev, V> : ::std::true_type {}

#define DECLARE_REVISION_VARIANT(Type, Rev, Var) \
    template<> struct ::details::HasExplicitRevision<Type, Rev, Var> : ::std::true_type {}

#define DEFINE_REVISION(T, Type, Rev) \
    template<Revision R, Variant V> \
    requires ::details::ExplicitRevision<Type, R, V, Rev> \
    T Type<R, V>

#define DEFINE_REVISION_VARIANT(T, Type, Rev, Var) \
    template<Revision R> \
    requires ::details::ExplicitRevision<Type, R, Var, Rev> \
    T Type<R, Var>

#define DEFINE_ENUM(name, base, ...) \
    namespace enum_details \
    { \
        template<Revision R, Variant V> \
        struct name \
        { \
            enum type : base \
            { \
                __VA_ARGS__ \
            }; \
        }; \
    } \
    \
    template<Revision R, Variant V> \
    using name = enum_details::name<R, V>::type

#define DEFINE_ENUM_REVISION(name, base, version, ...) \
    DECLARE_REVISION(enum_details::name, version); \
    namespace enum_details \
    { \
        DEFINE_REVISION(struct, name, version) \
        { \
            enum type : base \
            { \
                __VA_ARGS__ \
            }; \
        }; \
    }

#define DEFINE_ENUM_REVISION_VARIANT(name, base, version, variant, ...) \
    DECLARE_REVISION_VARIANT(enum_details::name, version, variant); \
    namespace enum_details \
    { \
        DEFINE_REVISION_VARIANT(struct, name, version, variant) \
        { \
            enum type : base \
            { \
                __VA_ARGS__ \
            }; \
        }; \
    }

//
namespace typetree_ripper
{
    // Primary vftable of GenerateTypeTreeTransfer in the loaded engine, 0 when
    // it could not be found. Written into the transfer object we build so that
    // an engine virtual call on it lands in engine code instead of our vtable.
    inline uintptr_t g_TransferVftable = 0;
}
