SET( DEFINE
)
SET( INCLUDE
C:/VulkanSDK/1.0.11.1/Include
)
SET( LINK
C:/VulkanSDK/1.0.11.1/Bin/vulkan-1.lib
C:/VulkanSDK/1.0.11.1/Bin32/vulkan-1.lib
)

create_project(CONSOLE "${DEFINE}" "${INCLUDE}" "${LINK}")
