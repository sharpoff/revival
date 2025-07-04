// vma
#define VK_NO_PROTOTYPES
#define VMA_IMPLEMENTATION
// To debug allocations uncomment this
// #define VMA_DEBUG_LOG_FORMAT(format, ...) do { \
//     printf((format), __VA_ARGS__); \
//     printf("\n"); \
// } while(false)
#include <vk_mem_alloc.h>

// volk
#define VOLK_IMPLEMENTATION
#include <volk.h>

// stb
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>