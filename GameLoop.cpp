// Complete through https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion

//#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <algorithm>
#include <fstream>
#include <vector>
#include <set>
#include <optional>

#ifdef _DEBUG
#define VERBOSE_ON
#define VALIDATION_ON
#endif

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
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(window_width, window_height, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() 
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        choosePhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
    }

    void mainLoop() 
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void cleanup() 
    {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyRenderPass(device, render_pass, nullptr);
        for (auto& imageview : swapchain_image_views) vkDestroyImageView(device, imageview, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        destroyDebugMessenger();
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
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

        // Filter on min properties & features here, e.g.
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
        
        return (queue_fam_idx.isComplete() && swap_chain_ok);
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

        // Features (none for now)
        VkPhysicalDeviceFeatures2 dev_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr };

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

    void createImageViews()
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

        VkRenderPassCreateInfo2 render_pass_ci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, nullptr };
        render_pass_ci.attachmentCount = 1;
        render_pass_ci.pAttachments = &attachment;
        render_pass_ci.subpassCount = 1;
        render_pass_ci.pSubpasses = &subpass;

        if (VK_SUCCESS != vkCreateRenderPass2(device, &render_pass_ci, nullptr, &render_pass))
        {
            throw std::runtime_error("Failed to create render pass");
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
        VkPipelineVertexInputStateCreateInfo vtx_in_ci = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr };
        vtx_in_ci.vertexBindingDescriptionCount = 0;
        vtx_in_ci.pVertexBindingDescriptions = nullptr;
        vtx_in_ci.vertexAttributeDescriptionCount = 0;
        vtx_in_ci.pVertexAttributeDescriptions = nullptr;

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
        layout_ci.setLayoutCount = 0;
        layout_ci.pSetLayouts = nullptr;
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
        pipe_ci.pDynamicState = &dyn_ci;
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
    VkPipelineLayout            pipeline_layout     = VK_NULL_HANDLE;
    VkRenderPass                render_pass         = VK_NULL_HANDLE;
    VkPipeline                  pipeline            = VK_NULL_HANDLE;

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