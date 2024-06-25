#include "pipeline.hpp"
#include <cstddef>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const uint64_t FenceTimeout = 100000000;
bool framebufferResized = false;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    framebufferResized = true;
}

vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription binding(
        0, sizeof(Vertex),
        vk::VertexInputRate::eVertex
    );
    return binding;
}

std::array<vk::VertexInputAttributeDescription, 2> Vertex::getAttributeDescription() {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescription;
    attributeDescription[0].setBinding(0);
    attributeDescription[0].setLocation(0);
    attributeDescription[0].setFormat(vk::Format::eR32G32Sfloat);
    attributeDescription[0].setOffset(offsetof(Vertex, pos));

    attributeDescription[1].setBinding(0);
    attributeDescription[1].setFormat(vk::Format::eR32G32B32Sfloat);
    attributeDescription[1].setLocation(1);
    attributeDescription[1].setOffset(offsetof(Vertex, color));

    return attributeDescription;
}

namespace vo::create {
    GLFWwindow* window(
        int width,
        int height,
        const std::string& winName
    ) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* handle = glfwCreateWindow(
            width, height, winName.c_str(),
            nullptr, nullptr
        );
        glfwSetFramebufferSizeCallback(handle, framebufferResizeCallback);

        return handle;
    }

    vk::raii::Instance instance(
        const vk::raii::Context& context,
        const std::string& appName,
        const std::string& engineName,
        const std::vector<const char*>& layers,
        const std::vector<const char*>& extensions
    ) {
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
            createInfo.ppEnabledLayerNames = layers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        return vk::raii::Instance(context, createInfo);
    }

    vk::raii::PhysicalDevice physicalDevice(const vk::raii::Instance& instance) {
        std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();

        auto pickPhysicalDevice = [](const vk::raii::PhysicalDevice& physicalDevice) {
            auto features = physicalDevice.getFeatures();
            features.fillModeNonSolid = vk::True;
            features.samplerAnisotropy = vk::True;
            auto properties = physicalDevice.getProperties();
            return features.geometryShader && properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
            };

        for (auto& device : devices | std::views::filter(pickPhysicalDevice)) {
            return device;
        }
        return devices[0];
    }

    vk::raii::Device logicalDevice(
        const vk::raii::PhysicalDevice& physicalDevice,
        const QueueFamily& family,
        const std::vector<const char*>& layers,
        const std::vector<const char*>& extensions
    ) {
        const float priority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo(
            {},
            family.graphicsFamily.value(),
            1,
            &priority
        );
        vk::PhysicalDeviceFeatures features = physicalDevice.getFeatures();
        features.fillModeNonSolid = vk::True;
        vk::DeviceCreateInfo deviceInfo(
            {},
            queueInfo,
            layers,
            extensions,
            &features
        );

        return physicalDevice.createDevice(deviceInfo);
    }

    vk::raii::Queue queue(
        const vk::raii::Device& device,
        const QueueFamily& family
    ) {
        const float priority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo(
            {},
            family.graphicsFamily.value(),
            1,
            &priority
        );

        return device.getQueue(family.graphicsFamily.value(), 0);
    }

    vk::raii::SurfaceKHR surface(
        const vk::raii::Instance& instance,
        GLFWwindow* handle
    ) {
        VkSurfaceKHR _surface;
        glfwCreateWindowSurface(static_cast<VkInstance>(*instance), handle, nullptr, &_surface);
        return vk::raii::SurfaceKHR(instance, _surface);
    }

    std::vector<vk::Image> images(
        const vk::raii::SwapchainKHR& swapchain
    ) {
        return swapchain.getImages();
    }

    std::vector<vk::raii::ImageView> imageViews(
        const vk::raii::Device& device,
        const std::vector<vk::Image>& images,
        vk::Format imageFormat
    ) {
        std::vector<vk::raii::ImageView> views;
        views.reserve(images.size());
        vk::ComponentMapping mapping(
            vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eG,
            vk::ComponentSwizzle::eB,
            vk::ComponentSwizzle::eA
        );

        vk::ImageSubresourceRange resourceRange(
            vk::ImageAspectFlagBits::eColor,
            0,
            1, 0, 1
        );

        for (auto& image : images) {
            vk::ImageViewCreateInfo viewInfo(
                {},
                image,
                vk::ImageViewType::e2D,
                imageFormat,
                mapping,
                resourceRange
            );
            views.emplace_back(device.createImageView(viewInfo));
        }
        return views;
    }
    vk::raii::ShaderModule shaderModule(
        const vk::raii::Device& device,
        const std::vector<char>& code
    ) {
        vk::ShaderModuleCreateInfo shaderInfo(
            {},
            (size_t)code.size(),
            reinterpret_cast<const uint32_t*>(code.data())
        );
        return device.createShaderModule(shaderInfo);
    }

    vk::raii::RenderPass renderpass(const vk::raii::Device& device, const ImageInfo& imageInfo) {
        vk::AttachmentDescription attachmentDescription(
            {},
            imageInfo.format,
            vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::ePresentSrcKHR
        );

        vk::AttachmentReference attachmentReference(
            0,
            vk::ImageLayout::eColorAttachmentOptimal
        );
        vk::SubpassDescription subpassDescription(
            {},
            vk::PipelineBindPoint::eGraphics,
            0, nullptr,
            1, &attachmentReference
        );
        vk::RenderPassCreateInfo renderpassInfo(
            {},
            1,
            &attachmentDescription,
            1,
            &subpassDescription
        );

        return device.createRenderPass(renderpassInfo);
    }

    vk::raii::PipelineLayout layout(const vk::raii::Device& device) {
        vk::PipelineLayoutCreateInfo layoutInfo(
            {},
            0, nullptr
        );

        return device.createPipelineLayout(layoutInfo);
    }

    vk::raii::Pipeline pipeline(
        const vk::raii::Device& device,
        const vk::raii::ShaderModule& vertModule,
        const vk::raii::ShaderModule& fragModule,
        const vk::raii::RenderPass& renderpass,
        const vk::raii::PipelineLayout& layout,
        const SwapchainInfo& swapchainInfo
    ) {
        vk::PipelineShaderStageCreateInfo vertexShaderInfo(
            {},
            vk::ShaderStageFlagBits::eVertex,
            vertModule,
            "main"
        );
        vk::PipelineShaderStageCreateInfo fragmentShaderInfo(
            {},
            vk::ShaderStageFlagBits::eFragment,
            fragModule,
            "main"
        );

        std::vector<vk::PipelineShaderStageCreateInfo> shaderInfos = {
            vertexShaderInfo,fragmentShaderInfo
        };
        vk::PipelineInputAssemblyStateCreateInfo assemblyInfo(
            {},
            vk::PrimitiveTopology::eLineStrip
        );
        vk::PipelineTessellationStateCreateInfo tesselationInfo(
            {},
            10
        );
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescription = Vertex::getAttributeDescription();

        vk::PipelineVertexInputStateCreateInfo vertexInput(
            {},
            bindingDescription,
            attributeDescription
        );

        vk::PipelineRasterizationStateCreateInfo rasterizationInfo(
            {},
            vk::False,
            vk::False,
            vk::PolygonMode::eLine,
            vk::CullModeFlagBits::eBack,
            vk::FrontFace::eClockwise,
            vk::False,
            {},
            {},
            {},
            2.0f
        );

        vk::PipelineMultisampleStateCreateInfo multisampleInfo(
            {},
            vk::SampleCountFlagBits::e1,
            vk::True
        );

        vk::Viewport viewport(
            0, 0, swapchainInfo.extent.width,
            swapchainInfo.extent.height,
            0.0f, 1.0f
        );

        vk::Rect2D scissor(
            { 0,0 },
            swapchainInfo.extent
        );

        vk::PipelineViewportStateCreateInfo viewportInfo(
            {},
            viewport,
            scissor
        );

        vk::PipelineColorBlendAttachmentState colorAttachmentInfo(
            vk::False,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
        );

        vk::PipelineColorBlendStateCreateInfo colorBlendInfo(
            {},
            vk::False,
            vk::LogicOp::eClear,
            1,
            &colorAttachmentInfo
        );

        vk::GraphicsPipelineCreateInfo gpCreateInfo(
            {}, 2, shaderInfos.data(), &vertexInput,
            &assemblyInfo, &tesselationInfo,
            &viewportInfo,
            &rasterizationInfo, &multisampleInfo,
            nullptr, &colorBlendInfo,
            nullptr, layout, renderpass, 0
        );

        return device.createGraphicsPipeline(nullptr, gpCreateInfo);
    }
    std::vector<vk::raii::Framebuffer> framebuffers(
        const vk::raii::Device& device,
        const vk::raii::RenderPass& renderpass,
        const std::vector<vk::raii::ImageView>& views,
        const ImageInfo& imageInfo
    ) {
        std::vector<vk::raii::Framebuffer> buffers;
        buffers.reserve(views.size());
        for (int i = 0; i < views.size(); i++) {
            vk::FramebufferCreateInfo bufferInfo(
                {},
                renderpass,
                *views[i],
                imageInfo.width,
                imageInfo.height,
                1
            );
            buffers.emplace_back(device.createFramebuffer(bufferInfo));
        }
        return buffers;
    }
    vk::raii::CommandPool commandpool(
        const vk::raii::Device& device,
        const QueueFamily& family
    ) {
        vk::CommandPoolCreateInfo poolInfo(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            family.graphicsFamily.value()
        );
        return device.createCommandPool(poolInfo);
    }

    vk::raii::CommandBuffer commandbuffer(
        const vk::raii::Device& device,
        const vk::raii::CommandPool& pool
    ) {
        vk::CommandBufferAllocateInfo allocInfo(
            pool,
            vk::CommandBufferLevel::ePrimary,
            1
        );
        return std::move(device.allocateCommandBuffers(allocInfo).front());
    }

    vk::raii::Buffer vertexbuffer(
        const vk::raii::Device& device,
        std::vector<Vertex> vertices
    ) {
        vk::BufferCreateInfo bufferInfo(
            {},
            sizeof(vertices[0]) * vertices.size(),
            vk::BufferUsageFlagBits::eVertexBuffer,
            vk::SharingMode::eExclusive
        );

        return device.createBuffer(bufferInfo);
    }
}

namespace vo::utils {
    QueueFamily findQueueFamily(const vk::raii::PhysicalDevice& physicalDevice) {
        QueueFamily family{};
        std::vector<vk::QueueFamilyProperties> properties = physicalDevice.getQueueFamilyProperties();

        for (int i = 0; i < properties.size(); i++) {
            auto property = properties[i];
            if (property.queueFlags & vk::QueueFlagBits::eGraphics) {
                family.graphicsFamily = i;
            }
            if (property.queueFlags & vk::QueueFlagBits::eCompute) {
                family.computeFamily = i;
            }
            if (family.isFilled()) break;
        }
        assert(family.graphicsFamily);
        assert(family.computeFamily);
        return family;
    }

    SwapchainInfo querySwapChainInfo(
        const vk::raii::PhysicalDevice& device,
        const vk::raii::Device& logicalDevice,
        const vk::raii::SurfaceKHR& surface,
        GLFWwindow* handle
    ) {
        auto capabilities = device.getSurfaceCapabilitiesKHR(surface);
        std::vector<vk::SurfaceFormatKHR> formats = device.getSurfaceFormatsKHR(surface);
        std::vector<vk::PresentModeKHR> presentModes = device.getSurfacePresentModesKHR(surface);

        auto pickFormat = [](const vk::SurfaceFormatKHR& format) {
            return format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            };
        vk::SurfaceFormatKHR surfaceFormat;
        for (auto& sFormat : formats | std::views::filter(pickFormat))
            surfaceFormat = sFormat;

        auto pickPresentMode = [](const vk::PresentModeKHR& pMode) {
            return pMode == vk::PresentModeKHR::eMailbox;
            };
        vk::PresentModeKHR presentMode;
        for (auto& pMode : presentModes | std::views::filter(pickPresentMode))
            presentMode = pMode;

        if (presentMode != vk::PresentModeKHR::eMailbox) presentMode = vk::PresentModeKHR::eFifoRelaxed;

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (imageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }
        vk::Extent2D extent;

        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            extent = capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(handle, &width, &height);
            vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            actualExtent.width = std::clamp(
                actualExtent.width,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            );
            actualExtent.height = std::clamp(
                actualExtent.height,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height
            );
            extent = actualExtent;
        }

        vk::SwapchainCreateInfoKHR swapchainInfo(
            {},
            surface,
            imageCount,
            surfaceFormat.format,
            surfaceFormat.colorSpace,
            extent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment,
            vk::SharingMode::eExclusive,
            0, nullptr,
            capabilities.currentTransform,
            vk::CompositeAlphaFlagBitsKHR::eOpaque,
            presentMode,
            vk::True,
            nullptr
        );
        return SwapchainInfo{
            logicalDevice.createSwapchainKHR(swapchainInfo),
            capabilities,
            surfaceFormat,
            presentMode,
            extent
        };
    }

    std::pair<vk::Result, vk::Result> drawFrame(
        const vk::raii::Device& device,
        const SwapchainInfo& swapchain,
        const vk::raii::RenderPass& renderpass,
        const vk::raii::Pipeline& pipeline,
        const vk::raii::CommandBuffer& commandBuffer,
        const vk::raii::Semaphore& waitSemaphore,
        const vk::raii::Semaphore& signalSemaphore,
        const vk::raii::Fence& fence,
        const std::vector<vk::raii::Framebuffer>& framebuffers,
        const vk::raii::Queue& graphicsQueue,
        const std::vector<vk::raii::Buffer>& vertexbuffers,
        const std::vector<int>& counts
    ) {
        device.waitForFences(*fence, vk::True, UINT64_MAX);
        device.resetFences(*fence);
        auto [imgResult, imageIndex] = swapchain.swapchain.acquireNextImage(FenceTimeout, waitSemaphore);
        commandBuffer.reset();
        vk::CommandBufferBeginInfo beginInfo(
            {}, nullptr
        );
        commandBuffer.begin(beginInfo);
        vk::Rect2D area(
            { 0,0 },
            swapchain.extent
        );
        vk::ClearValue clearValue(
            vk::ClearColorValue(1.0f, 1.0f, 1.0f, 0.0f)
        );
        vk::RenderPassBeginInfo rpbeginInfo(
            renderpass,
            framebuffers[imageIndex],
            area,
            1,
            &clearValue
        );
        commandBuffer.beginRenderPass(rpbeginInfo, vk::SubpassContents::eInline);
        vk::Viewport viewport(
            0, 0, swapchain.extent.width, swapchain.extent.height,
            0.0f, 1.0f
        );
        vk::Rect2D scissor(
            { 0,0 },
            swapchain.extent
        );
        commandBuffer.setViewport(
            0,
            viewport
        );
        commandBuffer.setScissor(
            0,
            scissor
        );
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        for (int i = 0; i < vertexbuffers.size();i++) {
            vk::DeviceSize offset[] = { 0 };
            commandBuffer.bindVertexBuffers(0, *vertexbuffers[i], offset);
            commandBuffer.draw(counts[i], 1, 0, 0);
        }
        commandBuffer.endRenderPass();
        commandBuffer.end();
        vk::PresentInfoKHR presentInfo(
            *signalSemaphore,
            *swapchain.swapchain,
            imageIndex
        );
        vk::PipelineStageFlags flags(
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        );
        vk::SubmitInfo submitInfo(
            *waitSemaphore,
            flags,
            *commandBuffer,
            *signalSemaphore
        );
        graphicsQueue.submit(
            submitInfo,
            *fence
        );
        vk::Result presentResult = graphicsQueue.presentKHR(presentInfo);

        return { imgResult,presentResult };
    }

    uint32_t findMemoryType(
        const vk::raii::PhysicalDevice& device,
        uint32_t typeFilter,
        vk::MemoryPropertyFlags properties
    ) {
        vk::PhysicalDeviceMemoryProperties memProperties = device.getMemoryProperties();

        for (int i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (properties & memProperties.memoryTypes[i].propertyFlags) == properties) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }
    vk::raii::DeviceMemory allocateBuffer(
        const vk::raii::Device& device,
        const vk::MemoryRequirements& memRequirements,
        uint32_t memTypeIndex
    ) {
        vk::MemoryAllocateInfo allocInfo(
            memRequirements.size,
            memTypeIndex
        );
        return device.allocateMemory(allocInfo);
    }

    void fillBuffer(
        const vk::raii::Buffer& vertexbuffer,
        const vk::raii::DeviceMemory& deviceMemory,
        const vk::MemoryRequirements& memRequirements,
        std::vector<Vertex> vertices
    ) {
        uint32_t size = sizeof(vertices[0]) * vertices.size();
        void* data = deviceMemory.mapMemory(0, size);
        memcpy(data, vertices.data(), size);
        deviceMemory.unmapMemory();
        vertexbuffer.bindMemory(deviceMemory, 0);
    }
}

