#pragma once

#include <vulkan/vulkan.h>
#include <tuple>

namespace vulkan {
	class swapchain {
	public:
		swapchain(VkDevice vulkan_device);
		~swapchain();

		void init();

		std::pair<uint32_t, uint32_t> get_surface_dimensions() const {
			return{ _width, _height };
		}

	private:
		VkDevice _vulkan_device;
		VkSwapchainKHR _swapchain = VK_NULL_HANDLE;

		uint32_t _width = 1, _height = 1;

	};

}