#include <cstring>
#include <cstdio>
#include <memory>
#include <assert.h>
#include "vulkan_pipeline.hpp"

namespace vulkan {
	pipeline::pipeline(VkDevice vulkan_device) : _vulkan_device(vulkan_device) {



	}

	pipeline::~pipeline() {

	}


	VkShaderModule pipeline::create_shader_module(const char * filename) {

		auto load_code = [](const char * filename, size_t& psize) -> void * {
			long int size;
			size_t retval;
			void * shader_code;

			FILE *fp = fopen(filename, "rb");
			if (!fp)
				return NULL;

			fseek(fp, 0L, SEEK_END);
			size = ftell(fp);

			fseek(fp, 0L, SEEK_SET);

			shader_code = malloc(size);
			retval = fread(shader_code, size, 1, fp);
			assert(retval == 1);

			psize = size;

			fclose(fp);
			return shader_code;
		};

		VkShaderModule vert_shader_module;
		{
			size_t size;

			void * vert_shader_code = load_code(filename, size);

			VkShaderModuleCreateInfo module_create_info;
			VkResult err;

			module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			module_create_info.pNext = NULL;

			module_create_info.codeSize = size;
			module_create_info.pCode = (uint32_t *)vert_shader_code;
			module_create_info.flags = 0;

			err = vkCreateShaderModule(_vulkan_device, &module_create_info, NULL, &vert_shader_module);
			assert(!err);

			free(vert_shader_code);

		}

		return vert_shader_module;
	}

	void pipeline::init() {

		VkResult err;

		VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
		memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);

		VkPipelineDynamicStateCreateInfo dynamicState;
		memset(&dynamicState, 0, sizeof dynamicState);
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables;

		VkPipelineVertexInputStateCreateInfo vi;
		memset(&vi, 0, sizeof(vi));
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		VkPipelineInputAssemblyStateCreateInfo ia;
		memset(&ia, 0, sizeof(ia));
		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineRasterizationStateCreateInfo rs;
		memset(&rs, 0, sizeof(rs));
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.depthClampEnable = VK_FALSE;
		rs.rasterizerDiscardEnable = VK_FALSE;
		rs.depthBiasEnable = VK_FALSE;
		rs.lineWidth = 1.0f;

		VkPipelineColorBlendStateCreateInfo cb;
		memset(&cb, 0, sizeof(cb));
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		VkPipelineColorBlendAttachmentState att_state[1];
		memset(att_state, 0, sizeof(att_state));
		att_state[0].colorWriteMask = 0xf;
		att_state[0].blendEnable = VK_FALSE;
		cb.attachmentCount = 1;
		cb.pAttachments = att_state;

		VkPipelineViewportStateCreateInfo vp;
		memset(&vp, 0, sizeof(vp));
		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.viewportCount = 1;
		dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
		vp.scissorCount = 1;
		dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;

		VkPipelineDepthStencilStateCreateInfo ds;
		memset(&ds, 0, sizeof(ds));
		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.depthTestEnable = VK_TRUE;
		ds.depthWriteEnable = VK_TRUE;
		ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		ds.depthBoundsTestEnable = VK_FALSE;
		ds.back.failOp = VK_STENCIL_OP_KEEP;
		ds.back.passOp = VK_STENCIL_OP_KEEP;
		ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
		ds.stencilTestEnable = VK_FALSE;
		ds.front = ds.back;

		VkPipelineMultisampleStateCreateInfo ms;
		memset(&ms, 0, sizeof(ms));
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.pSampleMask = NULL;
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Two stages: vs and fs

		VkPipelineShaderStageCreateInfo shaderStages[2];
		memset(&shaderStages, 0, 2 * sizeof(VkPipelineShaderStageCreateInfo));

		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

		auto vert_shader_module = create_shader_module("cube-vert.spv");

		shaderStages[0].module = vert_shader_module;
		shaderStages[0].pName = "main";

		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkShaderModule frag_shader_module = create_shader_module("cube-frag.spv");

		shaderStages[1].module = frag_shader_module;
		shaderStages[1].pName = "main";


		VkGraphicsPipelineCreateInfo pipeline_create_info;
		memset(&pipeline_create_info, 0, sizeof(pipeline_create_info));
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.layout = _pipeline_layout;
		pipeline_create_info.stageCount = 2;

		pipeline_create_info.pVertexInputState = &vi;
		pipeline_create_info.pInputAssemblyState = &ia;
		pipeline_create_info.pRasterizationState = &rs;
		pipeline_create_info.pColorBlendState = &cb;
		pipeline_create_info.pMultisampleState = &ms;
		pipeline_create_info.pViewportState = &vp;
		pipeline_create_info.pDepthStencilState = &ds;
		pipeline_create_info.pStages = shaderStages;
		pipeline_create_info.renderPass = _render_pass;
		pipeline_create_info.pDynamicState = &dynamicState;

		err = vkCreateGraphicsPipelines(_vulkan_device, _pipeline_cache, 1, &pipeline_create_info, NULL, &_pipeline);
		assert(!err);

		vkDestroyShaderModule(_vulkan_device, frag_shader_module, NULL);
		vkDestroyShaderModule(_vulkan_device, vert_shader_module, NULL);
	}


	VkPipeline pipeline::get() {
		return _pipeline;
	}
}