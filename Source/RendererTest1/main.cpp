//*
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <future>
#include <regex>
#include <vector>
#include <assert.h>
//*/
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include "vulkan/vulkan.h"

struct SwapChainBuffer
{
	VkImage image;
	VkImageView view;
};

using std::vector;

struct SwapChain
{
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;
	VkFormat colorFormat;
	VkColorSpaceKHR colorSpace;
	VkSwapchainKHR swapChain;
	vector<VkImage> images;
	vector<SwapChainBuffer> buffers;
	size_t nodeIndex;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
	PFN_vkQueuePresentKHR fpQueuePresentKHR;
};

void *swapChainGetInstanceProc(SwapChain *swapChain, const char *name)
{
	return vkGetInstanceProcAddr(swapChain->instance, name);
}
void *swapChainGetDeviceProc(SwapChain *swapChain, const char *name)
{
	return vkGetDeviceProcAddr(swapChain->device, name);
}

#define GET_IPROC(NAME) \
    do { \
        *(void **)&swapChain->fp##NAME = swapChainGetInstanceProc(swapChain, "vk" #NAME); \
        if (!swapChain->fp##NAME) { \
            fprintf(stderr, "Failed to get Vulkan instance procedure: `%s'\n", #NAME); \
            return false; \
        } \
    } while (0)

#define GET_DPROC(NAME) \
    do { \
        *(void **)&swapChain->fp##NAME = swapChainGetDeviceProc(swapChain, "vk" #NAME); \
        if (!swapChain->fp##NAME) { \
            fprintf(stderr, "Failed to get Vulkan device procedure: `%s'\n", #NAME); \
            return false; \
        } \
    } while (0)

bool swapChainConnect(SwapChain *swapChain,
	VkInstance instance,
	VkPhysicalDevice physicalDevice,
	VkDevice device)
{
	swapChain->instance = instance;
	swapChain->physicalDevice = physicalDevice;
	swapChain->device = device;

	// Get some instance local procedures
	GET_IPROC(GetPhysicalDeviceSurfaceSupportKHR);
	GET_IPROC(GetPhysicalDeviceSurfaceCapabilitiesKHR);
	GET_IPROC(GetPhysicalDeviceSurfaceFormatsKHR);
	GET_IPROC(GetPhysicalDeviceSurfacePresentModesKHR);

	// Get some device local procedures
	GET_DPROC(CreateSwapchainKHR);
	GET_DPROC(DestroySwapchainKHR);
	GET_DPROC(GetSwapchainImagesKHR);
	GET_DPROC(AcquireNextImageKHR);
	GET_DPROC(QueuePresentKHR);

	return true;
}

#if defined(_WIN32)
bool swapChainPlatformConnect(SwapChain *swapChain,
	HINSTANCE handle,
	HWND window)
#elif defined(__ANDROID__)
bool swapChainPlatformConnect(SwapChain *swapChain,
	ANativeWindow *window)
#else
bool swapChainPlatformConnect(SwapChain *swapChain,
	xcb_connection_t *connection,
	xcb_window_t *window)
#endif
{
	VkResult result;
#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = handle;
	surfaceCreateInfo.hwnd = window;
	result = vkCreateWin32SurfaceKHR(swapChain->instance,
		&surfaceCreateInfo,
		NULL,
		&swapChain->surface);
#elif defined(__ANDROID__)
	VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.window = window;
	VkResult result = vkCreateAndroidSurfaceKHR(swapChain->instance,
		&surfaceCreateInfo,
		NULL,
		&swapChain->surface);
#else
	VkXcbSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.connection = connection;
	surfaceCreateInfo.window = window;
	VkResult result = vkCreateXcbSurfaceKHR(swapChain->instance,
		&surfaceCreateInfo,
		NULL,
		&swapChain->surface);
#endif
	return result == VK_SUCCESS;
}

#if defined(_WIN32)
bool swapChainInit(SwapChain *swapChain,
	HINSTANCE handle,
	HWND window)
#elif defined(__ANDROID__)
bool swapChainInit(SwapChain *swapChain,
	ANativeWindow *window)
#else
bool swapChainInit(SwapChain *swapChain,
	xcb_connection_t *connection,
	xcb_window_t *window)
#endif
{
#if defined(_WIN32)
	if(!swapChainPlatformConnect(swapChain, handle, window))
		return false;
#elif defined(__ANDROID__)
	if(!swapChainPlatformConnect(swapChain, window))
		return false;
#else
	if(!swapChainPlatformConnect(swapChain, connection, window))
		return false;
#endif

	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(swapChain->physicalDevice,
		&queueCount,
		NULL);
	if(queueCount == 0)
		return false;

	vector<VkQueueFamilyProperties> queueProperties(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(swapChain->physicalDevice,
		&queueCount,
		&queueProperties[0]);

	// In previous tutorials we just picked which ever queue was readily
	// available. The problem is not all queues support presenting. Here
	// we make use of vkGetPhysicalDeviceSurfaceSupportKHR to find queues
	// with present support.
	vector<VkBool32> supportsPresent(queueCount);
	for(uint32_t i = 0; i < queueCount; i++)
	{
		swapChain->fpGetPhysicalDeviceSurfaceSupportKHR(swapChain->physicalDevice,
			i,
			swapChain->surface,
			&supportsPresent[i]);
	}

	// Now we have a list of booleans for which queues support presenting.
	// We now must walk the queue to find one which supports
	// VK_QUEUE_GRAPHICS_BIT and has supportsPresent[index] == VK_TRUE
	// (indicating it supports both.)
	uint32_t graphicIndex = UINT32_MAX;
	uint32_t presentIndex = UINT32_MAX;
	for(uint32_t i = 0; i < queueCount; i++)
	{
		if((queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			if(graphicIndex == UINT32_MAX)
				graphicIndex = i;
			if(supportsPresent[i] == VK_TRUE)
			{
				graphicIndex = i;
				presentIndex = i;
				break;
			}
		}
	}

	if(presentIndex == UINT32_MAX)
	{
		// If there is no queue that supports both present and graphics;
		// try and find a separate present queue. They don't necessarily
		// need to have the same index.
		for(uint32_t i = 0; i < queueCount; i++)
		{
			if(supportsPresent[i] != VK_TRUE)
				continue;
			presentIndex = i;
			break;
		}
	}

	// If neither a graphics or presenting queue was found then we cannot
	// render
	if(graphicIndex == UINT32_MAX || presentIndex == UINT32_MAX)
		return false;

	// In a future tutorial we'll look at supporting a separate graphics
	// and presenting queue
	if(graphicIndex != presentIndex)
	{
		fprintf(stderr, "Not supported\n");
		return false;
	}

	swapChain->nodeIndex = graphicIndex;

	// Now get a list of supported surface formats, as covered in the
	// previous tutorial
	uint32_t formatCount;
	if(swapChain->fpGetPhysicalDeviceSurfaceFormatsKHR(swapChain->physicalDevice,
		swapChain->surface,
		&formatCount,
		NULL) != VK_SUCCESS)
		return false;
	if(formatCount == 0)
		return false;
	vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	if(swapChain->fpGetPhysicalDeviceSurfaceFormatsKHR(swapChain->physicalDevice,
		swapChain->surface,
		&formatCount,
		&surfaceFormats[0]) != VK_SUCCESS)
		return false;

	// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
	// the surface has no preferred format. Otherwise, at least one
	// supported format will be returned
	if(formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
		swapChain->colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
	else
	{
		if(formatCount == 0)
			return false;
		swapChain->colorFormat = surfaceFormats[0].format;
	}
	swapChain->colorSpace = surfaceFormats[0].colorSpace;
	return true;
}

// This lovely piece of code is from SaschaWillems, check out his Vulkan repositories on
// GitHub if you're feeling adventurous.
void setImageLayout(VkCommandBuffer commandBuffer,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout)
{
	VkImageMemoryBarrier imageMemoryBarrier; // :E
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange.aspectMask = aspectMask;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	// Undefined layout:
	//   Note: Only allowed as initial layout!
	//   Note: Make sure any writes to the image have been finished
	if(oldImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

	// Old layout is color attachment:
	//   Note: Make sure any writes to the color buffer have been finished
	if(oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// Old layout is transfer source:
	//   Note: Make sure any reads from the image have been finished
	if(oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	// Old layout is shader read (sampler, input attachment):
	//   Note: Make sure any shader reads from the image have been finished
	if(oldImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;

	// New layout is transfer destination (copy, blit):
	//   Note: Make sure any copyies to the image have been finished
	if(newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	// New layout is transfer source (copy, blit):
	//   Note: Make sure any reads from and writes to the image have been finished
	if(newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	}

	// New layout is color attachment:
	//   Note: Make sure any writes to the color buffer hav been finished
	if(newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	}

	// New layout is depth attachment:
	//   Note: Make sure any writes to depth/stencil buffer have been finished
	if(newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	// New layout is shader read (sampler, input attachment):
	//   Note: Make sure any writes to the image have been finished
	if(newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}

	// Put barrier inside the setup command buffer
	vkCmdPipelineBarrier(commandBuffer,
		// Put the barriers for source and destination on
		// top of the command buffer
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &imageMemoryBarrier);
}

bool swapChainCreate(SwapChain *swapChain,
	VkCommandBuffer commandBuffer,
	uint32_t *width,
	uint32_t *height)
{
	VkSwapchainKHR oldSwapChain = swapChain->swapChain;

	// Get physical device surface properties and formats. This was not
	// covered in previous tutorials. Effectively does what it says it does.
	// We will be using the result of this to determine the number of
	// images we should use for our swap chain and set the appropriate
	// sizes for them.
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	if(swapChain->fpGetPhysicalDeviceSurfaceCapabilitiesKHR(swapChain->physicalDevice,
		swapChain->surface,
		&surfaceCapabilities) != VK_SUCCESS)
		return false;

	// Also not covered in previous tutorials: used to get the available
	// modes for presentation.
	uint32_t presentModeCount;
	if(swapChain->fpGetPhysicalDeviceSurfacePresentModesKHR(swapChain->physicalDevice,
		swapChain->surface,
		&presentModeCount,
		NULL) != VK_SUCCESS)
		return false;
	if(presentModeCount == 0)
		return false;
	vector<VkPresentModeKHR> presentModes(presentModeCount);
	if(swapChain->fpGetPhysicalDeviceSurfacePresentModesKHR(swapChain->physicalDevice,
		swapChain->surface,
		&presentModeCount,
		&presentModes[0]) != VK_SUCCESS)
		return false;

	// When constructing a swap chain we must supply our surface resolution.
	// Like all things in Vulkan there is a structure for representing this
	VkExtent2D swapChainExtent = {};

	// The way surface capabilities work is rather confusing but width
	// and height are either both -1 or both not -1. A size of -1 indicates
	// that the surface size is undefined, which means you can set it to
	// effectively any size. If the size however is defined, the swap chain
	// size *MUST* match.
	if(surfaceCapabilities.currentExtent.width == -1)
	{
		swapChainExtent.width = *width;
		swapChainExtent.height = *height;
	}
	else
	{
		swapChainExtent = surfaceCapabilities.currentExtent;
		*width = surfaceCapabilities.currentExtent.width;
		*height = surfaceCapabilities.currentExtent.height;
	}

	// Prefer mailbox mode if present
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // always supported
	for(uint32_t i = 0; i < presentModeCount; i++)
	{
		if(presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
		if(presentMode != VK_PRESENT_MODE_MAILBOX_KHR
			&&  presentMode != VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			// The horrible fallback
			presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	// Determine the number of images for our swap chain
	uint32_t desiredImages = surfaceCapabilities.minImageCount + 1;
	if(surfaceCapabilities.maxImageCount > 0
		&& desiredImages > surfaceCapabilities.maxImageCount)
	{
		desiredImages = surfaceCapabilities.maxImageCount;
	}

	// This will be covered in later tutorials
	VkSurfaceTransformFlagsKHR preTransform = surfaceCapabilities.currentTransform;
	if(surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	// Mandatory fields
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = NULL;

	swapChainCreateInfo.surface = swapChain->surface;
	swapChainCreateInfo.minImageCount = desiredImages;
	swapChainCreateInfo.imageFormat = swapChain->colorFormat;
	swapChainCreateInfo.imageColorSpace = swapChain->colorSpace;
	swapChainCreateInfo.imageExtent = { swapChainExtent.width, swapChainExtent.height };
	// This is literally the same as GL_COLOR_ATTACHMENT0
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
	swapChainCreateInfo.imageArrayLayers = 1; // Only one attachment
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // No sharing
	swapChainCreateInfo.queueFamilyIndexCount = 0; // Covered in later tutorials
	swapChainCreateInfo.pQueueFamilyIndices = NULL; // Covered in later tutorials
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = true; // If we want clipping outside the extents

										// Alpha on the window surface should be opaque:
										// If it was not we could create transparent regions of our window which
										// would require support from the Window compositor. You can totally do
										// that if you wanted though ;)
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;


	if(swapChain->fpCreateSwapchainKHR(swapChain->device,
		&swapChainCreateInfo,
		NULL,
		&swapChain->swapChain) != VK_SUCCESS)
		return false;

	// If an existing swap chain is recreated, destroy the old one. This
	// also cleans up all presentable images
	if(oldSwapChain != VK_NULL_HANDLE)
	{
		swapChain->fpDestroySwapchainKHR(swapChain->device,
			oldSwapChain,
			NULL);
	}

	// Now get the presentable images from the swap chain
	uint32_t imageCount;
	if(swapChain->fpGetSwapchainImagesKHR(swapChain->device,
		swapChain->swapChain,
		&imageCount,
		NULL) != VK_SUCCESS)
		goto failed;
	swapChain->images.resize(imageCount);
	if(swapChain->fpGetSwapchainImagesKHR(swapChain->device,
		swapChain->swapChain,
		&imageCount,
		&swapChain->images[0]) != VK_SUCCESS)
	{
	failed:
		swapChain->fpDestroySwapchainKHR(swapChain->device,
			swapChain->swapChain,
			NULL);
		return false;
	}

	// Create the image views for the swap chain. They will all be single
	// layer, 2D images, with no mipmaps.
	// Check the VkImageViewCreateInfo structure to see other views you
	// can potentially create.
	swapChain->buffers.resize(imageCount);
	for(uint32_t i = 0; i < imageCount; i++)
	{
		VkImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = swapChain->colorFormat;
		colorAttachmentView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0; // mandatory

									   // Wire them up
		swapChain->buffers[i].image = swapChain->images[i];
		// Transform images from the initial (undefined) layer to
		// present layout
		setImageLayout(commandBuffer,
			swapChain->buffers[i].image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		colorAttachmentView.image = swapChain->buffers[i].image;
		// Create the view
		if(vkCreateImageView(swapChain->device,
			&colorAttachmentView,
			NULL,
			&swapChain->buffers[i].view) != VK_SUCCESS)
			goto failed;
	}

	return true;
}

VkResult swapChainAcquireNext(SwapChain *swapChain,
	VkSemaphore presentCompleteSemaphore,
	uint32_t *currentBuffer)
{
	return swapChain->fpAcquireNextImageKHR(swapChain->device,
		swapChain->swapChain,
		UINT64_MAX,
		presentCompleteSemaphore,
		(VkFence)0,
		currentBuffer);
}

VkResult swapChainQueuePresent(SwapChain *swapChain,
	VkQueue queue,
	uint32_t currentBuffer)
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain->swapChain;
	presentInfo.pImageIndices = &currentBuffer;
	return swapChain->fpQueuePresentKHR(queue, &presentInfo);
}

void swapChainCleanup(SwapChain *swapChain)
{
	for(auto &it : swapChain->buffers)
		vkDestroyImageView(swapChain->device, it.view, NULL);
	swapChain->fpDestroySwapchainKHR(swapChain->device,
		swapChain->swapChain,
		NULL);
	vkDestroySurfaceKHR(swapChain->instance,
		swapChain->surface,
		NULL);
}

int main()
{

	VkApplicationInfo applicationInfo;
	VkInstanceCreateInfo instanceInfo;
	VkInstance instance;

	// Filling out application description:
	// sType is mandatory
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	// pNext is mandatory
	applicationInfo.pNext = NULL;
	// The name of our application
	applicationInfo.pApplicationName = "Tutorial 1";
	// The name of the engine (e.g: Game engine name)
	applicationInfo.pEngineName = NULL;
	// The version of the engine
	applicationInfo.engineVersion = 1;
	// The version of Vulkan we're using for this application
	applicationInfo.apiVersion = VK_API_VERSION_1_0;

	// Filling out instance description:
	// sType is mandatory
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	// pNext is mandatory
	instanceInfo.pNext = NULL;
	// flags is mandatory
	instanceInfo.flags = 0;
	// The application info structure is then passed through the instance
	instanceInfo.pApplicationInfo = &applicationInfo;
	// Don't enable and layer
	instanceInfo.enabledLayerCount = 0;
	instanceInfo.ppEnabledLayerNames = NULL;
	// Don't enable any extensions
	instanceInfo.enabledExtensionCount = 0;
	instanceInfo.ppEnabledExtensionNames = NULL;

	vector<const char *> enabledExtensions;
	enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
	enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
	enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
	instanceInfo.enabledExtensionCount = enabledExtensions.size();
	instanceInfo.ppEnabledExtensionNames = &enabledExtensions[0];

	// Now create the desired instance
	VkResult result = vkCreateInstance(&instanceInfo, NULL, &instance);
	if(result != VK_SUCCESS)
	{
		fprintf(stderr, "Failed to create instance: %d\n", result);
		abort();
	}

	// To Come Later
	// ...
	// ...

	// Query how many devices are present in the system
	uint32_t deviceCount = 0;
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
	if(result != VK_SUCCESS)
	{
		fprintf(stderr, "Failed to query the number of physical devices present: %d\n", result);
		abort();
	}

	// There has to be at least one device present
	if(deviceCount == 0)
	{
		fprintf(stderr, "Couldn't detect any device present with Vulkan support: %d\n", result);
		abort();
	}

	// Get the physical devices
	vector<VkPhysicalDevice> physicalDevices(deviceCount);
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, &physicalDevices[0]);
	if(result != VK_SUCCESS)
	{
		fprintf(stderr, "Faied to enumerate physical devices present: %d\n", result);
		abort();
	}

	// Enumerate all physical devices
	VkPhysicalDeviceProperties deviceProperties;



	// Print the families
	for(uint32_t i = 0; i < deviceCount; i++)
	{
		memset(&deviceProperties, 0, sizeof deviceProperties);
		vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
		printf("Driver Version: %d\n", deviceProperties.driverVersion);
		printf("Device Name:    %s\n", deviceProperties.deviceName);
		printf("Device Type:    %d\n", deviceProperties.deviceType);
		printf("API Version:    %d.%d.%d\n",
			// See note below regarding this:
			(deviceProperties.apiVersion >> 22) & 0x3FF,
			(deviceProperties.apiVersion >> 12) & 0x3FF,
			(deviceProperties.apiVersion & 0xFFF));

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, NULL);
		vector<VkQueueFamilyProperties> familyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, &familyProperties[0]);

		for(uint32_t j = 0; j < queueFamilyCount; j++)
		{
			printf("Queue family %d:\n", j);
			printf("\t Queue count: %d\n", familyProperties[j].queueCount);
			printf("\t Supported operations:\n");
			if(familyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				printf("\t\t\t Graphics\n");
			if(familyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT)
				printf("\t\t\t Compute\n");
			if(familyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT)
				printf("\t\t\t Transfer\n");
			if(familyProperties[j].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
				printf("\t\t\t Sparse Binding\n");
		}
	}

	VkDeviceCreateInfo deviceInfo;
	// Mandatory fields
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext = NULL;
	deviceInfo.flags = 0;

	// We won't bother with extensions or layers
	deviceInfo.enabledLayerCount = 0;
	deviceInfo.ppEnabledLayerNames = NULL;
	deviceInfo.enabledExtensionCount = 0;
	deviceInfo.ppEnabledExtensionNames = NULL;

	// We don't want any any features,:the wording in the spec for DeviceCreateInfo
	// excludes that you can pass NULL to disable features, which GetPhysicalDeviceFeatures
	// tells you is a valid value. This must be supplied - NULL is legal here.
	deviceInfo.pEnabledFeatures = NULL;

	// Here's where we initialize our queues
	VkDeviceQueueCreateInfo deviceQueueInfo;
	deviceQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueInfo.pNext = NULL;
	deviceQueueInfo.flags = 0;
	// Use the first queue family in the family list
	deviceQueueInfo.queueFamilyIndex = 0;

	// Create only one queue
	float queuePriorities[] = { 1.0f };
	deviceQueueInfo.queueCount = 1;
	deviceQueueInfo.pQueuePriorities = queuePriorities;
	// Set queue(s) into the device
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &deviceQueueInfo;

	VkDevice device;
	result = vkCreateDevice(physicalDevices[0], &deviceInfo, NULL, &device);
	if(result != VK_SUCCESS)
	{
		fprintf(stderr, "Failed creating logical device: %d\n", result);
		abort();
	}

	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = NULL;

	// somewhere in initialization code
	*(void **)&vkGetPhysicalDeviceSurfaceFormatsKHR = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	
	if(!vkGetPhysicalDeviceSurfaceFormatsKHR)
	{
		abort();
	}

	VkSurfaceKHR surface;
	HINSTANCE platformHandle;
	HWND platformWindow;

#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = (HINSTANCE)platformHandle; // provided by the platform code
	surfaceCreateInfo.hwnd = (HWND)platformWindow;           // provided by the platform code
	result = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface);
#elif defined(__ANDROID__)
	VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.window = window;                       // provided by the platform code
	VkResult result = vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface);
#else
	VkXcbSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.connection = connection;               // provided by the platform code
	surfaceCreateInfo.window = window;                       // provided by the platform code
	VkResult result = vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface);
#endif
	if(result != VK_SUCCESS)
	{
		fprintf(stderr, "Failed to create Vulkan surface: %d\n", result);
		abort();
	}

	VkFormat colorFormat;
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevices[0], surface, &formatCount, NULL);
	vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevices[0], surface, &formatCount, &surfaceFormats[0]);

	// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
	// the surface has no preferred format. Otherwise, at least one
	// supported format will be returned
	if(formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
		colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
	else
	{
		assert(formatCount >= 1);
		colorFormat = surfaceFormats[0].format;
	}
	VkColorSpaceKHR	colorSpace = surfaceFormats[0].colorSpace;

	getchar();

	// Never forget to free resources
	vkDestroyInstance(instance, NULL);

	return 0;
}