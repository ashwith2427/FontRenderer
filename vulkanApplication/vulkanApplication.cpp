#include <ft2build.h>
#include <freetype/freetype.h>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <format>
#include <ranges>
#include <fstream>
#include "pipeline.hpp"
#include <span>

std::vector<const char*> instanceExtensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
    "VK_KHR_win32_surface",
};

std::vector<const char*> instanceLayers = {
    "VK_LAYER_KHRONOS_validation"
};

std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

static void normalize(std::vector<glm::vec2>& points) {
	auto xCompare = [](const glm::vec2& a, const glm::vec2& b) {
		return a.x < b.x;
	};
	auto yCompare = [](const glm::vec2& a, const glm::vec2& b) {
		return a.y < b.y;
	};
	int xmax = std::ranges::max_element(points,xCompare)->x;
	int ymax = std::ranges::max_element(points,yCompare)->y;
	int xmin = std::ranges::min_element(points,xCompare)->x;
	int ymin = std::ranges::min_element(points,yCompare)->y;

	for (auto& point : points) {
		float xCalc = (point.x - xmin) / (xmax - xmin)-0.5f;
		float yCalc = -(point.y - ymin) / (ymax - ymin)+0.5f;
		point = { xCalc,yCalc };
	}
}
std::string gotoPath(const std::string& path) {
	std::string FontPath = "C:\\Users\\ashwi\\source\\repos\\vulkanApplication\\vulkanApplication\\";
	return FontPath + path;
}

auto loadFont(std::string fontPath) -> std::pair<std::vector<glm::vec2>, std::vector<int>> {
    FT_Library library;
    FT_Face face;
    FT_Error error;

    error = FT_Init_FreeType(&library);
    error = FT_New_Face(library, "C:\\Users\\ashwi\\Downloads\\Roboto\\Roboto-Regular.ttf", 0, &face);
    FT_Load_Char(face, 'X', FT_LOAD_DEFAULT);
    FT_GlyphSlot slot = face->glyph;
    FT_Outline outline = slot->outline;
    int numPoints = outline.n_points;
    FT_Vector* fpoints = outline.points;
    int numContours = outline.n_contours;
    short* fcontours = outline.contours;
    std::vector<glm::vec2> points;
    for (int i = 0; i < numPoints; i++) {
        points.push_back({ fpoints[i].x,fpoints[i].y });
    }
    std::vector<int> contours;
    for (int i = 0; i < numContours; i++) {
        contours.push_back(fcontours[i]);
    }
    return { points,contours };
}


static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::runtime_error(std::format("Failed to open file: {}", errno));
    }

    int size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), size);
    file.close();
    return buffer;
}

int main() {
    auto [points,endpoints] = loadFont(gotoPath("fonts/Roboto-Regular.ttf"));
	std::string vert = gotoPath("shaders/vert.spv");
	std::string frag = gotoPath("shaders/frag.spv");
    GLFWwindow* handle = vo::create::window(600, 500, "Vulkan");
    vk::raii::Context context{};
    vk::raii::Instance instance = vo::create::instance(
        context,
        "Vulkan",
        "No engine",
        instanceLayers,
        instanceExtensions
    );
    vk::raii::PhysicalDevice physicalDevice = vo::create::physicalDevice(instance);
    QueueFamily family = vo::utils::findQueueFamily(physicalDevice);
    vk::raii::Device device = vo::create::logicalDevice(
        physicalDevice, family, {}, deviceExtensions
    );
    vk::raii::Queue graphicsQueue = vo::create::queue(device, family);
    vk::raii::SurfaceKHR surface = vo::create::surface(instance, handle);
    SwapchainInfo swapchainInfo = vo::utils::querySwapChainInfo(
        physicalDevice,
        device,
        surface,
        handle
    );
    ImageInfo imageInfo = {
        swapchainInfo.surfaceFormat.format,
        swapchainInfo.extent.width,
        swapchainInfo.extent.height
    };
    std::vector<vk::Image> images = vo::create::images(swapchainInfo.swapchain);
    std::vector<vk::raii::ImageView> imageViews = vo::create::imageViews(
        device, images, swapchainInfo.surfaceFormat.format
    );
    std::vector<vk::raii::DeviceMemory> mems;
    std::vector<vk::raii::Buffer> buffers;
    std::vector<int> counts;

    std::cout << points.size();
    for (int i = 0; i < endpoints.size(); i++) {
        std::cout << endpoints[i] << std::endl;
    }
    normalize(points);
    int startpoint = 0;
    for (int ep = 0; ep < endpoints.size();ep++) {
        int contourIndex = endpoints[ep] - startpoint + 1;
        int count = 0;
        std::vector<Vertex> vertices;
        for (int i = startpoint; i < startpoint+contourIndex; i++) {
            vertices.push_back({ points[i] ,{0.0f,0.0f,0.0f} });
            count++;
        }
        count++;
        counts.push_back(count);
        vertices.push_back({ points[startpoint],{0.0f,0.0f,0.0f} });
        vk::raii::Buffer vertexBuffer = vo::create::vertexbuffer(device, vertices);
        vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
        uint32_t memType = vo::utils::findMemoryType(
            physicalDevice,
            memRequirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent
        );

        vk::raii::DeviceMemory memory = vo::utils::allocateBuffer(device, memRequirements, memType);
        vo::utils::fillBuffer(vertexBuffer, memory, memRequirements, vertices);
        buffers.push_back(std::move(vertexBuffer));
        mems.push_back(std::move(memory));
        startpoint = endpoints[ep] + 1;
    }

    auto vertexCode = readFile(vert);
    auto fragmentCode = readFile(frag);
    vk::raii::ShaderModule vertexShaderModule = vo::create::shaderModule(device, vertexCode);
    vk::raii::ShaderModule fragmentShaderModule = vo::create::shaderModule(device, fragmentCode);
    vk::raii::RenderPass renderpass = vo::create::renderpass(device, imageInfo);
    vk::raii::PipelineLayout layout = vo::create::layout(device);
    vk::raii::Pipeline pipeline = vo::create::pipeline(
        device, vertexShaderModule, fragmentShaderModule, renderpass,
        layout, swapchainInfo
    );
    std::vector<vk::raii::Framebuffer> framebuffers = vo::create::framebuffers(
        device,
        renderpass,
        imageViews,
        imageInfo
    );
    vk::raii::CommandPool pool = vo::create::commandpool(device, family);
    vk::raii::CommandBuffer commandbuffer = vo::create::commandbuffer(device, pool);

    vk::raii::Semaphore imageAcquiredSemaphore(device, vk::SemaphoreCreateInfo());
    vk::raii::Semaphore renderFinishedSemaphore(device, vk::SemaphoreCreateInfo());
    vk::raii::Fence fence(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));


    while (!glfwWindowShouldClose(handle)) {
        glfwPollEvents();
        auto [waitRes, presentRes] = vo::utils::drawFrame(
            device,
            swapchainInfo,
            renderpass,
            pipeline,
            commandbuffer,
            imageAcquiredSemaphore,
            renderFinishedSemaphore,
            fence,
            framebuffers,
            graphicsQueue,
            buffers,
            counts
        );
        device.waitIdle();
    }
    exit(0);
}