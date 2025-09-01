#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <optional>
#include <ranges>
#include <vector>

#include "Utils.h"

#include "OpenType/Defines.h" // for enumerate() support on clang

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        uint32_t num_families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &num_families, nullptr);

        auto families = std::vector<VkQueueFamilyProperties>(num_families);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &num_families, families.data());

        for (auto&& [i, family] : enumerate(families)) {
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics = i;
            }

            auto present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

            if (present_support)
                present = i;

            if (is_complete())
                break;
        }
    }

    [[nodiscard]] auto as_array() const noexcept -> std::array<uint32_t, 2>
    {
        return { *graphics, *present };
    }

    [[nodiscard]] auto is_complete() const noexcept -> bool
    {
        return graphics.has_value() && present.has_value();
    }
};
