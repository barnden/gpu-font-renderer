#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <stdexcept>

class CommandPool {
    VkDevice m_device;
    VkCommandPool m_pool;

public:
    CommandPool() = default;
    CommandPool(VkDevice device,
                VkCommandPoolCreateFlags flags,
                uint32_t family)
        : m_device(device)
    {
        auto info = VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .queueFamilyIndex = family,
        };

        if (vkCreateCommandPool(device, &info, nullptr, &m_pool) != VK_SUCCESS) {
            throw std::runtime_error("CommandPool: Creation failed.");
        }
    }

    [[nodiscard]] auto get() const noexcept -> VkCommandPool const&
    {
        return m_pool;
    }

    [[nodiscard]] auto get() noexcept -> VkCommandPool&
    {
        return m_pool;
    }

    auto destroy() noexcept -> void
    {
        vkDestroyCommandPool(m_device, m_pool, nullptr);
    }
};