#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "Renderer/Vulkan/QueueFamilyIndices.h"

struct SwapChainDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;

    SwapChainDetails(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            present_modes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, present_modes.data());
        }
    }

    auto get_format() -> VkSurfaceFormatKHR
    {
        for (auto&& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        return formats[0];
    }

    auto get_present_mode() -> VkPresentModeKHR
    {
        for (auto&& mode : present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return mode;
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    auto get_extent(GLFWwindow* window) -> VkExtent2D
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        auto actual_extent = VkExtent2D {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        };

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }
};

class SwapChain {
    VkDevice m_device;
    VkPhysicalDevice m_physical_device;
    VkSurfaceKHR m_surface;

public:
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat format;
    VkExtent2D extent;

    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;

    SwapChain() = default;
    SwapChain(GLFWwindow* window,
              VkPhysicalDevice physical_device,
              VkDevice device,
              VkSurfaceKHR surface)
        : m_device(device)
        , m_physical_device(physical_device)
        , m_surface(surface)
    {
        create(window);
    }

    auto create(GLFWwindow* window) -> void
    {
        auto details = SwapChainDetails(m_physical_device, m_surface);
        auto surface_format = details.get_format();
        auto present_mode = details.get_present_mode();
        extent = details.get_extent(window);
        auto image_count = details.capabilities.minImageCount + 1;

        if (details.capabilities.maxImageCount > 0)
            image_count = std::min(image_count, details.capabilities.maxImageCount);

        auto create_info = VkSwapchainCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = m_surface,
            .minImageCount = image_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = details.capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = present_mode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE
        };

        auto indices = QueueFamilyIndices(m_physical_device, m_surface);
        auto arr = indices.as_array();

        if (indices.graphics != indices.present) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = arr.data();
        }

        if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &handle) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain");
        }

        vkGetSwapchainImagesKHR(m_device, handle, &image_count, nullptr);
        images.resize(image_count);
        vkGetSwapchainImagesKHR(m_device, handle, &image_count, images.data());

        format = surface_format.format;
        create_image_views();
    }

    auto create_image_views() -> void
    {
        auto create_info = VkImageViewCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = 0,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        views.resize(images.size());
        for (auto&& [image, view] : std::views::zip(images, views)) {
            create_info.image = image;

            if (vkCreateImageView(m_device, &create_info, nullptr, &view) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views");
            }
        }
    }

    auto create_framebuffers(VkRenderPass render_pass) -> void
    {
        framebuffers.resize(views.size());

        auto framebuffer_info = VkFramebufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = render_pass,
            .attachmentCount = 0,
            .pAttachments = nullptr,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        for (auto&& [view, framebuffer] : std::views::zip(views, framebuffers)) {
            auto attachments = std::array<VkImageView, 1> { view };

            framebuffer_info.attachmentCount = attachments.size();
            framebuffer_info.pAttachments = attachments.data();

            if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer");
            }
        }
    }

    auto recreate(GLFWwindow* window, VkRenderPass render_pass) -> void
    {
        int width = 0;
        int height = 0;

        do {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        } while (width == 0 || height == 0);

        vkDeviceWaitIdle(m_device);

        destroy();
        create(window);
        create_framebuffers(render_pass);
    }

    auto destroy() noexcept -> void
    {
        for (auto&& framebuffer : framebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        for (auto&& view : views) {
            vkDestroyImageView(m_device, view, nullptr);
        }

        vkDestroySwapchainKHR(m_device, handle, nullptr);
    }
};
