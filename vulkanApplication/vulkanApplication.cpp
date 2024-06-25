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
#include <unordered_map>
#include <stack>

std::ostream& operator<<(std::ostream& os, glm::vec2 a) {
    os << std::format("x: {}\ty: {}\n", a.x, a.y);
    return os;
}

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

struct Vec2Hash {
    size_t operator()(const glm::vec2& v) const {
        size_t h1 = std::hash<float>{}(v.x);
        size_t h2 = std::hash<float>{}(v.y);
        return h1 ^ (h2 << 1); // Combine the hashes
    }
};

bool operator==(const glm::vec2& lhs, const glm::vec2& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}


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

glm::vec2 linear(glm::vec2 p1, glm::vec2 p2,float t) {
    return p1 + t * (p2 - p1);
}

glm::vec2 bezier(glm::vec2 p1,glm::vec2 p2,glm::vec2 p3,float t) {
    glm::vec2 lerp1 = linear(p1,p2,t);
    glm::vec2 lerp2 = linear(p2, p3, t);
    return linear(lerp1,lerp2,t);
}

glm::vec2 interpolated_point(glm::vec2 p1,glm::vec2 p2) {
    return { (p1.x + p2.x) / 2,(p1.y + p2.y) / 2 };
}


bool isOnCurve(char tag) {
    return FT_CURVE_TAG(tag) == FT_CURVE_TAG_ON;
}



void add_bezier_curve(glm::vec2 point1,glm::vec2 point2,glm::vec2 point3,std::vector<glm::vec2>& points) {
     for (float t = 0.0f; t < 1.0f; t += 0.1f) {
         points.push_back(bezier(point1, point2, point3, t));
     }
}

static std::pair<std::vector<glm::vec2>,std::vector<char>> interpolated_points(
    std::vector<glm::vec2> points,
    char* ftags
) {
    std::vector<glm::vec2> interp_points;
    std::vector<char> new_tags;
    for (int i = 0; i < points.size(); i++) {
        if (isOnCurve(ftags[i])) {
            interp_points.push_back(points[i]);
            new_tags.push_back(ftags[i]);
        }
        else {
            interp_points.push_back(points[i]);
            new_tags.push_back(ftags[i]);
            if (!isOnCurve(ftags[(i + 1) % points.size()])) {
                interp_points.push_back(interpolated_point(points[i], points[(i + 1) % points.size()]));
                new_tags.push_back(FT_CURVE_TAG_ON);
            }
        }
    }
    return { interp_points,new_tags };
}

auto add_curves(std::vector<glm::vec2> points,char* ftags,std::vector<glm::vec2> contours) 
->std::pair<std::vector<glm::vec2>,std::vector<int>>
{
    std::vector<int> new_contours;
    std::vector<glm::vec2> new_points;
    auto [interp_points, new_tags] = interpolated_points(points, ftags);

    glm::vec2 startpoint = interp_points[0];
    new_points.push_back(startpoint);
    for (int i = 1, idx = 0; i < interp_points.size(); i++) {
        if (new_tags[i] == FT_CURVE_TAG_ON) {
            new_points.push_back(interp_points[i]);
            if (contours[idx] == interp_points[i]) {
                new_contours.push_back(new_points.size() - 1);
                if (i + 1 < interp_points.size()) startpoint = interp_points[i + 1];
                idx++;
            }
        }
        else {
            if (contours[idx] == interp_points[i]) {
                add_bezier_curve(interp_points[i - 1], interp_points[i], startpoint, new_points);
                new_contours.push_back(new_points.size() - 1);
                if (i + 1 < interp_points.size()) startpoint = interp_points[i + 1];
                idx++;
            }
            else add_bezier_curve(interp_points[i - 1], interp_points[i], interp_points[(i + 1) % interp_points.size()], new_points);
        }
    }
    return { new_points,new_contours };
}


auto loadFont(std::string fontPath) -> std::pair<std::vector<glm::vec2>, std::vector<int>> {
    FT_Library library;
    FT_Face face;
    FT_Error error;
    error = FT_Init_FreeType(&library);
    error = FT_New_Face(library, "C:\\Users\\ashwi\\Downloads\\Roboto\\Roboto-Regular.ttf", 0, &face);
    FT_Load_Char(face, 'C', FT_LOAD_DEFAULT);
    FT_GlyphSlot slot = face->glyph;
    FT_Outline outline = slot->outline;
    int numPoints = outline.n_points;
    FT_Vector* fpoints = outline.points;
    int numContours = outline.n_contours;
    short* fcontours = outline.contours;
    char* ftags=outline.tags;
    int idx = 0;
    int count = 0;
    std::vector<glm::vec2> contours;
    std::vector<glm::vec2> points;
    for (int i = 0; i < numPoints; i++) {
        points.push_back({ fpoints[i].x,fpoints[i].y });
    }
    for (int i = 0; i < numContours; i++) {
        contours.push_back(points[fcontours[i]]);
    }

    auto [new_points, new_contours] = add_curves(points,ftags,contours);

    
    return { new_points,new_contours };
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
    normalize(points);
    /*for (int i = 0; i < points.size(); i++) {
        std::cout << std::format("x: {}\ty:{}\n",points[i].x,points[i].y);
    }*/
    //for (int i = 0; i < endpoints.size(); i++) {
    //    std::cout << endpoints[i] << "   ";
    //}
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