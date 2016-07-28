#pragma once

#include <vulkan/vulkan.h>

namespace vulkan {
	class pipeline {
	public:
		pipeline(VkDevice vulkan_device);
		~pipeline();

		void init();

		VkPipeline get();

		bool has_pipeline_cache() const {
			return _pipeline_cache == VK_NULL_HANDLE;
		}

		void set_pipeline_cache(VkPipelineCache pipeline_cache) {
			_pipeline_cache = _pipeline_cache;
		}

		VkPipelineCache create_pipeline_cache() const {
			VkPipelineCache pipeline_cache;
			VkResult err;
			VkPipelineCacheCreateInfo pipeline_cache_create_info;
			memset(&pipeline_cache_create_info, 0, sizeof(pipeline_cache_create_info));
			pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

			err = vkCreatePipelineCache(_vulkan_device, &pipeline_cache_create_info, NULL, &pipeline_cache);
			assert(!err);
			return pipeline_cache;
		}
	private:
		VkShaderModule create_shader_module(const char * filename);

		VkPipeline _pipeline = VK_NULL_HANDLE;

		VkPipelineLayout _pipeline_layout = VK_NULL_HANDLE;
		VkRenderPass _render_pass = VK_NULL_HANDLE;
		VkPipelineCache _pipeline_cache = VK_NULL_HANDLE;

		VkDevice _vulkan_device = VK_NULL_HANDLE;
	};
}