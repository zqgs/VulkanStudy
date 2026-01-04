#ifndef VKBASE_H
#define VKBASE_H
#include "EasyVKStart.h"
#define VK_RESULT_THROW

#ifndef QT_NO_DEBUG
#define ENABLE_DEBUG_MESSENGER true
#else
#define ENABLE_DEBUG_MESSENGER false
#endif

//用于析构器中销毁Vulkan对象的宏
#define DestroyHandleBy(Func) if (handle) { Func(graphicsBase::Base().Device(), handle, nullptr); handle = (decltype(handle))VK_NULL_HANDLE; }
//用于移动构造器中的宏
#define MoveHandle handle = other.handle; other.handle = (decltype(other.handle))VK_NULL_HANDLE;
//该宏定义转换函数,通过返回handle将封装类型对象隐式转换到被封装handle的原始类型：
#define DefineHandleTypeOperator(Type, Member) operator Type() const { return Member; }
//该宏定义转换函数，用于取得被封装handle的地址：
#define DefineAddressFunction const decltype(handle)* Address() const { return &handle; }

//定义vulkan命名空间，之后会把Vulkan中一些基本对象的封装写在其中
namespace vulkan {
    static VkExtent2D defaultWindowSize = { 1280, 720 };
#ifdef VK_RESULT_THROW
    class result_t {
        VkResult result;
    public:
        static void(*callback_throw)(VkResult);
        result_t(VkResult result) :result(result) {}
        result_t(result_t&& other) :result(other.result) { other.result = VK_SUCCESS; }
        ~result_t() {
            if (uint32_t(result) < VK_RESULT_MAX_ENUM)
                return;
            if (callback_throw)
                callback_throw(result);
            //throw result;
        }
        operator VkResult() {
            VkResult result = this->result;
            this->result = VK_SUCCESS;
            return result;
        }
    };

#elif VK_RESULT_NODISCARD //情况2：若抛弃函数返回值，让编译器发出警告
    struct [[nodiscard]] result_t {
        VkResult result;
        result_t(VkResult result) :result(result) {}
        operator VkResult() const { return result; }
    };
    //在本文件中关闭弃值提醒（因为我懒得做处理）
    #pragma warning(disable:4834)
    #pragma warning(disable:6031)
#else //情况3：啥都不干
    using result_t = VkResult;
#endif

/*graphicsBase创建逻辑
 * 1.创建vkInstance实例
 * 2.开启debugMessager
 * 3.创建surface
 * 4.枚举物理设备
 * 5.根据物理设备创建逻辑设备
 * 6.根据物理设备创建交换链
 * 7.根据交换链创建队列、图像、视图
 */
    class graphicsBasePlus; //graphicsBasePlus扩展graphicsBase的功能，用于默认创建一些对于Vulkan图形编程有必要的对象。
    class graphicsBase{
        //static graphicsBase singleton;
        graphicsBase() = default;
        graphicsBase(graphicsBase&&) = delete;
        ~graphicsBase();

        graphicsBasePlus* pPlus;
    public:
        static graphicsBase& Base(){
            static graphicsBase singleton;
            return singleton;
        }
        static graphicsBasePlus& Plus() { return *Base().pPlus; }
        static void Plus(graphicsBasePlus& plus) { if (!Base().pPlus) Base().pPlus = &plus; }
    private:
        VkInstance instance;
        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;

        //向层容器和扩展容器中添加字符串，并确保不重复
        static void AddLayerOrExtension(std::vector<const char*>& container,const char* name){
            for(auto& i : container){
                if(!strcmp(name,i)){
                    return;
                }
            }
            container.push_back(name);
        }
//创建vkInstance实例
    public:
        VkInstance Instance();
        const std::vector<const char*>& InstanceLayers() const;
        const std::vector<const char*>& InstanceExtensions() const;
        void AddInstanceLayers(const char* layerName);
        void AddInstanceExtensions(const char* layerName);
        //以下函数用于创建Vulkan实例
        result_t CreateInstance(VkInstanceCreateFlags flag = 0);
        //以下函数用于创建Vulkan实例失败后执行
        VkResult CheckInstanceLayers(std::vector<const char*>& layersToCheck);
        void InstanceLayers(const std::vector<const char*>& layerNames);
        VkResult CheckInstanceExtensions(std::vector<const char*>& extensionsToCheck,const char* layerName);
        void InstanceExtensions(const std::vector<const char*>& extensionsNames);


/*创建debug messenger的步骤
 * 1.创建了Vulkan实例后，即可创建debug messenger，以便检查初始化流程中的所有其他步骤。
 * 2.创建debug messenger可以粗略地说是只有一步
 * 3.Vulkan中，扩展相关的函数，若非设备特定，大都通过vkGetInstanceProcAddr(...)来获取
*/
    private:
        VkDebugUtilsMessengerEXT debugMessenger;
        //以下函数用于创建debug messenger
        VkResult CreateDebugMessenger();

/*创建window surface的步骤
 * 注意:多窗口会存在多个surface,也会存在多个交换链
*/
    private:
        VkSurfaceKHR surface;
    public:
        VkSurfaceKHR Surface() const;
        void Surface(VkSurfaceKHR vk_s);

/*创建逻辑设备的步骤
 * 1.获取物理设备列表
 * 2.检查物理设备是否满足所需的队列族类型，从中选择能满足要求的设备并顺便取得队列族索引
 * 3.确定所需的设备级别扩展，不检查是否可用
 * 4.用vkCreateDevice(...)创建逻辑设备，取得队列
 * 5.取得物理设备属性、物理设备内存属性，以备之后使用
 *  注意:
 *      1.物理设备(真实gpu)。仅用来查询gpu能力，选择合适gpu用来创建逻辑设备
 *      2.逻辑设备。Vulkan 中 所有渲染、计算、内存操作必须通过逻辑设备提交命令。
 *      3.一个物理设备可以创建n个逻辑设备
*/
    private:
        //物理设备
        VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceProperties physicalDeviceProperties;
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
        std::vector<VkPhysicalDevice> availablePhysicalDevices;

        //逻辑设备
        VkDevice device;
        //有效的索引从0开始，因此使用特殊值VK_QUEUE_FAMILY_IGNORED（为UINT32_MAX）为队列族索引的默认值
        uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;

         //图形队列
        VkQueue queue_graphics;
        //呈现队列
        VkQueue queue_presentation;
        //计算队列
        VkQueue queue_compute;

        std::vector<const char*> deviceExtensions;

        //该函数被DeterminePhysicalDevice(...)调用，用于检查物理设备是否满足所需的队列族类型，并将对应的队列族索引返回到queueFamilyIndices，执行成功时直接将索引写入相应成员变量
        VkResult GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue, bool enableComputeQueue, uint32_t (&queueFamilyIndices)[3]);
    public:
        //Getter
        VkPhysicalDevice PhysicalDevice() const;
        const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const;
        const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const;
        VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const;
        uint32_t AvailablePhysicalDeviceCount() const;
        VkDevice Device() const;
        uint32_t QueueFamilyIndex_Graphics() const;
        uint32_t QueueFamilyIndex_Presentation() const;
        uint32_t QueueFamilyIndex_Compute() const;
        VkQueue Queue_Graphics() const;
        VkQueue Queue_Presentation() const;
        VkQueue Queue_Compute() const;
        const std::vector<const char*>& DeviceExtensions() const;
        //该函数用于创建逻辑设备前
        void AddDeviceExtension(const char* extensionName);
        //该函数用于获取物理设备
        VkResult GetPhysicalDevices();
        //该函数用于指定所用物理设备并调用GetQueueFamilyIndices(...)取得队列族索引
        VkResult DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true);
        //该函数用于创建逻辑设备，并取得队列
        VkResult CreateDevice(VkDeviceCreateFlags flags = 0);
        //以下函数用于创建逻辑设备失败后
        VkResult CheckDeviceExtensions(std::vector<const char*> extensionsToCheck, const char* layerName = nullptr) const;
        void DeviceExtensions(const std::vector<const char*>& extensionNames);

/*创建交换链的步骤
 * 1.填写一大堆信息
 * 2.创建交换链并取得交换链图像，为交换链图像创建image view
*/
    private:
        std::vector <VkSurfaceFormatKHR> availableSurfaceFormats;
        VkSwapchainKHR swapchain;
        std::vector <VkImage> swapchainImages;
        std::vector <VkImageView> swapchainImageViews;
        //保存交换链的创建信息以便重建交换链
        VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
        //该函数被CreateSwapchain(...)和RecreateSwapchain()调用
        VkResult CreateSwapchain_Internal();

    public:
        //Getter
        const VkFormat& AvailableSurfaceFormat(uint32_t index) const;
        const VkColorSpaceKHR& AvailableSurfaceColorSpace(uint32_t index) const;
        uint32_t AvailableSurfaceFormatCount() const;
        VkSwapchainKHR Swapchain() const;
        VkImage SwapchainImage(uint32_t index) const;
        VkImageView SwapchainImageView(uint32_t index) const;
        uint32_t SwapchainImageCount() const;
        const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const;
        VkResult GetSurfaceFormats();
        VkResult SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat);
        //该函数用于创建交换链
        VkResult CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0);
        //该函数用于重建交换链
        VkResult RecreateSwapchain();

/*Vulkan版本*/
    private:
        uint32_t apiVersion = VK_API_VERSION_1_0;
    public:
        //Getter
        uint32_t ApiVersion() const;
        VkResult UseLatestApiVersion();

/*创建和销毁交换链时的回调函数*/
    private:
        std::vector<std::function<void()>> callbacks_createSwapchain;
        std::vector<std::function<void()>> callbacks_destroySwapchain;
    public:
        void AddCallback_CreateSwapchain(std::function<void()> function);
        void AddCallback_DestroySwapchain(std::function<void()> function);
    private:
        static void ExecuteCallbacks(std::vector<std::function<void()>> callbacks);
/*创建和销毁逻辑设备时的回调函数*/
    private:
        static std::mutex callback_mtx;
        std::vector<std::function<void()>> callbacks_createDevice;
        std::vector<std::function<void()>> callbacks_destroyDevice;

    public:
        void AddCallback_CreateDevice(std::function<void()> function);
        void AddCallback_DestroyDevice(std::function<void()> function);
/*等待逻辑设备空闲*/
    public:
        VkResult WaitIdle() const;

/*
 * 重建逻辑设备
 *    比如:运行过程中切换显卡，或逻辑设备丢失等情况
 */
    public:
        VkResult RecreateDevice(VkDeviceCreateFlags flags = 0);

/*程序运行过程中销毁vulkan*/
    public:
        void Terminate();

 //获取交换链索引
    private:
        uint32_t currentImageIndex = 0;
    public:
        uint32_t CurrentImageIndex(){return currentImageIndex;}
        result_t SwapImage(VkSemaphore semaphore_imageIsAvailable);

//提交命令缓冲区
    public:
        //提交命令缓冲区到图形队列
        result_t SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence = (VkFence)VK_NULL_HANDLE) const;
        //提交命令缓冲区到图形队列的常用参数
        result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer,
            VkSemaphore semaphore_imageIsAvailable = (VkSemaphore)VK_NULL_HANDLE,
            VkSemaphore semaphore_renderingIsOver = (VkSemaphore)VK_NULL_HANDLE,
            VkFence fence = (VkFence)VK_NULL_HANDLE,
            VkPipelineStageFlags waitDstStage_imageIsAvailable = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const;

        //提交命令缓冲区到图形队列只使用栅栏常用参数
        result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence = (VkFence)VK_NULL_HANDLE) const;

        //将命令缓冲区提交的计算队列
        result_t SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence = (VkFence)VK_NULL_HANDLE) const;

        //将命令缓冲区提交的计算队列只使用栅栏常用参数
        result_t SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence = (VkFence)VK_NULL_HANDLE) const;

//呈现图像
        result_t PresentImage(VkPresentInfoKHR& presentInfo);
        //呈现图像的常用参数
        result_t PresentImage(VkSemaphore semaphore_renderingIsOver = (VkSemaphore)VK_NULL_HANDLE);
    public:
        result_t SubmitCommandBuffer_Presentation(VkCommandBuffer commandBuffer,
            VkSemaphore semaphore_renderingIsOver = (VkSemaphore)VK_NULL_HANDLE,
            VkSemaphore semaphore_ownershipIsTransfered = (VkSemaphore)VK_NULL_HANDLE,
            VkFence fence = (VkFence)VK_NULL_HANDLE) const;
//内存屏障
    public:
        void CmdTransferImageOwnership(VkCommandBuffer commandBuffer) const;
    };

/*  CPU                             GPU
     |                               |
     | vkAcquireNextImageKHR()        |
     |------------------------------>|
     |                               |  signal semaphoreA when image available
     | record command buffer          |  <-- CPU可以立即录制命令缓冲区
     |------------------------------>|
     | vkQueueSubmit(waitSemaphore=A, signalSemaphore=B)
     |------------------------------>|
     |                               |  GPU 挂起等待 semaphoreA
     |                               |  semaphoreA triggered -> 执行命令缓冲区
     |                               |  渲染完成 -> 触发 semaphoreB & fence
     | waitFence / CPU reuse cmdBuf  |
     |                               |
     | vkQueuePresentKHR(waitSemaphore=B)
     |------------------------------>|
     |                               |  GPU 等待 semaphoreB -> 呈现图像
     |                               |

    信号量 A（imageAvailable）
        1.由 vkAcquireNextImageKHR 触发
        2.GPU 在 vkQueueSubmit 时等待它，确保交换链图像可用
        3.CPU 可在此阶段自由录制命令缓冲区，不阻塞

    信号量 B（renderFinished）
        1.由 GPU 在命令缓冲区执行完成时触发
        2.呈现队列 vkQueuePresentKHR 等待它，保证图像已渲染完

    CPU/GPU 分工
        CPU：获取 image、录制命令缓冲区、提交队列、等待 fence（可选）
        GPU：等待信号量 A、执行命令缓冲区、触发信号量 B、呈现
 */
//创建栅栏 -- 介于cpu和gpu之间的一种信号量
    class fence{
        VkFence handle = (VkFence)VK_NULL_HANDLE;
    public:
        fence(VkFenceCreateInfo& createInfo) {
            Create(createInfo);
        }
        //默认构造器创建未置位的栅栏
        fence(VkFenceCreateFlags flags = 0) {
            Create(flags);
        }
        fence(fence&& other){MoveHandle;}
        ~fence(){DestroyHandleBy(vkDestroyFence);}
        //Getter
        DefineHandleTypeOperator(VkFence,handle);
        DefineAddressFunction;
        // DefineAddressFunction;
        result_t Wait() const{
            VkResult result =vkWaitForFences(graphicsBase::Base().Device(),1,&handle,false,UINT64_MAX);
            if(result){
                qDebug("[ fence ] ERROR\nFailed to wait for the fence!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Reset() const{
            VkResult result = vkResetFences(graphicsBase::Base().Device(), 1, &handle);
            if (result){
                qDebug("[ fence ] ERROR\nFailed to reset the fence!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        //此情形出现较多,重置后立即等待
        result_t WaitAndReset() const {
            VkResult result = Wait();
            result || (result = Reset());
            return result;
        }
        result_t Status() const{
            VkResult result = vkGetFenceStatus(graphicsBase::Base().Device(), handle);
            if (result < 0){ //vkGetFenceStatus(...)成功时有两种结果，所以不能仅仅判断result是否非0
                qDebug("[ fence ] ERROR\nFailed to get the status of the fence!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(VkFenceCreateInfo& createInfo){
            createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkResult result = vkCreateFence(graphicsBase::Base().Device(),&createInfo,nullptr,&handle);
            if(result){
                qDebug("[ fence ] ERROR\nFailed to create a fence!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(VkFenceCreateFlags flags){
            VkFenceCreateInfo createInfo = {};
            createInfo.flags = flags;
            return Create(createInfo);
        }
    };

//创建信号量 -- 完全GPU内部使用的一种信号量
    //1.2版本之后支持时间线信号量，时间线信号量兼顾了栅栏的功能
    //时间线信号量并不完全涵盖二值信号量，在提交命令缓冲时可以替代二值信号量，在渲染玄幻获取下一张交换图像时或者呈现图像时必须使用二值信号量
    class semaphore{
        VkSemaphore handle = (VkSemaphore)VK_NULL_HANDLE;
    public:
        semaphore(VkSemaphoreCreateInfo createInfo){
            Create(createInfo);
        }
        semaphore(/*VkSemaphoreCreateFlags flags*/){
            Create();
        }
        semaphore(semaphore&& other){MoveHandle;}
        ~semaphore() { DestroyHandleBy(vkDestroySemaphore); }
        //Getter
        DefineHandleTypeOperator(VkSemaphore,handle);
        DefineAddressFunction;
        result_t Create(VkSemaphoreCreateInfo createInfo){
            createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkResult result = vkCreateSemaphore(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", int32_t(result));
            }
            return result;
        }
        result_t Create(/*VkSemaphoreCreateFlags flags*/) {
            VkSemaphoreCreateInfo createInfo = {};
            return Create(createInfo);
        }
    };

//创建Event类
    class event {
        VkEvent handle = (VkEvent)VK_NULL_HANDLE;
    public:
        //event() = default;
        event(VkEventCreateInfo& createInfo) {
            Create(createInfo);
        }
        event(VkEventCreateFlags flags = 0) {
            Create(flags);
        }
        event(event&& other) { MoveHandle; }
        ~event() { DestroyHandleBy(vkDestroyEvent); }
        //Getter
        DefineHandleTypeOperator(VkEvent,handle);
        DefineAddressFunction;
        //Const Function
        void CmdSet(VkCommandBuffer commandBuffer, VkPipelineStageFlags stage_from) const {
            vkCmdSetEvent(commandBuffer, handle, stage_from);
        }
        void CmdReset(VkCommandBuffer commandBuffer, VkPipelineStageFlags stage_from) const {
            vkCmdResetEvent(commandBuffer, handle, stage_from);
        }
        void CmdWait(VkCommandBuffer commandBuffer,
                     VkPipelineStageFlags stage_from,
                     VkPipelineStageFlags stage_to,
                     arrayRef<VkMemoryBarrier> memoryBarriers,
                     arrayRef<VkBufferMemoryBarrier> bufferMemoryBarriers,
                     arrayRef<VkImageMemoryBarrier> imageMemoryBarriers) const {
            for (auto& i : memoryBarriers){
                i.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            }
            for (auto& i : bufferMemoryBarriers){
                i.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            }
            for (auto& i : imageMemoryBarriers){
                i.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            }
            vkCmdWaitEvents(commandBuffer, 1, &handle, stage_from, stage_to,
                uint32_t(memoryBarriers.Count()), memoryBarriers.Pointer(),
                uint32_t(bufferMemoryBarriers.Count()), bufferMemoryBarriers.Pointer(),
                uint32_t(imageMemoryBarriers.Count()), imageMemoryBarriers.Pointer());
        }
        result_t Set() const {
            VkResult result = vkSetEvent(graphicsBase::Base().Device(), handle);
            if (result){
                qDebug("[ event ] ERROR\nFailed to singal the event!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Reset() const {
            VkResult result = vkResetEvent(graphicsBase::Base().Device(), handle);
            if (result){
                qDebug("[ event ] ERROR\nFailed to unsingal the event!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Status() const {
            VkResult result = vkGetEventStatus(graphicsBase::Base().Device(), handle);
            if (result < 0){ //vkGetEventStatus(...)成功时有两种结果
                qDebug("[ event ] ERROR\nFailed to get the status of the event!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        //Non-const Function
        result_t Create(VkEventCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
            VkResult result = vkCreateEvent(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ event ] ERROR\nFailed to create a event!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(VkEventCreateFlags flags = 0) {
            VkEventCreateInfo createInfo = {};
            createInfo.flags = flags;
            return Create(createInfo);
        }
    };

//创建命令缓冲区
    //Command Buffer 1
    // ├── vkCmdBindPipeline
    // ├── vkCmdBindVertexBuffer
    // ├── vkCmdDraw
    // └── vkCmdCopyBuffer
    class commandBuffer{
        friend class commandPool; //封装命令池的commandPool类负责分配和释放命令缓冲区，需要让其能访问私有成员handle
        VkCommandBuffer handle = (VkCommandBuffer)VK_NULL_HANDLE;
    public:
        commandBuffer() = default;
        commandBuffer(commandBuffer&& other) { MoveHandle; }

        //Getter
        DefineHandleTypeOperator(VkCommandBuffer,handle);
        DefineAddressFunction;

        result_t Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const {
            inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = usageFlags;
            beginInfo.pInheritanceInfo = &inheritanceInfo;
            VkResult result =vkBeginCommandBuffer(handle,&beginInfo);
            if (result){
                qDebug("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Begin(VkCommandBufferUsageFlags usageFlags = 0) const {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = usageFlags;
            VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
            if (result){
                qDebug("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t End() const {
            VkResult result = vkEndCommandBuffer(handle);
            if (result){
                qDebug("[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };

//创建命令池
    //Command Pool
    // ├── Command Buffer 1
    // ├── Command Buffer 2
    // └── Command Buffer 3
    class commandPool {
        VkCommandPool handle = (VkCommandPool)VK_NULL_HANDLE;
    public:
        commandPool() = default;
        commandPool(VkCommandPoolCreateInfo& createInfo) {
            Create(createInfo);
        }
        commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
            Create(queueFamilyIndex, flags);
        }
        commandPool(commandPool&& other) { MoveHandle; }
        ~commandPool() { DestroyHandleBy(vkDestroyCommandPool); }

        //Getter
        DefineHandleTypeOperator(VkCommandPool,handle);
        DefineAddressFunction;

        result_t AllocateBuffers(arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const {
            VkCommandBufferAllocateInfo allocateInfo = {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateInfo.commandPool  = handle;
            allocateInfo.level  = level;
            allocateInfo.commandBufferCount  = uint32_t(buffers.Count());
            VkResult result = vkAllocateCommandBuffers(graphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
            if (result){
                qDebug("[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t AllocateBuffers(arrayRef<commandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const {
            return AllocateBuffers({ &buffers[0].handle, buffers.Count() }, level);
        }
        void FreeBuffers(arrayRef<VkCommandBuffer> buffers) const {
            vkFreeCommandBuffers(graphicsBase::Base().Device(), handle, uint32_t(buffers.Count()), buffers.Pointer());
            memset(buffers.Pointer(), 0, uint32_t(buffers.Count() * sizeof(VkCommandBuffer)));
        }
        void FreeBuffers(arrayRef<commandBuffer> buffers) const {
            FreeBuffers({ &buffers[0].handle, buffers.Count() });
        }

        result_t Create(VkCommandPoolCreateInfo& createInfo){
            createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            VkResult result = vkCreateCommandPool(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
               qDebug("[ commandPool ] ERROR\nFailed to create a command pool!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0){
            VkCommandPoolCreateInfo createInfo;
            createInfo.queueFamilyIndex = queueFamilyIndex;
            createInfo.flags = flags;
            return Create(createInfo);
        }
    };

//创建RenderPass
    class renderPass {
        VkRenderPass handle = (VkRenderPass)VK_NULL_HANDLE;
    public:
        renderPass() = default;
        renderPass(VkRenderPassCreateInfo& createInfo) {
            Create(createInfo);
        }
        renderPass(renderPass&& other) { MoveHandle; }
        ~renderPass() { DestroyHandleBy(vkDestroyRenderPass); }
        //Getter
        DefineHandleTypeOperator(VkRenderPass,handle);
        DefineAddressFunction;
        //Const Function
        void CmdBegin(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo& beginInfo, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = handle;
            vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
        }
        void CmdBegin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea, arrayRef<const VkClearValue> clearValues = {}, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
            VkRenderPassBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = handle;
            beginInfo.framebuffer = framebuffer;
            beginInfo.renderArea = renderArea;
            beginInfo.clearValueCount = uint32_t(clearValues.Count());
            beginInfo.pClearValues = clearValues.Pointer();
            vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
        }
        void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
            vkCmdNextSubpass(commandBuffer, subpassContents);
        }
        void CmdEnd(VkCommandBuffer commandBuffer) const {
            vkCmdEndRenderPass(commandBuffer);
        }
        //Non-const Function
        result_t Create(VkRenderPassCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            VkResult result = vkCreateRenderPass(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ renderPass ] ERROR\nFailed to create a render pass!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };

//创建framebuffer类
    class framebuffer {
        VkFramebuffer handle = (VkFramebuffer)VK_NULL_HANDLE;
    public:
        framebuffer() = default;
        framebuffer(VkFramebufferCreateInfo& createInfo) {
            Create(createInfo);
        }
        framebuffer(framebuffer&& other) { MoveHandle; }
        ~framebuffer() { DestroyHandleBy(vkDestroyFramebuffer); }
        //Getter
        DefineHandleTypeOperator(VkFramebuffer,handle);
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkFramebufferCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            VkResult result = vkCreateFramebuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };

//创建着色器模块类
    class shaderModule {
        VkShaderModule handle = (VkShaderModule)VK_NULL_HANDLE;
    public:
        shaderModule() = default;
        shaderModule(VkShaderModuleCreateInfo& createInfo) {
            Create(createInfo);
        }
        shaderModule(const char* filepath /*VkShaderModuleCreateFlags flags*/) {
            Create(filepath);
        }
        shaderModule(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/) {
            Create(codeSize, pCode);
        }
        shaderModule(shaderModule&& other) { MoveHandle; }
        ~shaderModule() { DestroyHandleBy(vkDestroyShaderModule); }
        //Getter
        DefineHandleTypeOperator(VkShaderModule,handle);
        DefineAddressFunction;
        //Const Function
        VkPipelineShaderStageCreateInfo StageCreateInfo(VkShaderStageFlagBits stage, const char* entry = "main") const {
            return {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //sType
                nullptr,                                             //pNext
                0,                                                   //flags
                stage,                                               //stage
                handle,                                              //module
                entry,                                               //pName
                nullptr                                              //pSpecializationInfo
            };
        }
        //Non-const Function
        result_t Create(VkShaderModuleCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            VkResult result = vkCreateShaderModule(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ shader ] ERROR\nFailed to create a shader module!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(const char* filepath /*VkShaderModuleCreateFlags flags*/) {
            std::ifstream file(filepath, std::ios::ate | std::ios::binary);
            if (!file) {
                qDebug("[ shader ] ERROR\nFailed to open the file: %s\n", filepath);
                return VK_RESULT_MAX_ENUM; //没有合适的错误代码，别用VK_ERROR_UNKNOWN
            }
            size_t fileSize = size_t(file.tellg());
            std::vector<uint32_t> binaries(fileSize / 4);
            file.seekg(0);
            file.read(reinterpret_cast<char*>(binaries.data()), fileSize);
            file.close();
            return Create(fileSize, binaries.data());
        }
        result_t Create(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/) {
            VkShaderModuleCreateInfo createInfo = {};
            createInfo.codeSize = codeSize;
            createInfo.pCode = pCode;
            return Create(createInfo);
        }
    };

//创建pipeline layout
    class pipelineLayout {
        VkPipelineLayout handle = (VkPipelineLayout)VK_NULL_HANDLE;
    public:
        pipelineLayout() = default;
        pipelineLayout(VkPipelineLayoutCreateInfo& createInfo) {
            Create(createInfo);
        }
        pipelineLayout(pipelineLayout&& other) { MoveHandle; }
        ~pipelineLayout() { DestroyHandleBy(vkDestroyPipelineLayout); }
        //Getter
        DefineHandleTypeOperator(VkPipelineLayout,handle);
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkPipelineLayoutCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            VkResult result = vkCreatePipelineLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ pipelineLayout ] ERROR\nFailed to create a pipeline layout!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };

//创建pipeline layout
    class pipeline {
        VkPipeline handle = (VkPipeline)VK_NULL_HANDLE;
    public:
        pipeline() = default;
        pipeline(VkGraphicsPipelineCreateInfo& createInfo) {
            Create(createInfo);
        }
        pipeline(VkComputePipelineCreateInfo& createInfo) {
            Create(createInfo);
        }
        pipeline(pipeline&& other) { MoveHandle; }
        ~pipeline() { DestroyHandleBy(vkDestroyPipeline); }
        //Getter
        DefineHandleTypeOperator(VkPipeline,handle);
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkGraphicsPipelineCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            VkResult result = vkCreateGraphicsPipelines(graphicsBase::Base().Device(), (VkPipelineCache)VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ pipeline ] ERROR\nFailed to create a graphics pipeline!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(VkComputePipelineCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            VkResult result = vkCreateComputePipelines(graphicsBase::Base().Device(), (VkPipelineCache)VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ pipeline ] ERROR\nFailed to create a compute pipeline!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };

//封装deviceMemory类
    class deviceMemory {
        VkDeviceMemory handle = (VkDeviceMemory)VK_NULL_HANDLE;
        VkDeviceSize allocationSize = 0;            //实际分配的内存大小
        VkMemoryPropertyFlags memoryProperties = 0; //内存属性
        //--------------------
        //该函数用于在映射内存区时，调整非host coherent的内存区域的范围
        VkDeviceSize AdjustNonCoherentMemoryRange(VkDeviceSize& size, VkDeviceSize& offset) const {
            const VkDeviceSize& nonCoherentAtomSize = graphicsBase::Base().PhysicalDeviceProperties().limits.nonCoherentAtomSize;
            VkDeviceSize _offset = offset;
            offset = offset / nonCoherentAtomSize * nonCoherentAtomSize;
            size = std::min((size + _offset + nonCoherentAtomSize - 1) / nonCoherentAtomSize * nonCoherentAtomSize, allocationSize) - offset;
            return _offset - offset;
        }
    protected:
        //用于bufferMemory或imageMemory，定义于此以节省8个字节
        class AreBoundFlag {
            friend class bufferMemory;
            friend class imageMemory;
            bool value = false;
            operator bool() const { return value; }
            AreBoundFlag & operator=(bool value) { this->value = value; return *this; }
        };
        AreBoundFlag areBound;
    public:
        deviceMemory() = default;
        deviceMemory(VkMemoryAllocateInfo& allocateInfo) {
            Allocate(allocateInfo);
        }
        deviceMemory(deviceMemory&& other) {
            MoveHandle;
            allocationSize = other.allocationSize;
            memoryProperties = other.memoryProperties;
            other.allocationSize = 0;
            other.memoryProperties = 0;
        }
        ~deviceMemory() { DestroyHandleBy(vkFreeMemory); allocationSize = 0; memoryProperties = 0; }
        //Getter
        DefineHandleTypeOperator(VkDeviceMemory,handle);
        DefineAddressFunction;
        VkDeviceSize AllocationSize() const { return allocationSize; }
        VkMemoryPropertyFlags MemoryProperties() const { return memoryProperties; }
        //Const Function
        //映射host visible的内存区
        result_t MapMemory(void*& pData, VkDeviceSize size, VkDeviceSize offset = 0) const {
            VkDeviceSize inverseDeltaOffset;
            if (!(memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                inverseDeltaOffset = AdjustNonCoherentMemoryRange(size, offset);
            if (VkResult result = vkMapMemory(graphicsBase::Base().Device(), handle, offset, size, 0, &pData)) {
                qDebug("[ deviceMemory ] ERROR\nFailed to map the memory!\nError code: %d\n", int32_t(result));
                return result;
            }
            if (!(memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                pData = static_cast<uint8_t*>(pData) + inverseDeltaOffset;
                VkMappedMemoryRange mappedMemoryRange = {};
                mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                mappedMemoryRange.memory = handle;
                mappedMemoryRange.offset = offset;
                mappedMemoryRange.size = size;

                if (VkResult result = vkInvalidateMappedMemoryRanges(graphicsBase::Base().Device(), 1, &mappedMemoryRange)) {
                    qDebug("[ deviceMemory ] ERROR\nFailed to flush the memory!\nError code: %d\n", int32_t(result));
                    return result;
                }
            }
            return VK_SUCCESS;
        }
        //取消映射host visible的内存区
        result_t UnmapMemory(VkDeviceSize size, VkDeviceSize offset = 0) const {
            if (!(memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                AdjustNonCoherentMemoryRange(size, offset);
                VkMappedMemoryRange mappedMemoryRange = {};
                mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                mappedMemoryRange.memory = handle;
                mappedMemoryRange.offset = offset;
                mappedMemoryRange.size = size;

                if (VkResult result = vkFlushMappedMemoryRanges(graphicsBase::Base().Device(), 1, &mappedMemoryRange)) {
                    qDebug("[ deviceMemory ] ERROR\nFailed to flush the memory!\nError code: %d\n", int32_t(result));
                    return result;
                }
            }
            vkUnmapMemory(graphicsBase::Base().Device(), handle);
            return VK_SUCCESS;
        }
        //BufferData(...)用于方便地更新设备内存区，适用于用memcpy(...)向内存区写入数据后立刻取消映射的情况
        result_t BufferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset = 0) const {
            void* pData_dst;
            if (VkResult result = MapMemory(pData_dst, size, offset))
                return result;
            memcpy(pData_dst, pData_src, size_t(size));
            return UnmapMemory(size, offset);
        }
        result_t BufferData(const void* data_src) const {
            return BufferData(&data_src, sizeof(data_src));
        }
        //RetrieveData(...)用于方便地从设备内存区取回数据，适用于用memcpy(...)从内存区取得数据后立刻取消映射的情况
        result_t RetrieveData(void* pData_dst, VkDeviceSize size, VkDeviceSize offset = 0) const {
            void* pData_src;
            if (VkResult result = MapMemory(pData_src, size, offset))
                return result;
            memcpy(pData_dst, pData_src, size_t(size));
            return UnmapMemory(size, offset);
        }
        //Non-const Function
        result_t Allocate(VkMemoryAllocateInfo& allocateInfo) {
            if (allocateInfo.memoryTypeIndex >= graphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypeCount) {
                qDebug("[ deviceMemory ] ERROR\nInvalid memory type index!\n");
                return VK_RESULT_MAX_ENUM; //没有合适的错误代码，别用VK_ERROR_UNKNOWN
            }
            allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            if (VkResult result = vkAllocateMemory(graphicsBase::Base().Device(), &allocateInfo, nullptr, &handle)) {
                qDebug("[ deviceMemory ] ERROR\nFailed to allocate memory!\nError code: %d\n", int32_t(result));
                return result;
            }
            //记录实际分配的内存大小
            allocationSize = allocateInfo.allocationSize;
            //取得内存属性
            memoryProperties = graphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypes[allocateInfo.memoryTypeIndex].propertyFlags;
            return VK_SUCCESS;
        }
    };

//创建buffer: 类似于cpu内存中的uint8_t* 属于线性数据
    class buffer {
        VkBuffer handle = (VkBuffer)VK_NULL_HANDLE;
    public:
        buffer() = default;
        buffer(VkBufferCreateInfo& createInfo) {
            Create(createInfo);
        }
        buffer(buffer&& other) { MoveHandle; }
        ~buffer() { DestroyHandleBy(vkDestroyBuffer); }
        //Getter
        DefineHandleTypeOperator(VkBuffer,handle);
        DefineAddressFunction;
        //Const Function
        VkMemoryAllocateInfo MemoryAllocateInfo(VkMemoryPropertyFlags desiredMemoryProperties) const {
            VkMemoryAllocateInfo memoryAllocateInfo = {};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

            VkMemoryRequirements memoryRequirements;
            vkGetBufferMemoryRequirements(graphicsBase::Base().Device(), handle, &memoryRequirements);
            memoryAllocateInfo.allocationSize = memoryRequirements.size;
            memoryAllocateInfo.memoryTypeIndex = UINT32_MAX;
            auto& physicalDeviceMemoryProperties = graphicsBase::Base().PhysicalDeviceMemoryProperties();
            for (size_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++){
                if (memoryRequirements.memoryTypeBits & 1 << i &&
                    (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & desiredMemoryProperties) == desiredMemoryProperties) {
                    memoryAllocateInfo.memoryTypeIndex = uint32_t(i);
                    break;
                }
            }
            //不在此检查是否成功取得内存类型索引，因为会把memoryAllocateInfo返回出去，交由外部检查
            //if (memoryAllocateInfo.memoryTypeIndex == UINT32_MAX)
            //    outStream << std::format("[ buffer ] ERROR\nFailed to find any memory type satisfies all desired memory properties!\n");
            return memoryAllocateInfo;
        }
        result_t BindMemory(VkDeviceMemory deviceMemory, VkDeviceSize memoryOffset = 0) const {
            VkResult result = vkBindBufferMemory(graphicsBase::Base().Device(), handle, deviceMemory, memoryOffset);
            if (result){
                qDebug("[ buffer ] ERROR\nFailed to attach the memory!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        //Non-const Function
        result_t Create(VkBufferCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            VkResult result = vkCreateBuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ buffer ] ERROR\nFailed to create a buffer!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };

//封装bufferMemory类  -->包含buffer的创建与释放，内存由deviceMemory申请,然后绑定在一起
    class bufferMemory :public buffer, public deviceMemory {
    public:
        bufferMemory() = default;
        bufferMemory(VkBufferCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
            Create(createInfo, desiredMemoryProperties);
        }
        bufferMemory(bufferMemory&& other) :
            buffer(std::move(other)), deviceMemory(std::move(other)) {
            areBound = other.areBound;
            other.areBound = false;
        }
        ~bufferMemory() { areBound = false; }
        //Getter
        //不定义到VkBuffer和VkDeviceMemory的转换函数，因为32位下这俩类型都是uint64_t的别名，会造成冲突（虽然，谁他妈还用32位PC！）
        VkBuffer Buffer() const { return static_cast<const buffer&>(*this); }
        const VkBuffer* AddressOfBuffer() const { return buffer::Address(); }
        VkDeviceMemory Memory() const { return static_cast<const deviceMemory&>(*this); }
        const VkDeviceMemory* AddressOfMemory() const { return deviceMemory::Address(); }
        //若areBond为true，则成功分配了设备内存、创建了缓冲区，且成功绑定在一起
        bool AreBound() const { return areBound; }
        using deviceMemory::AllocationSize;
        using deviceMemory::MemoryProperties;
        //Const Function
        using deviceMemory::MapMemory;
        using deviceMemory::UnmapMemory;
        using deviceMemory::BufferData;
        using deviceMemory::RetrieveData;
        //Non-const Function
        //以下三个函数仅用于Create(...)可能执行失败的情况
        result_t CreateBuffer(VkBufferCreateInfo& createInfo) {
            return buffer::Create(createInfo);
        }
        result_t AllocateMemory(VkMemoryPropertyFlags desiredMemoryProperties) {
            VkMemoryAllocateInfo allocateInfo = MemoryAllocateInfo(desiredMemoryProperties);
            if (allocateInfo.memoryTypeIndex >= graphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypeCount)
                return VK_RESULT_MAX_ENUM; //没有合适的错误代码，别用VK_ERROR_UNKNOWN
            return Allocate(allocateInfo);
        }
        result_t BindMemory() {
            if (VkResult result = buffer::BindMemory(Memory()))
                return result;
            areBound = true;
            return VK_SUCCESS;
        }
        //分配设备内存、创建缓冲、绑定
        result_t Create(VkBufferCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
            VkResult result;
            false || //这行用来应对Visual Studio中代码的对齐
                (result = CreateBuffer(createInfo)) || //用||短路执行
                (result = AllocateMemory(desiredMemoryProperties)) ||
                (result = BindMemory());
            return result;
        }
    };

//创建BufferView: 定义了将纹理缓冲区作为1D图像使用的方式
    class bufferView {
        VkBufferView handle = (VkBufferView)VK_NULL_HANDLE;
    public:
        bufferView() = default;
        bufferView(VkBufferViewCreateInfo& createInfo) {
            Create(createInfo);
        }
        bufferView(VkBuffer buffer, VkFormat format, VkDeviceSize offset = 0, VkDeviceSize range = 0 /*VkBufferViewCreateFlags flags*/) {
            Create(buffer, format, offset, range);
        }
        bufferView(bufferView&& other) { MoveHandle; }
        ~bufferView() { DestroyHandleBy(vkDestroyBufferView); }
        //Getter
        DefineHandleTypeOperator(VkBufferView,handle);
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkBufferViewCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
            VkResult result = vkCreateBufferView(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ bufferView ] ERROR\nFailed to create a buffer view!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(VkBuffer buffer, VkFormat format, VkDeviceSize offset = 0, VkDeviceSize range = 0 /*VkBufferViewCreateFlags flags*/) {
            VkBufferViewCreateInfo createInfo = {};
            createInfo.buffer = buffer;
            createInfo.format = format;
            createInfo.offset = offset;
            createInfo.range = range;
            return Create(createInfo);
        }
    };

//创建VKImage: GPU可以使用的图像数据(具有格式和内存布局)
    class image {
        VkImage handle = (VkImage)VK_NULL_HANDLE;
    public:
        image() = default;
        image(VkImageCreateInfo& createInfo) {
            Create(createInfo);
        }
        image(image&& other){ MoveHandle; }
        ~image() { DestroyHandleBy(vkDestroyImage); }
        //Getter
        DefineHandleTypeOperator(VkImage,handle);
        DefineAddressFunction;
        //Const Function
        VkMemoryAllocateInfo MemoryAllocateInfo(VkMemoryPropertyFlags desiredMemoryProperties) const {
            VkMemoryAllocateInfo memoryAllocateInfo = {};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

            VkMemoryRequirements memoryRequirements;
            vkGetImageMemoryRequirements(graphicsBase::Base().Device(), handle, &memoryRequirements);
            memoryAllocateInfo.allocationSize = memoryRequirements.size;
            auto GetMemoryTypeIndex = [](uint32_t memoryTypeBits, VkMemoryPropertyFlags desiredMemoryProperties)->uint32_t {
                auto& physicalDeviceMemoryProperties = graphicsBase::Base().PhysicalDeviceMemoryProperties();
                for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++){
                    if (memoryTypeBits & 1 << i &&
                        (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & desiredMemoryProperties) == desiredMemoryProperties){
                        return i;
                    }
                }
                return UINT32_MAX;
            };
            memoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(memoryRequirements.memoryTypeBits, desiredMemoryProperties);
            if (memoryAllocateInfo.memoryTypeIndex == UINT32_MAX &&
                desiredMemoryProperties & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
                memoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(memoryRequirements.memoryTypeBits, desiredMemoryProperties & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
            //不在此检查是否成功取得内存类型索引，因为会把memoryAllocateInfo返回出去，交由外部检查
            //if (memoryAllocateInfo.memoryTypeIndex == -1)
            //    outStream << std::format("[ image ] ERROR\nFailed to find any memory type satisfies all desired memory properties!\n");
            return memoryAllocateInfo;
        }
        result_t BindMemory(VkDeviceMemory deviceMemory, VkDeviceSize memoryOffset = 0) const {
            VkResult result = vkBindImageMemory(graphicsBase::Base().Device(), handle, deviceMemory, memoryOffset);
            if (result){
                qDebug("[ image ] ERROR\nFailed to attach the memory!\nError code: {}\n", int32_t(result));
            }
            return result;
        }
        //Non-const Function
        result_t Create(VkImageCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            VkResult result = vkCreateImage(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ image ] ERROR\nFailed to create an image!\nError code: {}\n", int32_t(result));
            }
            return result;
        }
    };

//封装imageMemory类  -->包含image的创建与释放，内存由deviceMemory申请,然后绑定在一起
    class imageMemory :public image, public deviceMemory {
    public:
        imageMemory() = default;
        imageMemory(VkImageCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
            Create(createInfo, desiredMemoryProperties);
        }
        imageMemory(imageMemory&& other) :
            image(std::move(other)), deviceMemory(std::move(other)) {
            areBound = other.areBound;
            other.areBound = false;
        }
        ~imageMemory() { areBound = false; }
        //Getter
        VkImage Image() const { return static_cast<const image&>(*this); }
        const VkImage* AddressOfImage() const { return image::Address(); }
        VkDeviceMemory Memory() const { return static_cast<const deviceMemory&>(*this); }
        const VkDeviceMemory* AddressOfMemory() const { return deviceMemory::Address(); }
        bool AreBound() const { return areBound; }
        using deviceMemory::AllocationSize;
        using deviceMemory::MemoryProperties;
        //Non-const Function
        //以下三个函数仅用于Create(...)可能执行失败的情况
        result_t CreateImage(VkImageCreateInfo& createInfo) {
            return image::Create(createInfo);
        }
        result_t AllocateMemory(VkMemoryPropertyFlags desiredMemoryProperties) {
            VkMemoryAllocateInfo allocateInfo = MemoryAllocateInfo(desiredMemoryProperties);
            if (allocateInfo.memoryTypeIndex >= graphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypeCount)
                return VK_RESULT_MAX_ENUM; //没有合适的错误代码，别用VK_ERROR_UNKNOWN
            return Allocate(allocateInfo);
        }
        result_t BindMemory() {
            if (VkResult result = image::BindMemory(Memory())){
                return result;
            }
            areBound = true;
            return VK_SUCCESS;
        }
        //分配设备内存、创建图像、绑定
        result_t Create(VkImageCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
            VkResult result;
            false || //这行用来应对Visual Studio中代码的对齐
                (result = CreateImage(createInfo)) || //用||短路执行
                (result = AllocateMemory(desiredMemoryProperties)) ||
                (result = BindMemory());
            return result;
        }
    };

//创建imageView: 定义了Image的使用方式。(与bufferView类似,bufferView定义的是buffer的使用方式)
    class imageView {
        VkImageView handle = (VkImageView)VK_NULL_HANDLE;
    public:
        imageView() = default;
        imageView(VkImageViewCreateInfo& createInfo) {
            Create(createInfo);
        }
        imageView(VkImage image, VkImageViewType viewType, VkFormat format, const VkImageSubresourceRange& subresourceRange, VkImageViewCreateFlags flags = 0) {
            Create(image, viewType, format, subresourceRange, flags);
        }
        imageView(imageView&& other) { MoveHandle; }
        ~imageView() { DestroyHandleBy(vkDestroyImageView); }
        //Getter
        DefineHandleTypeOperator(VkImageView,handle);
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkImageViewCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            VkResult result = vkCreateImageView(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ imageView ] ERROR\nFailed to create an image view!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(VkImage image, VkImageViewType viewType, VkFormat format, const VkImageSubresourceRange& subresourceRange, VkImageViewCreateFlags flags = 0) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.flags = flags;
            createInfo.image = image;
            createInfo.viewType = viewType;
            createInfo.format = format;
            createInfo.subresourceRange = subresourceRange;
            return Create(createInfo);
        }
    };
//以下是整个描述符的使用，创建步骤如下: 创建描述符布局->创建描述符池(按照描述符布局规则)->获得描述符集合(从pool中分配)->操作描述符
    //封装为descriptorSetLayout类 此类的作用: 一般是用于UBO、SSBO给shader绑定资源用
    class descriptorSetLayout {
        VkDescriptorSetLayout handle = (VkDescriptorSetLayout)VK_NULL_HANDLE;
    public:
        descriptorSetLayout() = default;
        descriptorSetLayout(VkDescriptorSetLayoutCreateInfo& createInfo) {
            Create(createInfo);
        }
        descriptorSetLayout(descriptorSetLayout&& other) { MoveHandle; }
        ~descriptorSetLayout() { DestroyHandleBy(vkDestroyDescriptorSetLayout); }
        //Getter
        DefineHandleTypeOperator(VkDescriptorSetLayout,handle);
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkDescriptorSetLayoutCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            VkResult result = vkCreateDescriptorSetLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ descriptorSetLayout ] ERROR\nFailed to create a descriptor set layout!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };

    //封装descriptorSet类
    class descriptorSet {
        friend class descriptorPool;
        VkDescriptorSet handle = (VkDescriptorSet)VK_NULL_HANDLE;
    public:
        descriptorSet() = default;
        descriptorSet(descriptorSet&& other) { MoveHandle; }

        //Getter
        DefineHandleTypeOperator(VkDescriptorSet,handle);
        DefineAddressFunction;
        //更新采样器、图像或带采样器资源至描述符
        void write(arrayRef<const VkDescriptorImageInfo> descriptorInfos,
                   VkDescriptorType descriptorType,
                   uint32_t dstBinding = 0,
                   uint32_t dstArrayElement = 0) const{
            VkWriteDescriptorSet writeDescriptorSet = {};
            writeDescriptorSet.dstSet = handle;
            writeDescriptorSet.dstBinding = dstBinding;
            writeDescriptorSet.dstArrayElement = dstArrayElement;
            writeDescriptorSet.descriptorCount = uint32_t(descriptorInfos.Count());
            writeDescriptorSet.descriptorType = descriptorType;
            writeDescriptorSet.pImageInfo = descriptorInfos.Pointer();
            Update(writeDescriptorSet);
        }
        //更新uniform或storage缓冲区（或对应的动态缓冲区）资源至描述符
        void write(arrayRef<const VkDescriptorBufferInfo> descriptorInfos,
                   VkDescriptorType descriptorType,
                   uint32_t dstBinding = 0,
                   uint32_t dstArrayElement = 0) const{
            VkWriteDescriptorSet writeDescriptorSet = {};
            writeDescriptorSet.dstSet = handle;
            writeDescriptorSet.dstBinding = dstBinding;
            writeDescriptorSet.dstArrayElement = dstArrayElement;
            writeDescriptorSet.descriptorCount = uint32_t(descriptorInfos.Count());
            writeDescriptorSet.descriptorType = descriptorType;
            writeDescriptorSet.pBufferInfo = descriptorInfos.Pointer();
            Update(writeDescriptorSet);
        }
        //更新storage或uniform纹理缓冲区资源至描述符
        void write(arrayRef<const VkBufferView> descriptorInfos,
                   VkDescriptorType descriptorType,
                   uint32_t dstBinding = 0,
                   uint32_t dstArrayElement = 0) const{
            VkWriteDescriptorSet writeDescriptorSet = {};
            writeDescriptorSet.dstSet = handle;
            writeDescriptorSet.dstBinding = dstBinding;
            writeDescriptorSet.dstArrayElement = dstArrayElement;
            writeDescriptorSet.descriptorCount = uint32_t(descriptorInfos.Count());
            writeDescriptorSet.descriptorType = descriptorType;
            writeDescriptorSet.pTexelBufferView = descriptorInfos.Pointer();
            Update(writeDescriptorSet);
        }

        //Static Function
        static void Update(arrayRef<VkWriteDescriptorSet> writes,arrayRef<VkCopyDescriptorSet> copies = {}){
            for(auto& i :writes){
                i.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            }
            for(auto& i :copies){
                i.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
            }
            vkUpdateDescriptorSets(graphicsBase::Base().Device(),writes.Count(),writes.Pointer(),copies.Count(),copies.Pointer());
        }
    };

    //封装描述符池类
    class descriptorPool{
        VkDescriptorPool handle = (VkDescriptorPool)VK_NULL_HANDLE;
    public:
        descriptorPool() = default;
        descriptorPool(VkDescriptorPoolCreateInfo& createInfo){
            Create(createInfo);
        }
        descriptorPool(uint32_t maxSetCount,arrayRef<const VkDescriptorPoolSize> poolSizes,VkDescriptorPoolCreateFlags flags = 0){
            Create(maxSetCount,poolSizes,flags);
        }
        descriptorPool(descriptorPool&& other) { MoveHandle; }
        ~descriptorPool() { DestroyHandleBy(vkDestroyDescriptorPool); }

        DefineHandleTypeOperator(VkDescriptorPool,handle);
        DefineAddressFunction;

        result_t AllocateSets(arrayRef<VkDescriptorSet> sets,arrayRef<const VkDescriptorSetLayout> setLayouts) const{
            if(sets.Count() != setLayouts.Count()){
                if(sets.Count() < setLayouts.Count()){
                    qDebug("[ descriptorPool ] ERROR\nFor each descriptor set, must provide a corresponding layout!\n");
                    return VK_RESULT_MAX_ENUM;
                }
                else{
                    qDebug("[ descriptorPool ] WARNING\nProvided layouts are more than sets!\n");
                }
            }
            VkDescriptorSetAllocateInfo allocateInfo = {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocateInfo.descriptorPool = handle; //指示要分配Set的Pool
            allocateInfo.descriptorSetCount = uint32_t(sets.Count());  //分配多少个集合
            allocateInfo.pSetLayouts = setLayouts.Pointer();//指定描述符布局
            VkResult result = vkAllocateDescriptorSets(graphicsBase::Base().Device(),&allocateInfo,sets.Pointer());
            if(result){
                qDebug("[ descriptorPool ] ERROR\nFailed to allocate descriptor sets!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        //用vkDescSet 和 descriptorSetLayout 分配描述符集合
        result_t AllocateSets(arrayRef<VkDescriptorSet> sets, arrayRef<const descriptorSetLayout> setLayouts) const {
            return AllocateSets(sets,
                                arrayRef<const VkDescriptorSetLayout>(setLayouts[0].Address(), setLayouts.Count()));
        }
        //用descriptorSet 和 VkDescriptorSetLayout 分配描述符集合
        result_t AllocateSets(arrayRef<descriptorSet> sets, arrayRef<const VkDescriptorSetLayout> setLayouts) const {
            return AllocateSets(arrayRef<VkDescriptorSet>(&sets[0].handle, sets.Count()),
                                setLayouts);
        }
        //用descriptorSet 和 descriptorSetLayout 分配描述符集合
        result_t AllocateSets(arrayRef<descriptorSet> sets, arrayRef<const descriptorSetLayout> setLayouts) const {
            return AllocateSets(arrayRef<VkDescriptorSet>(&sets[0].handle, sets.Count()),
                                arrayRef<const VkDescriptorSetLayout>(setLayouts[0].Address(), setLayouts.Count()));
        }
        result_t FreeSets(arrayRef<VkDescriptorSet> sets) const {
            VkResult result = vkFreeDescriptorSets(graphicsBase::Base().Device(), handle, sets.Count(), sets.Pointer());
            memset(sets.Pointer(), 0, sets.Count() * sizeof(VkDescriptorSet));
            return result; //Though vkFreeDescriptorSets(...) can only return VK_SUCCESS
        }
        result_t FreeSets(arrayRef<descriptorSet> sets) const {
            return FreeSets({ &sets[0].handle, sets.Count() });
        }

        result_t Create(VkDescriptorPoolCreateInfo& createInfo){
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            VkResult result = vkCreateDescriptorPool(graphicsBase::Base().Device(),&createInfo,nullptr,&handle);
            if(result){
                qDebug("[ descriptorPool ] ERROR\nFailed to create a descriptor pool!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
        result_t Create(uint32_t maxSetCount,arrayRef<const VkDescriptorPoolSize> poolSizes,VkDescriptorPoolCreateFlags flags = 0){
            VkDescriptorPoolCreateInfo createInfo = {};
            createInfo.flags = flags;
            createInfo.maxSets = maxSetCount;
            createInfo.poolSizeCount = uint32_t(poolSizes.Count());
            createInfo.pPoolSizes = poolSizes.Pointer();
            return Create(createInfo);
        }
    };

//封装采样器(GPU纹理取样策略 告诉gpu该如何从纹理种读取数据。使用方式和SSBO一样，需要通过描述符使用)
    class sampler {
        VkSampler handle = (VkSampler)VK_NULL_HANDLE;
    public:
        sampler() = default;
        sampler(VkSamplerCreateInfo& createInfo) {
            Create(createInfo);
        }
        sampler(sampler&& other) noexcept { MoveHandle; }
        ~sampler() { DestroyHandleBy(vkDestroySampler); }
        //Getter
        DefineHandleTypeOperator(VkSampler,handle);
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkSamplerCreateInfo& createInfo) {
            createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            VkResult result = vkCreateSampler(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result){
                qDebug("[ sampler ] ERROR\nFailed to create a sampler!\nError code: %d\n", int32_t(result));
            }
            return result;
        }
    };
}
#endif // VKBASE_H

