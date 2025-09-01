#pragma once

#include <type_traits>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <bitset>

#include <ranges>
#include <stdexcept>
#include <vector>

#include "Utils.h"
#include "Renderer/Vulkan/Buffer.h"

class GraphicsPipeline {
    friend class GraphicsPipelineBuilder;

    VkDevice m_device;
    VkPipeline m_pipeline;
    VkPipelineLayout m_layout;
    VkDescriptorSetLayout m_descriptor_set_layout;
    VkDescriptorPool m_uniform_pool;
    VkDescriptorPool m_storage_pool;
    std::vector<VkDescriptorSet> m_descriptor_sets;

public:
    GraphicsPipeline() = default;
    GraphicsPipeline(VkDevice device)
        : m_device(device) { };

    auto update_descriptor_sets(auto& buffers)
    {
        using std::views::zip;
        for (auto&& [descriptor_set, buffer] : zip(m_descriptor_sets, buffers)) {
            auto buffer_info = VkDescriptorBufferInfo {
                .buffer = buffer.get(),
                .offset = 0,
                .range = buffer.size()
            };

            auto descriptor_write = VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptor_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &buffer_info,
                .pTexelBufferView = nullptr
            };

            vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
        }
    }

    auto bind_descriptors(VkCommandBuffer buffer, size_t frame)
    {
        vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &m_descriptor_sets[frame], 0, nullptr);
    }

    [[nodiscard]] auto get() const noexcept -> VkPipeline
    {
        return m_pipeline;
    }

    [[nodiscard]] auto layout() const noexcept -> VkPipelineLayout
    {
        return m_layout;
    }

    auto destroy() const -> void
    {
        vkDestroyDescriptorPool(m_device, m_uniform_pool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    }
};

class RasterizerBuilder {
    VkPipelineRasterizationStateCreateInfo m_rasterizer {};

public:
    RasterizerBuilder()
    {
        m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_rasterizer.lineWidth = 1.0f;
    }

    auto clamp_depth(VkBool32 enabled)
    {
        m_rasterizer.depthClampEnable = enabled;

        return *this;
    }

    auto depth_bias(float constant_factor, float clamp, float slope_factor)
    {
        m_rasterizer.depthBiasEnable = true;
        m_rasterizer.depthBiasConstantFactor = constant_factor;
        m_rasterizer.depthBiasClamp = clamp;
        m_rasterizer.depthBiasSlopeFactor = slope_factor;

        return *this;
    }

    auto polygon_mode(VkPolygonMode mode)
    {
        m_rasterizer.polygonMode = mode;

        return *this;
    }

    auto cull_mode(VkCullModeFlags mode)
    {
        m_rasterizer.cullMode = mode;

        return *this;
    }

    auto front_face(VkFrontFace face)
    {
        m_rasterizer.frontFace = face;

        return *this;
    }

    auto line_width(float width)
    {
        m_rasterizer.lineWidth = width;

        return *this;
    }

    auto finish() -> VkPipelineRasterizationStateCreateInfo
    {
        return m_rasterizer;
    }
};

class GraphicsPipelineBuilder {
    GraphicsPipeline m_pipeline;
    std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages {};
    std::vector<VkShaderModule> m_shader_modules {};
    struct {
        std::vector<VkDynamicState> states;
        VkPipelineDynamicStateCreateInfo info;
    } m_dynamic {};

    struct {
        std::vector<VkVertexInputBindingDescription> bindings {};
        std::vector<VkVertexInputAttributeDescription> attributes {};

        [[nodiscard]] auto input_info() const noexcept -> VkPipelineVertexInputStateCreateInfo
        {
            return {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size()),
                .pVertexBindingDescriptions = bindings.data(),
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
                .pVertexAttributeDescriptions = attributes.data(),
            };
        }
    } m_vertex {};
    VkPipelineInputAssemblyStateCreateInfo m_input_assembly {};
    VkPipelineViewportStateCreateInfo m_viewport_state {};
    VkPipelineRasterizationStateCreateInfo m_rasterizer {};
    std::vector<VkDescriptorSetLayoutBinding> m_layout_bindings {};
    VkRenderPass m_render_pass {};

    VkPipelineMultisampleStateCreateInfo m_multisampling {};
    VkPipelineColorBlendAttachmentState m_color_blend_attachment {};
    VkPipelineColorBlendStateCreateInfo m_color_blending {};

    enum RequiredSteps {
        SET_DEVICE,
        SET_ASSEMBLY,
        ATTACH_SHADERS,
        VERTEX_ATTRIBUTES,
        ADD_RASTERIZER,
        ADD_PIPELINE_LAYOUT,
        RequiredSteps_MAX
    };

    std::bitset<RequiredSteps::RequiredSteps_MAX> m_completed;

    auto create_shader_module(std::vector<char> const& code) -> VkShaderModule
    {
        auto create_info = VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = code.size(),
            .pCode = reinterpret_cast<uint32_t const*>(code.data()),
        };

        VkShaderModule module;
        if (vkCreateShaderModule(m_pipeline.m_device, &create_info, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module.");
        }

        return module;
    }

    void create_descriptor_sets(auto size)
    {
        m_pipeline.m_descriptor_sets.resize(size);
        auto layouts = std::vector<VkDescriptorSetLayout>(size, m_pipeline.m_descriptor_set_layout);
        auto alloc_info = VkDescriptorSetAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_pipeline.m_uniform_pool,
            .descriptorSetCount = static_cast<uint32_t>(size),
            .pSetLayouts = layouts.data()
        };

        if (vkAllocateDescriptorSets(m_pipeline.m_device, &alloc_info, m_pipeline.m_descriptor_sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set");
        }
    }

    void create_descriptor_pool(auto size)
    {
        auto pool_size = VkDescriptorPoolSize {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<uint32_t>(size)
        };

        auto pool_info = VkDescriptorPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = static_cast<uint32_t>(size),
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size
        };

        if (vkCreateDescriptorPool(m_pipeline.m_device, &pool_info, nullptr, &m_pipeline.m_uniform_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool");
        }
    }

    void create_descriptor_set_layout(std::vector<VkDescriptorSetLayoutBinding> const& layout_bindings)
    {
        auto layout_info = VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
            .pBindings = layout_bindings.data(),
        };

        if (vkCreateDescriptorSetLayout(m_pipeline.m_device, &layout_info, nullptr, &m_pipeline.m_descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout");
        }
    }

public:
    GraphicsPipelineBuilder() { };

    auto set_device(VkDevice device)
    {
        m_pipeline.m_device = device;

        m_completed[RequiredSteps::SET_DEVICE] = true;

        return *this;
    }

    auto set_assembly(VkPrimitiveTopology topology,
                      VkBool32 primitiveRestartEnable)
    {
        m_input_assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = topology,
            .primitiveRestartEnable = primitiveRestartEnable,
        };
        m_completed[RequiredSteps::SET_ASSEMBLY] = true;
        return *this;
    }

    auto set_render_pass(VkRenderPass render_pass)
    {
        m_render_pass = render_pass;

        return *this;
    }

    template <typename... Args>
        requires std::conjunction_v<std::is_same<VkDynamicState, Args>...>
    auto set_dynamic_states(Args... states)
    {
        (m_dynamic.states.push_back(states), ...);

        m_dynamic.info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(m_dynamic.states.size()),
            .pDynamicStates = m_dynamic.states.data(),
        };

        return *this;
    }

    auto set_dynamic_states(std::initializer_list<VkDynamicState> states)
    {
        m_dynamic.states = { states.begin(), states.end() };
        m_dynamic.info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(m_dynamic.states.size()),
            .pDynamicStates = m_dynamic.states.data(),
        };

        return *this;
    }

    auto set_viewport_state(uint32_t num_viewports,
                            uint32_t num_scissors,
                            VkViewport* viewports = nullptr,
                            VkRect2D* scissors = nullptr)
    {
        m_viewport_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = num_viewports,
            .pViewports = viewports,
            .scissorCount = num_scissors,
            .pScissors = scissors,
        };

        return *this;
    }

    auto add_rasterizer(VkPipelineRasterizationStateCreateInfo rasterizer)
    {
        m_rasterizer = rasterizer;

        m_completed[RequiredSteps::ADD_RASTERIZER] = true;
        return *this;
    }

    template <typename Container>
    auto add_vertex_attributes(VkVertexInputBindingDescription binding_description,
                               Container attribute_description)
    {
        m_vertex.bindings.push_back(binding_description);
        std::copy(std::begin(attribute_description), std::end(attribute_description), std::back_inserter(m_vertex.attributes));

        m_completed[RequiredSteps::VERTEX_ATTRIBUTES] = true;
        return *this;
    }

    auto attach_shader(std::string const& shader_path,
                       VkShaderStageFlagBits shader_stage)
    {
        if (!m_completed[RequiredSteps::SET_DEVICE])
            throw std::runtime_error("GraphicsPipelineBuilder: required device");

        auto code = read_file(shader_path);
        auto module = create_shader_module(code);

        auto shader_stage_info = VkPipelineShaderStageCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = shader_stage,
            .module = module,
            .pName = "main",
            .pSpecializationInfo = {},
        };

        m_shader_modules.push_back(module);
        m_shader_stages.push_back(shader_stage_info);

        m_completed[RequiredSteps::ATTACH_SHADERS] = true;

        return *this;
    }

    auto add_layout_binding(VkDescriptorSetLayoutBinding layout_binding)
    {
        m_layout_bindings.push_back(layout_binding);

        return *this;
    }

    auto add_layout_bindings(std::vector<VkDescriptorSetLayoutBinding> const& layout_bindings)
    {
        if (!m_completed[RequiredSteps::SET_DEVICE])
            throw std::runtime_error("GraphicsPipelineBuilder: required device");

        std::copy(layout_bindings.begin(), layout_bindings.end(),
                  std::back_inserter(m_layout_bindings));

        return *this;
    }

    auto add_storage_buffer(uint32_t binding,
                            uint32_t descriptorCount,
                            VkShaderStageFlags stageFlags,
                            VkSampler const* pImmutableSamplers = nullptr)
    {
        m_layout_bindings.push_back({
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptorCount,
            .stageFlags = stageFlags,
            .pImmutableSamplers = pImmutableSamplers,
        });

        return *this;
    }

    auto attach_storage_buffers(auto& buffers, size_t pool_size)
    {
        if (!m_completed[RequiredSteps::SET_DEVICE])
            throw std::runtime_error("GraphicsPipelineBuilder: required device");

        if (m_layout_bindings.empty())
            throw std::runtime_error("GraphicsPipelineBuilder: attempted to attach uniform buffer with no bindings");

        create_descriptor_set_layout(m_layout_bindings);
        create_descriptor_pool(pool_size);
        create_descriptor_sets(pool_size);

        m_pipeline.update_descriptor_sets(buffers);

        return *this;
    }

    auto add_uniform(uint32_t binding,
                     uint32_t descriptorCount,
                     VkShaderStageFlags stageFlags,
                     VkSampler const* pImmutableSamplers = nullptr)
    {
        m_layout_bindings.push_back({
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = descriptorCount,
            .stageFlags = stageFlags,
            .pImmutableSamplers = pImmutableSamplers,
        });

        return *this;
    }

    auto attach_uniform_buffers(auto& buffers, size_t pool_size)
    {
        if (!m_completed[RequiredSteps::SET_DEVICE])
            throw std::runtime_error("GraphicsPipelineBuilder: required device");

        if (m_layout_bindings.empty())
            throw std::runtime_error("GraphicsPipelineBuilder: attempted to attach uniform buffer with no bindings");

        create_descriptor_set_layout(m_layout_bindings);
        create_descriptor_pool(pool_size);
        create_descriptor_sets(pool_size);

        m_pipeline.update_descriptor_sets(buffers);

        return *this;
    }

    auto add_pipeline_layout()
    {
        if (!m_completed[RequiredSteps::SET_DEVICE])
            throw std::runtime_error("GraphicsPipelineBuilder: required device");

        auto layout_info = VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &m_pipeline.m_descriptor_set_layout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };

        if (vkCreatePipelineLayout(m_pipeline.m_device, &layout_info, nullptr, &m_pipeline.m_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        m_completed[RequiredSteps::ADD_PIPELINE_LAYOUT] = true;

        return *this;
    }

    class RasterizerBuilderWrapper {
        GraphicsPipelineBuilder* m_pipeline_builder;
        RasterizerBuilder m_rasterizer {};

        friend class GraphicsPipelineBuilder;
        RasterizerBuilderWrapper(GraphicsPipelineBuilder* builder)
            : m_pipeline_builder(builder) { };

    public:
        auto clamp_depth(VkBool32 enabled)
        {
            m_rasterizer.clamp_depth(enabled);
            return *this;
        }
        auto depth_bias(float constant_factor, float clamp, float slope_factor)
        {
            m_rasterizer.depth_bias(constant_factor, clamp, slope_factor);
            return *this;
        }
        auto polygon_mode(VkPolygonMode mode)
        {
            m_rasterizer.polygon_mode(mode);
            return *this;
        }
        auto cull_mode(VkCullModeFlags mode)
        {
            m_rasterizer.cull_mode(mode);
            return *this;
        }
        auto front_face(VkFrontFace face)
        {
            m_rasterizer.front_face(face);
            return *this;
        }
        auto line_width(float width)
        {
            m_rasterizer.line_width(width);
            return *this;
        }
        auto finish()
        {
            m_pipeline_builder->add_rasterizer(m_rasterizer.finish());
            return *m_pipeline_builder;
        }
    };

    auto configure_rasterizer()
    {
        return RasterizerBuilderWrapper(this);
    }

    class ColorBlendingBuilder {
        GraphicsPipelineBuilder* m_pipeline_builder;

        ColorBlendingBuilder(GraphicsPipelineBuilder* builder)
            : m_pipeline_builder(builder)
        {
            m_pipeline_builder->m_color_blend_attachment.blendEnable = VK_TRUE;
            m_pipeline_builder->m_color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            m_pipeline_builder->m_color_blending.attachmentCount = 1;
            m_pipeline_builder->m_color_blending.pAttachments = &m_pipeline_builder->m_color_blend_attachment;
        }

        friend class GraphicsPipelineBuilder;

    public:
        auto source_blend(VkBlendFactor color_blend,
                          VkBlendFactor alpha_blend)
        {
            m_pipeline_builder->m_color_blend_attachment.srcColorBlendFactor = color_blend;
            m_pipeline_builder->m_color_blend_attachment.srcAlphaBlendFactor = alpha_blend;

            return *this;
        }

        auto destination_blend(VkBlendFactor color_blend,
                               VkBlendFactor alpha_blend)
        {
            m_pipeline_builder->m_color_blend_attachment.dstColorBlendFactor = color_blend;
            m_pipeline_builder->m_color_blend_attachment.dstAlphaBlendFactor = alpha_blend;

            return *this;
        }

        auto blend_op(VkBlendOp color_op,
                      VkBlendOp alpha_op)
        {
            m_pipeline_builder->m_color_blend_attachment.colorBlendOp = color_op;
            m_pipeline_builder->m_color_blend_attachment.alphaBlendOp = alpha_op;

            return *this;
        }

        template <typename... Args>
            requires std::conjunction_v<std::is_same<VkColorComponentFlagBits, Args>...>
        auto color_write_mask(Args... args)
        {
            m_pipeline_builder->m_color_blend_attachment.colorWriteMask = (args | ...);

            return *this;
        }

        auto color_write_mask(VkColorComponentFlags flags)
        {
            m_pipeline_builder->m_color_blend_attachment.colorWriteMask = flags;

            return *this;
        }

        auto logic_op(VkLogicOp logic_op)
        {
            m_pipeline_builder->m_color_blending.logicOpEnable = true;
            m_pipeline_builder->m_color_blending.logicOp = logic_op;

            return *this;
        }

        auto blend_constants(float r, float g, float b, float a)
        {
            m_pipeline_builder->m_color_blending.blendConstants[0] = r;
            m_pipeline_builder->m_color_blending.blendConstants[1] = g;
            m_pipeline_builder->m_color_blending.blendConstants[2] = b;
            m_pipeline_builder->m_color_blending.blendConstants[3] = a;

            return *this;
        }

        auto finish()
        {
            return *m_pipeline_builder;
        }
    };

    auto configure_color_blending()
    {
        return ColorBlendingBuilder(this);
    }

    class MultisamplingBuilder {
        GraphicsPipelineBuilder* m_pipeline_builder;

        MultisamplingBuilder(GraphicsPipelineBuilder* builder)
            : m_pipeline_builder(builder) { };
        friend class GraphicsPipelineBuilder;

    public:
        auto rasterization_samples(VkSampleCountFlagBits samples)
        {
            m_pipeline_builder->m_multisampling.rasterizationSamples = samples;

            return *this;
        }

        auto sample_shading(float min_sample_shading)
        {
            m_pipeline_builder->m_multisampling.sampleShadingEnable = true;
            m_pipeline_builder->m_multisampling.minSampleShading = min_sample_shading;

            return *this;
        }

        auto sample_mask(VkSampleMask const* mask)
        {
            m_pipeline_builder->m_multisampling.pSampleMask = mask;

            return *this;
        }

        auto alpha_to_coverage(VkBool32 enable)
        {
            m_pipeline_builder->m_multisampling.alphaToCoverageEnable = enable;

            return *this;
        }

        auto alpha_to_one(VkBool32 enable)
        {
            m_pipeline_builder->m_multisampling.alphaToOneEnable = enable;

            return *this;
        }

        auto finish()
        {
            m_pipeline_builder->m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

            return *m_pipeline_builder;
        }
    };

    auto configure_multisampling()
    {
        return MultisamplingBuilder(this);
    }

    auto finish()
    {
        for (auto i = 0uz; i < static_cast<size_t>(RequiredSteps_MAX); i++) {
            if (!m_completed[i])
                throw std::runtime_error("Unfinished!");
        }

        auto vertex_input_info = m_vertex.input_info();
        auto pipelineInfo = VkGraphicsPipelineCreateInfo {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = {},
            .stageCount = static_cast<uint32_t>(m_shader_stages.size()),
            .pStages = m_shader_stages.data(),
            .pVertexInputState = &vertex_input_info,
            .pInputAssemblyState = &m_input_assembly,
            .pTessellationState = nullptr,
            .pViewportState = &m_viewport_state,
            .pRasterizationState = &m_rasterizer,
            .pMultisampleState = &m_multisampling,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &m_color_blending,
            .pDynamicState = &m_dynamic.info,
            .layout = m_pipeline.m_layout,
            .renderPass = m_render_pass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        if (vkCreateGraphicsPipelines(m_pipeline.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline.m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        for (auto&& module : m_shader_modules) {
            vkDestroyShaderModule(m_pipeline.m_device, module, nullptr);
        }

        return m_pipeline;
    }
};
