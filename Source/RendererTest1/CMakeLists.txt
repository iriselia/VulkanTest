SET( DEFINE
)
SET( INCLUDE
vulkan
)
SET( LINK
vulkan-1.lib
)

create_project(CONSOLE "${DEFINE}" "${INCLUDE}" "${LINK}")
