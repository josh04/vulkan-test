#pragma once

#include <Windows.h>
#include <vulkan/vulkan.h>
#include <assert.h>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/glm.hpp>

namespace vulkan {

	struct vulkan_texture {
		VkSampler sampler = VK_NULL_HANDLE;

		VkImage image = VK_NULL_HANDLE;
		VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkMemoryAllocateInfo memory_allocation_info;
		VkDeviceMemory device_memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		uint32_t width = 0, height = 0;
	};

	struct vulkan_buffer {
		VkBuffer buffer;
		VkMemoryAllocateInfo memory_allocation_info;
		VkDeviceMemory device_memory;
		VkDescriptorBufferInfo info;
	};

	class wrapper {
	public:
		wrapper(bool validate);
		~wrapper();

		void init(HWND hw, HINSTANCE hi);

		void create_swapchain();
		void set_image_layout(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkAccessFlagBits srcAccessMask);
		void create_command_buffer();
		void create_surface_depth_image();

		void destroy_vulkan_texture(vulkan_texture& texture) {
			if (texture.sampler != VK_NULL_HANDLE) {
				vkDestroySampler(_vulkan_device, texture.sampler, NULL);
			}

			if (texture.image != VK_NULL_HANDLE) {
				vkDestroyImage(_vulkan_device, texture.image, NULL);
			}

			if (texture.view != VK_NULL_HANDLE) {
				vkDestroyImageView(_vulkan_device, texture.view, NULL);
			}

			if (texture.device_memory != VK_NULL_HANDLE) {
				vkFreeMemory(_vulkan_device, texture.device_memory, NULL);
			}
		}

		vulkan_texture wrapper::load_texture(const char *filename, VkImageTiling tiling, VkImageUsageFlags usage, VkFlags required_properties);

		vulkan_texture create_texture(const char * filename, bool stage_textures = false);

		static VkImageCreateInfo create_image_defaults(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM) {
			return {
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				NULL,
				0,
				VK_IMAGE_TYPE_2D,
				format,
				{ width, height, 1 },
				1,
				1,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				nullptr,
				VK_IMAGE_LAYOUT_UNDEFINED
			};
		}

		VkImage create_image(const VkImageCreateInfo& info) {
			/*
			typedef struct VkImageCreateInfo {
			VkStructureType          sType;
			const void*              pNext;
			VkImageCreateFlags       flags;
			VkImageType              imageType;
			VkFormat                 format;
			VkExtent3D               extent;
			uint32_t                 mipLevels;
			uint32_t                 arrayLayers;
			VkSampleCountFlagBits    samples;
			VkImageTiling            tiling;
			VkImageUsageFlags        usage;
			VkSharingMode            sharingMode;
			uint32_t                 queueFamilyIndexCount;
			const uint32_t*          pQueueFamilyIndices;
			VkImageLayout            initialLayout;
			} VkImageCreateInfo;
			*/
			VkImage image = VK_NULL_HANDLE;

			VkResult err = vkCreateImage(_vulkan_device, &info, NULL, &image);
			assert(!err);

			return image;
		}

		std::pair<VkDeviceMemory, VkMemoryAllocateInfo> allocate_image_memory(VkImage image, VkFlags required_properties);
		std::pair<VkDeviceMemory, VkMemoryAllocateInfo> allocate_buffer_memory(VkBuffer buffer, VkFlags required_properties);

		static VkImageViewCreateInfo create_image_view_defaults(VkImage image = VK_NULL_HANDLE, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM) {
			return {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				NULL,
				0,
				image,
				VK_IMAGE_VIEW_TYPE_2D,
				format,
				{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0,  1, 0,  1 }
			};
		}

		VkImageView create_image_view(const VkImageViewCreateInfo& info) {
			VkImageView image_view = VK_NULL_HANDLE;
			VkResult err = vkCreateImageView(_vulkan_device, &info, NULL, &image_view);
			assert(!err);
			return image_view;
		}

		static VkSamplerCreateInfo create_sampler_defaults() {
			/*
			typedef struct VkSamplerCreateInfo {
				VkStructureType         sType;
				const void*             pNext;
				VkSamplerCreateFlags    flags;
				VkFilter                magFilter;
				VkFilter                minFilter;
				VkSamplerMipmapMode     mipmapMode;
				VkSamplerAddressMode    addressModeU;
				VkSamplerAddressMode    addressModeV;
				VkSamplerAddressMode    addressModeW;
				float                   mipLodBias;
				VkBool32                anisotropyEnable;
				float                   maxAnisotropy;
				VkBool32                compareEnable;
				VkCompareOp             compareOp;
				float                   minLod;
				float                   maxLod;
				VkBorderColor           borderColor;
				VkBool32                unnormalizedCoordinates;
			} VkSamplerCreateInfo;
			*/
			return{
				VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				NULL,
				0,
				VK_FILTER_NEAREST,
				VK_FILTER_NEAREST,
				VK_SAMPLER_MIPMAP_MODE_NEAREST,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				0.0f,
				VK_FALSE,
				1.0f,
				VK_FALSE,
				VK_COMPARE_OP_NEVER,
				0.0f,
				0.0f,
				VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
				VK_FALSE
			};
		}

		VkSampler create_sampler(VkSamplerCreateInfo& info) {
			VkSampler sampler;
			VkResult err;
			err = vkCreateSampler(_vulkan_device, &info, NULL, &sampler);
			assert(!err);

			return sampler;
		}

		void reset_command_buffer() {
			flush_command_buffer();
			create_command_buffer();
		}

		void flush_command_buffer() {
			VkResult err;

			if (_vulkan_command_buffer == VK_NULL_HANDLE) {
				return;
			}

			err = vkEndCommandBuffer(_vulkan_command_buffer);
			assert(!err);

			const VkCommandBuffer command_buffers[] = { _vulkan_command_buffer };

			VkFence nullFence = VK_NULL_HANDLE;

			/*
			typedef struct VkSubmitInfo {
				VkStructureType                sType;
				const void*                    pNext;
				uint32_t                       waitSemaphoreCount;
				const VkSemaphore*             pWaitSemaphores;
				const VkPipelineStageFlags*    pWaitDstStageMask;
				uint32_t                       commandBufferCount;
				const VkCommandBuffer*         pCommandBuffers;
				uint32_t                       signalSemaphoreCount;
				const VkSemaphore*             pSignalSemaphores;
			} VkSubmitInfo;
			*/

			VkSubmitInfo submit_info = { 
				VK_STRUCTURE_TYPE_SUBMIT_INFO,
				NULL,
				0,
				NULL,
				NULL,
				1,
				command_buffers,
				0,
				NULL };

			err = vkQueueSubmit(_vulkan_queue, 1, &submit_info, nullFence);
			assert(!err);

			err = vkQueueWaitIdle(_vulkan_queue);
			assert(!err);

			vkFreeCommandBuffers(_vulkan_device, _vulkan_command_pool, 1, command_buffers);
			_vulkan_command_buffer = VK_NULL_HANDLE;
		}

		void create_swapchain_command_buffers() {
			VkResult err;
			const VkCommandBufferAllocateInfo command_allocate_info = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				NULL,
				_vulkan_command_pool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				1,
			};
			for (uint32_t i = 0; i < _swapchain_image_count; i++) {
				VkCommandBuffer command_buffer;

				err = vkAllocateCommandBuffers(_vulkan_device, &command_allocate_info, &command_buffer);
				assert(!err);

				_swapchain_command_buffers.push_back(command_buffer);
			}
		}

		VkShaderModule create_shader_module(const char * filename) {

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

		void demo_setup_cube();

		void demo_build_render_pass();
		void demo_build_pipeline();
		void demo_prepare_pipeline_descriptors();

		void demo_perform_first_render(uint32_t swapchain_id);

		bool memory_type_from_properties(uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex);

		void demo_tick();
		void demo_update();
		void demo_draw();

		void demo_resize();
	private:
		uint32_t _tick = 0;
		bool _validate;

		uint32_t _surface_width = 1280, _surface_height = 720;

		VkPhysicalDevice _vulkan_physical_device = nullptr;
		VkDevice _vulkan_device = nullptr;

		VkCommandPool _vulkan_command_pool = nullptr;
		VkCommandBuffer _vulkan_command_buffer = VK_NULL_HANDLE;

		VkPhysicalDeviceMemoryProperties _device_memory_properties;

		VkSurfaceKHR _vulkan_surface = nullptr;
		VkFormat _vulkan_format;
		VkColorSpaceKHR _vulkan_colorspace;

		VkFormat _depth_format = VK_FORMAT_D16_UNORM;
		VkImageView _depth_view;
		VkImage _depth_image;
		VkDeviceMemory _depth_device_memory;
		VkQueue _vulkan_queue = nullptr;

		VkSwapchainKHR _vulkan_swapchain = nullptr;
		uint32_t _swapchain_image_count = 0;

		std::vector<VkImage> _swapchain_images;
		std::vector<VkImageView> _swapchain_views;
		std::vector<VkCommandBuffer> _swapchain_command_buffers;
		std::vector<VkFramebuffer> _swapchain_framebuffers;

		VkDescriptorPool _descriptor_pool;
		VkDescriptorSetLayout _descriptor_set_layout;
		VkDescriptorSet _descriptor_set;

		VkPipelineLayout _pipeline_layout;
		VkRenderPass _render_pass;
		VkPipelineCache _pipeline_cache;
		VkPipeline _pipeline;
		/*
		typedef struct {
			VkImage image;
			VkCommandBuffer cmd;
			VkImageView view;
		} SwapchainBuffers;
		*/
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR = nullptr;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
		PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR = nullptr;

		PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr = nullptr;

		PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR = nullptr;
		PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR = nullptr;
		PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR = nullptr; // ???
		PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR = nullptr;
		PFN_vkQueuePresentKHR fpQueuePresentKHR = nullptr;

		glm::mat4x4 _projection, _view, _model, _MVP, _VP;
		vulkan_buffer _cube_buffer;
		vulkan_texture _demo_texture;
	};

}