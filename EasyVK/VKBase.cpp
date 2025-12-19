#include "VKBase.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

using namespace vulkan;

void(*result_t::callback_throw)(VkResult);

//graphicsBase graphicsBase::singleton;
std::mutex graphicsBase::callback_mtx;

graphicsBase::~graphicsBase()
{
    //判断实例是否创建
    if(!instance){
        return;
    }
    //判断逻辑设备是否创建
    if(device){
        WaitIdle(); //等待队列空闲 --- 不判断返回值,程序即将销毁，即使出问题也需要执行销毁流程
        //判断交换链是否创建
        if(swapchain){
            ExecuteCallbacks(callbacks_destroySwapchain);
            for (auto& i : swapchainImageViews){
                if (i){
                    vkDestroyImageView(device, i, nullptr);
                }
            }
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }
        ExecuteCallbacks(callbacks_destroyDevice);
        vkDestroyDevice(device, nullptr);
    }
    //判断surface是否创建
    if(surface){
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    //判断是否开启了debugMessager
    if(debugMessenger){
        auto vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (vkDestroyDebugUtilsMessenger){
            vkDestroyDebugUtilsMessenger(instance, debugMessenger, nullptr);
        }
    }
    //销毁实例
    vkDestroyInstance(instance, nullptr);
}

VkInstance graphicsBase::Instance()
{
    return instance;
}

const std::vector<const char *> &graphicsBase::InstanceLayers() const
{
    return instanceLayers;
}

const std::vector<const char *> &graphicsBase::InstanceExtensions() const
{
    return instanceExtensions;
}

void graphicsBase::AddInstanceLayers(const char *layerName)
{
    AddLayerOrExtension(instanceLayers,layerName);
}

void graphicsBase::AddInstanceExtensions(const char *layerName)
{
    AddLayerOrExtension(instanceExtensions,layerName);
}

result_t graphicsBase::CreateInstance(VkInstanceCreateFlags flag)
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    bool foundValidation = false;
    for (auto &l : layers) {
        if (strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            foundValidation = true;
            break;
        }
    }

    //仅在编译选项为DEBUG时，在instanceLayers和instanceExtensions尾部加上所需的名称。
    if (ENABLE_DEBUG_MESSENGER){
        if(foundValidation){
            this->AddInstanceLayers("VK_LAYER_KHRONOS_validation");
        }
        this->AddInstanceExtensions(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.apiVersion = apiVersion;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.flags = flag;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = uint32_t(instanceLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = instanceLayers.data();
    instanceCreateInfo.enabledExtensionCount = uint32_t(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    for(int i =0;i < instanceExtensions.size();i++)
    {
        printf("EnabledExtensionNames:%s\n",instanceExtensions[i]);
    }

    if(VkResult result = vkCreateInstance(&instanceCreateInfo,nullptr,&instance)){
        //LOG(fmt::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result)));
        return result;
    }
    //创建实例后输出Vulkan版本
    printf("Vulkan API Version: %d.%d.%d\n",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion),
        VK_VERSION_PATCH(apiVersion));

    //创建完Vulkan实例后紧接着创建debug messenger
    if (ENABLE_DEBUG_MESSENGER){
        CreateDebugMessenger();
    }
    return VK_SUCCESS;
}

VkResult graphicsBase::CheckInstanceLayers(std::vector<const char *> &layersToCheck)
{
    uint32_t layerCount = 0;
    std::vector<VkLayerProperties> availableLayers;
    if(VkResult result = vkEnumerateInstanceLayerProperties(&layerCount,nullptr)){
        qDebug("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
        return result;
    }
    if (layerCount) {
        availableLayers.resize(layerCount);
        if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) {
            qDebug("[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: %d\n", int32_t(result));
            return result;
        }
        for (auto& i : layersToCheck) {
            bool found = false;
            for (auto& j : availableLayers)
                if (!strcmp(i, j.layerName)) {
                    found = true;
                    break;
                }
            if (!found)
                i = nullptr;
        }
    }
    else{
        for(auto& i : layersToCheck){
            i = nullptr;
        }
    }
    //一切顺利则返回VK_SUCCESS
    return VK_SUCCESS;
}

void graphicsBase::InstanceLayers(const std::vector<const char *> &layerNames)
{
    instanceLayers = layerNames;
}

VkResult graphicsBase::CheckInstanceExtensions(std::vector<const char *> &extensionsToCheck, const char *layerName)
{
    uint32_t extensionCount = 0;
    std::vector<VkExtensionProperties> availableExtensions;
    if(VkResult result = vkEnumerateInstanceExtensionProperties(layerName,&extensionCount,nullptr)){
        //layerName!= nullptr ?
        //std::cout << fmt::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName) :
        //std::cout << fmt::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
        return result;
    }
    if(extensionCount){
        availableExtensions.resize(extensionCount);
        if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) {
            //std::cout << fmt::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n", int32_t(result));
            return result;
        }
        for (auto& i : extensionsToCheck) {
            bool found = false;
            for (auto& j : availableExtensions)
                if (!strcmp(i, j.extensionName)) {
                    found = true;
                    break;
                }
            if (!found)
                i = nullptr;
        }
    }
    else{
        for(auto& i : extensionsToCheck){
            i = nullptr;
        }
    }
    return VK_SUCCESS;
}

void graphicsBase::InstanceExtensions(const std::vector<const char *> &extensionsNames)
{
    instanceExtensions = extensionsNames;
}

VkResult graphicsBase::CreateDebugMessenger()
{
    /*待Ch1-3填充*/
    static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback = [](
        VkDebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
        VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* /*pUserData*/)->VkBool32 {
            qDebug("%s\n", pCallbackData->pMessage);
            return VK_FALSE;
    };
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};
    debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsMessengerCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugUtilsMessengerCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;


    //Vulkan中，扩展相关的函数，若非设备特定，大都通过vkGetInstanceProcAddr(...)来获取,创建debug messenger也不例外
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if(vkCreateDebugUtilsMessenger){
        VkResult result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger);
        if (result){
            qDebug("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: %d\n", int32_t(result));
        }
        return result;
    }
    qDebug("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
    return VK_RESULT_MAX_ENUM;
}

VkSurfaceKHR graphicsBase::Surface() const
{
    return surface;
}

void graphicsBase::Surface(VkSurfaceKHR vk_s)
{
    if(!this->surface){
        surface = vk_s;
    }
}

VkResult graphicsBase::GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue, bool enableComputeQueue, uint32_t (&queueFamilyIndices)[3])
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (!queueFamilyCount){
        return VK_RESULT_MAX_ENUM;
    }
    std::vector<VkQueueFamilyProperties> queueFamilyPropertieses(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyPropertieses.data());

    auto& ig = queueFamilyIndices[0];
    auto& ip = queueFamilyIndices[1];
    auto& ic = queueFamilyIndices[2];
    ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;
    //遍历所有队列族的索引
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        //只在enableGraphicsQueue为true时获取支持图形操作的队列族的索引
        VkBool32 supportGraphics = enableGraphicsQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        ////只在enableComputeQueue为true时获取支持计算的队列族的索引
        VkBool32 supportCompute  = enableComputeQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
        //只在创建了window surface时获取支持呈现的队列族的索引
        VkBool32 supportPresentation  = false;
        if(surface){
            if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation)) {
                //std::cout << fmt::format("[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n", int32_t(result));
                return result;
            }
            //若某队列族同时支持图形操作和计算
            if (supportGraphics && supportCompute) {
                //若需要呈现，最好是三个队列族索引全部相同
                if (supportPresentation) {
                    ig = ip = ic = i;
                    break;
                }
                //除非ig和ic都已取得且相同，否则将它们的值覆写为i，以确保两个队列族索引相同
                if (ig != ic || ig == VK_QUEUE_FAMILY_IGNORED){
                    ig = ic = i;
                }
                //如果不需要呈现，那么已经可以break了
                if (!surface)
                    break;
            }
            //若任何一个队列族索引可以被取得但尚未被取得，将其值覆写为i
            if (supportGraphics && ig == VK_QUEUE_FAMILY_IGNORED){
                ig = i;
            }
            if (supportPresentation && ip == VK_QUEUE_FAMILY_IGNORED){
                ip = i;
            }
            if (supportCompute && ic == VK_QUEUE_FAMILY_IGNORED){
                ic = i;
            }
        }

    }
    //若任何需要被取得的队列族索引尚未被取得，则函数执行失败
    if (((ig == VK_QUEUE_FAMILY_IGNORED) && enableGraphicsQueue) ||
        ((ip == VK_QUEUE_FAMILY_IGNORED) && surface) ||
        ((ic == VK_QUEUE_FAMILY_IGNORED) && enableComputeQueue)){
        return VK_RESULT_MAX_ENUM;
    }
    //函数执行成功时，将所取得的队列族索引写入到成员变量
    queueFamilyIndex_graphics = ig;
    queueFamilyIndex_presentation = ip;
    queueFamilyIndex_compute = ic;
    return VK_SUCCESS;
}

VkPhysicalDevice graphicsBase::PhysicalDevice() const
{
    return physicalDevice;
}

const VkPhysicalDeviceProperties &graphicsBase::PhysicalDeviceProperties() const
{
    return physicalDeviceProperties;
}

const VkPhysicalDeviceMemoryProperties &graphicsBase::PhysicalDeviceMemoryProperties() const
{
    return physicalDeviceMemoryProperties;
}

VkPhysicalDevice graphicsBase::AvailablePhysicalDevice(uint32_t index) const
{
    assert(index < availablePhysicalDevices.size() && "AvailablePhysicalDevice inputr index >= availablePhysicalDevices.size()");
    return availablePhysicalDevices[index];
}

uint32_t graphicsBase::AvailablePhysicalDeviceCount() const
{
    return uint32_t(availablePhysicalDevices.size());
}

VkDevice graphicsBase::Device() const
{
    return device;
}

uint32_t graphicsBase::QueueFamilyIndex_Graphics() const
{
    return queueFamilyIndex_graphics;
}

uint32_t graphicsBase::QueueFamilyIndex_Presentation() const
{
    return queueFamilyIndex_presentation;
}

uint32_t graphicsBase::QueueFamilyIndex_Compute() const
{
    return queueFamilyIndex_compute;
}

VkQueue graphicsBase::Queue_Graphics() const
{
    return queue_graphics;
}

VkQueue graphicsBase::Queue_Presentation() const
{
    return queue_presentation;
}

VkQueue graphicsBase::Queue_Compute() const
{
    return queue_compute;
}

const std::vector<const char *> &graphicsBase::DeviceExtensions() const
{
    return deviceExtensions;
}

void graphicsBase::AddDeviceExtension(const char *extensionName)
{
    AddLayerOrExtension(deviceExtensions, extensionName);
}

VkResult graphicsBase::GetPhysicalDevices()
{
    /*待Ch1-3填充*/
    uint32_t deviceCount;
     if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr)) {
         qDebug("[ graphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: %d\n", int32_t(result));
         return result;
     }
     if (!deviceCount){
         qDebug("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"),
         abort();
     }
     availablePhysicalDevices.resize(deviceCount);
     VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
     if (result){
         qDebug("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: %d\n", int32_t(result));
     }
     return result;
}

VkResult graphicsBase::DeterminePhysicalDevice(uint32_t deviceIndex, bool enableGraphicsQueue, bool enableComputeQueue)
{
    //定义一个特殊值用于标记一个队列族索引已被找过但未找到
    static uint32_t notFound = INT32_MAX;
    //定义队列族索引组合的结构体
    struct queueFamilyIndexCombination {
        uint32_t graphics;
        uint32_t presentation;
        uint32_t compute;
    };
    static queueFamilyIndexCombination defaultCombination = {
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED
    };

    //queueFamilyIndexCombinations用于为各个物理设备保存一份队列族索引组合
    static std::vector<queueFamilyIndexCombination> queueFamilyIndexCombinations(this->availablePhysicalDevices.size(),defaultCombination);
    auto& ig =  queueFamilyIndexCombinations[deviceIndex].graphics;
    auto& ip =  queueFamilyIndexCombinations[deviceIndex].presentation;
    auto& ic =  queueFamilyIndexCombinations[deviceIndex].compute;

    //如果有任何队列族索引已被找过但未找到，返回VK_RESULT_MAX_ENUM
    if (((ig == notFound) && enableGraphicsQueue) ||
        ((ip == notFound) && this->surface) ||
        ((ic == notFound) && enableComputeQueue)){
        return VK_RESULT_MAX_ENUM;
    }
    //如果有任何队列族索引应被获取但还未被找过
    if ((ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue) ||
        (ip == VK_QUEUE_FAMILY_IGNORED && this->surface) ||
        (ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)) {
        uint32_t indices[3];
        VkResult result = this->GetQueueFamilyIndices(this->availablePhysicalDevices[deviceIndex], enableGraphicsQueue, enableComputeQueue, indices);
        //若GetQueueFamilyIndices(...)返回VK_SUCCESS或VK_RESULT_MAX_ENUM（vkGetPhysicalDeviceSurfaceSupportKHR(...)执行成功但没找齐所需队列族），
        //说明对所需队列族索引已有结论，保存结果到queueFamilyIndexCombinations[deviceIndex]中相应变量
        //应被获取的索引若仍为VK_QUEUE_FAMILY_IGNORED，说明未找到相应队列族，VK_QUEUE_FAMILY_IGNORED（~0u）与INT32_MAX做位与得到的数值等于notFound
        if (result == VK_SUCCESS ||
            result == VK_RESULT_MAX_ENUM) {
            if (enableGraphicsQueue){
                ig = indices[0] & INT32_MAX;
            }
            if (this->surface){
                ip = indices[1] & INT32_MAX;
            }
            if (enableComputeQueue){
                ic = indices[2] & INT32_MAX;
            }
        }
        //如果GetQueueFamilyIndices(...)执行失败，return
        if (result)
            return result;
    }
    //若以上两个if分支皆不执行，则说明所需的队列族索引皆已被获取，从queueFamilyIndexCombinations[deviceIndex]中取得索引
    else {
        this->queueFamilyIndex_graphics = enableGraphicsQueue ? ig : VK_QUEUE_FAMILY_IGNORED;
        this->queueFamilyIndex_presentation = this->surface ? ip : VK_QUEUE_FAMILY_IGNORED;
        this->queueFamilyIndex_compute = enableComputeQueue ? ic : VK_QUEUE_FAMILY_IGNORED;
    }
    this->physicalDevice = this->availablePhysicalDevices[deviceIndex];
    return VK_SUCCESS;
}

VkResult graphicsBase::CreateDevice(VkDeviceCreateFlags flags)
{
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[3] = {
        { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,0,1, &queuePriority},
        { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,0,1, &queuePriority},
        { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,0,1, &queuePriority}
    };

    uint32_t queueCreateInfoCount = 0;
    if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
        queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
    if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
        queueFamilyIndex_presentation != queueFamilyIndex_graphics)
        queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
    if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
        queueFamilyIndex_compute != queueFamilyIndex_graphics &&
        queueFamilyIndex_compute != queueFamilyIndex_presentation)
        queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;

    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.flags = flags;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos =  queueCreateInfos;
    deviceCreateInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

    //使用物理设备创建逻辑设备
    if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device)) {
        //std::cout << fmt::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n", int32_t(result));
        return result;
    }
    //从逻辑设备中取得队列
    if(queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED){
        vkGetDeviceQueue(device,queueFamilyIndex_graphics,0,&queue_graphics);
    }
    if(queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED){
        vkGetDeviceQueue(device,queueFamilyIndex_presentation,0,&queue_presentation);
    }
    if(queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED){
        vkGetDeviceQueue(device,queueFamilyIndex_presentation,0,&queue_compute);
    }
    //逻辑设备成功创建后，物理设备不会再变更。获取以下物理设备属性
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

    //输出所用的物理设备名称
    qDebug("Renderer: %s\n", physicalDeviceProperties.deviceName);

    //执行创建设备回调函数
    ExecuteCallbacks(callbacks_createDevice);
    return VK_SUCCESS;
}

VkResult graphicsBase::CheckDeviceExtensions(std::vector<const char *> /*extensionsToCheck*/, const char* /*layerName*/) const
{
    return VK_SUCCESS;
}

void graphicsBase::DeviceExtensions(const std::vector<const char *> &extensionNames)
{
    deviceExtensions = extensionNames;
}

VkResult graphicsBase::CreateSwapchain_Internal()
{
    if (VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain)) {
        qDebug("[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: %d\n", int32_t(result));
        return result;
    }
    //获取交换链图像
    uint32_t swapchainImageCount;
    if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr)) {
        qDebug("[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: %d\n", int32_t(result));
        return result;
    }
    swapchainImages.resize(swapchainImageCount);
    if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data())) {
        qDebug("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: %d\n", int32_t(result));
        return result;
    }
    //为交换链图像创建Image View
    swapchainImageViews.resize(swapchainImageCount);
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = swapchainCreateInfo.imageFormat;
    imageViewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};

    for(size_t i = 0; i < swapchainImageCount;i++){
        imageViewCreateInfo.image = swapchainImages[i];
        if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i])) {
            qDebug("[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: %d\n", int32_t(result));
            return result;
        }
    }
    /*Swapchain
     ├── Image[0] ──> ImageView[0] ──> Framebuffer[0]
     ├── Image[1] ──> ImageView[1] ──> Framebuffer[1]
     └── Image[2] ──> ImageView[2] ──> Framebuffer[2]
     */
    return VK_SUCCESS;
}

const VkFormat &graphicsBase::AvailableSurfaceFormat(uint32_t index) const
{
    assert(index < availableSurfaceFormats.size() && "AvailableSurfaceFormat inputr index >= availableSurfaceFormats.size()");
    return availableSurfaceFormats[index].format;
}

const VkColorSpaceKHR &graphicsBase::AvailableSurfaceColorSpace(uint32_t index) const
{
    assert(index < availableSurfaceFormats.size() && "AvailableSurfaceColorSpace inputr index >= availableSurfaceFormats.size()");
    return availableSurfaceFormats[index].colorSpace;
}

uint32_t graphicsBase::AvailableSurfaceFormatCount() const
{
    return uint32_t(availableSurfaceFormats.size());
}

VkSwapchainKHR graphicsBase::Swapchain() const
{
    return swapchain;
}

VkImage graphicsBase::SwapchainImage(uint32_t index) const
{
    return swapchainImages[index];
}

VkImageView graphicsBase::SwapchainImageView(uint32_t index) const
{
    return swapchainImageViews[index];
}

uint32_t graphicsBase::SwapchainImageCount() const
{
    return uint32_t(swapchainImages.size());
}

const VkSwapchainCreateInfoKHR &graphicsBase::SwapchainCreateInfo() const
{
    return swapchainCreateInfo;
}

VkResult graphicsBase::GetSurfaceFormats()
{
    VkResult result;
    uint32_t surfaceFormatCount = 0;
    if(result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr)){
        qDebug("[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: %d\n", int32_t(result));
        return result;
    }
    if(!surfaceFormatCount){
        qDebug("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n");
        abort();
    }
    availableSurfaceFormats.resize(surfaceFormatCount);
    if(result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data())){
        qDebug("[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: %d\n", int32_t(result));
    }
    return result;
}

VkResult graphicsBase::SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat)
{
    bool formatIsAvailable = false;

    //format未指定,则匹配色彩空间，匹配成功则直接使用
    if(!surfaceFormat.format){
        for(auto& i : availableSurfaceFormats){
            if(i.colorSpace == surfaceFormat.colorSpace){
                swapchainCreateInfo.imageFormat = i.format;
                swapchainCreateInfo.imageColorSpace = i.colorSpace;
                formatIsAvailable = true;
                break;
            }
        }
    }
    //format已经指定,则需要匹配format和色彩空间
    else{
        for(auto& i : availableSurfaceFormats){
            if(i.format == surfaceFormat.format &&
               i.colorSpace == surfaceFormat.colorSpace){
                swapchainCreateInfo.imageFormat = i.format;
                swapchainCreateInfo.imageColorSpace = i.colorSpace;
                formatIsAvailable = true;
                break;
            }
        }
    }
    //如果没有符合的格式，恰好有个语义相符的错误代码
    if (!formatIsAvailable){
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    //如果交换链已存在，调用RecreateSwapchain()重建交换链
    if (swapchain){
        return RecreateSwapchain();
    }
    return VK_SUCCESS;
}

VkResult graphicsBase::CreateSwapchain(bool limitFrameRate, VkSwapchainCreateFlagsKHR flags)
{
    //获取一下surface的支持能力与限制条件
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
        qDebug("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: %d\n", int32_t(result));
        return result;
    }
    //交换图像数量
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + (surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);

    //判断下交换链支持能力中是否已经确定
    VkExtent2D extent = {};
    if(surfaceCapabilities.currentExtent.width == -1){ //未确定,启用默认extent
        extent.width = glm::clamp(defaultWindowSize.width,surfaceCapabilities.minImageExtent.width,surfaceCapabilities.maxImageExtent.width);
        extent.height = glm::clamp(defaultWindowSize.height,surfaceCapabilities.minImageExtent.height,surfaceCapabilities.maxImageExtent.height);
    }
    else{ //已确定,启用交换链支持能力中的范围
        extent.width = surfaceCapabilities.currentExtent.width;
        extent.height = surfaceCapabilities.currentExtent.height;
    }
    //交换链图像尺寸
    swapchainCreateInfo.imageExtent = extent;

    //视点数为1，变换使用当前变换：
    swapchainCreateInfo.imageArrayLayers = 1;
    //一般有5种: 不变换、顺时针旋转90°、顺时针旋转180°、水平镜像、垂直镜像. (当前变换一般是不变换)
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;

    //透明度通道不着重处理--因为我们的目标是计算着色器，显示只是附加
    if(surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR){
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    else{
        for (size_t i = 0; i < 4; i++){
            if (surfaceCapabilities.supportedCompositeAlpha & 1 << i) {
                swapchainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & 1 << i);
                break;
            }
        }
    }
    //图像的用途 -- 非常重要。注意:有些设备(MacOS)不支持计算着色器直接写交换链，最佳做法先写入中间缓存，最后再写入交换链
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //图形管线输出到屏幕
    if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; //传输拷贝交换链图像
    if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; //传输拷贝交换链图像
    else
        qDebug("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!\n");


    //获取surface格式,如果没有可用格式则获取一下
    if(availableSurfaceFormats.empty()){
        if(VkResult result = GetSurfaceFormats()){
            return result;
        }
    }
    //设置交换链的图像格式(默认使用VK_FORMAT_R8G8B8A8_UNORM，如果找不到则使用surface中可用format的第一个)
    if (!swapchainCreateInfo.imageFormat){
        if (SetSurfaceFormat({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) &&
            SetSurfaceFormat({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })) {
            //如果找不到上述图像格式和色彩空间的组合，那只能有什么用什么，采用availableSurfaceFormats中的第一组
            swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
            swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
            qDebug("[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
        }
    }

    //获取一下surface中支持的呈现模式
    uint32_t surfacePresentModeCount = 0;
    if(VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(this->physicalDevice,this->surface,&surfacePresentModeCount,nullptr)){
        qDebug("[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: %d\n", int32_t(result));
        return result;
    }
    if (!surfacePresentModeCount){
        qDebug("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n"),
        abort();
    }
    std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
    if(VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(this->physicalDevice,this->surface,&surfacePresentModeCount,surfacePresentModes.data())){
        qDebug("[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: %d", int32_t(result));
        return result;
    }

    /*呈现模式
     * 注意:VK_PRESENT_MODE_IMMEDIATE_KHR和VK_PRESENT_MODE_FIFO_RELAXED_KHR可能导致画面撕裂
     * 1.不需要限制帧率时应当选择VK_PRESENT_MODE_MAILBOX_KHR
     * 2.需要限制帧率使其最大不超过屏幕刷新率时应选择VK_PRESENT_MODE_FIFO_KHR
     */
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!limitFrameRate){
        for (size_t i = 0; i < surfacePresentModeCount; i++){
            if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR){
                swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.flags = flags;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.clipped = VK_TRUE;
    if (VkResult result = CreateSwapchain_Internal())
        return result;

    //执行创建交换链回调函数
    ExecuteCallbacks(callbacks_createSwapchain);
    return VK_SUCCESS;
}

VkResult graphicsBase::RecreateSwapchain()
{
    //获取一下surface的支持能力与限制条件
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
        qDebug("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: %d\n", int32_t(result));
        return result;
    }
    if (surfaceCapabilities.currentExtent.width == 0 ||
        surfaceCapabilities.currentExtent.height == 0){
        return VK_SUBOPTIMAL_KHR;
    }
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.oldSwapchain = swapchain;


    //重建交换链最好等待图形和呈现队列空闲（交换链图像被图形队列写入，被呈现队列读取）
    VkResult result = vkQueueWaitIdle(queue_graphics); //仅在等待图形队列成功，且图形与呈现所用队列不同时等待呈现队列
    if (!result &&queue_graphics != queue_presentation){
        result = vkQueueWaitIdle(queue_presentation);
    }
    if (result) {
        qDebug("[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: %d\n", int32_t(result));
        return result;
    }
    //销毁旧有的image view(为什么不在此处销毁交换链图像,因为销毁旧的交换链时会一并销毁交换链图像)
    for (auto& i : swapchainImageViews){
        if (i){
            vkDestroyImageView(device, i, nullptr);
        }
    }
    swapchainImageViews.resize(0);

    //创建新的交换链
    if (result = CreateSwapchain_Internal()){
        return result;
    }
    //执行回调函数，ExecuteCallbacks(...)见后文
    ExecuteCallbacks(callbacks_createSwapchain);
    return VK_SUCCESS;
}

uint32_t graphicsBase::ApiVersion() const
{
    return apiVersion;
}

VkResult graphicsBase::UseLatestApiVersion()
{
    /*待Ch1-3填充*/
    if(vkGetInstanceProcAddr(this->instance,"vkEnumerateInstanceVersion")){
        return vkEnumerateInstanceVersion(&apiVersion);
    }
    return VK_SUCCESS;
}

void graphicsBase::AddCallback_CreateSwapchain(std::function<void()> function)
{
    callbacks_createSwapchain.push_back(function);
}

void graphicsBase::AddCallback_DestroySwapchain(std::function<void()> function)
{
    callbacks_destroySwapchain.push_back(function);
}

void graphicsBase::ExecuteCallbacks(std::vector<std::function<void ()> > callbacks)
{
    for (size_t size = callbacks.size(), i = 0; i < size; i++){
        callbacks[i]();
    }
}

void graphicsBase::AddCallback_CreateDevice(std::function<void()> function)
{
    callbacks_createDevice.push_back(function);
}

void graphicsBase::AddCallback_DestroyDevice(std::function<void()> function)
{
    callbacks_destroyDevice.push_back(function);
}

VkResult graphicsBase::WaitIdle() const
{
    VkResult result = vkDeviceWaitIdle(device);
    if (result){
        qDebug("[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: %d\n", int32_t(result));
    }
    return result;
}

VkResult graphicsBase::RecreateDevice(VkDeviceCreateFlags flags)
{
    //销毁原有的逻辑设备
    if (device) {
        VkResult result = WaitIdle();
        if (result != VK_SUCCESS &&
            result != VK_ERROR_DEVICE_LOST){

            if (swapchain) {
                //调用销毁交换链时的回调函数
                ExecuteCallbacks(callbacks_destroySwapchain);
                qDebug()<<"ExecuteCallbacks 2 callbacks_destroySwapchain: "<<callbacks_destroySwapchain.size();
                //销毁交换链图像的image view
                for (auto& i : swapchainImageViews){
                    if (i){
                        vkDestroyImageView(device, i, nullptr);
                    }
                }
                swapchainImageViews.resize(0);
                //销毁交换链
                vkDestroySwapchainKHR(device, swapchain, nullptr);
                //重置交换链handle
                swapchain = (VkSwapchainKHR)VK_NULL_HANDLE;
                //重置交换链创建信息
                swapchainCreateInfo = {};
            }
            ExecuteCallbacks(callbacks_destroyDevice);
            qDebug()<<"ExecuteCallbacks 3 callbacks_destroyDevice: "<<callbacks_destroyDevice.size();
            vkDestroyDevice(device, nullptr);
            device = (VkDevice)VK_NULL_HANDLE;;
        }
    }
    //创建新的逻辑设备
    return CreateDevice(flags);
}

void graphicsBase::Terminate()
{
    this->~graphicsBase();
    instance = (VkInstance)VK_NULL_HANDLE;
    physicalDevice = (VkPhysicalDevice)VK_NULL_HANDLE;
    device = (VkDevice)VK_NULL_HANDLE;
    surface = (VkSurfaceKHR)VK_NULL_HANDLE;
    swapchain = (VkSwapchainKHR)VK_NULL_HANDLE;
    swapchainImages.resize(0);
    swapchainImageViews.resize(0);
    swapchainCreateInfo = {};
    debugMessenger = (VkDebugUtilsMessengerEXT)VK_NULL_HANDLE;
}

result_t graphicsBase::SwapImage(VkSemaphore semaphore_imageIsAvailable)
{
    //检查交换链是否发生了变换，如果变化了则销毁旧的交换链
    if (swapchainCreateInfo.oldSwapchain &&
        swapchainCreateInfo.oldSwapchain != swapchain) {
        vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
        swapchainCreateInfo.oldSwapchain = (VkSwapchainKHR)VK_NULL_HANDLE;
    }
    //获取交换链图像索引
    while (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_imageIsAvailable, (VkFence)VK_NULL_HANDLE, &currentImageIndex)){
        switch (result) {
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR:
            if (VkResult result = RecreateSwapchain())
                return result;
            break;
        default:
            qDebug("[ graphicsBase ] ERROR\nFailed to acquire the next image!\nError code: %d\n", int32_t(result));
            return result;
        }
    }
    return VK_SUCCESS;
}

result_t graphicsBase::SubmitCommandBuffer_Graphics(VkSubmitInfo &submitInfo, VkFence fence) const
{
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkResult result = vkQueueSubmit(queue_graphics, 1, &submitInfo, fence);
    if (result){
        qDebug("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: %d\n", int32_t(result));
    }
    return result;
}

result_t graphicsBase::SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkSemaphore semaphore_imageIsAvailable, VkSemaphore semaphore_renderingIsOver, VkFence fence, VkPipelineStageFlags waitDstStage_imageIsAvailable) const
{
    VkSubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (semaphore_imageIsAvailable){
        submitInfo.waitSemaphoreCount = 1,
        submitInfo.pWaitSemaphores = &semaphore_imageIsAvailable,
        submitInfo.pWaitDstStageMask = &waitDstStage_imageIsAvailable;
    }
    if (semaphore_renderingIsOver){
        submitInfo.signalSemaphoreCount = 1,
        submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
    }
    return SubmitCommandBuffer_Graphics(submitInfo, fence);
}

result_t graphicsBase::SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence) const
{
    VkSubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    return SubmitCommandBuffer_Graphics(submitInfo, fence);
}

result_t graphicsBase::SubmitCommandBuffer_Compute(VkSubmitInfo &submitInfo, VkFence fence) const
{
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkResult result = vkQueueSubmit(queue_compute, 1, &submitInfo, fence);
    if (result){
        qDebug("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: %d\n", int32_t(result));
    }
    return result;
}

result_t graphicsBase::SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence) const
{
    VkSubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    return SubmitCommandBuffer_Compute(submitInfo, fence);
}

result_t graphicsBase::PresentImage(VkPresentInfoKHR &presentInfo)
{
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    switch (VkResult result = vkQueuePresentKHR(queue_presentation, &presentInfo)) {
    case VK_SUCCESS:
        return VK_SUCCESS;
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
        return RecreateSwapchain();
    default:
        qDebug("[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError code: %d\n", int32_t(result));
        return result;
    }
}

result_t graphicsBase::PresentImage(VkSemaphore semaphore_renderingIsOver)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &currentImageIndex;
    if (semaphore_renderingIsOver){
        presentInfo.waitSemaphoreCount = 1,
        presentInfo.pWaitSemaphores = &semaphore_renderingIsOver;
    }
    return PresentImage(presentInfo);
}

result_t graphicsBase::SubmitCommandBuffer_Presentation(VkCommandBuffer commandBuffer, VkSemaphore semaphore_renderingIsOver, VkSemaphore semaphore_ownershipIsTransfered, VkFence fence) const
{
    static VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    if (semaphore_renderingIsOver){
        submitInfo.waitSemaphoreCount = 1,
        submitInfo.pWaitSemaphores = &semaphore_renderingIsOver,
        submitInfo.pWaitDstStageMask = &waitDstStage;
    }
    if (semaphore_ownershipIsTransfered){
        submitInfo.signalSemaphoreCount = 1,
        submitInfo.pSignalSemaphores = &semaphore_ownershipIsTransfered;
    }
    VkResult result = vkQueueSubmit(queue_presentation, 1, &submitInfo, fence);
    if (result){
        qDebug("[ graphicsBase ] ERROR\nFailed to submit the presentation command buffer!\nError code: %d\n", int32_t(result));
    }
    return result;
}

void graphicsBase::CmdTransferImageOwnership(VkCommandBuffer commandBuffer) const
{
    VkImageMemoryBarrier imageMemoryBarrier_g2p = {};
    imageMemoryBarrier_g2p.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier_g2p.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier_g2p.dstAccessMask = 0;
    imageMemoryBarrier_g2p.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier_g2p.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier_g2p.srcQueueFamilyIndex = queueFamilyIndex_graphics;
    imageMemoryBarrier_g2p.dstQueueFamilyIndex = queueFamilyIndex_presentation;
    imageMemoryBarrier_g2p.image = swapchainImages[currentImageIndex];
    imageMemoryBarrier_g2p.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
        0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_g2p);
}
