
#define VK_USE_PLATFORM_WIN32_KHR
// #define VK_PROTOTYPES

#include <iostream>
#include <vector>
#include <array>
#include <tuple>
#include <assert.h>

#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>


#include "pngReader.hpp"
#include "vulkan_wrapper.hpp"

namespace vulkan {


	static VkBool32 demo_check_layers(uint32_t check_count, char **check_names, uint32_t layer_count, VkLayerProperties *layers) {
		for (uint32_t i = 0; i < check_count; i++) {
			VkBool32 found = 0;
			for (uint32_t j = 0; j < layer_count; j++) {
				if (!strcmp(check_names[i], layers[j].layerName)) {
					found = 1;
					break;
				}
			}
			if (!found) {
				fprintf(stderr, "Cannot find layer: %s\n", check_names[i]);
				return 0;
			}
		}
		return 1;
	}

	bool wrapper::memory_type_from_properties(uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
		// Search memtypes to find first index with those properties
		for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
			if ((typeBits & 1) == 1) {
				// Type is available, does it match user properties?
				if ((_device_memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
					*typeIndex = i;
					return true;
				}
			}
			typeBits >>= 1;
		}
		// No memory types matched, return failure
		return false;
	}

	std::pair<VkDeviceMemory, VkMemoryAllocateInfo> wrapper::allocate_image_memory(VkImage image, VkFlags required_properties) {
		VkMemoryRequirements memory_requirements;

		vkGetImageMemoryRequirements(_vulkan_device, image, &memory_requirements);

		VkMemoryAllocateInfo memory_allocation_info;
		memory_allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memory_allocation_info.pNext = NULL;
		memory_allocation_info.allocationSize = memory_requirements.size;
		memory_allocation_info.memoryTypeIndex = 0;

		bool pass = memory_type_from_properties(memory_requirements.memoryTypeBits, required_properties, &memory_allocation_info.memoryTypeIndex);
		assert(pass);

		/* allocate memory */
		VkDeviceMemory device_memory;
		VkResult err = vkAllocateMemory(_vulkan_device, &memory_allocation_info, NULL, &device_memory);
		assert(!err);

		/* bind memory */
		err = vkBindImageMemory(_vulkan_device, image, device_memory, 0);
		assert(!err);

		return { device_memory, memory_allocation_info };
	}

	std::pair<VkDeviceMemory, VkMemoryAllocateInfo> wrapper::allocate_buffer_memory(VkBuffer buffer, VkFlags required_properties) {
		VkMemoryRequirements memory_requirements;

		vkGetBufferMemoryRequirements(_vulkan_device, buffer, &memory_requirements);

		VkMemoryAllocateInfo memory_allocation_info;
		memory_allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memory_allocation_info.pNext = NULL;
		memory_allocation_info.allocationSize = memory_requirements.size;
		memory_allocation_info.memoryTypeIndex = 0;

		bool pass = memory_type_from_properties(memory_requirements.memoryTypeBits, required_properties, &memory_allocation_info.memoryTypeIndex);
		assert(pass);

		/* allocate memory */
		VkDeviceMemory device_memory;
		VkResult err = vkAllocateMemory(_vulkan_device, &memory_allocation_info, NULL, &device_memory);
		assert(!err);

		/* bind memory */
		err = vkBindBufferMemory(_vulkan_device, buffer, device_memory, 0);
		assert(!err);

		return{ device_memory, memory_allocation_info };
	}


	 void wrapper::set_image_layout(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkAccessFlagBits srcAccessMask) {
		/*
		typedef struct VkImageMemoryBarrier {
			VkStructureType            sType;
			const void*                pNext;
			VkAccessFlags              srcAccessMask;
			VkAccessFlags              dstAccessMask;
			VkImageLayout              oldLayout;
			VkImageLayout              newLayout;
			uint32_t                   srcQueueFamilyIndex;
			uint32_t                   dstQueueFamilyIndex;
			VkImage                    image;
			VkImageSubresourceRange    subresourceRange;
		} VkImageMemoryBarrier;
*/
		VkImageMemoryBarrier image_memory_barrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			NULL,
			srcAccessMask,
			0,
			old_image_layout,
			new_image_layout,
			0,
			0,
			image,
			{ aspectMask, 0, 1, 0, 1 } };

		if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			/* Make sure anything that was copying from this image has completed */
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			image_memory_barrier.dstAccessMask =
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			image_memory_barrier.dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			/* Make sure any Copy or CPU writes to image are flushed */
			image_memory_barrier.dstAccessMask =
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		}

		VkImageMemoryBarrier *pmemory_barrier = &image_memory_barrier;

		VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		vkCmdPipelineBarrier(_vulkan_command_buffer, src_stages, dst_stages, 0, 0, NULL, 0,
			NULL, 1, pmemory_barrier);
	}

	wrapper::wrapper(bool validate) : _validate(validate) {

	}

	wrapper::~wrapper() {

	}

	void wrapper::init(HWND hw, HINSTANCE hi) {
		VkResult err;
		uint32_t instance_extension_count = 0;
		uint32_t instance_layer_count = 0;
		uint32_t validation_layer_count = 0;

		//std::vector<const char *> validation_layer_names;
		std::vector<const char *> extension_names;

		char **instance_validation_layers = NULL;

		char *instance_validation_layers_alt1[] = {
			"VK_LAYER_LUNARG_standard_validation"
		};

		char *instance_validation_layers_alt2[] = {
			"VK_LAYER_GOOGLE_threading",       "VK_LAYER_LUNARG_parameter_validation",
			"VK_LAYER_LUNARG_object_tracker",  "VK_LAYER_LUNARG_image",
			"VK_LAYER_LUNARG_core_validation", "VK_LAYER_LUNARG_swapchain",
			"VK_LAYER_GOOGLE_unique_objects"
		};

		/* Look for validation layers */
		if (_validate) {

			VkBool32 validation_found = 0;

			err = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
			assert(!err);

			instance_validation_layers = instance_validation_layers_alt1;
			if (instance_layer_count > 0) {
				auto instance_layers = (VkLayerProperties *)malloc(sizeof(VkLayerProperties) * instance_layer_count);
				err = vkEnumerateInstanceLayerProperties(&instance_layer_count,
					instance_layers);
				assert(!err);

				validation_found = demo_check_layers(1, instance_validation_layers, instance_layer_count, instance_layers);

				if (validation_found) {
					validation_layer_count = 1;
				} else {
					// use alternative set of validation layers
					instance_validation_layers = instance_validation_layers_alt2;
					validation_found = demo_check_layers(7, instance_validation_layers, instance_layer_count, instance_layers);
					validation_layer_count = 7;
				}

				free(instance_layers);
			}

			if (!validation_found) {
				std::cerr << "vkEnumerateInstanceLayerProperties failed to find required validation layer." << std::endl;
			}
		}

		/* Look for instance extensions */
		VkBool32 surfaceExtFound = 0;
		VkBool32 platformSurfaceExtFound = 0;
		uint32_t enabledExtensionCount = 0;

		err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL);
		assert(!err);

		if (instance_extension_count > 0) {
			auto instance_extensions = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * instance_extension_count);

			err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, instance_extensions);
			assert(!err);

			for (uint32_t i = 0; i < instance_extension_count; i++) {
				if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME,
					instance_extensions[i].extensionName)) {
					surfaceExtFound = 1;
					enabledExtensionCount++;
					extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
					//demo->extension_names[demo->enabled_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
				}
				//#if defined(VK_USE_PLATFORM_WIN32_KHR)
				if (!strcmp(VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
					instance_extensions[i].extensionName)) {
					platformSurfaceExtFound = 1;
					enabledExtensionCount++;
					extension_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
					//demo->extension_names[demo->enabled_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
				}
				//#endif
				if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
					instance_extensions[i].extensionName)) {
					if (_validate) {
						enabledExtensionCount++;
						extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
						//demo->extension_names[demo->enabled_extension_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
					}
				}
				//assert(demo->enabled_extension_count < 64);
			}

			free(instance_extensions);
		}

		if (!surfaceExtFound) {
			std::cerr << "vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_SURFACE_EXTENSION_NAME " extension." << std::endl;
		}

		if (!platformSurfaceExtFound) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			std::cerr << "vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_WIN32_SURFACE_EXTENSION_NAME " extension." << std::endl;
#endif
		}


		/*
		const VkApplicationInfo app = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = NULL,
		.pApplicationName = APP_SHORT_NAME,
		.applicationVersion = 0,
		.pEngineName = APP_SHORT_NAME,
		.engineVersion = 0,
		.apiVersion = VK_API_VERSION_1_0,
		};

		VkInstanceCreateInfo inst_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = NULL,
		.pApplicationInfo = &app,
		.enabledLayerCount = demo->enabled_layer_count,
		.ppEnabledLayerNames = (const char *const *)instance_validation_layers,
		.enabledExtensionCount = demo->enabled_extension_count,
		.ppEnabledExtensionNames = (const char *const *)demo->extension_names,
		};
		*/

		const VkApplicationInfo app = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO,
			NULL,
			"test",
			0,
			"test",
			0,
			VK_API_VERSION_1_0,
		};

		VkInstanceCreateInfo instance_info = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			NULL,
			0,
			&app,
			validation_layer_count,
			(const char * const *)instance_validation_layers,
			enabledExtensionCount,
			(const char * const *)extension_names.data(),
		};

		/*
		/*
		* This is info for a temp callback to use during CreateInstance.
		* After the instance is created, we use the instance-based
		* function to register the final callback.
		*
		VkDebugReportCallbackCreateInfoEXT dbgCreateInfo;
		if (demo->validate) {
		dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		dbgCreateInfo.pNext = NULL;
		dbgCreateInfo.pfnCallback = demo->use_break ? BreakCallback : dbgFunc;
		dbgCreateInfo.pUserData = demo;
		dbgCreateInfo.flags =
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		inst_info.pNext = &dbgCreateInfo;
		}
		*/


		VkInstance _vulkan_instance = nullptr;
		//VkPhysicalDevice _vulkan_physical_device = nullptr;

		VkPhysicalDeviceProperties vulkan_device_props;
		uint32_t vulkan_device_count = 0;

		err = vkCreateInstance(&instance_info, NULL, &_vulkan_instance);

		if (err == VK_ERROR_INCOMPATIBLE_DRIVER) {
			std::cerr << "Cannot find a compatible Vulkan installable client driver (ICD)." << std::endl;
		} else if (err == VK_ERROR_EXTENSION_NOT_PRESENT) {
			std::cerr << "Cannot find a specified extension library" << std::endl;
		} else if (err) {
			std::cerr << "vkCreateInstance failed. Do you have a compatible Vulkan installable client driver (ICD) installed?" << std::endl;
		}

		/* Make initial call to query gpu_count, then second call for gpu info*/
		err = vkEnumeratePhysicalDevices(_vulkan_instance, &vulkan_device_count, NULL);
		assert(!err && vulkan_device_count > 0);

		if (vulkan_device_count > 0) {
			auto physical_devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * vulkan_device_count);
			err = vkEnumeratePhysicalDevices(_vulkan_instance, &vulkan_device_count, physical_devices);
			assert(!err);
			// grab the first eyyyyy
			_vulkan_physical_device = physical_devices[0];
			free(physical_devices);
		} else {
			std::cerr << "vkEnumeratePhysicalDevices reported zero accessible devices." << std::endl;
		}

		/* Look for device extensions */
		uint32_t device_extension_count = 0;
		VkBool32 swapchainExtFound = 0;

		uint32_t device_enabled_extension_count = 0;
		std::vector<const char *> device_extension_names;

		err = vkEnumerateDeviceExtensionProperties(_vulkan_physical_device, NULL, &device_extension_count, NULL);
		assert(!err);

		if (device_extension_count > 0) {
			auto device_extensions = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * device_extension_count);
			err = vkEnumerateDeviceExtensionProperties(_vulkan_physical_device, NULL, &device_extension_count, device_extensions);
			assert(!err);

			for (uint32_t i = 0; i < device_extension_count; i++) {
				if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME,
					device_extensions[i].extensionName)) {
					device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
					device_enabled_extension_count++;
				}
				assert(device_enabled_extension_count < 64);
			}

			free(device_extensions);
		}

		if (device_extension_names.size() == 0) {
			std::cerr << "vkEnumerateDeviceExtensionProperties failed to find the " VK_KHR_SWAPCHAIN_EXTENSION_NAME " extension." << std::endl;
		}

		/*
		if (validate) {
		demo->CreateDebugReportCallback =
		(PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
		demo->inst, "vkCreateDebugReportCallbackEXT");
		demo->DestroyDebugReportCallback =
		(PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		demo->inst, "vkDestroyDebugReportCallbackEXT");
		if (!demo->CreateDebugReportCallback) {
		ERR_EXIT(
		"GetProcAddr: Unable to find vkCreateDebugReportCallbackEXT\n",
		"vkGetProcAddr Failure");
		}
		if (!demo->DestroyDebugReportCallback) {
		ERR_EXIT(
		"GetProcAddr: Unable to find vkDestroyDebugReportCallbackEXT\n",
		"vkGetProcAddr Failure");
		}
		demo->DebugReportMessage =
		(PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(
		demo->inst, "vkDebugReportMessageEXT");
		if (!demo->DebugReportMessage) {
		ERR_EXIT("GetProcAddr: Unable to find vkDebugReportMessageEXT\n",
		"vkGetProcAddr Failure");
		}

		VkDebugReportCallbackCreateInfoEXT dbgCreateInfo;
		PFN_vkDebugReportCallbackEXT callback;
		callback = demo->use_break ? BreakCallback : dbgFunc;
		dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		dbgCreateInfo.pNext = NULL;
		dbgCreateInfo.pfnCallback = callback;
		dbgCreateInfo.pUserData = demo;
		dbgCreateInfo.flags =
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		err = demo->CreateDebugReportCallback(demo->inst, &dbgCreateInfo, NULL,
		&demo->msg_callback);
		switch (err) {
		case VK_SUCCESS:
		break;
		case VK_ERROR_OUT_OF_HOST_MEMORY:
		ERR_EXIT("CreateDebugReportCallback: out of host memory\n",
		"CreateDebugReportCallback Failure");
		break;
		default:
		ERR_EXIT("CreateDebugReportCallback: unknown failure\n",
		"CreateDebugReportCallback Failure");
		break;
		}
		}
		*/

		vkGetPhysicalDeviceProperties(_vulkan_physical_device, &vulkan_device_props);

		uint32_t vulkan_device_queue_count = 0;

		/* Call with NULL data to get count */
		vkGetPhysicalDeviceQueueFamilyProperties(_vulkan_physical_device, &vulkan_device_queue_count, NULL);
		assert(vulkan_device_queue_count >= 1);

		auto vulkan_device_queue_properties = (VkQueueFamilyProperties *)malloc(vulkan_device_queue_count * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(_vulkan_physical_device, &vulkan_device_queue_count, vulkan_device_queue_properties);

		// Find a queue that supports gfx
		std::vector<uint32_t> graphics_queue_ids;
		for (uint32_t graphics_queue_id = 0; graphics_queue_id < vulkan_device_queue_count; graphics_queue_id++) {
			if (vulkan_device_queue_properties[graphics_queue_id].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphics_queue_ids.push_back(graphics_queue_id);
			}
		}
		//assert(graphics_queue_id < vulkan_device_queue_count);

		// Query fine-grained feature support for this device.
		//  If app has specific feature requirements it should check supported
		//  features based on this query

		VkPhysicalDeviceFeatures physDevFeatures;
		vkGetPhysicalDeviceFeatures(_vulkan_physical_device, &physDevFeatures);


		fpGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(_vulkan_instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
		fpGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(_vulkan_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		fpGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(_vulkan_instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
		fpGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(_vulkan_instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
		//auto fpGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)vkGetInstanceProcAddr(vulkan_instance, "vkGetSwapchainImagesKHR");

		uint32_t i;

		// Create a WSI surface for the window:
		//#if defined(VK_USE_PLATFORM_WIN32_KHR)

		VkWin32SurfaceCreateInfoKHR createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.pNext = NULL;
		createInfo.flags = 0;
		createInfo.hinstance = hi;
		createInfo.hwnd = hw;

		//VkSurfaceKHR _vulkan_surface = nullptr;

		err = vkCreateWin32SurfaceKHR(_vulkan_instance, &createInfo, NULL, &_vulkan_surface);
		//#endif

		assert(!err);

		// Iterate over each queue to learn whether it supports presenting:
		auto is_presentable_queue = (VkBool32 *)malloc(vulkan_device_queue_count * sizeof(VkBool32));
		for (i = 0; i < vulkan_device_queue_count; i++) {
			fpGetPhysicalDeviceSurfaceSupportKHR(_vulkan_physical_device, i, _vulkan_surface, &is_presentable_queue[i]);
		}

		// Search for a graphics and a present queue in the array of queue
		// families, try to find one that supports both
		uint32_t presentable_queue_index = UINT32_MAX;
		uint32_t graphics_queue_id = UINT32_MAX;
		for (auto id : graphics_queue_ids) {
			if ((vulkan_device_queue_properties[id].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				if (is_presentable_queue[i] == VK_TRUE) {
					presentable_queue_index = i;
					graphics_queue_id = i;
					break;
				}
			}
		}

		if (graphics_queue_id == UINT32_MAX) {
			if (graphics_queue_ids.size() > 0) {
				graphics_queue_id = graphics_queue_ids[0];
			}
		}

		if (presentable_queue_index == UINT32_MAX) {
			// If didn't find a queue that supports both graphics and present, then
			// find a separate present queue.
			for (uint32_t i = 0; i < vulkan_device_queue_count; ++i) {
				if (is_presentable_queue[i] == VK_TRUE) {
					presentable_queue_index = i;
					break;
				}
			}
		}

		free(is_presentable_queue);

		// Generate error if could not find both a graphics and a present queue
		if (graphics_queue_id == UINT32_MAX || presentable_queue_index == UINT32_MAX) {
			std::cerr << "Could not find a graphics and a present queue." << std::endl;
		}

		// TODO: Add support for separate queues, including presentation,
		//       synchronization, and appropriate tracking for QueueSubmit.
		// NOTE: While it is possible for an application to use a separate graphics
		//       and a present queues, this demo program assumes it is only using
		//       one:
		if (graphics_queue_id != presentable_queue_index) {
			std::cerr << "Could not find a common graphics and a present queue." << std::endl;
		}

		// CREATE DEVICE

		float queue_priorities[1] = { 0.0 };

		/*
		typedef struct VkDeviceQueueCreateInfo {
		VkStructureType             sType;
		const void*                 pNext;
		VkDeviceQueueCreateFlags    flags;
		uint32_t                    queueFamilyIndex;
		uint32_t                    queueCount;
		const float*                pQueuePriorities;
		} VkDeviceQueueCreateInfo;
		*/

		const VkDeviceQueueCreateInfo vulkan_queue_create_info = {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			NULL,
			0,
			graphics_queue_id,
			1,
			queue_priorities };

		/*
		typedef struct VkDeviceCreateInfo {
		VkStructureType                    sType;
		const void*                        pNext;
		VkDeviceCreateFlags                flags;
		uint32_t                           queueCreateInfoCount;
		const VkDeviceQueueCreateInfo*     pQueueCreateInfos;
		uint32_t                           enabledLayerCount;
		const char* const*                 ppEnabledLayerNames;
		uint32_t                           enabledExtensionCount;
		const char* const*                 ppEnabledExtensionNames;
		const VkPhysicalDeviceFeatures*    pEnabledFeatures;
		} VkDeviceCreateInfo;
		*/

		VkDeviceCreateInfo vulkan_device_create_info = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			NULL,
			0,
			1,
			&vulkan_queue_create_info,
			0,
			NULL,
			device_enabled_extension_count,
			(const char * const *)device_extension_names.data(),
			NULL, // If specific features are required, pass them in here
		};

		//VkDevice _vulkan_device = nullptr;

		err = vkCreateDevice(_vulkan_physical_device, &vulkan_device_create_info, NULL, &_vulkan_device);
		assert(!err);

		fpGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(_vulkan_instance, "vkGetDeviceProcAddr");

		fpCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)fpGetDeviceProcAddr(_vulkan_device, "vkCreateSwapchainKHR");
		fpDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)fpGetDeviceProcAddr(_vulkan_device, "vkDestroySwapchainKHR");
		fpGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)fpGetDeviceProcAddr(_vulkan_device, "vkGetSwapchainImagesKHR"); // ???
		fpAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)fpGetDeviceProcAddr(_vulkan_device, "vkAcquireNextImageKHR");
		fpQueuePresentKHR = (PFN_vkQueuePresentKHR)fpGetDeviceProcAddr(_vulkan_device, "vkQueuePresentKHR");

		//VkQueue _vulkan_queue = nullptr;
		vkGetDeviceQueue(_vulkan_device, graphics_queue_id, 0, &_vulkan_queue);

		// Get the list of VkFormat's that are supported:
		uint32_t surface_format_count;
		err = fpGetPhysicalDeviceSurfaceFormatsKHR(_vulkan_physical_device, _vulkan_surface, &surface_format_count, NULL);
		assert(!err);

		//auto surface_formats = (VkSurfaceFormatKHR *)malloc(surface_format_count * sizeof(VkSurfaceFormatKHR));
		std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
		err = fpGetPhysicalDeviceSurfaceFormatsKHR(_vulkan_physical_device, _vulkan_surface, &surface_format_count, surface_formats.data());

		assert(!err);

		// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
		// the surface has no preferred format.  Otherwise, at least one
		// supported format will be returned.

		//VkFormat _vulkan_format;

		if (surface_format_count == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED) {
			_vulkan_format = VK_FORMAT_B8G8R8A8_UNORM;
		} else {
			assert(surface_format_count >= 1);
			_vulkan_format = surface_formats[0].format;
		}
		/*VkColorSpaceKHR*/ _vulkan_colorspace = surface_formats[0].colorSpace;

		// Get Memory information and properties
		//VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(_vulkan_physical_device, &_device_memory_properties);


		// PREPARE ??? 
		/*
typedef struct VkCommandPoolCreateInfo {
	VkStructureType             sType;
	const void*                 pNext;
	VkCommandPoolCreateFlags    flags;
	uint32_t                    queueFamilyIndex;
} VkCommandPoolCreateInfo;
		*/

		const VkCommandPoolCreateInfo command_pool_create_info = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			NULL,
			graphics_queue_id,
			0,
		};

		//VkCommandPool _vulkan_command_pool = nullptr;
		err = vkCreateCommandPool(_vulkan_device, &command_pool_create_info, NULL, &_vulkan_command_pool);
		assert(!err);

		/*
typedef struct VkCommandBufferAllocateInfo {
		VkStructureType         sType;
		const void*             pNext;
		VkCommandPool           commandPool;
		VkCommandBufferLevel    level;
		uint32_t                commandBufferCount;
} VkCommandBufferAllocateInfo;
		*/


		/*

		demo_prepare_buffers(demo);
		*/

		create_swapchain();

		create_command_buffer();

		create_surface_depth_image();

		_demo_texture = create_texture("test.png");
		
		demo_setup_cube();

		demo_build_render_pass();

		demo_build_pipeline();
		/*
		demo_prepare_textures(demo);
		demo_prepare_cube_data_buffer(demo);

		demo_prepare_descriptor_layout(demo);
		demo_prepare_render_pass(demo);
		demo_prepare_pipeline(demo);

		*/
		create_swapchain_command_buffers();

		demo_prepare_pipeline_descriptors();

		/*
		demo_prepare_descriptor_pool(demo);
		demo_prepare_descriptor_set(demo);

		demo_prepare_framebuffers(demo);
		*/
		for (uint32_t i = 0; i < _swapchain_image_count; i++) {
			demo_perform_first_render(i);
		}

		flush_command_buffer();

		/*
		// Prepare functions above may generate pipeline commands that need to be flushed before beginning the render loop.

		demo_flush_init_cmd(demo);

		demo->current_buffer = 0;
		demo->prepared = true;
		*/
	}

	void wrapper::create_swapchain() {
		VkResult err;

		VkSwapchainKHR oldSwapchain = _vulkan_swapchain;

		// Check the surface capabilities and formats
		VkSurfaceCapabilitiesKHR surface_capabilities;
		err = fpGetPhysicalDeviceSurfaceCapabilitiesKHR(_vulkan_physical_device, _vulkan_surface, &surface_capabilities);
		assert(!err);

		uint32_t present_modes_count = 0;
		err = fpGetPhysicalDeviceSurfacePresentModesKHR(_vulkan_physical_device, _vulkan_surface, &present_modes_count, NULL);
		assert(!err);

		auto present_modes = (VkPresentModeKHR *)malloc(present_modes_count * sizeof(VkPresentModeKHR));
		assert(present_modes);

		err = fpGetPhysicalDeviceSurfacePresentModesKHR(_vulkan_physical_device, _vulkan_surface, &present_modes_count, present_modes);
		assert(!err);

		VkExtent2D swapchain_size;
		// width and height are either both -1, or both not -1.
		if (surface_capabilities.currentExtent.width == (uint32_t)-1) {
			// If the surface size is undefined, the size is set to
			// the size of the images requested.
			swapchain_size.width = _surface_width;
			swapchain_size.height = _surface_height;
		} else {
			// If the surface size is defined, the swap chain size must match
			swapchain_size = surface_capabilities.currentExtent;
			_surface_width = surface_capabilities.currentExtent.width;
			_surface_height = surface_capabilities.currentExtent.height;
		}

		// If mailbox mode is available, use it, as is the lowest-latency non-
		// tearing mode.  If not, try IMMEDIATE which will usually be available,
		// and is fastest (though it tears).  If not, fall back to FIFO which is
		// always available.

		VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

		for (size_t i = 0; i < present_modes_count; i++) {
			if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
				swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}

			if ((swapchain_present_mode != VK_PRESENT_MODE_MAILBOX_KHR) && (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) {
				swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}

		// Determine the number of VkImage's to use in the swap chain (we desire to
		// own only 1 image at a time, besides the images being displayed and
		// queued for display):

		uint32_t swapchain_image_count_target = surface_capabilities.minImageCount + 1;
		if ((surface_capabilities.maxImageCount > 0) && (swapchain_image_count_target > surface_capabilities.maxImageCount)) {
			// Application must settle for fewer images than desired:
			swapchain_image_count_target = surface_capabilities.maxImageCount;
		}

		// looks like we're telling vulkan to fuck off with it's surface transformations
		VkSurfaceTransformFlagsKHR surface_pretransform;
		if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			surface_pretransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		} else {
			surface_pretransform = surface_capabilities.currentTransform;
		}

		/*
typedef struct VkSwapchainCreateInfoKHR {
	VkStructureType                  sType;
	const void*                      pNext;
	VkSwapchainCreateFlagsKHR        flags;
	VkSurfaceKHR                     surface;
	uint32_t                         minImageCount;
	VkFormat                         imageFormat;
	VkColorSpaceKHR                  imageColorSpace;
	VkExtent2D                       imageExtent;
	uint32_t                         imageArrayLayers;
	VkImageUsageFlags                imageUsage;
	VkSharingMode                    imageSharingMode;
	uint32_t                         queueFamilyIndexCount;
	const uint32_t*                  pQueueFamilyIndices;
	VkSurfaceTransformFlagBitsKHR    preTransform;
	VkCompositeAlphaFlagBitsKHR      compositeAlpha;
	VkPresentModeKHR                 presentMode;
	VkBool32                         clipped;
	VkSwapchainKHR                   oldSwapchain;
} VkSwapchainCreateInfoKHR;

		*/
		const VkSwapchainCreateInfoKHR swapchain_create_info = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		NULL,
		0,
		_vulkan_surface,
		swapchain_image_count_target,
		_vulkan_format,
		_vulkan_colorspace,
		{swapchain_size.width, swapchain_size.height, },
		1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		NULL,
		(VkSurfaceTransformFlagBitsKHR)surface_pretransform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		swapchain_present_mode,
		true,
		oldSwapchain
		};

		uint32_t i;

		err = fpCreateSwapchainKHR(_vulkan_device, &swapchain_create_info, NULL, &_vulkan_swapchain);
		assert(!err);

		// If we just re-created an existing swapchain, we should destroy the old
		// swapchain at this point.
		// Note: destroying the swapchain also cleans up all its associated
		// presentable images once the platform is done with them.
		if (oldSwapchain != VK_NULL_HANDLE) {
			fpDestroySwapchainKHR(_vulkan_device, oldSwapchain, NULL);
		}

		//uint32_t 
		_swapchain_image_count = 0;

		err = fpGetSwapchainImagesKHR(_vulkan_device, _vulkan_swapchain, &_swapchain_image_count, NULL);
		assert(!err);

		_swapchain_images = std::vector<VkImage>(_swapchain_image_count);

		err = fpGetSwapchainImagesKHR(_vulkan_device, _vulkan_swapchain, &_swapchain_image_count, _swapchain_images.data());
		assert(!err);

		_swapchain_views.clear();

		//auto swapchain_buffers = (SwapchainBuffers *)malloc(sizeof(SwapchainBuffers) * swapchain_image_count);
		//assert(swapchain_buffers);

		for (i = 0; i < _swapchain_image_count; i++) {
			VkImageView view = create_image_view(create_image_view_defaults(_swapchain_images[i], _vulkan_format));
			_swapchain_views.push_back(view);
		}


		if (NULL != present_modes) {
			free(present_modes);
		}
	}

	void wrapper::create_command_buffer() {
		VkResult err;
		if (_vulkan_command_buffer == VK_NULL_HANDLE) {
			/*
			typedef struct VkCommandBufferAllocateInfo {
			VkStructureType         sType;
			const void*             pNext;
			VkCommandPool           commandPool;
			VkCommandBufferLevel    level;
			uint32_t                commandBufferCount;
			} VkCommandBufferAllocateInfo;
			*/
			const VkCommandBufferAllocateInfo command_allocate_info = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				NULL,
				_vulkan_command_pool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				1,
			};

			err = vkAllocateCommandBuffers(_vulkan_device, &command_allocate_info, &_vulkan_command_buffer);
			assert(!err);

			/*
			typedef struct VkCommandBufferBeginInfo {
			VkStructureType                          sType;
			const void*                              pNext;
			VkCommandBufferUsageFlags                flags;
			const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
			} VkCommandBufferBeginInfo;
			*/
			VkCommandBufferBeginInfo command_buffer_info = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				NULL,
				0,
				NULL,
			};
			err = vkBeginCommandBuffer(_vulkan_command_buffer, &command_buffer_info);
			assert(!err);

		}

	}

	void wrapper::create_surface_depth_image() {
		auto image_info = create_image_defaults(_surface_width, _surface_height, _depth_format);
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		
		/* VkImage */_depth_image = create_image(image_info);

		VkMemoryAllocateInfo depth_memory_allocate_info;

		//VkDeviceMemory _depth_device_memory = nullptr;

		std::tie(_depth_device_memory, depth_memory_allocate_info) = allocate_image_memory(_depth_image, 0);


		set_image_layout(_depth_image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, (VkAccessFlagBits)0);
		
		auto image_view_info = create_image_view_defaults(_depth_image, _depth_format);
		image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		image_view_info.components = {};
		/*VkImageView*/ _depth_view = create_image_view(image_view_info);
	}

	vulkan_texture wrapper::create_texture(const char * filename, bool stage_textures) {
		const VkFormat texture_format = VK_FORMAT_R8G8B8A8_UNORM;
		VkFormatProperties props;

		vulkan_texture return_texture;

		vkGetPhysicalDeviceFormatProperties(_vulkan_physical_device, texture_format, &props);

		if ((props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) && !stage_textures) {
			return_texture = load_texture(filename, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		} else if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {

			/* Must use staging buffer to copy linear texture to optimized */

			auto staging_texture = load_texture(filename, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			return_texture = load_texture(filename, VK_IMAGE_TILING_OPTIMAL, (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			set_image_layout(staging_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, staging_texture.imageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (VkAccessFlagBits)0);

			set_image_layout(return_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, return_texture.imageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkAccessFlagBits)0);


			/*
typedef struct VkImageCopy {
	VkImageSubresourceLayers    srcSubresource;
	VkOffset3D                  srcOffset;
	VkImageSubresourceLayers    dstSubresource;
	VkOffset3D                  dstOffset;
	VkExtent3D                  extent;
} VkImageCopy;
*/
			VkImageCopy copy_region = {
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				{ 0, 0, 0 },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				{ 0, 0, 0 },
				{ staging_texture.width, staging_texture.height, 1 },
			};

			vkCmdCopyImage(_vulkan_command_buffer, staging_texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, return_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

			set_image_layout(return_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, return_texture.imageLayout, (VkAccessFlagBits)0);

			reset_command_buffer(); // flush + recreate

			destroy_vulkan_texture(staging_texture);

		} else {
			/* Can't support VK_FORMAT_R8G8B8A8_UNORM !? */
			assert(!"No support for R8G8B8A8_UNORM as texture image format");
		}
		auto sampler_info = create_sampler_defaults();
		return_texture.sampler = create_sampler(sampler_info);

		auto view_info = create_image_view_defaults(return_texture.image, texture_format);
		return_texture.view = create_image_view(view_info);
		return return_texture;
	}


	vulkan_texture wrapper::load_texture(const char *filename, VkImageTiling tiling, VkImageUsageFlags usage, VkFlags required_properties) {

		const VkFormat tex_format = VK_FORMAT_R8G8B8A8_UNORM;
		VkResult err;

		vulkan_texture return_texture;

		std::shared_ptr<uint8_t> raw_image = nullptr;
		/*
		std::array<uint8_t, 16> raw_image;
		raw_image[0] = 255;
		raw_image[1] = 0;
		raw_image[2] = 255;
		raw_image[3] = 255;

		raw_image[4] = 0;
		raw_image[5] = 255;
		raw_image[6] = 255;
		raw_image[7] = 255;

		raw_image[8] = 0;
		raw_image[9] = 0;
		raw_image[10] = 255;
		raw_image[11] = 255;

		raw_image[12] = 255;
		raw_image[13] = 255;
		raw_image[14] = 255;
		raw_image[15] = 255;
		*/
		try {
			raw_image = load_image::png(filename, return_texture.width, return_texture.height);
		} catch (std::exception& e) {
			std::cerr << "Failed to load textures: " << e.what() << std::endl;
		}

		auto image_info = wrapper::create_image_defaults(return_texture.width, return_texture.height, tex_format);
		image_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		image_info.tiling = tiling;
		image_info.usage = usage;
		
		return_texture.image = create_image(image_info);

		std::tie(return_texture.device_memory, return_texture.memory_allocation_info) = allocate_image_memory(return_texture.image, required_properties);

		
		if (required_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			const VkImageSubresource subres = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
			VkSubresourceLayout layout;
			void * data = NULL;

			vkGetImageSubresourceLayout(_vulkan_device, return_texture.image, &subres, &layout);

			err = vkMapMemory(_vulkan_device, return_texture.device_memory, 0, return_texture.memory_allocation_info.allocationSize, 0, &data);
			assert(!err);

			/*
			if (!loadTexture(filename, data, &layout, &tex_width, &tex_height)) {
				fprintf(stderr, "Error loading texture: %s\n", filename);
			}
			*/

			// rough n ready
			//uint32_t size = return_texture.width * return_texture.height * 4;
			//memcpy_s(data, size, raw_image.get(), size);
			//memcpy(data, raw_image.data(), size);
			uint8_t * img = (uint8_t *)data;

			for (uint32_t i = 0; i < return_texture.height; ++i) {
				memcpy(img, raw_image.get() + i * return_texture.width * 4, return_texture.width * 4);
				img = img + layout.rowPitch;
			}
			img = (uint8_t *)data;
			for (int i = 0; i < 16; ++i) {
				std::cout << i << ": " << (int)img[i] << std::endl;
			}


			vkUnmapMemory(_vulkan_device, return_texture.device_memory);
		}


		return_texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		set_image_layout(return_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, return_texture.imageLayout, VK_ACCESS_HOST_WRITE_BIT);

		/* setting the image layout does not reference the actual memory so no need
		* to add a mem ref */
		return return_texture;
	}


	void wrapper::demo_setup_cube() {
		VkResult err;

		glm::vec3 eye = { 0.0f, 3.0f, 5.0f };
		glm::vec3 origin = { 0, 0, 0 };
		glm::vec3 up = { 0.0f, 1.0f, 0.0 };

		_projection = glm::perspective(45.0f, 1.0f, 0.1f, 100.0f);
		_view = glm::lookAt(eye, origin, up);
		_model = glm::mat4(1.0f);

		glm::mat4x4 MVP, VP;

		struct vktexcube_vs_uniform {
			// Must start with MVP
			float mvp[4][4];
			float position[12 * 3][4];
			float attr[12 * 3][4];
		};

		struct vktexcube_vs_uniform data;

		_VP = _projection * _view;
		_MVP = _VP * _model;

		memcpy(data.mvp, &_MVP, sizeof(_MVP));
		//    dumpMatrix("MVP", MVP);

		const float g_vertex_buffer_data[] = {
			-1.0f,-1.0f,-1.0f,  // -X side
			-1.0f,-1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f,-1.0f,
			-1.0f,-1.0f,-1.0f,

			-1.0f,-1.0f,-1.0f,  // -Z side
			1.0f, 1.0f,-1.0f,
			1.0f,-1.0f,-1.0f,
			-1.0f,-1.0f,-1.0f,
			-1.0f, 1.0f,-1.0f,
			1.0f, 1.0f,-1.0f,

			-1.0f,-1.0f,-1.0f,  // -Y side
			1.0f,-1.0f,-1.0f,
			1.0f,-1.0f, 1.0f,
			-1.0f,-1.0f,-1.0f,
			1.0f,-1.0f, 1.0f,
			-1.0f,-1.0f, 1.0f,

			-1.0f, 1.0f,-1.0f,  // +Y side
			-1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f,-1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f,-1.0f,

			1.0f, 1.0f,-1.0f,  // +X side
			1.0f, 1.0f, 1.0f,
			1.0f,-1.0f, 1.0f,
			1.0f,-1.0f, 1.0f,
			1.0f,-1.0f,-1.0f,
			1.0f, 1.0f,-1.0f,

			-1.0f, 1.0f, 1.0f,  // +Z side
			-1.0f,-1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f,-1.0f, 1.0f,
			1.0f,-1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
		};

		const float g_uv_buffer_data[] = {
			0.0f, 0.0f,  // -X side
			1.0f, 0.0f,
			1.0f, 1.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f,

			1.0f, 0.0f,  // -Z side
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,

			1.0f, 1.0f,  // -Y side
			1.0f, 0.0f,
			0.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			0.0f, 1.0f,

			1.0f, 1.0f,  // +Y side
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,

			1.0f, 1.0f,  // +X side
			0.0f, 1.0f,
			0.0f, 0.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,

			0.0f, 1.0f,  // +Z side
			0.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
		};

		for (int i = 0; i < 12 * 3; i++) {
			data.position[i][0] = g_vertex_buffer_data[i * 3];
			data.position[i][1] = g_vertex_buffer_data[i * 3 + 1];
			data.position[i][2] = g_vertex_buffer_data[i * 3 + 2];
			data.position[i][3] = 1.0f;
			data.attr[i][0] = g_uv_buffer_data[2 * i];
			data.attr[i][1] = g_uv_buffer_data[2 * i + 1];
			data.attr[i][2] = 0;
			data.attr[i][3] = 0;
		}

		VkBufferCreateInfo buffer_create_info;
		memset(&buffer_create_info, 0, sizeof(buffer_create_info));

		buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		buffer_create_info.size = sizeof(data);

		//vulkan_buffer _cube_buffer;

		err = vkCreateBuffer(_vulkan_device, &buffer_create_info, NULL, &_cube_buffer.buffer);
		assert(!err);

		std::tie(_cube_buffer.device_memory, _cube_buffer.memory_allocation_info) = allocate_buffer_memory(_cube_buffer.buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		uint8_t *pData;

		err = vkMapMemory(_vulkan_device, _cube_buffer.device_memory, 0, _cube_buffer.memory_allocation_info.allocationSize, 0, (void **)&pData);
		assert(!err);

		memcpy(pData, &data, sizeof(data));

		vkUnmapMemory(_vulkan_device, _cube_buffer.device_memory);

		//err = vkBindBufferMemory(_vulkan_device, buffer.buffer, buffer.device_memory, 0);
		//assert(!err);

		_cube_buffer.info.buffer = _cube_buffer.buffer;
		_cube_buffer.info.offset = 0;
		_cube_buffer.info.range = sizeof(data);
	}

	void wrapper::demo_build_render_pass() {
		/*
		const VkDescriptorSetLayoutBinding layout_bindings[2] = {
		[0] =
		{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = NULL,
		},
		[1] =
		{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = DEMO_TEXTURE_COUNT,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = NULL,
		},
		};
		*/

		const VkDescriptorSetLayoutBinding layout_bindings[2] = {
			{
				0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				VK_SHADER_STAGE_VERTEX_BIT,
				NULL,
			},
			{
				1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1, // texture count
				VK_SHADER_STAGE_FRAGMENT_BIT,
				NULL,
			},
		};
		/*
		const VkDescriptorSetLayoutCreateInfo descriptor_layout = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.bindingCount = 2,
		.pBindings = layout_bindings,
		};
		*/

		const VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			NULL,
			0,
			2,
			layout_bindings,
		};

		//VkDescriptorSetLayout descriptor_layout;
		VkResult err;
		err = vkCreateDescriptorSetLayout(_vulkan_device, &descriptor_layout_create_info, NULL, &_descriptor_set_layout);
		assert(!err);

		/*
		typedef struct VkPipelineLayoutCreateInfo {
		VkStructureType                 sType;
		const void*                     pNext;
		VkPipelineLayoutCreateFlags     flags;
		uint32_t                        setLayoutCount;
		const VkDescriptorSetLayout*    pSetLayouts;
		uint32_t                        pushConstantRangeCount;
		const VkPushConstantRange*      pPushConstantRanges;
		} VkPipelineLayoutCreateInfo;
		*/
		const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			NULL,
			0,
			1,
			&_descriptor_set_layout,
			0,
			NULL
		};

		//VkPipelineLayout pipeline_layout;
		err = vkCreatePipelineLayout(_vulkan_device, &pipeline_layout_create_info, NULL,
			&_pipeline_layout);
		assert(!err);

		const VkAttachmentDescription attachments[2] = {
			{
				0,
				_vulkan_format,
				VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				0,
				_depth_format,
				VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};
		/*
		const VkAttachmentReference color_reference = {
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		const VkAttachmentReference depth_reference = {
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};
		*/
		const VkAttachmentReference color_reference = {
			0,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		const VkAttachmentReference depth_reference = {
			1,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};
		/*
		const VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.flags = 0,
			.inputAttachmentCount = 0,
			.pInputAttachments = NULL,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_reference,
			.pResolveAttachments = NULL,
			.pDepthStencilAttachment = &depth_reference,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = NULL,
		};
		const VkRenderPassCreateInfo rp_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = NULL,
			.attachmentCount = 2,
			.pAttachments = attachments,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 0,
			.pDependencies = NULL,
		};*/
		const VkSubpassDescription subpass = {
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			NULL,
			1,
			&color_reference,
			NULL,
			&depth_reference,
			0,
			NULL,
		};
		const VkRenderPassCreateInfo rp_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			NULL,
			0,
			2,
			attachments,
			1,
			&subpass,
			0,
			NULL,
		};

		err = vkCreateRenderPass(_vulkan_device, &rp_info, NULL, &_render_pass);
		assert(!err);
	}

	void wrapper::demo_prepare_pipeline_descriptors() {
		/*
		const VkDescriptorPoolSize type_counts[2] = {
			[0] =
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
		},
		[1] =
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = DEMO_TEXTURE_COUNT,
		},
		};
		*/


		const VkDescriptorPoolSize type_counts[2] = {
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
			},
			{
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1, // texture count
			},
		};
		/*
		const VkDescriptorPoolCreateInfo descriptor_pool = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = NULL,
			.maxSets = 1,
			.poolSizeCount = 2,
			.pPoolSizes = type_counts,
		};
		*/

		const VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			NULL,
			0,
			1,
			2,
			type_counts,
		};

		VkResult err;
		err = vkCreateDescriptorPool(_vulkan_device, &descriptor_pool_create_info, NULL, &_descriptor_pool);
		assert(!err);

		/*
		VkDescriptorSetAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = NULL,
			.descriptorPool = demo->desc_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &demo->desc_layout };
		*/

		VkDescriptorSetAllocateInfo descriptor_set_allocation_info = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			NULL,
			_descriptor_pool,
			1,
			&_descriptor_set_layout
		};

		err = vkAllocateDescriptorSets(_vulkan_device, &descriptor_set_allocation_info, &_descriptor_set);
		assert(!err);

		VkDescriptorImageInfo texture_descriptors[1];
		memset(&texture_descriptors, 0, sizeof(texture_descriptors));

		for (int i = 0; i < 1; i++) { // texture count
			texture_descriptors[i].sampler = _demo_texture.sampler;
			texture_descriptors[i].imageView = _demo_texture.view;
			texture_descriptors[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}

		VkWriteDescriptorSet writes[2];
		memset(&writes, 0, sizeof(writes));

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = _descriptor_set;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &_cube_buffer.info;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = _descriptor_set;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1; // texture count
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = texture_descriptors;

		vkUpdateDescriptorSets(_vulkan_device, 2, writes, 0, NULL);


		VkImageView attachments[2];
		attachments[1] = _depth_view;
		/*
		const VkFramebufferCreateInfo fb_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = NULL,
			.renderPass = demo->render_pass,
			.attachmentCount = 2,
			.pAttachments = attachments,
			.width = demo->width,
			.height = demo->height,
			.layers = 1,
		};
		*/
		
		const VkFramebufferCreateInfo framebuffer_create_info = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			NULL,
			0,
			_render_pass,
			2,
			attachments,
			_surface_width,
			_surface_height,
			1,
		};

		for (int i = 0; i < _swapchain_image_count; i++) {
			attachments[0] = _swapchain_views[i];

			VkFramebuffer framebuffer;
			err = vkCreateFramebuffer(_vulkan_device, &framebuffer_create_info, NULL, &framebuffer);
			assert(!err);

			_swapchain_framebuffers.push_back(framebuffer);
		}
	}

	void wrapper::demo_build_pipeline() {
		VkPipelineVertexInputStateCreateInfo vi;
		VkPipelineInputAssemblyStateCreateInfo ia;
		VkPipelineRasterizationStateCreateInfo rs;
		VkPipelineColorBlendStateCreateInfo cb;
		VkPipelineDepthStencilStateCreateInfo ds;
		VkPipelineViewportStateCreateInfo vp;
		VkPipelineMultisampleStateCreateInfo ms;
		VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkResult err;

		memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
		memset(&dynamicState, 0, sizeof dynamicState);
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables;

		memset(&vi, 0, sizeof(vi));
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		memset(&ia, 0, sizeof(ia));
		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		memset(&rs, 0, sizeof(rs));
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.depthClampEnable = VK_FALSE;
		rs.rasterizerDiscardEnable = VK_FALSE;
		rs.depthBiasEnable = VK_FALSE;
		rs.lineWidth = 1.0f;

		memset(&cb, 0, sizeof(cb));
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		VkPipelineColorBlendAttachmentState att_state[1];
		memset(att_state, 0, sizeof(att_state));
		att_state[0].colorWriteMask = 0xf;
		att_state[0].blendEnable = VK_FALSE;
		cb.attachmentCount = 1;
		cb.pAttachments = att_state;

		memset(&vp, 0, sizeof(vp));
		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.viewportCount = 1;
		dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
		vp.scissorCount = 1;
		dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;

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

		VkPipelineCacheCreateInfo pipeline_cache_create_info;
		memset(&pipeline_cache_create_info, 0, sizeof(pipeline_cache_create_info));
		pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		err = vkCreatePipelineCache(_vulkan_device, &pipeline_cache_create_info, NULL, &_pipeline_cache);
		assert(!err);

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

	void wrapper::demo_perform_first_render(uint32_t swapchain_id) {
		VkClearValue clear_values[2];
		clear_values[0].color.float32[0] = 0.2f;
		clear_values[0].color.float32[1] = 0.2f;
		clear_values[0].color.float32[2] = 0.2f;
		clear_values[0].color.float32[3] = 0.2f;
		clear_values[1].depthStencil = { 1.0f, 0 }; // depth, stencil value
		
/*
		const VkRenderPassBeginInfo rp_begin = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = NULL,
			.renderPass = demo->render_pass,
			.framebuffer = demo->framebuffers[demo->current_buffer],
			.renderArea.offset.x = 0,
			.renderArea.offset.y = 0,
			.renderArea.extent.width = demo->width,
			.renderArea.extent.height = demo->height,
			.clearValueCount = 2,
			.pClearValues = clear_values,
		};
		*/

		const VkRenderPassBeginInfo render_pass_begin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			NULL,
			_render_pass,
			_swapchain_framebuffers[swapchain_id],
			{ {0, 0}, {_surface_width, _surface_height} },
			2,
			clear_values,
		};

		/*
		const VkCommandBufferBeginInfo cmd_buf_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = NULL,
		.flags = 0,
		.pInheritanceInfo = NULL,
		};*/

		const VkCommandBufferBeginInfo command_buffer_info = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			NULL,
			0,
			NULL,
		};

		VkResult err;
		err = vkBeginCommandBuffer(_swapchain_command_buffers[swapchain_id], &command_buffer_info);
		assert(!err);

		/*
		// We can use LAYOUT_UNDEFINED as a wildcard here because we don't care what
		// happens to the previous contents of the image
		VkImageMemoryBarrier image_memory_barrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = NULL,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = demo->buffers[demo->current_buffer].image,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
			*/
		VkImageMemoryBarrier image_memory_barrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			NULL,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			_swapchain_images[swapchain_id],
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};


		vkCmdPipelineBarrier(_swapchain_command_buffers[swapchain_id], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);

		vkCmdBeginRenderPass(_swapchain_command_buffers[swapchain_id], &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

		//VkPipeline pipeline;
		vkCmdBindPipeline(_swapchain_command_buffers[swapchain_id], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
		vkCmdBindDescriptorSets(_swapchain_command_buffers[swapchain_id], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0, 1, &_descriptor_set, 0,
			NULL);
		VkViewport viewport;
		memset(&viewport, 0, sizeof(viewport));
		viewport.height = (float)_surface_height;
		viewport.width = (float)_surface_width;
		viewport.minDepth = (float)0.0f;
		viewport.maxDepth = (float)1.0f;
		vkCmdSetViewport(_swapchain_command_buffers[swapchain_id], 0, 1, &viewport);

		VkRect2D scissor;
		memset(&scissor, 0, sizeof(scissor));
		scissor.extent.width = _surface_width;
		scissor.extent.height = _surface_height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;

		vkCmdSetScissor(_swapchain_command_buffers[swapchain_id], 0, 1, &scissor);
		vkCmdDraw(_swapchain_command_buffers[swapchain_id], 12 * 3, 1, 0, 0);
		vkCmdEndRenderPass(_swapchain_command_buffers[swapchain_id]);
		/*
		VkImageMemoryBarrier prePresentBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = NULL,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
			*/

		VkImageMemoryBarrier pre_present_barrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			NULL,
		    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			_swapchain_images[swapchain_id],
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

		vkCmdPipelineBarrier(_swapchain_command_buffers[swapchain_id], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &pre_present_barrier);

		err = vkEndCommandBuffer(_swapchain_command_buffers[swapchain_id]);
		assert(!err);
	}

	void wrapper::demo_tick() {
		//vkDeviceWaitIdle(_vulkan_device);
		demo_update();

		demo_draw();

		// Wait for work to finish before updating MVP.
		//vkDeviceWaitIdle(_vulkan_device);

		_tick++;
		//if (demo->frameCount != INT_MAX && demo->curFrame == demo->frameCount) {
		//	PostQuitMessage(validation_error);
		//}
	}

	void wrapper::demo_update() {
		glm::mat4x4 MVP, model, VP;
		int matrixSize = sizeof(MVP);
		uint8_t * pData;
		VkResult err;

		// Rotate 22.5 degrees around the Y axis
		model = _model;
		_model = glm::rotate(model, 0.5f, { 0.0f, 1.0f, 0.0f });
		MVP = _VP * _model;
		
		err = vkMapMemory(_vulkan_device, _cube_buffer.device_memory, 0, _cube_buffer.memory_allocation_info.allocationSize, 0, (void **)&pData);
		assert(!err);

		memcpy(pData, (const void *)&MVP[0][0], matrixSize);

		vkUnmapMemory(_vulkan_device, _cube_buffer.device_memory);
		
	}

	void wrapper::demo_draw() {
		VkResult err;
		VkSemaphore imageAcquiredSemaphore, drawCompleteSemaphore;

		VkSemaphoreCreateInfo semaphoreCreateInfo = {
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			NULL,
			0,
		};

		VkFence nullFence = VK_NULL_HANDLE;

		err = vkCreateSemaphore(_vulkan_device, &semaphoreCreateInfo, NULL, &imageAcquiredSemaphore);
		assert(!err);

		err = vkCreateSemaphore(_vulkan_device, &semaphoreCreateInfo, NULL, &drawCompleteSemaphore);
		assert(!err);

		// Get the index of the next available swapchain image:
		uint32_t current_swapchain = 0;
		// no fencing
		err = fpAcquireNextImageKHR(_vulkan_device, _vulkan_swapchain, UINT64_MAX, imageAcquiredSemaphore, (VkFence)0, &current_swapchain);
		
		if (err == VK_ERROR_OUT_OF_DATE_KHR) {
			// demo->swapchain is out of date (e.g. the window was resized) and
			// must be recreated:
			demo_resize();
			demo_draw();
			vkDestroySemaphore(_vulkan_device, imageAcquiredSemaphore, NULL);
			vkDestroySemaphore(_vulkan_device, drawCompleteSemaphore, NULL);
			return;
		} else if (err == VK_SUBOPTIMAL_KHR) {
			// demo->swapchain is not as optimal as it could be, but the platform's
			// presentation engine will still present the image correctly.
		} else {
			assert(!err);
		}

		//flush_command_buffer();

		// Wait for the present complete semaphore to be signaled to ensure
		// that the image won't be rendered to until the presentation
		// engine has fully released ownership to the application, and it is
		// okay to render to the image.

		VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		/*
		VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = NULL,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &imageAcquiredSemaphore,
			.pWaitDstStageMask = &pipe_stage_flags,
			.commandBufferCount = 1,
			.pCommandBuffers =
			&demo->buffers[demo->current_buffer].cmd,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &drawCompleteSemaphore };
			*/

		VkSubmitInfo submit_info = { 
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			NULL,
			1,
			&imageAcquiredSemaphore,
			&pipe_stage_flags,
			1,
			&_swapchain_command_buffers[current_swapchain],
			1,
			&drawCompleteSemaphore };

		err = vkQueueSubmit(_vulkan_queue, 1, &submit_info, nullFence);
		assert(!err);
		/*
		VkPresentInfoKHR present = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = NULL,
			.swapchainCount = 1,
			.pSwapchains = &demo->swapchain,
			.pImageIndices = &demo->current_buffer,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &drawCompleteSemaphore,
		};
		*/
		VkPresentInfoKHR present_info = {
			VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			NULL,
			1,
			&drawCompleteSemaphore,
			1,
			&_vulkan_swapchain,
			&current_swapchain,
			NULL
		};

		// TBD/TODO: SHOULD THE "present" PARAMETER BE "const" IN THE HEADER?
		err = fpQueuePresentKHR(_vulkan_queue, &present_info);
		if (err == VK_ERROR_OUT_OF_DATE_KHR) {
			// demo->swapchain is out of date (e.g. the window was resized) and
			// must be recreated:
			demo_resize();
		} else if (err == VK_SUBOPTIMAL_KHR) {
			// demo->swapchain is not as optimal as it could be, but the platform's
			// presentation engine will still present the image correctly.
		} else {
			assert(!err);
		}

		err = vkQueueWaitIdle(_vulkan_queue);
		assert(err == VK_SUCCESS);

		vkDestroySemaphore(_vulkan_device, imageAcquiredSemaphore, NULL);
		vkDestroySemaphore(_vulkan_device, drawCompleteSemaphore, NULL);
	}

	void wrapper::demo_resize() {

		// In order to properly resize the window, we must re-create the swapchain
		// AND redo the command buffers, etc.
		//
		// First, perform part of the demo_cleanup() function:

		for (uint32_t i = 0; i < _swapchain_image_count; i++) {
			vkDestroyFramebuffer(_vulkan_device, _swapchain_framebuffers[i], NULL);
		}
		_swapchain_framebuffers.clear();
		
		//vkDestroyDescriptorPool(_vulkan_device, _descriptor_pool, NULL);

		//vkDestroyPipeline(_vulkan_device, _pipeline, NULL);
		//vkDestroyPipelineCache(_vulkan_device, _pipeline_cache, NULL);
		//vkDestroyRenderPass(_vulkan_device, _render_pass, NULL);
		//vkDestroyPipelineLayout(_vulkan_device, _pipeline_layout, NULL);
		//vkDestroyDescriptorSetLayout(_vulkan_device, _descriptor_set_layout, NULL);

		/*
		vkDestroyImageView(_vulkan_device, _demo_texture.view, NULL);
		vkDestroyImage(_vulkan_device, _demo_texture.image, NULL);
		vkFreeMemory(_vulkan_device, _demo_texture.device_memory, NULL);
		vkDestroySampler(_vulkan_device, _demo_texture.sampler, NULL);
		*/

		vkDestroyImageView(_vulkan_device, _depth_view, NULL);
		vkDestroyImage(_vulkan_device, _depth_image, NULL);
		vkFreeMemory(_vulkan_device, _depth_device_memory, NULL);

		/*
		vkDestroyBuffer(_vulkan_device, _cube_buffer.buffer, NULL);
		vkFreeMemory(_vulkan_device, _cube_buffer.device_memory, NULL);
		*/

		for (uint32_t i = 0; i < _swapchain_image_count; i++) {
			vkDestroyImageView(_vulkan_device, _swapchain_views[i], NULL);
			vkFreeCommandBuffers(_vulkan_device, _vulkan_command_pool, 1, &_swapchain_command_buffers[i]);
		}

		_swapchain_views.clear();
		_swapchain_command_buffers.clear();
		//vkDestroyCommandPool(_vulkan_device, _vulkan_command_pool, NULL);


		// Second, re-perform the demo_prepare() function, which will re-create the
		// swapchain:

		create_swapchain();

		create_command_buffer();

		create_surface_depth_image();

		//_demo_texture = create_texture("test.png");

		//demo_setup_cube();

		//demo_build_render_pass();

		//demo_build_pipeline();

		create_swapchain_command_buffers();

		demo_prepare_pipeline_descriptors();

		
		for (uint32_t i = 0; i < _swapchain_image_count; i++) {
			demo_perform_first_render(i);
		}
		

		flush_command_buffer();
	}
}