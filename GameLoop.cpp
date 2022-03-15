// Complete through https://vulkan-tutorial.com/en/Uniform_buffers/Descriptor_layout_and_buffer

//#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <vector>
#include <set>
#include <optional>

#ifdef _DEBUG
#define VERBOSE_ON
#define VALIDATION_ON
#endif

const uint32_t MAX_FRAMES_IN_FLIGHT = 4;

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDesc()
    {
        VkVertexInputBindingDescription bind_desc{};
        bind_desc.binding = 0;
        bind_desc.stride = sizeof(Vertex);
        bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bind_desc;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttribDesc()
    {
        std::array<VkVertexInputAttributeDescription, 2> attrib_desc{};
        attrib_desc[0].binding = 0;     // Matches binding index 0 above
        attrib_desc[0].location = 0;    // Input location in shader (position attribute)
        attrib_desc[0].format = VK_FORMAT_R32G32_SFLOAT;    // 2 32-bit floats    
        attrib_desc[0].offset = offsetof(Vertex, pos);      // 1st element

        attrib_desc[1].binding = 0;     // Matches binding index 0 above
        attrib_desc[1].location = 1;    // Input location in shader (color attribute
        attrib_desc[1].format = VK_FORMAT_R32G32B32_SFLOAT; // 3 32-bit floats    
        attrib_desc[1].offset = offsetof(Vertex, color);    // 2nd element

        return attrib_desc;
    }
};

struct mvp_ubo
{
    // Default C++ alignments don't match Vulkan spec: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout
    // i.e. without explicit alignments, the foo vec2 will break this struct
    glm::vec2 foo;
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
};

const std::vector<Vertex> vertices = { {{-0.5, -0.5}, {1.0, 0.0, 0.0}},
                                       {{ 0.5, -0.5}, {0.0, 1.0, 0.0}},
                                       {{ 0.5,  0.5}, {0.0, 0.0, 1.0}},
                                       {{-0.5,  0.5}, {1.0, 1.0, 1.0}} };

const std::vector<uint16_t> indices = { 0, 1, 2, 0, 2, 3 };

class HelloTriangleApplication
{
public:
    void run() 
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    void initWindow()
    {
        glfwSetErrorCallback(glfwErrorCallback);

        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(window_width, window_height, "Vulkan", nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, onFramebufferResize);
    }

    static void onFramebufferResize(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->frame_buffer_resized = true;
    }

    void initVulkan() 
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        choosePhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createSwapImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFrameBuffers();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandPool();
        createTextureImage();
        createTextureSampler();
        createVertexBuffers();
        createIndexBuffers();
        createSyncObjects();
    }

    void mainLoop() 
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device);   // wait for idle before cleaning up
    }

    void cleanup() 
    {
        vkDestroyFence(device, fence_in_flight, nullptr);
        vkDestroySemaphore(device, sem_image_available, nullptr);
        vkDestroySemaphore(device, sem_render_complete, nullptr);
        vkFreeCommandBuffers(device, command_pool, 1, &render_cmd_buf);
        vkDestroyCommandPool(device, command_pool, nullptr);

        cleanupSwapchain();

        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, ubo_desc_layout, nullptr);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyBuffer(device, uniform_buffer[i], nullptr);
            vkFreeMemory(device, uniform_buffer_memory[i], nullptr);
        }
        vkDestroyBuffer(device, vertex_buffer, nullptr);
        vkDestroyBuffer(device, index_buffer, nullptr);
        vkFreeMemory(device, vertex_buffer_mem, nullptr);
        vkFreeMemory(device, index_buffer_mem, nullptr);
        vkDestroyImageView(device, tex_image_view, nullptr);
        vkDestroyImage(device, tex_image, nullptr);
        vkFreeMemory(device, tex_image_mem, nullptr);
        vkDestroySampler(device, tex_sampler, nullptr);
        vkDestroyDevice(device, nullptr);
        destroyDebugMessenger();
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void drawFrame()
    {
        vkWaitForFences(device, 1, &fence_in_flight, VK_TRUE, UINT64_MAX);

        uint32_t image_idx;
        VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sem_image_available, VK_NULL_HANDLE, &image_idx);
        if (VK_ERROR_OUT_OF_DATE_KHR == res)
        {
            recreateSwapChain();    // Something has changed that makes present impossible
            return;
        }
        if (VK_SUCCESS != res && VK_SUBOPTIMAL_KHR != res)  // both are 'successful-ish' results
        {
            throw std::runtime_error("Error acquiring next swap chain image");
        }

        // By here we've assured we'll be submitting work, so we should reset the fence
        vkResetFences(device, 1, &fence_in_flight);

        updateUniformBuffer(image_idx);

        // Allocate on 1st pass
        if (VK_NULL_HANDLE == render_cmd_buf) render_cmd_buf = createCommandBuffer();

        vkResetCommandBuffer(render_cmd_buf, 0);
        recordCommandBuffer(render_cmd_buf, image_idx);

        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &sem_image_available;
        si.pWaitDstStageMask = wait_stages;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &render_cmd_buf;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &sem_render_complete;

        if (VK_SUCCESS != vkQueueSubmit(gfx_queue, 1, &si, fence_in_flight))
        {
            throw std::runtime_error("Error submitting draw command buffer");
        }

        VkPresentInfoKHR present = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &sem_render_complete;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &image_idx;
        present.pResults = nullptr;

        res = vkQueuePresentKHR(present_queue, &present);
        if (VK_ERROR_OUT_OF_DATE_KHR == res || VK_SUBOPTIMAL_KHR == res || frame_buffer_resized)
        {
            frame_buffer_resized = false;
            recreateSwapChain();    // Something has changed that we should adjust for
        }
        else if (VK_SUCCESS != res)
        {
            throw std::runtime_error("Error on image present");
        }
    }

    void createInstance() 
    {
        // Fetch list of available instance extensions
        uint32_t ext_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> extensions(ext_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, extensions.data());

#ifdef VERBOSE_ON
        // dump the list
        std::cout << "Instance extensions" << std::endl;
        for (const auto& ext : extensions)
        {
            std::cout << '\t' << ext.extensionName << std::endl;
        }
#endif

        auto required_extensions = getRequiredInstanceExtensions();

        // Check for validation layers, if requested
        if (enable_validation && !checkValidationLayerSupport()) 
        {
            throw std::runtime_error("Validation layers requested, but not available!");
        }

        // App info struct
        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr};
        appInfo.pApplicationName = "Simple Triangle";
        appInfo.applicationVersion = VK_API_VERSION_1_0;    // App version, not API version, but format works so... 
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = 0;
        appInfo.apiVersion = VK_HEADER_VERSION_COMPLETE;    // Use latest installed version (1.3)

        // Instance creation struct, which points at app info
        VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr };
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t> (required_extensions.size());
        createInfo.ppEnabledExtensionNames = required_extensions.data();
        if (enable_validation)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t> (validation_layers.size());
            createInfo.ppEnabledLayerNames = validation_layers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
        }

        // include a debug messenger for instance creation/destruction
        VkDebugUtilsMessengerCreateInfoEXT dum_ci{};
        if (enable_validation)
        {
            populateDebugMessengerCI(dum_ci);
            createInfo.pNext = &dum_ci;
        }

        // create the instance
        VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
        if (VK_SUCCESS != res) throw std::runtime_error("FATAL - Failed to create instance.");
    }

    bool checkValidationLayerSupport()
    {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

#ifdef VERBOSE_ON
        // dump the list
        std::cout << std::endl << "Available validation layers" << std::endl;
        for (uint32_t i = 0; i < layer_count; i++)
        {
            std::cout << '\t' << layers[i].layerName << std::endl;
        }
#endif

        // verify that all specified validation layers are available
        for (const char* layer : validation_layers)
        {
            bool found = false;
            for (const auto& layer_properties : layers)
            {
                if (strcmp(layer, layer_properties.layerName) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (!found) return false;
        }

        return true;
    }

    std::vector<const char*> getRequiredInstanceExtensions()
    {
        // glfw required extensions list
        uint32_t     glfw_ext_count = 0;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        std::vector<const char*> required_extensions(glfw_extensions, glfw_extensions + glfw_ext_count);

        if (enable_validation)
        {
            required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

#ifdef VERBOSE_ON
        // dump the list
        std::cout << std::endl << "All required extensions" << std::endl;
        for (const auto& ext : required_extensions)
        {
            std::cout << '\t' << ext << std::endl;
        }
#endif
        return required_extensions;
    }

    void createSurface()
    {
#if 0
        // 'by-hand' version
        VkWin32SurfaceCreateInfoKHR surf_ci = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, nullptr };
        surf_ci.hwnd = glfwGetWin32Window(window);
        surf_ci.hinstance = GetModuleHandle(nullptr);   // HINSTANCE of current process
        if (vkCreateWin32SurfaceKHR(instance, &surf_ci, nullptr, &surface) != VK_SUCCESS) 
        {
            throw std::runtime_error("Failed to create window surface!");
        }
#endif

        // Use glfw to create surface for us
        if (VK_SUCCESS != glfwCreateWindowSurface(instance, window, nullptr, &surface))
        {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    void choosePhysicalDevice()
    {
        uint32_t dev_count = 0;
        vkEnumeratePhysicalDevices(instance, &dev_count, nullptr);
        if (0 == dev_count) throw std::runtime_error("No Vulkan-capable physical devices");

        std::vector<VkPhysicalDevice> devices(dev_count);
        vkEnumeratePhysicalDevices(instance, &dev_count, devices.data());

        for (const VkPhysicalDevice& device : devices)
        {
            if (physDeviceAcceptable(device))
            {
                physical_device = device;
                break;
            }
        }

        if (VK_NULL_HANDLE == physical_device) throw std::runtime_error("No suitable physical device found");
    }

    struct QueueFamilies
    {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> compute_family;
        std::optional<uint32_t> transfer_family;
        std::optional<uint32_t> sparse_binding_family;
        std::optional<uint32_t> protected_family;

        std::optional<uint32_t> present_family;

        bool isComplete()   // minimal required set of queues are present
        {
            return (graphics_family.has_value() && present_family.has_value());
        }
    };

    bool physDeviceAcceptable(VkPhysicalDevice phys)
    {
        VkPhysicalDeviceProperties2 dev_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, nullptr };
        VkPhysicalDeviceFeatures2   dev_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, nullptr };

        vkGetPhysicalDeviceProperties2(phys, &dev_props);
        vkGetPhysicalDeviceFeatures2(phys, &dev_features);

        // for fun - force integrated GPU
        //if (VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU != dev_props.properties.deviceType) return false;

        // Filter on min properties & features here, e.g.
        VkBool32 reqd_features = (dev_features.features.samplerAnisotropy);

        // for example...
        if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == dev_props.properties.deviceType &&
            dev_features.features.vertexPipelineStoresAndAtomics)
        {
            bool gpu_ok = true; // ignore this for now, any Vulkan device is ok
        }

        // Check for required device extensions
        bool has_extensions = checkDeviceExtensions(phys);

        // Verify swap chain support
        bool swap_chain_ok = false;
        if (has_extensions)
        {
            SwapChainDetails swap_details = querySwapChainSupport(phys);
            swap_chain_ok = !swap_details.formats.empty() && !swap_details.modes.empty();   // anything goes
        }

        // Check for presence of required queues
        QueueFamilies queue_fam_idx = findDeviceQueueFamilies(phys);
        
        bool found = (queue_fam_idx.isComplete() && swap_chain_ok && reqd_features);
#ifdef VERBOSE_ON
        if (found) std::cout << std::endl << "Physical GPU selected: " << dev_props.properties.deviceName << std::endl;
#endif
        return found;
    }

    QueueFamilies findDeviceQueueFamilies(VkPhysicalDevice phys)
    {
        QueueFamilies family_indices;
        uint32_t fam_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(phys, &fam_count, nullptr);
        if (fam_count > 0)
        {
            std::vector<VkQueueFamilyProperties2> fam_props(fam_count);
            for (auto& fam : fam_props) fam = { VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, nullptr };
            vkGetPhysicalDeviceQueueFamilyProperties2(phys, &fam_count, fam_props.data());

            // possible (if unlikely) that gfx and present are different queue indices
            int idx = 0;
            for (const auto& fam : fam_props)
            {
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)       family_indices.graphics_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT)        family_indices.compute_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT)       family_indices.transfer_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) family_indices.sparse_binding_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_PROTECTED_BIT)      family_indices.protected_family = idx;

                VkBool32 has_present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(phys, idx, surface, &has_present);
                if (has_present) family_indices.present_family = idx;
                if (family_indices.isComplete()) break;

                idx++;
            }
        }

        return family_indices;
    }

    bool checkDeviceExtensions(VkPhysicalDevice phys)
    {
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &ext_count, nullptr);
        if (0 == ext_count) return false;

        std::vector<VkExtensionProperties> ext_props(ext_count);
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &ext_count, ext_props.data());

#ifdef VERBOSE_ON
        // dump the list
        std::cout << std::endl << "Device extensions" << std::endl;
        for (const auto& ext : ext_props)
        {
            std::cout << '\t' << ext.extensionName << std::endl;
        }
#endif

        // Create set of req'd, erase each one as found in the available list
        std::set<std::string> required_set(device_extensions.begin(), device_extensions.end());
        for (const auto& ext : ext_props) required_set.erase(ext.extensionName);

        return required_set.empty();
    }

    void createLogicalDevice()
    {
        QueueFamilies queue_idx = findDeviceQueueFamilies(physical_device);
        float queue_priority = 1.0f;

        // Queues (gfx & present - which may be the same family - so creating either 1 or 2 queues)
        std::vector<VkDeviceQueueCreateInfo> dev_q_ci;// = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr };
        std::set<uint32_t> unique_q_families = { queue_idx.graphics_family.value(), queue_idx.present_family.value() };
        for (uint32_t q_fam : unique_q_families)
        {
            VkDeviceQueueCreateInfo ci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr };
            ci.queueFamilyIndex = q_fam;
            ci.queueCount = 1;
            ci.pQueuePriorities = &queue_priority;    // highest priority (range 0.0...1.0)
            dev_q_ci.push_back(ci);
        }

        // Features
        VkPhysicalDeviceFeatures2 dev_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr };
        dev_features.features.samplerAnisotropy = VK_TRUE;

        // Logical Device
        VkDeviceCreateInfo dev_ci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr };
        dev_ci.pQueueCreateInfos = dev_q_ci.data();
        dev_ci.queueCreateInfoCount = static_cast<uint32_t>(dev_q_ci.size());
        dev_ci.pEnabledFeatures = &dev_features.features;
        dev_ci.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
        dev_ci.ppEnabledExtensionNames = device_extensions.data();
        
#if 0
        // these are obsoleted and ignored after 1.0, but may be set for backwards compatibility
        dev_ci.enabledExtensionCount = 0;
        if (enable_validation)
        {
            dev_ci.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            dev_ci.ppEnabledLayerNames = validation_layers.data();
        }
        else
        {
            dev_ci.enabledLayerCount = 0;
        }
#endif

        // Create the device
        if (VK_SUCCESS != vkCreateDevice(physical_device, &dev_ci, nullptr, &device))
        {
            throw std::runtime_error("Failed to create logical device");
        }

        // Query the device's queue handle(s)
        // (previously...) vkGetDeviceQueue(device, queue_idx.graphics_family.value(), 0, &gfx_queue);
        VkDeviceQueueInfo2 q_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2, nullptr };
        q_info.flags = 0;
        q_info.queueFamilyIndex = queue_idx.graphics_family.value();
        q_info.queueIndex = 0;
        vkGetDeviceQueue2(device, &q_info, &gfx_queue);

        q_info.queueFamilyIndex = queue_idx.present_family.value();
        vkGetDeviceQueue2(device, &q_info, &present_queue);
    }

    struct SwapChainDetails
    {
        VkSurfaceCapabilitiesKHR caps;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> modes;
    };

    SwapChainDetails querySwapChainSupport(VkPhysicalDevice phys)
    {
        SwapChainDetails swap;

        //vkGetPhysicalDeviceSurfaceCapabilities2KHR()

        // capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &swap.caps);

        // formats
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
        if (0 < count)
        {
            swap.formats.resize(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, swap.formats.data());
        }

        // presentation modes
        count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
        if (0 < count)
        {
            swap.modes.resize(count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, swap.modes.data());
        }

        return swap;
    }
    
    VkSurfaceFormatKHR chooseSwapFormat(const std::vector<VkSurfaceFormatKHR>& formats)
    {
        // Look for our 'ideal' surface format. If not available, then just use first listed
        for (const auto& format : formats)
        {
            if ((VK_FORMAT_B8G8R8A8_SRGB == format.format) && (VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == format.colorSpace))
                return format;
        }
        return formats[0];
    }

    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes)
    {
        // Use mailbox if available for least possible latency. If not, use FIFO
        for (const auto& mode : modes)
        {
            if (VK_PRESENT_MODE_MAILBOX_KHR == mode) return mode;
        }
        
        return VK_PRESENT_MODE_FIFO_KHR;    // guaranteed available by spec
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps)
    {
        const uint32_t swap_controlled = 0xffffffff;
        if (swap_controlled != caps.currentExtent.width) return caps.currentExtent; // Not controllable by swapchain

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        VkExtent2D ext;
        ext.width = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
        ext.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
        return ext;
    }

    void createSwapChain()
    {
        SwapChainDetails swap_details = querySwapChainSupport(physical_device);
        VkPresentModeKHR swap_mode = choosePresentMode(swap_details.modes);

        swapchain_format = chooseSwapFormat(swap_details.formats);
        swapchain_extent = chooseSwapExtent(swap_details.caps);

        uint32_t image_count = swap_details.caps.maxImageCount;
        if (0 == image_count) image_count = swap_details.caps.minImageCount + 1;    // max == 0 is flag for 'no limit'
        image_count = std::min(image_count, MAX_FRAMES_IN_FLIGHT);                  // discourage overly-large swap chains.

        // Build up the swapchain CI
        VkSwapchainCreateInfoKHR swap_ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr };
        swap_ci.surface = surface;
        swap_ci.minImageCount = image_count;
        swap_ci.imageFormat = swapchain_format.format;
        swap_ci.imageColorSpace = swapchain_format.colorSpace;
        swap_ci.imageExtent = swapchain_extent;
        swap_ci.imageArrayLayers = 1;
        swap_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;   // rendering directly to swap images
        swap_ci.presentMode = swap_mode;
        swap_ci.clipped = VK_TRUE;  // Don't render pixels that are obscured or clipped
        swap_ci.preTransform = swap_details.caps.currentTransform;  // No image transform
        swap_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Don't blend over other windows
        swap_ci.oldSwapchain = VK_NULL_HANDLE;  // This is not a replacement swapchain

        QueueFamilies q_idx = findDeviceQueueFamilies(physical_device);
        uint32_t qfi[] = { q_idx.graphics_family.value(), q_idx.present_family.value() };
        if (q_idx.graphics_family != q_idx.present_family)
        {
            swap_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;  // shared between queue families (without explicit xfers)
            swap_ci.queueFamilyIndexCount = 2;
            swap_ci.pQueueFamilyIndices = qfi;
        }
        else
        {
            swap_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;   // not shared
            swap_ci.queueFamilyIndexCount = 0;
            swap_ci.pQueueFamilyIndices = nullptr;
        }

        if (VK_SUCCESS != vkCreateSwapchainKHR(device, &swap_ci, nullptr, &swapchain))
        {
            throw std::runtime_error("Failed to create swap chain");
        }

        // Retrieve handles for the swapchain images
        image_count = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
        swapchain_images.resize(image_count);
        vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());
    }

    void recreateSwapChain() // Works on Intel, validation error "vkCreateSwapchainKHR: internal drawable creation failed" on nvidia
    {
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        while (0 == w || 0 == h)
        {
            glfwWaitEvents();   // We've been minimized, so just wait for an event that says otherwise
            glfwGetFramebufferSize(window, &w, &h);
        }

        vkDeviceWaitIdle(device);   // Big hammer synchronization

        cleanupSwapchain();         // Destroy all existing swapchain resources

        createSwapChain();
        createSwapImageViews();
        createRenderPass();
        createGraphicsPipeline();   // Can avoid re-creation by making viewport & scissor dynamic
        createFrameBuffers();
    }

    void cleanupSwapchain()
    {
        for (auto& fb : swapchain_framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        swapchain_framebuffers.clear();
        vkDestroyPipeline(device, pipeline, nullptr); 
        pipeline = VK_NULL_HANDLE;
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
        vkDestroyRenderPass(device, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
        for (auto& imageview : swapchain_image_views) vkDestroyImageView(device, imageview, nullptr);
        swapchain_image_views.clear();
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    void createSwapImageViews()
    {
        swapchain_image_views.resize(swapchain_images.size());

        for (size_t i = 0; i < swapchain_image_views.size(); i++)
        {
            VkImageViewCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
            ci.image        = swapchain_images[i];
            ci.viewType     = VK_IMAGE_VIEW_TYPE_2D;
            ci.format       = swapchain_format.format;
            ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            ci.subresourceRange.baseMipLevel    = 0;
            ci.subresourceRange.levelCount      = 1;
            ci.subresourceRange.baseArrayLayer  = 0;
            ci.subresourceRange.layerCount      = 1;

            if (VK_SUCCESS != vkCreateImageView(device, &ci, nullptr, &swapchain_image_views[i]))
            {
                throw std::runtime_error("Failure while creating swapchain image views");
            }
        }
    }

    void createTexImageView()
    {
        VkImageViewCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
        ci.image = tex_image;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = VK_FORMAT_R8G8B8A8_SRGB;
        ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;

        if (VK_SUCCESS != vkCreateImageView(device, &ci, nullptr, &tex_image_view))
        {
            throw std::runtime_error("Failure while creating texture image view");
        }
    }

    static std::vector<char> readSPIRV(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) throw std::runtime_error(std::string("Failed to open shader file: " + filename));

        size_t file_size = (size_t)file.tellg();
        std::vector<char> buffer(file_size);
        file.seekg(0);
        file.read(buffer.data(), file_size);

#ifdef VERBOSE_ON
        std::cout << std::endl << "Read SPIR-V shader file " << filename << ", size  = " << file_size << " bytes." << std::endl;
#endif

        file.close();
        return buffer;
    }

    VkShaderModule createShaderModule(const std::vector<char>& spirv)
    {
        VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr };
        ci.codeSize = spirv.size();
        ci.pCode = reinterpret_cast<const uint32_t*>(spirv.data());

        VkShaderModule shader;
        if (VK_SUCCESS != vkCreateShaderModule(device, &ci, nullptr, &shader)) throw std::runtime_error("Failed to create shader module");
        return shader;
    }

    void createRenderPass()
    {
        VkAttachmentDescription2 attachment = { VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, nullptr };
        attachment.format = swapchain_format.format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;         // Match the swapchain image views
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    // Clear before rendering
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Save render contents for display
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;   // We'll be presenting the result via swapchain

        VkAttachmentReference2 attach_ref = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr };
        attach_ref.attachment = 0;  // Index of the attachment - 0 since we have only 1
        attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;   // layout to transition to when this subpass is active

        VkSubpassDescription2 subpass = { VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, nullptr };
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &attach_ref;    // shader layout directive indexes into this array, e.g. layout(location=0)

        VkSubpassDependency2 dep = { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2, nullptr };
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;   // Before render pass
        dep.dstSubpass = 0; // Index 0 is our sole subpass
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;   // swap chain finished with color attachment
        dep.srcAccessMask = 0;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;   // we will modify color attachment
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo2 render_pass_ci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, nullptr };
        render_pass_ci.attachmentCount = 1;
        render_pass_ci.pAttachments = &attachment;
        render_pass_ci.subpassCount = 1;
        render_pass_ci.pSubpasses = &subpass;
        render_pass_ci.dependencyCount = 1;
        render_pass_ci.pDependencies = &dep;

        if (VK_SUCCESS != vkCreateRenderPass2(device, &render_pass_ci, nullptr, &render_pass))
        {
            throw std::runtime_error("Failed to create render pass");
        }
    }

    void createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding ubo_layout{};
        ubo_layout.binding = 0;    // Matches vertex shader binding layout
        ubo_layout.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Consumed only in vtx shader
        ubo_layout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout.descriptorCount = 1;
        ubo_layout.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo ds_ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
        ds_ci.bindingCount = 1;
        ds_ci.pBindings = &ubo_layout;

        if (VK_SUCCESS != vkCreateDescriptorSetLayout(device, &ds_ci, nullptr, &ubo_desc_layout))
        {
            throw std::runtime_error("Failed to create ubo descriptor set layout");
        }

    }

    void createGraphicsPipeline()
    {
        /////////////////////////////////////////////////////////////
        // Shaders
        /////////////////////////////////////////////////////////////
        auto vert_shader = readSPIRV("vert.spv");
        auto frag_shader = readSPIRV("frag.spv");

        VkShaderModule vert_shader_module = createShaderModule(vert_shader);
        VkShaderModule frag_shader_module = createShaderModule(frag_shader);

        VkPipelineShaderStageCreateInfo vert_ci = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
        vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_ci.module = vert_shader_module;
        vert_ci.pName = "main";
        vert_ci.pSpecializationInfo = nullptr;  // Specialization constants go here

        VkPipelineShaderStageCreateInfo frag_ci = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
        frag_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_ci.module = frag_shader_module;
        frag_ci.pName = "main";
        frag_ci.pSpecializationInfo = nullptr;  // Specialization constants go here

        VkPipelineShaderStageCreateInfo pipe_stages[] = { vert_ci, frag_ci };

        /////////////////////////////////////////////////////////////
        // Vertex Input (none, for the moment - vertices are hard-coded in the shader)
        /////////////////////////////////////////////////////////////
        auto bind_desc = Vertex::getBindingDesc();
        auto attrib_desc = Vertex::getAttribDesc();

        VkPipelineVertexInputStateCreateInfo vtx_in_ci = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr };
        vtx_in_ci.vertexBindingDescriptionCount = 1;
        vtx_in_ci.pVertexBindingDescriptions = &bind_desc;
        vtx_in_ci.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrib_desc.size());
        vtx_in_ci.pVertexAttributeDescriptions = attrib_desc.data();

        /////////////////////////////////////////////////////////////
        // Input Assembly
        /////////////////////////////////////////////////////////////
        VkPipelineInputAssemblyStateCreateInfo in_ass_ci = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr };
        in_ass_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        in_ass_ci.primitiveRestartEnable = VK_FALSE;

        /////////////////////////////////////////////////////////////
        // Viewport
        /////////////////////////////////////////////////////////////
        VkViewport viewport{};
        viewport.x = 0.0f; 
        viewport.y = 0.0f;
        viewport.width = (float)swapchain_extent.width;
        viewport.height = (float)swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapchain_extent;

        VkPipelineViewportStateCreateInfo view_ci = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr };
        view_ci.viewportCount = 1;
        view_ci.pViewports = &viewport;
        view_ci.scissorCount = 1;
        view_ci.pScissors = &scissor;

        /////////////////////////////////////////////////////////////
        // Rasterizer
        /////////////////////////////////////////////////////////////
        VkPipelineRasterizationStateCreateInfo rast_ci = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr };
        rast_ci.depthClampEnable = VK_FALSE;        // kill outside depth range
        rast_ci.rasterizerDiscardEnable = VK_FALSE; // pass geometry to rasterizer
        rast_ci.polygonMode = VK_POLYGON_MODE_FILL;
        rast_ci.lineWidth = 1.0f;
        rast_ci.cullMode = VK_CULL_MODE_BACK_BIT;   // enable back face culling
        rast_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rast_ci.depthBiasEnable = VK_FALSE;
        rast_ci.depthBiasConstantFactor = 0.0f; // disabled
        rast_ci.depthBiasClamp = 0.0f;          // disabled
        rast_ci.depthBiasSlopeFactor = 0.0f;    // disabled

        /////////////////////////////////////////////////////////////
        // Multisampling
        /////////////////////////////////////////////////////////////
        VkPipelineMultisampleStateCreateInfo multi_ci = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr };
        multi_ci.sampleShadingEnable = VK_FALSE;
        multi_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  
        multi_ci.minSampleShading = 1.0f;           // disabled
        multi_ci.pSampleMask = nullptr;             // disabled
        multi_ci.alphaToCoverageEnable = VK_FALSE;  // disabled
        multi_ci.alphaToOneEnable = VK_FALSE;       // disabled

        /////////////////////////////////////////////////////////////
        // Depth / Stencil (unused for now)
        /////////////////////////////////////////////////////////////
        VkPipelineDepthStencilStateCreateInfo ds_ci = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
        ds_ci.depthTestEnable = VK_FALSE;
        ds_ci.depthWriteEnable = VK_FALSE;
        ds_ci.depthBoundsTestEnable = VK_FALSE;
        ds_ci.stencilTestEnable = VK_FALSE;

        /////////////////////////////////////////////////////////////
        // Color Blending (none)
        /////////////////////////////////////////////////////////////
        VkPipelineColorBlendAttachmentState blend_attach{};
        blend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT;
        blend_attach.blendEnable            = VK_FALSE;
        blend_attach.srcColorBlendFactor    = VK_BLEND_FACTOR_ONE;  // disabled
        blend_attach.dstColorBlendFactor    = VK_BLEND_FACTOR_ZERO; // disabled
        blend_attach.colorBlendOp           = VK_BLEND_OP_ADD;      // disabled
        blend_attach.srcAlphaBlendFactor    = VK_BLEND_FACTOR_ONE;  // disabled
        blend_attach.dstAlphaBlendFactor    = VK_BLEND_FACTOR_ZERO; // disabled
        blend_attach.alphaBlendOp           = VK_BLEND_OP_ADD;      // disabled

        VkPipelineColorBlendStateCreateInfo blend_ci = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr };
        blend_ci.attachmentCount = 1;
        blend_ci.pAttachments = &blend_attach;
        blend_ci.logicOpEnable = VK_FALSE;
        blend_ci.logicOp = VK_LOGIC_OP_COPY;    // disabled
        // blend_ci.blendConstants[0] = ...

        /////////////////////////////////////////////////////////////
        // Dynamic State (example - unused)
        /////////////////////////////////////////////////////////////
        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };

        VkPipelineDynamicStateCreateInfo dyn_ci = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr };
        dyn_ci.dynamicStateCount = 2;
        dyn_ci.pDynamicStates = dynamic_states;

        /////////////////////////////////////////////////////////////
        // Pipeline Layout
        /////////////////////////////////////////////////////////////
        VkPipelineLayoutCreateInfo layout_ci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr };
        layout_ci.setLayoutCount = 1;
        layout_ci.pSetLayouts = &ubo_desc_layout;
        layout_ci.pushConstantRangeCount = 0;
        layout_ci.pPushConstantRanges = nullptr;

        if (VK_SUCCESS != vkCreatePipelineLayout(device, &layout_ci, nullptr, &pipeline_layout))
        {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        /////////////////////////////////////////////////////////////
        // Create pipeline
        /////////////////////////////////////////////////////////////
        VkGraphicsPipelineCreateInfo pipe_ci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr };
        pipe_ci.stageCount = 2;
        pipe_ci.pStages = pipe_stages;
        pipe_ci.pVertexInputState = &vtx_in_ci;
        pipe_ci.pInputAssemblyState = &in_ass_ci;
        pipe_ci.pViewportState = &view_ci;
        pipe_ci.pRasterizationState = &rast_ci;
        pipe_ci.pMultisampleState = &multi_ci;
        pipe_ci.pDepthStencilState = &ds_ci;
        pipe_ci.pColorBlendState = &blend_ci;
        pipe_ci.pDynamicState = nullptr;    // &dyn_ci;
        pipe_ci.layout = pipeline_layout;
        pipe_ci.renderPass = render_pass;
        pipe_ci.subpass = 0;    // Index of the render_pass subpass that uses this pipeline
        pipe_ci.basePipelineHandle = VK_NULL_HANDLE;    // Not deriving from another pipeline
        pipe_ci.basePipelineIndex = -1;                 // disabled

        if (VK_SUCCESS != vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &pipeline))
        {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        /////////////////////////////////////////////////////////////
        // Cleanup
        /////////////////////////////////////////////////////////////
        vkDestroyShaderModule(device, vert_shader_module, nullptr);
        vkDestroyShaderModule(device, frag_shader_module, nullptr);
    }

    void createFrameBuffers()
    {
        swapchain_framebuffers.resize(swapchain_image_views.size());

        for (size_t i = 0; i < swapchain_image_views.size(); i++)
        {
            VkImageView image_attachments[] = { swapchain_image_views[i] };

            VkFramebufferCreateInfo fb_ci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr };
            fb_ci.renderPass = render_pass;
            fb_ci.attachmentCount = 1;
            fb_ci.pAttachments = image_attachments;
            fb_ci.width = swapchain_extent.width;
            fb_ci.height = swapchain_extent.height;
            fb_ci.layers = 1;

            if (VK_SUCCESS != vkCreateFramebuffer(device, &fb_ci, nullptr, &swapchain_framebuffers[i]))
            {
                throw std::runtime_error("Failed to create framebuffer");
            }
        }
    }

    void createTextureSampler()
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physical_device, &props);

        VkSamplerCreateInfo ci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr };
        ci.minFilter = VK_FILTER_LINEAR;
        ci.magFilter = VK_FILTER_LINEAR;
        ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        ci.anisotropyEnable = VK_TRUE;
        ci.maxAnisotropy = props.limits.maxSamplerAnisotropy;
        ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        ci.unnormalizedCoordinates = VK_FALSE;
        ci.compareEnable = VK_FALSE;
        ci.compareOp = VK_COMPARE_OP_ALWAYS;
        ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        ci.mipLodBias = 0.f;
        ci.minLod = 0.f;
        ci.maxLod = 0.f;

        if (VK_SUCCESS != vkCreateSampler(device, &ci, nullptr, &tex_sampler))
        {
            throw std::runtime_error("Failed to create texture sampler");
        }
    }

    void createTextureImage()
    {
        // Load image
        int width, height, channels;
        stbi_uc* pixels = stbi_load("textures/statue.jpg", &width, &height, &channels, STBI_rgb_alpha);
        VkDeviceSize image_size = width * height * (uint64_t)STBI_rgb_alpha;    // RGB in, RGBA out
        if (!pixels) throw std::runtime_error("Failed to to load texture");

        // Copy to a staging buffer
        VkBuffer staging_buffer;
        VkDeviceMemory sb_mem;
        createBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer, sb_mem);
        void* data;
        vkMapMemory(device, sb_mem, 0, image_size, 0, &data);
        memcpy(data, pixels, image_size);
        vkUnmapMemory(device, sb_mem);
        stbi_image_free(pixels);

        // Create the device-local tex image
        createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,      // Dest for staging copy, will be sampled by shaders
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,        
                    tex_image, tex_image_mem);

        // Copy buffer data to image
        transitionImageLayout(tex_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(staging_buffer, tex_image, width, height);

        // Prepare image for use as texture source
        transitionImageLayout(tex_image, VK_FORMAT_R8G8B8A8_SRGB, 
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Clean up
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, sb_mem, nullptr);
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                     VkImage& image, VkDeviceMemory& image_mem)
    {
        // Create the tex image
        VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr };
        ici.format          = format;
        ici.tiling          = tiling;
        ici.usage           = usage;
        ici.extent.width    = width;
        ici.extent.height   = height;
        ici.imageType       = VK_IMAGE_TYPE_2D;
        ici.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.sharingMode     = VK_SHARING_MODE_EXCLUSIVE;
        ici.extent.depth    = 1;
        ici.mipLevels       = 1;
        ici.arrayLayers     = 1;
        ici.samples         = VK_SAMPLE_COUNT_1_BIT;
        ici.flags           = 0;
        if (VK_SUCCESS != vkCreateImage(device, &ici, nullptr, &image)) throw std::runtime_error("Failed to to create image");

        // Alloc and bind image memory
        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(device, image, &mem_req);
        VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
        ai.allocationSize = mem_req.size;
        ai.memoryTypeIndex = findMemoryTypeIdx(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (VK_SUCCESS != vkAllocateMemory(device, &ai, nullptr, &image_mem)) throw std::runtime_error("Failed to to allocate image memory");
        vkBindImageMemory(device, image, image_mem, 0);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout in_layout, VkImageLayout out_layout)
    {
        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr };
        barrier.oldLayout = in_layout;
        barrier.newLayout = out_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // for transfering queue family ownership only
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        barrier.srcAccessMask = 0;  // TBD - which ops must precede
        barrier.dstAccessMask = 0;  // TBD - which ops must follow
        VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_NONE;                // Not produced or modified in pipeline
        VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Texture image is consumed in fragment shader

        // We only have 2 transitions to worry about, but this approach wouldn't scale well...
        if (VK_IMAGE_LAYOUT_UNDEFINED == in_layout && VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == out_layout)
        {
            barrier.srcAccessMask = VK_ACCESS_NONE;                 // 
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;   // transfer dest access must follow
            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;          // 
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;             // transfer stage
        } 
        else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == in_layout && VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == out_layout)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;   // transfer complete 
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;      // shader read access
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;             // transfer complete
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;      // fragment shader consumes the image
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        // Submit barrier
        VkCommandBuffer cb = beginOneOffCommandBuffer();
        vkCmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        finishOneOffCommandBuffer(cb);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        VkCommandBuffer cb = beginOneOffCommandBuffer();

        VkBufferImageCopy bic{};
        bic.bufferOffset = 0;
        bic.bufferRowLength = 0;    // Buffer is tightly packed, ie no row alignment padding
        bic.bufferImageHeight = 0;  // Single image in buffer

        bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bic.imageSubresource.layerCount = 1;
        bic.imageSubresource.baseArrayLayer = 0;
        bic.imageSubresource.mipLevel = 0;

        bic.imageOffset = { 0, 0, 0 };
        bic.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(cb, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

        finishOneOffCommandBuffer(cb);
    }

    void createCommandPool()
    {
        QueueFamilies queue_indices = findDeviceQueueFamilies(physical_device);

        VkCommandPoolCreateInfo pool_ci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
        pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;    // Reset buffers individually
        pool_ci.queueFamilyIndex = queue_indices.graphics_family.value();

        if (VK_SUCCESS != vkCreateCommandPool(device, &pool_ci, nullptr, &command_pool))
        {
            throw std::runtime_error("Failed to create command pool");
        }
    }

    VkCommandBuffer createCommandBuffer()
    {
        VkCommandBuffer cb;
        VkCommandBufferAllocateInfo cb_ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
        cb_ai.commandPool = command_pool;
        cb_ai.commandBufferCount = 1;
        cb_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // Submit to queue directly

        if (VK_SUCCESS != vkAllocateCommandBuffers(device, &cb_ai, &cb))
        {
            throw std::runtime_error("Failed to create command buffer");
        }
        return cb;
    }

    // Create a one-time-use gfx command buffer and begin recording
    VkCommandBuffer beginOneOffCommandBuffer()
    {
        VkCommandBuffer cb = createCommandBuffer();

        VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = nullptr;
        if (VK_SUCCESS != vkBeginCommandBuffer(cb, &begin_info))
        {
            throw std::runtime_error("Failure on begin one-off command buffer");
        }
        return cb;
    }

    // Finalize one-time-use gfx command buffer and submit to gfx queue. (Very heavy-weight synchronization via QueueWaitIdle)
    void finishOneOffCommandBuffer(VkCommandBuffer cb)
    {
        vkEndCommandBuffer(cb);
        
        VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        si.signalSemaphoreCount = 0;
        si.waitSemaphoreCount = 0;

        vkQueueSubmit(gfx_queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(gfx_queue);
        vkFreeCommandBuffers(device, command_pool, 1, &cb);
    }

    void recordCommandBuffer(VkCommandBuffer buf, uint32_t image_idx)
    {
        // Init buffer
        VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = nullptr;
        if (VK_SUCCESS != vkBeginCommandBuffer(render_cmd_buf, &begin_info))
        {
            throw std::runtime_error("Failure on begin command buffer recording");
        }

        // Init render pass
        VkClearValue clear = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        VkRenderPassBeginInfo rp = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
        rp.renderPass = render_pass;
        rp.framebuffer = swapchain_framebuffers[image_idx];
        rp.renderArea.offset = { 0, 0 };
        rp.renderArea.extent = swapchain_extent;
        rp.clearValueCount = 1;
        rp.pClearValues = &clear;
        
        vkCmdBeginRenderPass(render_cmd_buf, &rp, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the pipeline
        vkCmdBindPipeline(render_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind the vertex buffer
        VkBuffer vtx_buffers[] = { vertex_buffer };
        VkDeviceSize vb_offsets[] = { 0 };
        vkCmdBindVertexBuffers(render_cmd_buf, 0, 1, vtx_buffers, vb_offsets);

        // Bind the index buffer
        vkCmdBindIndexBuffer(render_cmd_buf, index_buffer, 0, VK_INDEX_TYPE_UINT16);

        // Bind the ubo descriptor
        vkCmdBindDescriptorSets(render_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 
                                0, 1, &descriptor_sets[image_idx], 0, nullptr);

        // Submit a draw call
        vkCmdDrawIndexed(render_cmd_buf, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        // End the render pass and finish recording
        vkCmdEndRenderPass(render_cmd_buf);
        if (VK_SUCCESS != vkEndCommandBuffer(render_cmd_buf))
        {
            throw std::runtime_error("Error ending command buffer recording");
        }
    }

    void createSyncObjects()
    {
        VkSemaphoreCreateInfo sem_ci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr };
        VkFenceCreateInfo fence_ci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
        fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Create as already signaled

        if (VK_SUCCESS != vkCreateSemaphore(device, &sem_ci, nullptr, &sem_image_available) ||
            VK_SUCCESS != vkCreateSemaphore(device, &sem_ci, nullptr, &sem_render_complete) ||
            VK_SUCCESS != vkCreateFence(device, &fence_ci, nullptr, &fence_in_flight))
        {
            throw std::runtime_error("Error creating sync objects");
        }
    }
    
    uint32_t findMemoryTypeIdx(uint32_t type, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties2 mem_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, nullptr };
        vkGetPhysicalDeviceMemoryProperties2(physical_device, &mem_props);

        // Look for type and exact properties match
        for (uint32_t i = 0; i < mem_props.memoryProperties.memoryTypeCount; i++)
        {
            if ((type & (1 << i)) && (props == (mem_props.memoryProperties.memoryTypes[i].propertyFlags & props))) 
                return i;
        }

        // Look for type and any matching property bits
        for (uint32_t i = 0; i < mem_props.memoryProperties.memoryTypeCount; i++)
        {
            if ((type & (1 << i)) && (mem_props.memoryProperties.memoryTypes[i].propertyFlags & props))
                return i;
        }

        throw std::runtime_error("Failed to find compatible physical memory type/properties");
    }

    void createBuffer(VkDeviceSize size, 
                      VkBufferUsageFlags usage, 
                      VkMemoryPropertyFlags props, 
                      VkBuffer& buffer, 
                      VkDeviceMemory &buffer_mem)
    {
        // Create buffer
        VkBufferCreateInfo vb_ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
        vb_ci.size = size;
        vb_ci.usage = usage;
        vb_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // exclusively used by graphics pipe

        if (VK_SUCCESS != vkCreateBuffer(device, &vb_ci, nullptr, &buffer))
        {
            throw std::runtime_error("Error creating buffer");
        }

        // Allocate memory (note that one-off allocations are bad for perf, use a memory pool w/ offsets)
        // Consider https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        VkMemoryRequirements2 mem_req = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr };
        VkBufferMemoryRequirementsInfo2 buf_mem_req = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, nullptr };
        buf_mem_req.buffer = buffer;
        vkGetBufferMemoryRequirements2(device, &buf_mem_req, &mem_req);

        VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
        alloc_info.allocationSize = mem_req.memoryRequirements.size;
        alloc_info.memoryTypeIndex = findMemoryTypeIdx(mem_req.memoryRequirements.memoryTypeBits, props);
        if (VK_SUCCESS != vkAllocateMemory(device, &alloc_info, nullptr, &buffer_mem))
        {
            throw std::runtime_error("Error allocating memory for buffer");
        }

        // Bind memory to buffer
        vkBindBufferMemory(device, buffer, buffer_mem, 0);
    }

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
    {
        VkCommandBuffer cb = beginOneOffCommandBuffer();

        // Copy staging to vtx
        VkBufferCopy2 copy_rgn = { VK_STRUCTURE_TYPE_BUFFER_COPY_2, nullptr };
        copy_rgn.srcOffset = 0;
        copy_rgn.dstOffset = 0;
        copy_rgn.size = size;

        VkCopyBufferInfo2 copy_info = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2, nullptr };
        copy_info.srcBuffer = src;
        copy_info.dstBuffer = dst;
        copy_info.regionCount = 1;
        copy_info.pRegions = &copy_rgn;

        vkCmdCopyBuffer2(cb, &copy_info);

        finishOneOffCommandBuffer(cb);
    }

    void createVertexBuffers()
    {
        VkDeviceSize vb_size = sizeof(vertices[0]) * vertices.size(); // vb size in bytes

        VkBuffer staging = VK_NULL_HANDLE;
        VkDeviceMemory staging_mem = VK_NULL_HANDLE;
        createBuffer(vb_size,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     staging, 
                     staging_mem);

        // Map & fill staging buffer
        void* data;
        vkMapMemory(device, staging_mem, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, vertices.data(), (size_t) vb_size);
        vkUnmapMemory(device, staging_mem);

        // Create the on-device vertex buffer & copy data from staging
        createBuffer(vb_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertex_buffer,
            vertex_buffer_mem);

        copyBuffer(staging, vertex_buffer, vb_size);

        // Clean up
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, staging_mem, nullptr);
    }
    
    void createIndexBuffers()
    {
        VkDeviceSize ib_size = sizeof(indices[0]) * indices.size(); // ib size in bytes

        VkBuffer staging = VK_NULL_HANDLE;
        VkDeviceMemory staging_mem = VK_NULL_HANDLE;
        createBuffer( ib_size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging,
                      staging_mem );

        // Map & fill staging buffer
        void* data;
        vkMapMemory(device, staging_mem, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, indices.data(), (size_t)ib_size);
        vkUnmapMemory(device, staging_mem);

        // Create the on-device index buffer & copy data from staging
        createBuffer(ib_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            index_buffer,
            index_buffer_mem);

        copyBuffer(staging, index_buffer, ib_size);

        // Clean up
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, staging_mem, nullptr);
    }

    void createDescriptorPool()
    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_size.descriptorCount = MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo dp_ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
        dp_ci.poolSizeCount = 1;
        dp_ci.pPoolSizes = &pool_size;
        dp_ci.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (VK_SUCCESS != vkCreateDescriptorPool(device, &dp_ci, nullptr, &descriptor_pool))
        {
            throw std::runtime_error("Error creating descriptor pool");
        }
    }

    void createDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> ds_layouts(MAX_FRAMES_IN_FLIGHT, ubo_desc_layout);
        VkDescriptorSetAllocateInfo ds_ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
        ds_ai.descriptorPool = descriptor_pool;
        ds_ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        ds_ai.pSetLayouts = ds_layouts.data();

        descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        if (VK_SUCCESS != vkAllocateDescriptorSets(device, &ds_ai, descriptor_sets.data()))
        {
            throw std::runtime_error("Error allocating descriptor sets");
        }
        // cleaned up implicitly when pool is destroyed

        // Populate the descriptor sets with the uniform buffers
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkDescriptorBufferInfo bi{};
            bi.buffer = uniform_buffer[i];
            bi.offset = 0;
            bi.range = sizeof(mvp_ubo);
            
            VkWriteDescriptorSet write_info = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
            write_info.dstSet = descriptor_sets[i];
            write_info.dstBinding = 0;
            write_info.dstArrayElement = 0;
            write_info.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write_info.descriptorCount = 1;
            write_info.pBufferInfo = &bi;
            write_info.pImageInfo = nullptr;        // 
            write_info.pTexelBufferView = nullptr;  // 

            vkUpdateDescriptorSets(device, 1, &write_info, 0, nullptr);
        }
    }

    void createUniformBuffers()
    {
        VkDeviceSize ubo_size = sizeof(mvp_ubo);
        uniform_buffer.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffer_memory.resize(MAX_FRAMES_IN_FLIGHT);

        // UBOs are modified frequently, so little to gain from using a staging buffer. Just make them host accessible.
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            createBuffer(ubo_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform_buffer[i], uniform_buffer_memory[i]);
        }
    }

    void updateUniformBuffer(uint32_t idx)
    {
        static auto start_time = std::chrono::high_resolution_clock::now();

        auto cur_time = std::chrono::high_resolution_clock::now();
        auto elapsed_time = std::chrono::duration<float, std::chrono::seconds::period>(cur_time - start_time).count();

        mvp_ubo ubo{};
        // rotate around Z at 90 deg/sec
        ubo.model = glm::rotate(glm::mat4(1.0f), elapsed_time * glm::radians(90.0f), glm::vec3(0.f, 0.f, 1.f));

        // look at origin from 2,2,2
        ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f));

        // 45-deg FOV, z range 0.1 .. 10.0
        ubo.projection = glm::perspectiveRH(glm::radians(45.f), swapchain_extent.width / (float)swapchain_extent.height, 0.1f, 10.f);

        // copy (optimization would be to use push constants instead of map/copy/unmap)
        void* data;
        vkMapMemory(device, uniform_buffer_memory[idx], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(device, uniform_buffer_memory[idx]);
    }

    void populateDebugMessengerCI(VkDebugUtilsMessengerCreateInfoEXT& ci)
    {
        ci = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, nullptr };

        // select all severity above 'Info'
        ci.messageSeverity =    //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        // select all types
        ci.messageType =        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        ci.pfnUserCallback =    debugCallback;
        ci.pUserData =          nullptr;
    }

    void setupDebugMessenger()
    {
        if (!enable_validation) return;

        VkDebugUtilsMessengerCreateInfoEXT info;
        populateDebugMessengerCI(info);

        // load the create function and call it
        VkResult success = VK_ERROR_EXTENSION_NOT_PRESENT;
        auto pCDUM_fxn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (pCDUM_fxn != nullptr) success = pCDUM_fxn(instance, &info, nullptr, &debug_messenger);

        if (VK_SUCCESS != success) throw std::runtime_error("Failed to set up debug messenger.");
    }

    void destroyDebugMessenger()
    {
        if (!enable_validation) return;

        // load the destroy function and call it
        auto pDDUM_fxn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (pDDUM_fxn != nullptr) pDDUM_fxn(instance, debug_messenger, nullptr);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT         messageSeverity,
                                                         VkDebugUtilsMessageTypeFlagsEXT                messageType,
                                                         const VkDebugUtilsMessengerCallbackDataEXT*    pCallbackData,
                                                         void*                                          pUserData) 
    {
        std::cerr << "Validation: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    static void glfwErrorCallback(int code, const char* description)
    {
        std::cerr << std::endl << "GLFW Error " << code << ": " << description << std::endl;
    }

private:
    const uint32_t  window_width = 1200;
    const uint32_t  window_height = 900;
    GLFWwindow*     window;

    bool            frame_buffer_resized = false;

    VkInstance                  instance            = VK_NULL_HANDLE;
    VkSurfaceKHR                surface             = VK_NULL_HANDLE;
    VkPhysicalDevice            physical_device     = VK_NULL_HANDLE;
    VkDevice                    device              = VK_NULL_HANDLE;    // logical device
    VkQueue                     gfx_queue           = VK_NULL_HANDLE;
    VkQueue                     present_queue       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT    debug_messenger     = VK_NULL_HANDLE;
    VkSwapchainKHR              swapchain           = VK_NULL_HANDLE;
    VkSurfaceFormatKHR          swapchain_format;
    VkExtent2D                  swapchain_extent;
    std::vector<VkImage>        swapchain_images;
    std::vector<VkImageView>    swapchain_image_views;
    std::vector<VkFramebuffer>  swapchain_framebuffers;
    VkDescriptorSetLayout       ubo_desc_layout     = VK_NULL_HANDLE;
    VkDescriptorPool            descriptor_pool     = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptor_sets;
    VkPipelineLayout            pipeline_layout     = VK_NULL_HANDLE;
    VkRenderPass                render_pass         = VK_NULL_HANDLE;
    VkPipeline                  pipeline            = VK_NULL_HANDLE;
    VkCommandPool               command_pool        = VK_NULL_HANDLE;
    VkCommandBuffer             render_cmd_buf      = VK_NULL_HANDLE;
    VkBuffer                    vertex_buffer       = VK_NULL_HANDLE;
    VkDeviceMemory              vertex_buffer_mem   = VK_NULL_HANDLE;
    VkBuffer                    index_buffer        = VK_NULL_HANDLE;
    VkDeviceMemory              index_buffer_mem    = VK_NULL_HANDLE;
    std::vector<VkBuffer>       uniform_buffer;
    std::vector<VkDeviceMemory> uniform_buffer_memory;
    VkImage                     tex_image           = VK_NULL_HANDLE;
    VkDeviceMemory              tex_image_mem       = VK_NULL_HANDLE;
    VkImageView                 tex_image_view      = VK_NULL_HANDLE;
    VkSampler                   tex_sampler         = VK_NULL_HANDLE;

    VkSemaphore                 sem_image_available;
    VkSemaphore                 sem_render_complete;
    VkFence                     fence_in_flight;

    // conditional use of validation layers
    const std::vector<const char*> validation_layers = {"VK_LAYER_KHRONOS_validation"};

    // required device extensions
    const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef VALIDATION_ON
    const bool enable_validation = true;
#else
    const bool enable_validation = false;
#endif
};

int main() 
{
    HelloTriangleApplication app;

    try 
    {
        app.run();
    }
    catch (const std::exception& e) 
    {
        std::cerr << std::endl << "EXCEPTION: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}