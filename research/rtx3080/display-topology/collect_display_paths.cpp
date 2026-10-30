// SPDX-License-Identifier: GPL-2.0-only
// Read-only Windows display-path inventory. No D3D device or GPU workload.
// Only numeric adapter IDs and fixed labels leave the process; LUIDs remain local.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT32 query_flags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
constexpr UINT32 max_paths = 64;
constexpr UINT32 max_modes = 256;
constexpr unsigned max_attempts = 3;

struct Failure {
    const char* stage;
    const char* domain;
    std::uint32_t code;
};

bool same_adapter(LUID a, LUID b) {
    return a.HighPart == b.HighPart && a.LowPart == b.LowPart;
}

const char* boolean(bool value) {
    return value ? "true" : "false";
}

std::string hex4(UINT value) {
    std::ostringstream text;
    text << std::hex << std::setfill('0') << std::setw(4) << value;
    return text.str();
}

const char* connection_name(DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY value) {
    switch (value) {
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15: return "analog_vga";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DVI: return "dvi";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI: return "hdmi";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS: return "lvds";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EXTERNAL: return "displayport_external";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED: return "displayport_embedded";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_MIRACAST: return "miracast";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INDIRECT_WIRED: return "indirect_wired";
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INDIRECT_VIRTUAL: return "indirect_virtual";
    default: return "other_or_unmapped";
    }
}

DXGI_ADAPTER_DESC1 describe_adapter(IDXGIFactory4* factory, LUID id) {
    ComPtr<IDXGIAdapter1> adapter;
    HRESULT result = factory->EnumAdapterByLuid(id, IID_PPV_ARGS(&adapter));
    if (FAILED(result)) {
        throw Failure{"EnumAdapterByLuid", "hresult", static_cast<std::uint32_t>(result)};
    }
    DXGI_ADAPTER_DESC1 description{};
    result = adapter->GetDesc1(&description);
    if (FAILED(result)) {
        throw Failure{"GetDesc1", "hresult", static_cast<std::uint32_t>(result)};
    }
    if (!same_adapter(description.AdapterLuid, id)) {
        throw Failure{"adapter_identity_changed", "collector", 1};
    }
    return description;
}

void write_identity(std::ostream& out, const DXGI_ADAPTER_DESC1& description) {
    const bool pci = description.VendorId <= 0xffff && description.DeviceId <= 0xffff;
    const bool expected = description.VendorId == 0x10de &&
                          description.DeviceId == 0x2216 &&
                          description.SubSysId == 0x403f1458;
    out << "{\"vendor_id\":" << description.VendorId
        << ",\"device_id\":" << description.DeviceId
        << ",\"subsystem_id\":" << description.SubSysId
        << ",\"pci_device_id\":";
    if (pci) {
        out << '"' << hex4(description.VendorId) << ':' << hex4(description.DeviceId) << '"';
    } else {
        out << "null";
    }
    out << ",\"pci_subsystem_id\":";
    if (pci) {
        out << '"' << hex4(description.SubSysId & 0xffff)
            << ':' << hex4(description.SubSysId >> 16) << '"';
    } else {
        out << "null";
    }
    out << ",\"software_adapter\":"
        << boolean((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        << ",\"matches_inventory_gpu_ids\":" << boolean(expected) << '}';
}

void collect() {
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    UINT32 path_count = 0;
    UINT32 mode_count = 0;
    LONG result = ERROR_INSUFFICIENT_BUFFER;
    unsigned attempt = 0;

    for (attempt = 1; attempt <= max_attempts; ++attempt) {
        result = GetDisplayConfigBufferSizes(query_flags, &path_count, &mode_count);
        if (result != ERROR_SUCCESS) {
            throw Failure{"GetDisplayConfigBufferSizes", "win32", static_cast<std::uint32_t>(result)};
        }
        if (path_count > max_paths || mode_count > max_modes) {
            throw Failure{"topology_exceeds_application_limit", "collector", 2};
        }
        paths.assign(path_count == 0 ? 1 : path_count, {});
        modes.assign(mode_count == 0 ? 1 : mode_count, {});
        result = QueryDisplayConfig(query_flags, &path_count, paths.data(),
                                    &mode_count, modes.data(), nullptr);
        if (result == ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }
        if (result != ERROR_SUCCESS) {
            throw Failure{"QueryDisplayConfig", "win32", static_cast<std::uint32_t>(result)};
        }
        if (path_count > paths.size() || mode_count > modes.size()) {
            throw Failure{"returned_counts_exceed_buffers", "collector", 3};
        }
        paths.resize(path_count);
        break;
    }
    if (result != ERROR_SUCCESS) {
        throw Failure{"topology_changed_during_query", "win32", static_cast<std::uint32_t>(result)};
    }

    ComPtr<IDXGIFactory4> factory;
    const HRESULT factory_result = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(factory_result)) {
        throw Failure{"CreateDXGIFactory1", "hresult", static_cast<std::uint32_t>(factory_result)};
    }

    std::set<std::pair<LONG, DWORD>> targets;
    std::ostringstream rows;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        const auto& path = paths[index];
        const bool same = same_adapter(path.sourceInfo.adapterId, path.targetInfo.adapterId);
        const auto target = describe_adapter(factory.Get(), path.targetInfo.adapterId);
        const auto source = same ? target : describe_adapter(factory.Get(), path.sourceInfo.adapterId);
        targets.emplace(path.targetInfo.adapterId.HighPart, path.targetInfo.adapterId.LowPart);
        if (index != 0) {
            rows << ',';
        }
        rows << "{\"path_index\":" << (index + 1)
             << ",\"path_active\":" << boolean((path.flags & DISPLAYCONFIG_PATH_ACTIVE) != 0)
             << ",\"target_available\":" << boolean(path.targetInfo.targetAvailable != FALSE)
             << ",\"source_target_same_adapter\":" << boolean(same)
             << ",\"video_output_technology\":"
             << static_cast<std::uint32_t>(path.targetInfo.outputTechnology)
             << ",\"connection_type\":\"" << connection_name(path.targetInfo.outputTechnology)
             << "\",\"source_adapter\":";
        write_identity(rows, source);
        rows << ",\"target_adapter\":";
        write_identity(rows, target);
        rows << '}';
    }

    // Mode records are required buffers, but no mode-index union is interpreted.
    // Never serialize Description, AdapterLuid, device paths or monitor identifiers.
    std::cout << "{\"schema_version\":1,\"status\":\"observed\""
              << ",\"query_flags\":" << query_flags
              << ",\"query_attempts\":" << attempt
              << ",\"active_path_count\":" << paths.size()
              << ",\"returned_mode_count\":" << mode_count
              << ",\"unique_target_adapter_count\":" << targets.size()
              << ",\"paths\":[" << rows.str() << ']'
              << ",\"graphics_workload_submitted\":false"
              << ",\"display_configuration_changed\":false"
              << ",\"raw_device_paths_included\":false"
              << ",\"adapter_luids_included\":false"
              << ",\"monitor_identifiers_included\":false}\n";
}
} // namespace

int main(int argc, char**) {
    try {
        if (argc != 1) {
            throw Failure{"unexpected_arguments", "collector", 4};
        }
        collect();
        return 0;
    } catch (const Failure& failure) {
        std::cout << "{\"schema_version\":1,\"status\":\"failed\",\"stage\":\""
                  << failure.stage << "\",\"error_domain\":\"" << failure.domain
                  << "\",\"error_code\":" << failure.code << "}\n";
    } catch (...) {
        std::cout << "{\"schema_version\":1,\"status\":\"failed\","
                     "\"stage\":\"unexpected_exception\"}\n";
    }
    return 1;
}
