#include "d3d9_interop.h"
#include "d3d9_interface.h"
#include "d3d9_common_texture.h"
#include "d3d9_device.h"
#include "d3d9_texture.h"
#include "d3d9_surface.h"
#include "d3d9_buffer.h"
#include "d3d9_initializer.h"

namespace dxvk {

  ////////////////////////////////
  // Interface Interop
  ///////////////////////////////

  D3D9VkInteropInterface::D3D9VkInteropInterface(
          D3D9InterfaceEx*      pInterface)
  : m_interface(pInterface), m_extensions(pInterface->GetInstance()->getExtensionList()) {

  }

  D3D9VkInteropInterface::~D3D9VkInteropInterface() {

  }

  ULONG STDMETHODCALLTYPE D3D9VkInteropInterface::AddRef() {
    return m_interface->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkInteropInterface::Release() {
    return m_interface->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkInteropInterface::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  void STDMETHODCALLTYPE D3D9VkInteropInterface::GetInstanceHandle(
          VkInstance*           pInstance) {
    if (pInstance != nullptr)
      *pInstance = m_interface->GetInstance()->handle();
  }

  void STDMETHODCALLTYPE D3D9VkInteropInterface::GetPhysicalDeviceHandle(
          UINT                  Adapter,
          VkPhysicalDevice*     pPhysicalDevice) {
    if (pPhysicalDevice != nullptr) {
      D3D9Adapter* adapter = m_interface->GetAdapter(Adapter);
      *pPhysicalDevice = adapter ? adapter->GetDXVKAdapter()->handle() : nullptr;
    }
  }

  HRESULT STDMETHODCALLTYPE D3D9VkInteropInterface::GetInstanceExtensions(
          UINT* pExtensionCount, const char** ppExtensions) {
    if (pExtensionCount == nullptr)
      return D3DERR_INVALIDCALL;

    if (!ppExtensions) {
      *pExtensionCount = m_extensions.size();
      return D3D_OK;
    }

    UINT count = 0;
    UINT maxCount = *pExtensionCount;

    for (uint32_t i = 0; i < m_extensions.size() && i < maxCount; i++) {
      ppExtensions[i] = m_extensions[i].extensionName;
      count++;
    }

    *pExtensionCount = count;
    return (count < maxCount) ? D3DERR_MOREDATA : D3D_OK;
  }

  ////////////////////////////////
  // Texture Interop
  ///////////////////////////////

  D3D9VkInteropTexture::D3D9VkInteropTexture(
          IUnknown*             pInterface,
          D3D9CommonTexture*    pTexture)
    : m_interface(pInterface)
    , m_texture  (pTexture) {

  }

  D3D9VkInteropTexture::~D3D9VkInteropTexture() {

  }

  ULONG STDMETHODCALLTYPE D3D9VkInteropTexture::AddRef() {
    return m_interface->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkInteropTexture::Release() {
    return m_interface->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkInteropTexture::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE D3D9VkInteropTexture::GetVulkanImageInfo(
          VkImage*              pHandle,
          VkImageLayout*        pLayout,
          VkImageCreateInfo*    pInfo) {
    const Rc<DxvkImage> image = m_texture->GetImage();
    
    if (unlikely(!image)) {
      if (pHandle != nullptr)
        *pHandle = VK_NULL_HANDLE;

      if (pLayout != nullptr)
        *pLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      return D3DERR_NOTFOUND;
    }

    const DxvkImageCreateInfo& info = image->info();
    
    if (pHandle != nullptr)
      *pHandle = image->handle();
    
    if (pLayout != nullptr)
      *pLayout = info.layout;
    
    if (pInfo != nullptr) {
      // We currently don't support any extended structures
      if (pInfo->sType != VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
       || pInfo->pNext != nullptr)
        return D3DERR_INVALIDCALL;
      
      pInfo->flags          = 0;
      pInfo->imageType      = info.type;
      pInfo->format         = info.format;
      pInfo->extent         = info.extent;
      pInfo->mipLevels      = info.mipLevels;
      pInfo->arrayLayers    = info.numLayers;
      pInfo->samples        = info.sampleCount;
      pInfo->tiling         = info.tiling;
      pInfo->usage          = info.usage;
      pInfo->sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
      pInfo->queueFamilyIndexCount = 0;
      pInfo->initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    
    return S_OK;
  }

  ////////////////////////////////
  // Device Interop
  ///////////////////////////////

  D3D9VkInteropDevice::D3D9VkInteropDevice(
          D3D9DeviceEx*         pInterface)
    : m_device(pInterface) {

  }

  D3D9VkInteropDevice::~D3D9VkInteropDevice() {

  }

  ULONG STDMETHODCALLTYPE D3D9VkInteropDevice::AddRef() {
    return m_device->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkInteropDevice::Release() {
    return m_device->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkInteropDevice::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::GetVulkanHandles(
          VkInstance*           pInstance,
          VkPhysicalDevice*     pPhysDev,
          VkDevice*             pDevice) {
    auto device   = m_device->GetDXVKDevice();
    auto adapter  = device->adapter();
    auto instance = device->instance();
    
    if (pDevice != nullptr)
      *pDevice = device->handle();
    
    if (pPhysDev != nullptr)
      *pPhysDev = adapter->handle();
    
    if (pInstance != nullptr)
      *pInstance = instance->handle();
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::GetSubmissionQueue(
          VkQueue*              pQueue,
          uint32_t*             pQueueIndex,
          uint32_t*             pQueueFamilyIndex) {
    auto device = m_device->GetDXVKDevice();
    DxvkDeviceQueue queue = device->queues().graphics;
    
    if (pQueue != nullptr)
      *pQueue = queue.queueHandle;
    
    if (pQueueIndex != nullptr)
      *pQueueIndex = queue.queueIndex;
    
    if (pQueueFamilyIndex != nullptr)
      *pQueueFamilyIndex = queue.queueFamily;
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::TransitionTextureLayout(
          ID3D9VkInteropTexture*    pTexture,
    const VkImageSubresourceRange*  pSubresources,
          VkImageLayout             OldLayout,
          VkImageLayout             NewLayout) {
    auto texture = static_cast<D3D9VkInteropTexture *>(pTexture)->GetCommonTexture();

    m_device->EmitCs([
      cImage        = texture->GetImage(),
      cSubresources = *pSubresources,
      cOldLayout    = OldLayout,
      cNewLayout    = NewLayout
    ] (DxvkContext* ctx) {
      ctx->transformImage(
        cImage, cSubresources,
        cOldLayout, cNewLayout);
    });
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::FlushRenderingCommands() {
    m_device->Flush();
    m_device->SynchronizeCsThread(DxvkCsThread::SynchronizeAll);
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::LockSubmissionQueue() {
    m_device->GetDXVKDevice()->lockSubmission();
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::ReleaseSubmissionQueue() {
    m_device->GetDXVKDevice()->unlockSubmission();
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::LockDevice() {
    m_lock = m_device->LockDevice();
  }
  
  void STDMETHODCALLTYPE D3D9VkInteropDevice::UnlockDevice() {
    m_lock = D3D9DeviceLock();
  }

  static Rc<DxvkPagedResource> GetDxvkResource(IDirect3DResource9 *pResource) {
    switch (pResource->GetType()) {
      case D3DRTYPE_SURFACE:       return static_cast<D3D9Surface*>     (pResource)->GetCommonTexture()->GetImage();
      // Does not inherit from IDirect3DResource9... lol.
      //case D3DRTYPE_VOLUME:        return static_cast<D3D9Volume*>      (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_TEXTURE:       return static_cast<D3D9Texture2D*>   (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_VOLUMETEXTURE: return static_cast<D3D9Texture3D*>   (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_CUBETEXTURE:   return static_cast<D3D9TextureCube*> (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_VERTEXBUFFER:  return static_cast<D3D9VertexBuffer*>(pResource)->GetCommonBuffer()->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>();
      case D3DRTYPE_INDEXBUFFER:   return static_cast<D3D9IndexBuffer*> (pResource)->GetCommonBuffer()->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>();
      default:                     return nullptr;
    }
  }

  bool STDMETHODCALLTYPE D3D9VkInteropDevice::WaitForResource(
          IDirect3DResource9*  pResource,
          DWORD                MapFlags) {
    return m_device->WaitForResource(*GetDxvkResource(pResource), DxvkCsThread::SynchronizeAll, MapFlags);
  }

  HRESULT STDMETHODCALLTYPE D3D9VkInteropDevice::CreateImage(
          const D3D9VkExtImageDesc* params,
          IDirect3DResource9**      ppResult) {
    InitReturnPtr(ppResult);

    if (unlikely(ppResult == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(params == nullptr))
      return D3DERR_INVALIDCALL;

    /////////////////////////////
    // Image desc validation

    // Cannot create a volume by itself, use D3DRTYPE_VOLUMETEXTURE
    if (unlikely(params->Type == D3DRTYPE_VOLUME))
      return D3DERR_INVALIDCALL;

    // Only allowed: SURFACE, TEXTURE, CUBETEXTURE, VOLUMETEXTURE
    if (unlikely(params->Type < D3DRTYPE_SURFACE || params->Type > D3DRTYPE_CUBETEXTURE))
      return D3DERR_INVALIDCALL;

    // Only volume textures can have depth > 1
    if (unlikely(params->Type != D3DRTYPE_VOLUMETEXTURE && params->Depth > 1))
      return D3DERR_INVALIDCALL;

    if (params->Type == D3DRTYPE_SURFACE) {
      // Surfaces can only have 1 mip level
      if (unlikely(params->MipLevels > 1))
        return D3DERR_INVALIDCALL;

      if (unlikely(params->MultiSample > D3DMULTISAMPLE_16_SAMPLES))
        return D3DERR_INVALIDCALL;
    } else {
      // Textures can't be multisampled
      if (unlikely(params->MultiSample != D3DMULTISAMPLE_NONE))
        return D3DERR_INVALIDCALL;
    }

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = params->Width;
    desc.Height             = params->Height;
    desc.Depth              = params->Depth;
    desc.ArraySize          = params->Type == D3DRTYPE_CUBETEXTURE ? 6 : 1;
    desc.MipLevels          = params->MipLevels;
    desc.Usage              = params->Usage;
    desc.Format             = EnumerateFormat(params->Format);
    desc.Pool               = params->Pool;
    desc.Discard            = params->Discard;
    desc.MultiSample        = params->MultiSample;
    desc.MultisampleQuality = params->MultiSampleQuality;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = params->IsAttachmentOnly;
    desc.IsLockable         = params->IsLockable;
    desc.ImageUsage         = params->ImageUsage;
    
    D3DRESOURCETYPE textureType = params->Type == D3DRTYPE_SURFACE ? D3DRTYPE_TEXTURE : params->Type;

    HRESULT hr = D3D9CommonTexture::NormalizeTextureProperties(m_device, textureType, &desc);
    if (FAILED(hr))
      return hr;

    switch (params->Type) {
      case D3DRTYPE_SURFACE:
        return CreateTextureResource<D3D9Surface>(desc, ppResult);

      case D3DRTYPE_TEXTURE:
        return CreateTextureResource<D3D9Texture2D>(desc, ppResult);

      case D3DRTYPE_VOLUMETEXTURE:
        return CreateTextureResource<D3D9Texture3D>(desc, ppResult);

      case D3DRTYPE_CUBETEXTURE:
        return CreateTextureResource<D3D9TextureCube>(desc, ppResult);

      default:
        return D3DERR_INVALIDCALL;
    }
  }

  template <typename ResourceType>
  HRESULT D3D9VkInteropDevice::CreateTextureResource(
          const D3D9_COMMON_TEXTURE_DESC& desc,
          IDirect3DResource9**            ppResult) {
    try {
      const bool isExtended = m_device->IsD3DCompatibile(D3DCompatibility::D3D9Ex);
      const Com<ResourceType> texture = new ResourceType(m_device, &desc, isExtended);
      m_device->m_initializer->InitTexture(texture->GetCommonTexture());
      *ppResult = texture.ref();

      if (desc.Pool == D3DPOOL_DEFAULT)
        m_device->m_losableResourceCounter++;

      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }

  ////////////////////////////////
  // Morrowind Interop
  ///////////////////////////////

  DxvkMorrowindInterop::DxvkMorrowindInterop(
          D3D9DeviceEx*         pInterface)
    : m_device(pInterface) {

  }

  DxvkMorrowindInterop::~DxvkMorrowindInterop() {

  }

  ULONG STDMETHODCALLTYPE DxvkMorrowindInterop::AddRef() {
    return m_device->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkMorrowindInterop::Release() {
    return m_device->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkMorrowindInterop::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  uint32_t STDMETHODCALLTYPE DxvkMorrowindInterop::GetInterfaceVersion() {
    return DXVK_MORROWIND_INTEROP_VERSION;
  }

  uint64_t DxvkMorrowindInterop::GetCapabilitiesLocked() const {
    D3D9Surface* sourceSurface = m_device->m_autoDepthStencil.ptr();

    if (!sourceSurface)
      return 0;

    D3D9CommonTexture* sourceTexture = sourceSurface->GetCommonTexture();
    const Rc<DxvkImage> sourceImage = sourceTexture->GetImage();

    if (!sourceImage)
      return 0;

    const DxvkImageCreateInfo& sourceInfo = sourceImage->info();
    const DxvkFormatInfo* formatInfo = sourceImage->formatInfo();

    if (!(formatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
     || sourceInfo.sampleCount == VK_SAMPLE_COUNT_1_BIT
     || !(sourceInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
      return 0;

    const auto& properties = m_device->GetDXVKDevice()->properties().vk12;
    const bool hasStencil =
      formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT;
    const bool supportsMin =
      properties.supportedDepthResolveModes & VK_RESOLVE_MODE_MIN_BIT;
    const bool supportsIndependentNone =
      !hasStencil || properties.independentResolveNone;

    return supportsMin && supportsIndependentNone
      ? DXVK_MORROWIND_CAP_MSAA_DEPTH_RESOLVE
      : 0;
  }

  uint64_t STDMETHODCALLTYPE DxvkMorrowindInterop::GetCapabilities() {
    D3D9DeviceLock lock = m_device->LockDevice();
    return GetCapabilitiesLocked();
  }

  static bool GetMorrowindInteropSurface(
          IDirect3DSurface9*    surfaceInterface,
          D3D9Surface*&         surface,
          D3D9CommonTexture*&   texture) {
    ID3D9VkInteropTexture* interop = nullptr;

    if (FAILED(surfaceInterface->QueryInterface(
        __uuidof(ID3D9VkInteropTexture),
        reinterpret_cast<void**>(&interop))))
      return false;

    texture = static_cast<D3D9VkInteropTexture*>(interop)->GetCommonTexture();
    interop->Release();

    if (!texture)
      return false;

    surface = static_cast<D3D9Surface*>(surfaceInterface);
    return true;
  }

  HRESULT STDMETHODCALLTYPE DxvkMorrowindInterop::ResolveDepthMinV1(
          IDirect3DSurface9*    sourceMsaaDepth,
          IDirect3DSurface9*    destinationIntz) {
    D3D9DeviceLock lock = m_device->LockDevice();

    if (!sourceMsaaDepth || !destinationIntz
     || sourceMsaaDepth == destinationIntz)
      return D3DERR_INVALIDCALL;

    D3D9Surface* sourceSurface = nullptr;
    D3D9Surface* destinationSurface = nullptr;
    D3D9CommonTexture* sourceTexture = nullptr;
    D3D9CommonTexture* destinationTexture = nullptr;

    if (!GetMorrowindInteropSurface(
          sourceMsaaDepth, sourceSurface, sourceTexture)
     || !GetMorrowindInteropSurface(
          destinationIntz, destinationSurface, destinationTexture))
      return D3DERR_INVALIDCALL;

    if (sourceTexture->Device() != m_device
     || destinationTexture->Device() != m_device)
      return D3DERR_INVALIDCALL;

    if (sourceSurface != m_device->m_autoDepthStencil.ptr()
     || sourceSurface != m_device->m_state.depthStencil.ptr())
      return D3DERR_INVALIDCALL;

    const D3D9_COMMON_TEXTURE_DESC* sourceDesc = sourceTexture->Desc();
    const D3D9_COMMON_TEXTURE_DESC* destinationDesc = destinationTexture->Desc();

    if (sourceDesc->Pool != D3DPOOL_DEFAULT
     || destinationDesc->Pool != D3DPOOL_DEFAULT
     || !(sourceDesc->Usage & D3DUSAGE_DEPTHSTENCIL)
     || !(destinationDesc->Usage & D3DUSAGE_DEPTHSTENCIL))
      return D3DERR_INVALIDCALL;

    if (sourceSurface->GetBaseTexture() != nullptr
     || destinationSurface->GetBaseTexture() == nullptr
     || sourceSurface->GetMipLevel() != 0
     || sourceSurface->GetFace() != 0
     || sourceSurface->GetSubresource() != 0
     || destinationSurface->GetMipLevel() != 0
     || destinationSurface->GetFace() != 0
     || destinationSurface->GetSubresource() != 0)
      return D3DERR_INVALIDCALL;

    if (sourceDesc->MipLevels != 1
     || sourceDesc->ArraySize != 1
     || sourceDesc->Depth != 1
     || sourceDesc->MultiSample == D3DMULTISAMPLE_NONE
     || destinationDesc->Format != D3D9Format::INTZ
     || destinationDesc->MipLevels != 1
     || destinationDesc->ArraySize != 1
     || destinationDesc->Depth != 1
     || destinationDesc->MultiSample != D3DMULTISAMPLE_NONE)
      return D3DERR_INVALIDCALL;

    const Rc<DxvkImage> sourceImage = sourceTexture->GetImage();
    const Rc<DxvkImage> destinationImage = destinationTexture->GetImage();

    if (!sourceImage || !destinationImage)
      return D3DERR_INVALIDCALL;

    const DxvkImageCreateInfo& sourceInfo = sourceImage->info();
    const DxvkImageCreateInfo& destinationInfo = destinationImage->info();

    if (sourceInfo.sampleCount == VK_SAMPLE_COUNT_1_BIT
     || destinationInfo.sampleCount != VK_SAMPLE_COUNT_1_BIT
     || sourceInfo.mipLevels != 1
     || destinationInfo.mipLevels != 1
     || sourceInfo.numLayers != 1
     || destinationInfo.numLayers != 1
     || !(sourceInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
     || !(destinationInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
      return D3DERR_INVALIDCALL;

    if (sourceInfo.extent.width != destinationInfo.extent.width
     || sourceInfo.extent.height != destinationInfo.extent.height
     || sourceInfo.extent.depth != destinationInfo.extent.depth
     || sourceInfo.extent.width != sourceDesc->Width
     || sourceInfo.extent.height != sourceDesc->Height
     || destinationInfo.extent.width != destinationDesc->Width
     || destinationInfo.extent.height != destinationDesc->Height)
      return D3DERR_INVALIDCALL;

    if (sourceInfo.format != destinationInfo.format)
      return D3DERR_NOTAVAILABLE;

    const VkImageAspectFlags aspectMask =
      sourceImage->formatInfo()->aspectMask;

    if (!(aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT))
      return D3DERR_INVALIDCALL;

    VkImageResolve region = { };
    region.srcSubresource = VkImageSubresourceLayers {
      aspectMask, 0, 0, 1 };
    region.srcOffset = VkOffset3D { 0, 0, 0 };
    region.dstSubresource = VkImageSubresourceLayers {
      aspectMask, 0, 0, 1 };
    region.dstOffset = VkOffset3D { 0, 0, 0 };
    region.extent = sourceImage->mipLevelExtent(0);

    if (!sourceImage->isFullSubresource(
          region.srcSubresource, region.extent)
     || !destinationImage->isFullSubresource(
          region.dstSubresource, region.extent))
      return D3DERR_INVALIDCALL;

    if (!(GetCapabilitiesLocked()
        & DXVK_MORROWIND_CAP_MSAA_DEPTH_RESOLVE))
      return D3DERR_NOTAVAILABLE;

    m_device->EmitCs([
      cSourceImage      = sourceImage,
      cDestinationImage = destinationImage,
      cRegion           = region,
      cFormat           = sourceInfo.format
    ] (DxvkContext* ctx) {
      ctx->resolveImage(
        cDestinationImage,
        cSourceImage,
        cRegion,
        cFormat,
        VK_RESOLVE_MODE_MIN_BIT,
        VK_RESOLVE_MODE_NONE);
    });

    return S_OK;
  }


  DxvkMorrowindPplInterop::DxvkMorrowindPplInterop(
          D3D9DeviceEx*         pInterface)
    : m_device(pInterface) {

  }

  DxvkMorrowindPplInterop::~DxvkMorrowindPplInterop() {

  }

  ULONG STDMETHODCALLTYPE DxvkMorrowindPplInterop::AddRef() {
    return m_device->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkMorrowindPplInterop::Release() {
    return m_device->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkMorrowindPplInterop::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  uint64_t STDMETHODCALLTYPE DxvkMorrowindPplInterop::GetCapabilities() {
    // The V1 bit still reports the method's presence, so a client built against
    // the V1 packet reaches the struct size/version rejection in DrawPplV1 and
    // falls back. The V2 bit reports the expanded packet, which this build also
    // requires the ordinary fixed-function path to support.
    // The expanded-limit bit is separate from the packet version so that a
    // client can authorize its irreversible engine patch on the renderer as a
    // whole, not just on the native draw path. The static_asserts in
    // dxvk_morrowind_interop.h are what let this build make that claim.
    // The V3 bit adds the light fade on top of V2, which stays accepted.
    return DXVK_MORROWIND_CAP_PPL_DRAW_V1
         | DXVK_MORROWIND_CAP_PPL_DRAW_V2
         | DXVK_MORROWIND_CAP_PPL_DRAW_V3
         | DXVK_MORROWIND_CAP_EXPANDED_LIGHT_LIMIT;
  }

  HRESULT STDMETHODCALLTYPE DxvkMorrowindPplInterop::DrawPplV1(
          const DxvkMorrowindPplDrawV1* draw) {
    if (!draw)
      return E_POINTER;

    const bool isV2 = draw->structSize == sizeof(DxvkMorrowindPplDrawV1)
                   && draw->structVersion == DXVK_MORROWIND_PPL_STRUCT_VERSION;
    const bool isV3 = draw->structSize == sizeof(DxvkMorrowindPplDrawV3)
                   && draw->structVersion == DXVK_MORROWIND_PPL_STRUCT_VERSION_V3;

    if (!isV2 && !isV3)
      return E_INVALIDARG;

    // A version 2 packet is the version 3 prefix and renders without fade.
    D3D9DeviceLock lock = m_device->LockDevice();
    DxvkMorrowindPplDrawV3 packet = { };
    if (isV3)
      packet = *reinterpret_cast<const DxvkMorrowindPplDrawV3*>(draw);
    else
      packet.base = *draw;
    return m_device->DrawMorrowindPpl(packet);
  }


  DxvkMorrowindMemoryInterop::DxvkMorrowindMemoryInterop(
          D3D9DeviceEx*         pInterface)
    : m_device(pInterface) {

  }

  DxvkMorrowindMemoryInterop::~DxvkMorrowindMemoryInterop() {

  }

  ULONG STDMETHODCALLTYPE DxvkMorrowindMemoryInterop::AddRef() {
    return m_device->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkMorrowindMemoryInterop::Release() {
    return m_device->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkMorrowindMemoryInterop::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE DxvkMorrowindMemoryInterop::GetDeviceLocalMemoryBudgetV1(
          uint64_t*             memoryBudget,
          uint64_t*             memoryUsed) {
    if (!memoryBudget || !memoryUsed)
      return E_POINTER;

    DxvkMemoryStats stats = { };
    if (!m_device->GetDXVKDevice()->getDeviceLocalBufferMemoryStats(stats)
     || !stats.memoryBudget)
      return D3DERR_NOTAVAILABLE;

    *memoryBudget = stats.memoryBudget;
    *memoryUsed = stats.memoryUsed;
    return S_OK;
  }

  D3D9VkExtInterface::D3D9VkExtInterface(D3D9InterfaceEx *pInterface)
    : m_interface(pInterface) {

  }
  
  ULONG STDMETHODCALLTYPE D3D9VkExtInterface::AddRef() {
    return m_interface->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkExtInterface::Release() {
    return m_interface->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkExtInterface::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  void STDMETHODCALLTYPE D3D9VkExtInterface::UnlockAdditionalFormats() {
    m_interface->EnableAdditionalFormats();
  }

}
