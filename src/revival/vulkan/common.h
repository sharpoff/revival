#pragma once

#include <revival/math/math.h>
#include <volk.h>
#include <assert.h>

#define API_VERSION VK_API_VERSION_1_3

#define VK_CHECK(code) \
    do { \
        VkResult result_ = (code); \
        assert(result_ == VK_SUCCESS); \
    } while (0)
