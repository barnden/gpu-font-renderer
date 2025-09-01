#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <cstring>
#include <initializer_list>
#include <iostream>
#include <print>
#include <set>
#include <vector>

#include "Renderer/Vulkan/QueueFamilyIndices.h"
#include "Renderer/Vulkan/Utils.h"

class Application {
protected:
    static constexpr std::initializer_list<char const*> validation_layers {
        "VK_LAYER_KHRONOS_validation"
    };

    static constexpr std::initializer_list<char const*> device_extensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
    };

#ifdef NDEBUG
    static constexpr bool use_validation_layers = false;
#else
    static constexpr bool use_validation_layers = true;
#endif

    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debug_messenger;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    virtual auto create_window() -> void = 0;
    virtual auto init_vulkan() -> void
    {
        create_instance();

        if constexpr (use_validation_layers) {
            setup_debug();
        }
    }
    virtual auto loop() -> void = 0;

    template <typename FunctionEXT, CompileTimeString FunctionEXTName, typename... Args>
        requires std::is_convertible_v<typename return_type<FunctionEXT>::type, VkResult>
    auto call_vulkan_extension(Args... args) -> VkResult
    {
        auto func = reinterpret_cast<FunctionEXT>(vkGetInstanceProcAddr(m_instance, FunctionEXTName.data));

        if (func == nullptr)
            return VK_ERROR_EXTENSION_NOT_PRESENT;

        return func(std::forward<Args>(args)...);
    }

    template <typename FunctionEXT, CompileTimeString FunctionEXTName, typename... Args>
        requires std::is_same_v<typename return_type<FunctionEXT>::type, void>
    auto call_vulkan_extension(Args... args) -> void
    {
        auto func = reinterpret_cast<FunctionEXT>(vkGetInstanceProcAddr(m_instance, FunctionEXTName.data));

        if (func == nullptr)
            return;

        func(std::forward<Args>(args)...);
    }

    auto has_validation_layers() -> bool
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (auto&& layerName : validation_layers) {
            bool found = false;

            for (auto&& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;
        }

        return true;
    }

    auto create_instance() -> void
    {
        if constexpr (use_validation_layers) {
            if (!has_validation_layers()) {
                throw std::runtime_error("Requested validation layers unavailable.");
            }
        }

        auto app_info = VkApplicationInfo {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_4,
        };

        auto extensions = get_required_extensions();
        auto flags = VkInstanceCreateFlags {};

#ifdef __APPLE__
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        auto instance_info = VkInstanceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        auto debug_info = VkDebugUtilsMessengerCreateInfoEXT {};
        if constexpr (use_validation_layers) {
            instance_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            instance_info.ppEnabledLayerNames = validation_layers.begin();

            debug_info = generate_DebugUtilsMessengerCreateInfo();
            instance_info.pNext = &debug_info;
        }

        auto result = vkCreateInstance(&instance_info, nullptr, &m_instance);
        if (result != VK_SUCCESS) {
            if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
                throw std::runtime_error("vkCreateInstance(): VK_ERROR_INCOMPATIBLE_DRIVER");
            }

            throw std::runtime_error("vkCreateInstance()");
        }
    }

    auto get_required_extensions() -> std::vector<char const*>
    {
        uint32_t glfwExtensionCount = 0;
        char const** glfwExtensions = nullptr;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        auto extensions = std::vector<char const*>(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if constexpr (use_validation_layers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

#ifdef __APPLE__
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

        return extensions;
    }

    auto has_device_extension_support(VkPhysicalDevice device) const -> bool
    {
        uint32_t num_extensions;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, nullptr);

        auto available_extensions = std::vector<VkExtensionProperties>(num_extensions);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, available_extensions.data());

        auto required_extensions = std::set<std::string>(device_extensions.begin(), device_extensions.end());

        for (auto&& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    virtual auto is_suitable(VkPhysicalDevice device) const -> bool = 0;

    [[nodiscard]] auto get_physical_device() const -> VkPhysicalDevice
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("No suitable GPUs found.");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for (auto&& device : devices) {
            if (is_suitable(device)) {
                return device;
            }
        }

        throw std::runtime_error("Failed to find suitable GPU.");
    }

    auto create_logical_device(VkSurfaceKHR surface,
                               VkQueue& graphics,
                               VkQueue& present) -> VkDevice
    {
        VkDevice device;
        auto indices = QueueFamilyIndices(m_physical_device, surface);
        auto queue_create_infos = std::vector<VkDeviceQueueCreateInfo> {};
        auto families = std::set<uint32_t> {
            *indices.graphics,
            *indices.present
        };

        auto const queue_priority = 1.0f;
        for (auto family : families) {
            queue_create_infos.push_back({
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = family,
                .queueCount = 1,
                .pQueuePriorities = &queue_priority,
            });
        }

        auto extensions = std::vector<char const*> {
            device_extensions.begin(),
            device_extensions.end()
        };
        auto device_features = VkPhysicalDeviceFeatures {};
        auto create_info = VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
            .pQueueCreateInfos = queue_create_infos.data(),
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
            .pEnabledFeatures = &device_features,
        };

        if constexpr (use_validation_layers) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            create_info.ppEnabledLayerNames = validation_layers.begin();
        }

        if (vkCreateDevice(m_physical_device, &create_info, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        vkGetDeviceQueue(device, *indices.graphics, 0, &graphics);
        vkGetDeviceQueue(device, *indices.present, 0, &present);

        return device;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        (void)messageType;
        (void)pUserData;

        std::string prefix = "";
        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            prefix = "LOG";
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            prefix = "INFO";
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            prefix = "WARN";
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        default:
            prefix = "ERROR";
        }

        std::println(std::cerr, "[{}] {}", prefix, pCallbackData->pMessage);

        return VK_FALSE;
    }

    auto generate_DebugUtilsMessengerCreateInfo() -> VkDebugUtilsMessengerCreateInfoEXT
    {
        return {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr,
        };
    }

    void setup_debug()
    {
        auto create_info = generate_DebugUtilsMessengerCreateInfo();
        if (call_vulkan_extension<PFN_vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessengerEXT">(
                m_instance, &create_info, nullptr, &m_debug_messenger)) {
            throw std::runtime_error("Failed to create debug messenger");
        }
    }

public:
    virtual auto run() -> void
    {
        create_window();
        init_vulkan();
        loop();
    }
};
