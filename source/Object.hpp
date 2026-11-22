#pragma once
#include <cstdint>
#include "common.hpp"
#include "RTTI.hpp"
#include "GenerateTypeTreeTransfer.hpp"

enum ObjectCreationMode : int
{
    kCreateObjectDefault,
    kCreateObjectFromNonMainThread,
    kCreateObjectDefaultNoLock
};

template<Revision R, Variant V>
class Object;

DECLARE_REVISION(Object, Revision::V2022_3_0_0);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_0_0, Variant::RuntimeDev);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_0_0, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_2_0, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_2_8, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_2_17, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_27_0, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_61_1, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_61_2, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_62_0, Variant::Editor);
DECLARE_REVISION_VARIANT(Object, Revision::V2022_3_62_8, Variant::Editor);
DECLARE_REVISION(Object, Revision::V6000_5_0_0);
DECLARE_REVISION_VARIANT(Object, Revision::V6000_5_0_0, Variant::RuntimeDev);
DECLARE_REVISION_VARIANT(Object, Revision::V6000_5_0_0, Variant::Editor);

//
// 3.0 (default)
//

enum AwakeFromLoadMode : int;

template<Revision R, Variant V>
class Object
{
    using ProxyTransfer = ::GenerateTypeTreeTransfer<R, V>;
public:
    template<bool kSwap> class StreamedBinaryWrite;
    template<bool kSwap> class StreamedBinaryRead;
    virtual ~Object() = 0;
    virtual void MainThreadCleanup() = 0;
    virtual void AwakeFromLoad(AwakeFromLoadMode) = 0;
    virtual void AwakeFromLoadThreaded() = 0;
    virtual void CheckConsistency() = 0;
    virtual void Reset() = 0;
    virtual void SmartReset() = 0;
    virtual char const *GetName() const = 0;
    virtual void SetName(char const *) = 0;
    virtual void ClearPropertiesForCopySerialized() = 0;
    virtual bool ShouldIgnoreInGarbageDependencyTracking() = 0;
    virtual int32_t GetClassID() const = 0;
    virtual void SetHideFlags(int32_t) = 0;
    virtual int32_t GetRuntimeMemorySize() const = 0;
    virtual bool GetNeedsPerObjectTypeTree() const = 0;
    virtual void VirtualRedirectTransfer(ProxyTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class SafeBinaryRead &) = 0;
    virtual void VirtualRedirectTransfer(StreamedBinaryWrite<true> &) = 0;
    virtual void VirtualRedirectTransfer(StreamedBinaryWrite<false> &) = 0;
    virtual void VirtualRedirectTransfer(StreamedBinaryRead<true> &) = 0;
    virtual void VirtualRedirectTransfer(StreamedBinaryRead<false> &) = 0;
    virtual void VirtualRedirectTransfer(class RemapPPtrTransfer &) = 0;
};

enum HideFlags : int;

enum PreviewSceneFlags : int;

//
// 2022.2
//

// Added 'GetMetaData'
// TODO: Determine exact version this was added in

DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_0_0, Variant::RuntimeDev)
{
    using RTTI = ::RTTI<R, Variant::RuntimeDev>;
    using GenerateTypeTreeTransfer = ::GenerateTypeTreeTransfer<R, Variant::RuntimeDev>;
public:
    virtual ~Object() = 0;
    virtual void MainThreadCleanup() = 0;
    virtual void AwakeFromLoad(AwakeFromLoadMode) = 0;
    virtual void AwakeFromLoadThreaded() = 0;
    virtual void CheckConsistency() = 0;
    virtual void Reset() = 0;
    virtual void SmartReset() = 0;
    virtual bool IsAScriptedObject() = 0;
    virtual RTTI const *GetTypeVirtualInternal() = 0;
    virtual char const *GetName() const = 0;
    virtual void SetName(char const *) = 0;
    virtual void SetHideFlags(HideFlags) = 0;
    virtual size_t GetRuntimeMemorySize() const = 0;
    virtual bool GetMetaData(class GrowableBuffer &) = 0;
    virtual void SetCachedScriptingObject(class ScriptingObjectPtr) = 0;
    virtual bool GetNeedsPerObjectTypeTree() const = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryWrite &) = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryRead &) = 0;
    virtual void VirtualRedirectTransfer(class RemapPPtrTransfer &) = 0;
    virtual void VirtualRedirectTransfer(GenerateTypeTreeTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class SafeBinaryRead &) = 0;
};

//
// 2022.3 Editor vtable
//
// One declaration serves every Tuanjie 2022.3 Editor build; what changes is how
// many virtuals sit between SetCachedScriptingObject and
// VirtualRedirectTransfer(GenerateTypeTreeTransfer&), because the engine's
// vtable grew inside the 2t line:
//
//   2t3  .. 2t7    VirtualRedirectTransfer(GTT&) = slot 29
//   2t8  .. 2t16   slot 33   (three entries added at 1.1.0)
//   2t17 .. 2t19   slot 34   (a fourth added at 1.2.4)
//   2022.3.61      slot 30   (modern family)
//
// PAD only contributes placeholder virtuals, one per extra entry. Their names
// are irrelevant - nothing calls them - only the count decides where the
// transfer entry lands, which is why this is a macro and not four copies that
// drift apart.
//
#define OBJECT_2022_3_EDITOR_PAD_TO_29
#define OBJECT_2022_3_EDITOR_PAD_TO_30 \
    virtual void SetFalloff_5(void*) = 0;
#define OBJECT_2022_3_EDITOR_PAD_TO_33 \
    virtual void SetFalloff_5(void*) = 0; \
    virtual void SetFalloff_6(void*) = 0; \
    virtual void SetFalloff_7(void*) = 0; \
    virtual void SetFalloff_8(void*) = 0;
#define OBJECT_2022_3_EDITOR_PAD_TO_32 \
    virtual void SetFalloff_5(void*) = 0; \
    virtual void SetFalloff_6(void*) = 0; \
    virtual void SetFalloff_7(void*) = 0;
#define OBJECT_2022_3_EDITOR_PAD_TO_38 \
    virtual void SetFalloff_5(void*) = 0; \
    virtual void SetFalloff_6(void*) = 0; \
    virtual void SetFalloff_7(void*) = 0; \
    virtual void SetFalloff_8(void*) = 0; \
    virtual void SetFalloff_9(void*) = 0; \
    virtual void SetFalloff_10(void*) = 0; \
    virtual void SetFalloff_11(void*) = 0; \
    virtual void SetFalloff_12(void*) = 0; \
    virtual void SetFalloff_13(void*) = 0;
#define OBJECT_2022_3_EDITOR_PAD_TO_34 \
    virtual void SetFalloff_5(void*) = 0; \
    virtual void SetFalloff_6(void*) = 0; \
    virtual void SetFalloff_7(void*) = 0; \
    virtual void SetFalloff_8(void*) = 0; \
    virtual void SetFalloff_9(void*) = 0;

#define OBJECT_2022_3_EDITOR_BODY(PAD) \
    using RTTI = ::RTTI<R, Variant::Editor>; \
    using GenerateTypeTreeTransfer = ::GenerateTypeTreeTransfer<R, Variant::Editor>; \
public: \
    virtual ~Object() = 0; \
    virtual void SceneWasUnloaded() = 0; \
    virtual void AwakeFromLoad(AwakeFromLoadMode) = 0; \
    virtual void SceneWasUnloaded_2() = 0; \
    virtual void CheckConsistency() = 0; \
    virtual void SceneWasUnloaded_3() = 0; \
    virtual void SceneWasUnloaded_4() = 0; \
    virtual void SceneWasUnloaded_5() = 0; \
    virtual void SetFalloff(void*) = 0; \
    virtual RTTI const *GetTypeVirtualInternal() = 0; \
    virtual char const *GetName() const = 0; \
    virtual void SetName(char const *) = 0; \
    virtual void SetFalloff_2(void*) = 0; \
    virtual void SceneWasUnloaded_6() = 0; \
    virtual void SceneWasUnloaded_7() = 0; \
    virtual void SetFalloff_3(void*) = 0; \
    virtual void SetPersistentDirtyIndex(uint32_t) = 0; \
    virtual uint32_t GetNodeId() = 0; \
    virtual void ClearPersistentDirty() = 0; \
    virtual void SetCompressorQuality(int) = 0; \
    virtual void SetHideFlags(HideFlags) = 0; \
    virtual size_t GetRuntimeMemorySize() const = 0; \
    virtual void SetFalloff_4(void*) = 0; \
    virtual void SetCachedScriptingObject(class ScriptingObjectPtr) = 0; \
    PAD \
    virtual void VirtualRedirectTransfer1(class JSONWrite &) = 0; \
    virtual void VirtualRedirectTransfer2(class JSONRead &) = 0; \
    virtual void VirtualRedirectTransfer3(class YAMLWrite &) = 0; \
    virtual void VirtualRedirectTransfer4(class YAMLRead &) = 0; \
    virtual void VirtualRedirectTransfer5(class SafeBinaryRead &) = 0; \
    virtual void VirtualRedirectTransfer(GenerateTypeTreeTransfer &) = 0; \
    virtual void VirtualRedirectTransfer7(class RemapPPtrTransfer &) = 0; \
    virtual void VirtualRedirectTransfer8(class StreamedBinaryRead &) = 0; \
    virtual void VirtualRedirectTransfer9(class StreamedBinaryWrite &) = 0; \
    virtual void VirtualRedirectTransfer_2(class StreamedBinaryWrite &) = 0; \
    virtual void VirtualStrippedRedirectTransfer(GenerateTypeTreeTransfer &) = 0; \
    virtual void VirtualStrippedRedirectTransfer(class StreamedBinaryRead &) = 0; \
    virtual void VirtualStrippedRedirectTransfer(class JSONWrite &) = 0; \
    virtual void VirtualStrippedRedirectTransfer(class JSONRead &) = 0; \
    virtual void VirtualStrippedRedirectTransfer(class YAMLWrite &) = 0; \
    virtual void VirtualStrippedRedirectTransfer(class YAMLRead &) = 0; \
    virtual void VirtualStrippedRedirectTransfer(class SafeBinaryRead &) = 0; \
    virtual bool IsPrefabAsset() const = 0;

DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_0_0, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_30)
};

DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_2_0, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_29)
};

DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_2_8, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_33)
};

DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_2_17, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_34)
};

// 2022.3.27t1 (1.3.0) keeps the same vtable shape as 2t17: GetTypeTree dispatches
// through [rax+0x110], and the entry at 34 reaches BeginTransfer in Transform,
// AnimationClip and AnnotationManager alike.
DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_27_0, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_34)
};

// Tuanjie 1.6.0 (61t1) has the 2t17 shape (slot 34, measured against the
// engine's own dispatch); 1.6.1 and later are back at slot 30.
DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_61_1, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_34)
};

DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_61_2, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_30)
};

// 2022.3.62t2: slot 30 is VirtualRedirectTransfer(YAMLRead&) there (crash inside
// YAMLRead::GetValueForKeyWithNameConversion), so the transfer group sits two
// entries later than in 61t15: YAMLRead 30, SafeBinaryRead 31, GTT 32.
DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_62_0, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_32)
};

// 2022.3.62t8: measured by sweeping the vtable with the canary - slot 38 gives
// the full tree for Transform (26 nodes) and AnnotationManager (32), while 30,
// 34, 35 are no-ops and the readers between them fault.
DEFINE_REVISION_VARIANT(class, Object, Revision::V2022_3_62_8, Variant::Editor)
{
    OBJECT_2022_3_EDITOR_BODY(OBJECT_2022_3_EDITOR_PAD_TO_38)
};


//
// 6000.5
//

DEFINE_REVISION(class, Object, Revision::V6000_5_0_0)
{
    using RTTI = ::RTTI<R, V>;
    using GenerateTypeTreeTransfer = ::GenerateTypeTreeTransfer<R, V>;
public:
    virtual ~Object() = 0;
    virtual void MainThreadCleanup() = 0;
    virtual void AwakeFromLoad(AwakeFromLoadMode) = 0;
    virtual void AwakeFromLoadThreaded() = 0;
    virtual void CheckConsistency() = 0;
    virtual void Reset() = 0;
    virtual void SmartReset() = 0;
    virtual bool IsAScriptedObject() = 0;
    virtual RTTI const *GetTypeVirtualInternal() = 0;
    virtual char const *GetName() const = 0;
    virtual void SetName(char const *) = 0;
    virtual void SetHideFlags(HideFlags) = 0;
    virtual size_t GetRuntimeMemorySize() const = 0;
    virtual void SetCachedScriptingObject(class ScriptingObjectPtr) = 0;
    virtual bool GetNeedsPerObjectTypeTree() const = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryWrite &) = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryRead &) = 0;
    virtual void VirtualRedirectTransfer(class RemapPPtrTransfer &) = 0;
    virtual void VirtualRedirectTransfer(GenerateTypeTreeTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class UDMReadTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class UDMWriteTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class SafeBinaryRead &) = 0;
};

DEFINE_REVISION_VARIANT(class, Object, Revision::V6000_5_0_0, Variant::RuntimeDev)
{
    using RTTI = ::RTTI<R, Variant::RuntimeDev>;
    using GenerateTypeTreeTransfer = ::GenerateTypeTreeTransfer<R, Variant::RuntimeDev>;
public:
    virtual ~Object() = 0;
    virtual void MainThreadCleanup() = 0;
    virtual void AwakeFromLoad(AwakeFromLoadMode) = 0;
    virtual void AwakeFromLoadThreaded() = 0;
    virtual void CheckConsistency() = 0;
    virtual void Reset() = 0;
    virtual void SmartReset() = 0;
    virtual bool IsAScriptedObject() = 0;
    virtual RTTI const *GetTypeVirtualInternal() = 0;
    virtual char const *GetName() const = 0;
    virtual void SetName(char const *) = 0;
    virtual void SetHideFlags(HideFlags) = 0;
    virtual size_t GetRuntimeMemorySize() const = 0;
    virtual bool GetMetaData(class GrowableBuffer &) = 0;
    virtual void SetCachedScriptingObject(class ScriptingObjectPtr) = 0;
    virtual bool GetNeedsPerObjectTypeTree() const = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryWrite &) = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryRead &) = 0;
    virtual void VirtualRedirectTransfer(class RemapPPtrTransfer &) = 0;
    virtual void VirtualRedirectTransfer(GenerateTypeTreeTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class UDMReadTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class UDMWriteTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class SafeBinaryRead &) = 0;
};

DEFINE_REVISION_VARIANT(class, Object, Revision::V6000_5_0_0, Variant::Editor)
{
    using RTTI = ::RTTI<R, Variant::Editor>;
    using GenerateTypeTreeTransfer = ::GenerateTypeTreeTransfer<R, Variant::Editor>;
public:
    virtual ~Object() = 0;
    virtual void MainThreadCleanup() = 0;
    virtual void AwakeFromLoad(AwakeFromLoadMode) = 0;
    virtual void AwakeFromLoadThreaded() = 0;
    virtual void CheckConsistency() = 0;
    virtual void Reset() = 0;
    virtual void SmartReset() = 0;
    virtual bool IsAScriptedObject() = 0;
    virtual RTTI const *GetTypeVirtualInternal() = 0;
    virtual char const *GetName() const = 0;
    virtual void SetName(char const *) = 0;
    virtual bool HasDebugmodeAutoRefreshInspector() = 0;
    virtual void WarnInstantiateDisallowed() = 0;
    virtual void CloneAdditionalEditorProperties(Object const &) = 0;
    virtual bool CanAssignMonoVariable(char const *, Object *) = 0;
    virtual void SetPersistentDirtyIndex(uint32_t) = 0;
    virtual uint32_t GetPersistentDirtyIndex() = 0;
    virtual void ClearPersistentDirty() = 0;
    virtual void SetPreviewSceneFlags(PreviewSceneFlags) = 0;
    virtual void SetHideFlags(HideFlags) = 0;
    virtual size_t GetRuntimeMemorySize() const = 0;
    virtual bool GetMetaData(class GrowableBuffer &) = 0;
    virtual void SetCachedScriptingObject(class ScriptingObjectPtr) = 0;
    virtual bool GetNeedsPerObjectTypeTree() const = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryWrite &) = 0;
    virtual void VirtualRedirectTransfer(class StreamedBinaryRead &) = 0;
    virtual void VirtualRedirectTransfer(class RemapPPtrTransfer &) = 0;
    virtual void VirtualRedirectTransfer(GenerateTypeTreeTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class UDMReadTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class UDMWriteTransfer &) = 0;
    virtual void VirtualRedirectTransfer(class SafeBinaryRead &) = 0;
    virtual void VirtualRedirectTransfer(class YAMLRead &) = 0;
    virtual void VirtualRedirectTransfer(class YAMLWrite &) = 0;
    virtual void VirtualRedirectTransfer(class JSONRead &) = 0;
    virtual void VirtualRedirectTransfer(class JSONWrite &) = 0;
    virtual void VirtualRedirectTransfer(class UDMGenerateSchemaTransfer &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class SafeBinaryRead &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class YAMLRead &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class YAMLWrite &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class JSONRead &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class JSONWrite &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class UDMWriteTransfer &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class StreamedBinaryRead &) = 0;
    virtual void VirtualStrippedRedirectTransfer(class StreamedBinaryWrite &) = 0;
    virtual void VirtualStrippedRedirectTransfer(GenerateTypeTreeTransfer &) = 0;
};
