#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <array>
#include <stdexcept>
#include <vector>

template <size_t MaxFramesInFlight>
class Synchronization {
    VkDevice m_device;

    auto create_fences(VkDevice device) -> void
    {
        static constexpr auto fenceInfo = VkFenceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (auto&& fence : in_flight) {
            if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
                throw std::runtime_error("Failed to create fence");
        }
    }

    auto create_semaphores(VkDevice device, size_t num_images) -> void
    {
        static constexpr auto semaphoreInfo = VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };

        for (auto&& semaphore : image_available) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
                throw std::runtime_error("Failed to create semaphore");
        }

        render_finished.resize(num_images);
        for (auto&& semaphore : render_finished) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
                throw std::runtime_error("Failed to create semaphore");
        }
    }

public:
    std::array<VkSemaphore, MaxFramesInFlight> image_available;
    std::array<VkFence, MaxFramesInFlight> in_flight;
    std::vector<VkSemaphore> render_finished;

    Synchronization() = default;
    Synchronization(VkDevice device, size_t num_images)
        : m_device(device)
        , render_finished(num_images)
    {
        create_semaphores(device, num_images);
        create_fences(device);
    }

    auto destroy() noexcept -> void
    {
        for (auto&& semaphore : image_available) {
            vkDestroySemaphore(m_device, semaphore, nullptr);
        }

        for (auto&& semaphore : render_finished) {
            vkDestroySemaphore(m_device, semaphore, nullptr);
        }

        for (auto&& fence : in_flight) {
            vkDestroyFence(m_device, fence, nullptr);
        }
    }
};