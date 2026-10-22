/*
 * MarathonRecomp - PS5 filesystem access policy.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Jailbroken PS5 titles have read/write access to /data by default. Do not
 * probe that access or gate filesystem use on a probe result.
 */
#include "elevation.hpp"

namespace
{
const char *selected_path = "none";
}

elevation::Status elevation::request(Capability capability, const char *helper_path) noexcept
{
    (void)helper_path; // Kept for source compatibility; no helper is needed for /data.
    if (capability != Capability::filesystem)
    {
        selected_path = "none";
        return Status::unsupported_capability;
    }

    selected_path = "native";
    return Status::ok;
}

const char *elevation::path() noexcept
{
    return selected_path;
}
