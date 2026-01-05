#include "GlfwGeneral.h"
#include "EasyVulkan.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

void setupVulkanEnv() {
#if defined(__APPLE__)
    const std::string vulkanSdk = "/Users/zengqingguo/VulkanSDK/1.4.321.0/macOS";
    setenv("VULKAN_SDK", vulkanSdk.c_str(), 1);
    setenv("VK_ICD_FILENAMES", (vulkanSdk + "/share/vulkan/icd.d/MoltenVK_icd.json").c_str(), 1);
    setenv("DYLD_LIBRARY_PATH", (vulkanSdk + "/lib").c_str(), 1);
    setenv("VK_ADD_LAYER_PATH", (vulkanSdk + "/share/vulkan/explicit_layer.d").c_str(), 1);

#elif defined(__linux__)
    const std::string vulkanSdk = "/usr";  // 或者你的 Vulkan SDK 路径
    setenv("VULKAN_SDK", vulkanSdk.c_str(), 1);
    setenv("VK_ICD_FILENAMES", (vulkanSdk + "/share/vulkan/icd.d/nvidia_icd.json").c_str(), 1);
    setenv("LD_LIBRARY_PATH", (vulkanSdk + "/lib").c_str(), 1);

#elif defined(_WIN32)
    const std::string vulkanSdk = "E:\\VulkanSDK\\1.3.290.0";
    SetEnvironmentVariableA("VULKAN_SDK", vulkanSdk.c_str());
    SetEnvironmentVariableA("VK_ICD_FILENAMES", (vulkanSdk + "\\Bin\\VkICD.json").c_str());
    // Windows 的 DLL 通常直接在 PATH 里
    char* path = nullptr;
    size_t len;
    _dupenv_s(&path, &len, "PATH");
    std::string newPath = vulkanSdk + "\\Bin;" + (path ? path : "");
    SetEnvironmentVariableA("PATH", newPath.c_str());
    free(path);
#endif
}

using namespace vulkan;
descriptorSetLayout descriptorSetLayout_triangle; //描述符布局
pipelineLayout pipelineLayout_triangle; //管线布局
pipeline pipeline_triangle;             //管线


struct vertex {
    glm::vec2 position;
    glm::vec4 color;
};

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers() {
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}
//该函数用于创建布局
void CreateLayout() {

    //创建描述符布局
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_uniformPosition = {};
    descriptorSetLayoutBinding_uniformPosition.binding = 0;                                        //描述符被绑定到0号binding
    descriptorSetLayoutBinding_uniformPosition.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; //类型为uniform缓冲区
    descriptorSetLayoutBinding_uniformPosition.descriptorCount = 1;                                //个数是1个
    descriptorSetLayoutBinding_uniformPosition.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;            //在顶点着色器阶段读取uniform缓冲区

    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_array[] =
    {
        descriptorSetLayoutBinding_uniformPosition
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo_triangle = {};
    descriptorSetLayoutCreateInfo_triangle.bindingCount = sizeof(descriptorSetLayoutBinding_array)/sizeof(VkDescriptorSetLayoutBinding);
    descriptorSetLayoutCreateInfo_triangle.pBindings = descriptorSetLayoutBinding_array;
    descriptorSetLayout_triangle.Create(descriptorSetLayoutCreateInfo_triangle);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayout_triangle.Address();
    pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);

}
//该函数用于创建管线
void CreatePipeline() {
    struct ShaderStruct
    {
        VkShaderStageFlagBits stage;
        QString               shader;
        QString               output;
    };

    QList<ShaderStruct> shader_struct_list{
        ShaderStruct
        {   VK_SHADER_STAGE_VERTEX_BIT,
            QString("%1/shader/UniformBuffer.vert.shader").arg(CODE_DIR),
            QString("%1/UniformBuffer.vert.spv").arg(APP_PATH)
        },
        ShaderStruct
        {   VK_SHADER_STAGE_FRAGMENT_BIT,
            QString("%1/shader/VertexBuffer.frag.shader").arg(CODE_DIR),
            QString("%1/VertexBuffer.frag.spv").arg(APP_PATH)
        }
    };

    static std::vector<shaderModule> shaderModules;
    static std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos_triangle;
    for(const ShaderStruct& item : shader_struct_list){
        compileShader(VK_GLSLC, item.shader, item.output);

        // 创建 shader module（注意：不能是 static）
        shaderModules.emplace_back(item.output.toStdString().c_str());

        shaderStageCreateInfos_triangle.push_back(shaderModules.back().StageCreateInfo(item.stage));
    }

    auto Create = [] {
        graphicsPipelineCreateInfoPack pipelineCiPack;
        pipelineCiPack.createInfo.layout = pipelineLayout_triangle;
        pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;

        //数据来自0号顶点缓冲区，输入频率是逐顶点输入
        pipelineCiPack.vertexInputBindings.emplace_back(VkVertexInputBindingDescription{0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX});
        //location为0，数据来自0号顶点缓冲区，vec2对应VK_FORMAT_R32G32_SFLOAT，用offsetof计算position在vertex中的起始位置
        pipelineCiPack.vertexInputAttributes.emplace_back(VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex, position)});
        //location为1，数据来自0号顶点缓冲区，vec4对应VK_FORMAT_R32G32B32A32_SFLOAT，用offsetof计算color在vertex中的起始位置
        pipelineCiPack.vertexInputAttributes.emplace_back(VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, color)});

        //数据来自1号顶点缓冲区，输入频率是逐实例输入
        pipelineCiPack.vertexInputBindings.emplace_back(VkVertexInputBindingDescription
                                                        {   1,  // 绑定槽号，对应 vkCmdBindVertexBuffers 的第几个缓冲
                                                            sizeof(glm::vec2),  // 每个顶点占用多少字节
                                                            VK_VERTEX_INPUT_RATE_INSTANCE // 数据速率：VK_VERTEX_INPUT_RATE_VERTEX(逐顶点) 或 VK_VERTEX_INPUT_RATE_INSTANCE(逐实例)
                                                        });
        //location为2，数据来自1号顶点缓冲区，vec2对应VK_FORMAT_R32G32_SFLOAT
        pipelineCiPack.vertexInputAttributes.emplace_back(VkVertexInputAttributeDescription
                                                          {
                                                              2,    // shader 中 layout(location = X)
                                                              1,    // 对应绑定槽号
                                                              VK_FORMAT_R32G32_SFLOAT, // 顶点属性类型（vec2 = VK_FORMAT_R32G32_SFLOAT）
                                                              0     // 顶点结构内偏移
                                                          });


        //只绘制一个三角型，所以图元拓扑类型VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST或VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        //指定视口和剪裁范围，填满屏幕
        pipelineCiPack.viewports.emplace_back(VkViewport{0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f});
        pipelineCiPack.scissors.emplace_back(VkRect2D{VkOffset2D{}, windowSize});

        //不开多重采样，所以每个像素点采样一次：
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        //不开启混色，只指定RGBA四通道的写入遮罩为全部写入
        VkPipelineColorBlendAttachmentState Pcbas = {};
        Pcbas.colorWriteMask =  VK_COLOR_COMPONENT_R_BIT |
                                VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT |
                                VK_COLOR_COMPONENT_A_BIT;
        pipelineCiPack.colorBlendAttachmentStates.push_back(Pcbas);

        pipelineCiPack.UpdateAllArrays();
        pipelineCiPack.createInfo.stageCount = 2;
        pipelineCiPack.createInfo.pStages = shaderStageCreateInfos_triangle.data();

        pipeline_triangle.Create(pipelineCiPack);
    };
    auto Destroy = [] {
        pipeline_triangle.~pipeline();
    };
    graphicsBase::Base().AddCallback_CreateSwapchain(Create);
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
    //调用Create()以创建管线
    Create();
}


int main_076(int argc, char *argv[])
{
    QCoreApplication a(argc,argv);

    //set vulkan env
    setupVulkanEnv();

    if (!InitializeWindow({ 640, 480 }))
        return -1;

    //076节增加 -- 拷贝图像到屏幕
    QString ImagePath = QString("%1/%2").arg(APP_PATH).arg("1.jpg");
    easyVulkan::BootScreen(ImagePath.toLocal8Bit().data(),VK_FORMAT_R8G8B8A8_UNORM);
    glfwPollEvents(); //注意MacOS如果不增加事件循环会导致窗口无法弹出
    QThread::msleep(1000);

    const auto& rpwf = RenderPassAndFramebuffers();

    CreateLayout();
    CreatePipeline();

    //以置位状态创建栅栏
    vulkan::fence fence(VK_FENCE_CREATE_SIGNALED_BIT);

    //创建二值信号量
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    vulkan::commandBuffer commandBuffer;
    commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    VkClearValue clearColor = {};
    clearColor.color = { 0.0f, 0.5f, 1.f, 1.f };

    std::vector<vertex> vertices = {
        { {  .0f, -.5f }, { 1, 0, 0, 1 } },
        { { -.5f,  .5f }, { 0, 1, 0, 1 } },
        { {  .5f,  .5f }, { 0, 0, 1, 1 } }
    };
    vertexBuffer vertexBuffer_perVertex(vertices.size() * sizeof(vertex));
    vertexBuffer_perVertex.TransferData(vertices.data(),vertices.size() * sizeof(vertex));

    //创建描述符池
    VkDescriptorPoolSize descriptorPoolSizes[] =
    {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
    };
    descriptorPool descriptor_pool(1, descriptorPoolSizes);

    //分配描述符集
    descriptorSet descriptorSet_trianglePosition;
    descriptor_pool.AllocateSets(descriptorSet_trianglePosition,descriptorSetLayout_triangle);

    //uniform缓冲区的信息写入描述符. 注意:uniform buffer 遵循std140 标准，uniform block步长为16. vec2 = 8  2*vec2 => vec4
    std::vector<glm::vec4> uniform_positions = {
        glm::vec4( 0.0f, 0.0f,0,0),
        glm::vec4(-0.5f, 0.0f,0,0),
        glm::vec4( 0.5f, 0.0f,0,0),
    };

    //每组数据的大小向上凑整到单位对齐距离的整数倍并相加，得到整个缓冲区的大小
    //VkDeviceSize uniformBufferSize = uniformAlignment * (std::ceil(float(dataSize[0]) / uniformAlignment) + ... + std::ceil(float(dataSize[2]) / uniformAlignment));

    VkDeviceSize uniformAlignment = graphicsBase::Base().PhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    uniformAlignment *= (std::ceil(float(sizeof(glm::vec4)) / uniformAlignment));


    uniformBuffer uniform_buffer(uniform_positions.size() * uniformAlignment);
    uniform_buffer.TransferData(uniform_positions.data(),
                                uniform_positions.size(),
                                sizeof(glm::vec4),
                                sizeof(glm::vec4),
                                uniformAlignment);

    VkDescriptorBufferInfo ubufferInfo = {};
    ubufferInfo.buffer = uniform_buffer;
    ubufferInfo.offset = 0;
    ubufferInfo.range = sizeof(glm::vec4);
    descriptorSet_trianglePosition.write(ubufferInfo,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0);

    while (!glfwWindowShouldClose(pWindow)) {
        //窗口最小化时停止渲染循环
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)){
            glfwWaitEvents();
        }

        //重置栅栏
        fence.Reset();

        //获取交换链图像索引
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        auto imageIndex = graphicsBase::Base().CurrentImageIndex();


        //开始录制命令
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        //开始渲染通道
        rpwf.renderPass.CmdBegin(commandBuffer, rpwf.framebuffers[imageIndex], { {}, windowSize }, clearColor);

        VkDeviceSize offsets = {};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer_perVertex.Address(), &offsets);

        //绑定渲染管线  --- 崩溃原因(上一次提交不小心删除了)
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
        //绑定描述符并绘制
        for(size_t ubo_idx = 0; ubo_idx < uniform_positions.size();ubo_idx++){
            uint32_t dynamicOffset = uniformAlignment * ubo_idx;
            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout_triangle,
                                    0,
                                    1,
                                    descriptorSet_trianglePosition.Address(),
                                    1,
                                    &dynamicOffset);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        }


        //结束渲染通道
        rpwf.renderPass.CmdEnd(commandBuffer);

        //结束录制命令
        commandBuffer.End();

        //提交命令缓冲
        graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);

        //呈现图像
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        //等待并重置fence
        fence.WaitAndReset();

    }
    TerminateWindow();

    a.quit();
    return 0;
}


#include "Examples/Samples2DCompute.h"
int main/*Samples2DCompute*/(int argc, char *argv[])
{
    QCoreApplication a(argc,argv);

    //set vulkan env
    setupVulkanEnv();

    if (!InitializeWindow({ 640, 480 }))
        return -1;

    QString ImagePath = QString("%1/%2").arg(qApp->applicationDirPath()).arg("1.jpg");

    Samples2DCmp samples2d_cmp;
    samples2d_cmp.initResource(ImagePath);

    //1. 交换链获取GPU可用图像CMD，需知道CMD执行是否完成需要指定信号
    semaphore semaphore_available; //需要知道交换链是否给GPU分配了可以使用图像

    /*创建录制命令 -- 创建录制命令，需要有命令池，创建命令池需要缓冲区*/
    //1.开始创建命令缓冲区(作用:渲染，从Graphics创建命令缓冲池)
    commandBuffer command_buffer;
    graphicsBase::Plus().CommandPool_Graphics().AllocateBuffers(command_buffer);
    while (!glfwWindowShouldClose(pWindow)) {
        //窗口最小化时停止渲染循环
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)){
            glfwWaitEvents();
        }
        //提交从交换链获取GPU可用图像CMD
        graphicsBase::Base().SwapImage(semaphore_available);
        //录制命令(预留)
        command_buffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        samples2d_cmp.runDispatch(command_buffer);
        command_buffer.End();

        /*提交录制命令 -- 提交后有两种方式可以获得命令是否已经执行结束(GPU侧:信号,CPU侧:栅栏)*/
        //1. 使用CPU侧同步，创建栅栏(fence)
        fence fence_sync_flag;

        //2. 提交录制命令前必须确保GPU已经从GPU拿到可用的图像(需要等待semaphore_available信号<由GPU完成同步>)
        VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; //拷贝和blit属于传输命令
        VkSubmitInfo submit_info = {};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = semaphore_available.Address(); //指定执行命令前需要信号
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = command_buffer.Address();//指定执行的命令缓冲
        submit_info.pWaitDstStageMask = &waitDstStage;
        graphicsBase::Base().SubmitCommandBuffer_Graphics(submit_info,fence_sync_flag);//命令缓冲执行结束后输出状态

        /*提交呈现命令 -- 提交呈现命令前提是命令缓冲区已经执行结束(CPU测需要等待fence,GPU测需要等待渲染结束信号量<在submit输出可以指定>)*/
        //1. 等待命令缓冲执行结束
        fence_sync_flag.WaitAndReset();
        //2. 呈现图像并向交换链归还使用完毕的图像
        graphicsBase::Base().PresentImage();

        glfwPollEvents();
        TitleFps();
    }

    /*释放命令缓冲*/
    graphicsBase::Plus().CommandPool_Graphics().FreeBuffers(command_buffer);

    //关闭窗口
    TerminateWindow();

    a.quit();
    return 0;
}
#include <stdexcept>
#include <vector>
#include <cstring>

struct Texture {
    VkImage image = (VkImage)VK_NULL_HANDLE;
    VkDeviceMemory memory = (VkDeviceMemory)VK_NULL_HANDLE;
    VkImageView view = (VkImageView)VK_NULL_HANDLE;
    VkSampler sampler = (VkSampler)VK_NULL_HANDLE;
    VkExtent2D extent;
};

// ----------- 工具函数（你可能已有，可直接复用）-----------
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer!");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory!");

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, (VkFence)VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                           VkImage image, VkFormat /*format*/, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
}

void CopyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                       VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
}

void CreateImage(VkDevice device, VkPhysicalDevice physicalDevice,
                 uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image!");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory!");

    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view!");

    return imageView;
}

VkSampler CreateTextureSampler(VkDevice device)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler!");

    return sampler;
}

// ----------- 主函数：加载图片并生成纹理 -----------
Texture LoadTexture(VkDevice device,
                    VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool,
                    VkQueue graphicsQueue,
                    const std::string& filename)
{
    Texture tex{};

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("Failed to load texture image!");

    VkDeviceSize imageSize = texWidth * texHeight * 4;
    tex.extent = {(uint32_t)texWidth,(uint32_t)texHeight};

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(device, physicalDevice, imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);



    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    CreateImage(device, physicalDevice, texWidth, texHeight,
                VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex.image, tex.memory);

    TransitionImageLayout(device, commandPool, graphicsQueue,
                          tex.image, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyBufferToImage(device, commandPool, graphicsQueue,
                      stagingBuffer, tex.image, texWidth, texHeight);

    TransitionImageLayout(device, commandPool, graphicsQueue,
                          tex.image, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    tex.view = CreateImageView(device, tex.image, VK_FORMAT_R8G8B8A8_SRGB);
    tex.sampler = CreateTextureSampler(device);

    return tex;
}
void DestroyTexture(VkDevice device, Texture& texture) {
    if (texture.sampler != (VkSampler)VK_NULL_HANDLE)
        vkDestroySampler(device, texture.sampler, nullptr);

    if (texture.view != (VkImageView)VK_NULL_HANDLE)
        vkDestroyImageView(device, texture.view, nullptr);

    if (texture.image != (VkImage)VK_NULL_HANDLE)
        vkDestroyImage(device, texture.image, nullptr);

    if (texture.memory != (VkDeviceMemory)VK_NULL_HANDLE)
        vkFreeMemory(device, texture.memory, nullptr);

    texture = Texture{}; // 重置为默认值
}

void ClearColorImage(VkCommandBuffer commandBuffer,VkImage swapChainImage){

    //清屏命令
    VkClearColorValue clearColor = { {1.0f, 0.0f, 0.0f, 1.0f} };
    VkImageSubresourceRange range = {};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    vkCmdClearColorImage(
        commandBuffer,
        swapChainImage,
        VK_IMAGE_LAYOUT_GENERAL,  // 如果交换链布局是 PRESENT_SRC_KHR, 先转换到 GENERAL
        &clearColor,
        1,
        &range
    );
}


void BlitImageToSwapchain(
    VkCommandBuffer cmd,
    VkImage srcImage, VkExtent2D srcExtent,
    VkImage dstImage, VkExtent2D desExtent)
{

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 当前布局
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;     // blit 源需要
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = srcImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // 根据需要可优化
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // 源图像：一般是 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    // 目标图像：一般是 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {(int32_t)srcExtent.width, (int32_t)srcExtent.height, 1};

    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {(int32_t)desExtent.width, (int32_t)desExtent.height, 1};

    // 执行线性滤波缩放拷贝
    vkCmdBlitImage(
        cmd,
        srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit,
        VK_FILTER_LINEAR
    );
}


int main_test_load_texture(/*int argc, char *argv[]*/)
{
    //set vulkan env
    setupVulkanEnv();

    if (!InitializeWindow({ 640, 480 }))
        return -1;

    using namespace vulkan;
    //以置位状态创建栅栏
    vulkan::fence fence(VK_FENCE_CREATE_SIGNALED_BIT);

    //创建二值信号量
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    vulkan::commandBuffer commandBuffer;
    commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);



    while (!glfwWindowShouldClose(pWindow)) {
        //窗口最小化时停止渲染循环
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)){
            glfwWaitEvents();
        }

        //Texture texture;
        //  texture = LoadTexture(graphicsBase::Base().Device(),
        //              graphicsBase::Base().PhysicalDevice(),
        //              commandPool,
        //              graphicsBase::Base().Queue_Graphics(),
        //              "E:/project/SPL_Camera/1733392738313.png");

        //加载图像
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("/Users/zengqingguo/Desktop/gitHub/VulkanStudy/EasyVK/build/macOS/debug/1.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels){
            throw std::runtime_error("Failed to load texture image!");
        }

        VkDeviceSize imageSize = texWidth * texHeight * 4;
        VkExtent2D RenderExtent = {(uint32_t)texWidth,(uint32_t)texHeight};

        //创建buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkMemoryPropertyFlags desiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        bufferMemory stagingBufferMemory(bufferInfo,desiredMemoryProperties);

        //上传cpu数据至gpu内存
        stagingBufferMemory.BufferData(pixels,imageSize,0);

        //释放图像
        stbi_image_free(pixels);

        //创建image -->可以被GPU读取和采样的数据
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = texWidth;
        imageInfo.extent.height = texHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageMemory RenderImage(imageInfo,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        CopyBufferToImage(graphicsBase::Base().Device(), commandPool, graphicsBase::Base().Queue_Graphics(),
                          stagingBufferMemory, RenderImage, texWidth, texHeight);

        TransitionImageLayout(graphicsBase::Base().Device(), commandPool, graphicsBase::Base().Queue_Graphics(),
                              RenderImage, VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // texture.view = CreateImageView(graphicsBase::Base().Device(), image, VK_FORMAT_R8G8B8A8_SRGB);
        // texture.sampler = CreateTextureSampler(graphicsBase::Base().Device());
        //texture.image = image;

        //获取交换链图像索引
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        auto imageIndex = graphicsBase::Base().CurrentImageIndex();
        //获取交换链图像
        auto swapChainImage = graphicsBase::Base().SwapchainImage(imageIndex);

        //开始录制命令
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;         // 当前布局
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;   // 呈现需要的布局
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapChainImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0; // 从 UNDEFINED，没有有效访问
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // 从未初始化
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier
        );

        ClearColorImage(commandBuffer,swapChainImage);
        BlitImageToSwapchain(commandBuffer,
                             RenderImage,
                             RenderExtent,
                             swapChainImage,
                             graphicsBase::Base().SwapchainCreateInfo().imageExtent);

        //结束录制命令
        commandBuffer.End();

        fence.Reset();
        graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        //等待并重置fence
        fence.WaitAndReset();

        //DestroyTexture(graphicsBase::Base().Device(),texture);
    }
    TerminateWindow();
    return 0;
}


int main_test_load_texture_test(/*int argc, char *argv[]*/)
{
    //set vulkan env
    setupVulkanEnv();

    if (!InitializeWindow({ 640, 480 }))
        return -1;

    using namespace vulkan;
    //以置位状态创建栅栏
    vulkan::fence fence(VK_FENCE_CREATE_SIGNALED_BIT);

    //创建二值信号量
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    vulkan::commandBuffer commandBuffer;
    commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);


    //加载图像
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("E:/project/SPL_Camera/1733392738313.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels){
        throw std::runtime_error("Failed to load texture image!");
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;
    VkExtent2D RenderExtent = {(uint32_t)texWidth,(uint32_t)texHeight};

    //创建buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkMemoryPropertyFlags desiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bufferMemory stagingBufferMemory(bufferInfo,desiredMemoryProperties);

    //上传cpu数据至gpu内存
    stagingBufferMemory.BufferData(pixels,imageSize,0);

    //释放图像
    stbi_image_free(pixels);



    //创建image -->可以被GPU读取和采样的数据
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = texWidth;
    imageInfo.extent.height = texHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageMemory RenderImage(imageInfo,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


    while (!glfwWindowShouldClose(pWindow)) {
        //窗口最小化时停止渲染循环
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)){
            glfwWaitEvents();
        }

        CopyBufferToImage(graphicsBase::Base().Device(), commandPool, graphicsBase::Base().Queue_Graphics(),
                          stagingBufferMemory, RenderImage, texWidth, texHeight);

        TransitionImageLayout(graphicsBase::Base().Device(), commandPool, graphicsBase::Base().Queue_Graphics(),
                              RenderImage, VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // texture.view = CreateImageView(graphicsBase::Base().Device(), image, VK_FORMAT_R8G8B8A8_SRGB);
        // texture.sampler = CreateTextureSampler(graphicsBase::Base().Device());

        //获取交换链图像索引
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        auto imageIndex = graphicsBase::Base().CurrentImageIndex();
        //获取交换链图像
        auto swapChainImage = graphicsBase::Base().SwapchainImage(imageIndex);

        //开始录制命令
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;         // 当前布局
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;   // 呈现需要的布局
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapChainImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0; // 从 UNDEFINED，没有有效访问
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // 从未初始化
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &barrier
                    );


        BlitImageToSwapchain(commandBuffer,
                             RenderImage,
                             RenderExtent,
                             swapChainImage,
                             graphicsBase::Base().SwapchainCreateInfo().imageExtent);

        //ClearColorImage(commandBuffer,swapChainImage);
        //结束录制命令
        commandBuffer.End();

        fence.Reset();
        graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        //等待并重置fence
        fence.WaitAndReset();


    }
    TerminateWindow();
    return 0;
}
