#pragma once

#include <vulkan/vulkan.h>

namespace vulkan {
	class pipeline {
	public:
		pipeline(VkDevice _vulkan_device);
		~pipeline();

	private:
		VkDevice _vulkan_device = VK_NULL_HANDLE;
	};
}