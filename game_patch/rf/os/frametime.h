#pragma once

#include <patch_common/MemUtils.h>

namespace rf
{
    static auto& current_fps = addr_as_ref<float>(0x005A4018);
    static auto& frametime = addr_as_ref<float>(0x005A4014);
    static auto& frametime_min = addr_as_ref<float>(0x005A4024);
}
