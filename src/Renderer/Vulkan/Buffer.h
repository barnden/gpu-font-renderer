#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <cstring>
#include <stdexcept>

class Buffer {
public:
    VkDevice m_device;
    VkDeviceSize m_size;
    VkBuffer m_buffer {};
    VkDeviceMemory m_memory {};

    [[nodiscard]] auto find_memory_type(VkPhysicalDevice physical_device,
                                        uint32_t type_filter,
                                        VkMemoryPropertyFlags flags) -> uint32_t
    {
        auto memory_properties = VkPhysicalDeviceMemoryProperties {};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

        for (auto i = 0uz; i < memory_properties.memoryTypeCount; i++) {
            if ((type_filter & (1 << i)) == 0)
                continue;

            if ((memory_properties.memoryTypes[i].propertyFlags & flags) != flags)
                continue;

            return i;
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }

public:
    Buffer() = default;
    Buffer(VkPhysicalDevice physical_device,
           VkDevice device,
           VkDeviceSize size,
           VkBufferUsageFlags usage,
           VkMemoryPropertyFlags properties)
        : m_device(device)
        , m_size(size)
    {
        auto buffer_info = VkBufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        if (vkCreateBuffer(device, &buffer_info, nullptr, &m_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer");
        }

        auto memory_requirements = VkMemoryRequirements {};
        vkGetBufferMemoryRequirements(device, m_buffer, &memory_requirements);

        auto alloc_info = VkMemoryAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = find_memory_type(physical_device, memory_requirements.memoryTypeBits, properties)
        };

        if (vkAllocateMemory(device, &alloc_info, nullptr, &m_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to malloc vertex buffer");
        }

        vkBindBufferMemory(device, m_buffer, m_memory, 0);
    }

    [[nodiscard]] auto get() noexcept -> VkBuffer&
    {
        return m_buffer;
    }

    [[nodiscard]] auto get() const noexcept -> VkBuffer const&
    {
        return m_buffer;
    }

    [[nodiscard]] auto get_memory() const noexcept -> VkDeviceMemory const&
    {
        return m_memory;
    }

    [[nodiscard]] auto size() const noexcept -> VkDeviceSize
    {
        return m_size;
    }

    virtual auto copy(void* src) -> void
    {
        void* dst;
        vkMapMemory(m_device, m_memory, 0, m_size, 0, &dst);
        memcpy(dst, src, m_size);
        vkUnmapMemory(m_device, m_memory);
    }

    virtual auto copy(Buffer const& src,
                      VkCommandPool const& command_pool,
                      VkQueue const& queue) -> void
    {
        auto alloc_info = VkCommandBufferAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        auto command_buffer = VkCommandBuffer {};
        vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);

        auto begin_info = VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        vkBeginCommandBuffer(command_buffer, &begin_info);

        auto copy_region = VkBufferCopy {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = m_size,
        };

        vkCmdCopyBuffer(command_buffer, src.m_buffer, m_buffer, 1, &copy_region);
        vkEndCommandBuffer(command_buffer);

        auto submit_info = VkSubmitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
    }

    virtual auto destroy() -> void
    {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        vkFreeMemory(m_device, m_memory, nullptr);
    }
};

class StagedBuffer {
    Buffer m_host_buffer;
    Buffer m_device_buffer;

public:
    StagedBuffer() = default;
    StagedBuffer(VkPhysicalDevice physical_device,
                 VkDevice device,
                 VkDeviceSize size,
                 VkBufferUsageFlags host_flags,
                 VkBufferUsageFlags device_flags,
                 VkMemoryPropertyFlags host_properties,
                 VkMemoryPropertyFlags device_properties)
    {
        m_host_buffer = Buffer(physical_device, device, size, host_flags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host_properties);
        m_device_buffer = Buffer(physical_device, device, size, device_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, device_properties);
    }

    [[nodiscard]] auto host_buffer() const noexcept -> Buffer const&
    {
        return m_host_buffer;
    }

    [[nodiscard]] auto device_buffer() const noexcept -> Buffer const&
    {
        return m_device_buffer;
    }

    [[nodiscard]] auto host_buffer() noexcept -> Buffer&
    {
        return m_host_buffer;
    }

    [[nodiscard]] auto device_buffer() noexcept -> Buffer&
    {
        return m_device_buffer;
    }

    template <typename T>
    void copy_host(T* src_data)
    {
        m_host_buffer.copy(src_data);
    }

    void copy_device(VkCommandPool command_pool, VkQueue queue)
    {
        m_device_buffer.copy(m_host_buffer, command_pool, queue);
    }

    template <typename T>
    void copy(T* src_data, VkCommandPool command_pool, VkQueue queue)
    {
        copy_host(src_data);
        copy_device(command_pool, queue);
    }

    auto destroy() -> void
    {
        m_device_buffer.destroy();
        m_host_buffer.destroy();
    }
};

class PersistentBuffer : public Buffer {
    void* m_map;

public:
    PersistentBuffer() = default;
    PersistentBuffer(VkPhysicalDevice physical_device,
                     VkDevice device,
                     VkDeviceSize size,
                     VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties)
        : Buffer(physical_device, device, size, usage, properties)
    {
        vkMapMemory(device, m_memory, 0, m_size, 0, &m_map);
    }

    virtual auto copy(void* src) -> void override
    {
        memcpy(m_map, src, m_size);
    }

    virtual auto copy(Buffer const&,
                      VkCommandPool const&,
                      VkQueue const&) -> void override
    {
        // FIXME: ASSERT_NOT_REACHED
    }

    virtual auto destroy() -> void override
    {
        vkUnmapMemory(m_device, m_memory);
        Buffer::destroy();
    }
};
