// Complete through https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Physical_devices_and_queue_families

//#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>
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
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(window_width, window_height, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() 
    {
        createInstance();
        setupDebugMessenger();
        choosePhysicalDevice();
        createLogicalDevice();
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
        vkDestroyDevice(device, nullptr);
        destroyDebugMessenger();
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

        auto required_extensions = getRequiredExtensions();

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

    std::vector<const char*> getRequiredExtensions()
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

        bool isComplete()   // minimal required set of queues are present (just gfx, for the moment)
        {
            return graphics_family.has_value();
        }
    };

    bool physDeviceAcceptable(VkPhysicalDevice dev)
    {
        VkPhysicalDeviceProperties2 dev_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, nullptr };
        VkPhysicalDeviceFeatures2   dev_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, nullptr };

        vkGetPhysicalDeviceProperties2(dev, &dev_props);
        vkGetPhysicalDeviceFeatures2(dev, &dev_features);

        // Filter on min properties & features here, e.g.
        if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == dev_props.properties.deviceType &&
            dev_features.features.vertexPipelineStoresAndAtomics)
        {
            bool gpu_ok = true; // ignore this for now, any Vulkan device is ok
        }

        // Check for presence of required queues
        QueueFamilies queue_fam_idx = findDeviceQueueFamilies(dev);
        return queue_fam_idx.isComplete();
    }

    QueueFamilies findDeviceQueueFamilies(VkPhysicalDevice dev)
    {
        QueueFamilies family_indices;
        uint32_t fam_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(dev, &fam_count, nullptr);
        if (fam_count > 0)
        {
            std::vector<VkQueueFamilyProperties2> fam_props(fam_count);
            for (auto& fam : fam_props) fam = { VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, nullptr };
            vkGetPhysicalDeviceQueueFamilyProperties2(dev, &fam_count, fam_props.data());

            // 
            int idx = 0;
            for (const auto& fam : fam_props)
            {
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)       family_indices.graphics_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT)        family_indices.compute_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT)       family_indices.transfer_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) family_indices.sparse_binding_family = idx;
                if (fam.queueFamilyProperties.queueFlags & VK_QUEUE_PROTECTED_BIT)      family_indices.protected_family = idx;
                idx++;
            }
        }

        return family_indices;
    }

    void createLogicalDevice()
    {
        QueueFamilies queue_idx = findDeviceQueueFamilies(physical_device);
        float queue_priority = 1.0f;

        // Queues
        VkDeviceQueueCreateInfo dev_q_ci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr };
        dev_q_ci.queueFamilyIndex = queue_idx.graphics_family.value();
        dev_q_ci.queueCount = 1;
        dev_q_ci.pQueuePriorities = &queue_priority;    // highest priority (range 0.0...1.0)

        // Features (none for now)
        VkPhysicalDeviceFeatures2 dev_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr };

        // Logical Device
        VkDeviceCreateInfo dev_ci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr };
        dev_ci.pQueueCreateInfos = &dev_q_ci;
        dev_ci.queueCreateInfoCount = 1;
        dev_ci.pEnabledFeatures = &dev_features.features;
        
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

private:
    const uint32_t  window_width = 1200;
    const uint32_t  window_height = 900;
    GLFWwindow*     window;

    VkInstance                  instance;
    VkPhysicalDevice            physical_device = VK_NULL_HANDLE;
    VkDevice                    device = VK_NULL_HANDLE;    // logical device
    VkQueue                     gfx_queue = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT    debug_messenger;

    // conditional use of validation layers
    const std::vector<const char*> validation_layers = {"VK_LAYER_KHRONOS_validation"};

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