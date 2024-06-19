#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <iostream>
#include <ranges>
#include <optional>
#include <sstream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cstddef>

extern bool framebufferResized;

struct QueueFamily {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> computeFamily;
    bool isFilled() {
        return graphicsFamily.has_value() && computeFamily.has_value();
    }
};

struct SwapchainInfo {
    vk::raii::SwapchainKHR swapchain;
    vk::SurfaceCapabilitiesKHR capabilities;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::PresentModeKHR presentMode;
    vk::Extent2D extent;
};

struct ImageInfo {
    vk::Format format;
    uint32_t width;
    uint32_t height;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescription();
};


namespace vo {
    namespace create {
        GLFWwindow* window(
            int width,
            int height,
            const std::string& winName
        );
        vk::raii::Instance instance(
            const vk::raii::Context& context,
            const std::string& appName,
            const std::string& engineName,
            const std::vector<const char*>& layers = {},
            const std::vector<const char*>& extensions = {}
        );
        vk::raii::DebugUtilsMessengerEXT debugMessenger(
            const vk::raii::Instance& instance
        );
        vk::raii::PhysicalDevice physicalDevice(
            const vk::raii::Instance& instance
        );
        vk::raii::Device logicalDevice(
            const vk::raii::PhysicalDevice& physicalDevice,
            const QueueFamily& family,
            const std::vector<const char*>& layers = {},
            const std::vector<const char*>& extensions = {}
        );
        vk::raii::Queue queue(
            const vk::raii::Device& device,
            const QueueFamily& family
        );
        vk::raii::SurfaceKHR surface(
            const vk::raii::Instance& instance,
            GLFWwindow* handle
        );
        std::vector<vk::Image> images(const vk::raii::SwapchainKHR& swapchain);
        std::vector<vk::raii::ImageView> imageViews(
            const vk::raii::Device& device,
            const std::vector<vk::Image>& images,
            vk::Format imageFormat
        );
        vk::raii::ShaderModule shaderModule(
            const vk::raii::Device& device,
            const std::vector<char>& code
        );
        vk::raii::RenderPass renderpass(
            const vk::raii::Device& device,
            const ImageInfo& imageInfo
        );
        vk::raii::PipelineLayout layout(
            const vk::raii::Device& device
        );
        vk::raii::Pipeline pipeline(
            const vk::raii::Device& device,
            const vk::raii::ShaderModule& vertModule,
            const vk::raii::ShaderModule& fragModule,
            const vk::raii::RenderPass& renderpass,
            const vk::raii::PipelineLayout& layout,
            const SwapchainInfo& swapchainInfo
        );
        std::vector<vk::raii::Framebuffer> framebuffers(
            const vk::raii::Device& device,
            const vk::raii::RenderPass& renderpass,
            const std::vector<vk::raii::ImageView>& views,
            const ImageInfo& imageInfo
        );
        vk::raii::CommandPool commandpool(
            const vk::raii::Device& device,
            const QueueFamily& family
        );
        vk::raii::CommandBuffer commandbuffer(
            const vk::raii::Device& device,
            const vk::raii::CommandPool& pool
        );
        vk::raii::Buffer vertexbuffer(
            const vk::raii::Device& device,
            std::vector<Vertex> vertices
        );

    };
    namespace utils {
        QueueFamily findQueueFamily(
            const vk::raii::PhysicalDevice& physicalDevice
        );
        SwapchainInfo querySwapChainInfo(
            const vk::raii::PhysicalDevice& device,
            const vk::raii::Device& logicalDevice,
            const vk::raii::SurfaceKHR& surface,
            GLFWwindow* handle
        );
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
        );
        uint32_t findMemoryType(
            const vk::raii::PhysicalDevice& device,
            uint32_t typeFilter,
            vk::MemoryPropertyFlags properties
        );
        vk::raii::DeviceMemory allocateBuffer(
            const vk::raii::Device& device,
            const vk::MemoryRequirements& memRequirements,
            uint32_t memTypeIndex
        );
        void fillBuffer(
            const vk::raii::Buffer& vertexbuffer,
            const vk::raii::DeviceMemory& deviceMemory,
            const vk::MemoryRequirements& memRequirements,
            std::vector<Vertex> vertices
        );
    };
};