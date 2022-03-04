// Complete through https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Physical_devices_and_queue_families

//#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>

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

    bool physDeviceAcceptable(VkPhysicalDevice dev)
    {
        VkPhysicalDeviceProperties2 dev_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, nullptr };
        VkPhysicalDeviceFeatures2   dev_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, nullptr };

        vkGetPhysicalDeviceProperties2(dev, &dev_props);
        vkGetPhysicalDeviceFeatures2(dev, &dev_features);

        // Filter on min properties & features here
        return true; // for now, any Vulkan device is ok
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