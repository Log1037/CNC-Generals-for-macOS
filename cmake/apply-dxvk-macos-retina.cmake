if(NOT DEFINED DXVK_SOURCE_DIR)
  message(FATAL_ERROR "DXVK_SOURCE_DIR is required")
endif()

set(FUNCS_FILE "${DXVK_SOURCE_DIR}/src/wsi/sdl3/wsi_platform_sdl3_funcs.h")
set(WINDOW_FILE "${DXVK_SOURCE_DIR}/src/wsi/sdl3/wsi_window_sdl3.cpp")
set(FF_HEADER_FILE "${DXVK_SOURCE_DIR}/src/d3d9/d3d9_fixed_function.h")
set(FF_DEVICE_FILE "${DXVK_SOURCE_DIR}/src/d3d9/d3d9_device.cpp")
set(FF_COMPILER_FILE "${DXVK_SOURCE_DIR}/src/d3d9/d3d9_fixed_function.cpp")
set(D3D8_DEVICE_FILE "${DXVK_SOURCE_DIR}/src/d3d8/d3d8_device.cpp")

file(READ "${FUNCS_FILE}" FUNCS_TEXT)
if(NOT FUNCS_TEXT MATCHES "SDL_GetWindowSizeInPixels")
  string(REPLACE
    "SDL_PROC(bool, SDL_GetWindowSize, (SDL_Window*, int*, int*))"
    "SDL_PROC(bool, SDL_GetWindowSize, (SDL_Window*, int*, int*))\nSDL_PROC(bool, SDL_GetWindowSizeInPixels, (SDL_Window*, int*, int*))"
    FUNCS_TEXT "${FUNCS_TEXT}")
  file(WRITE "${FUNCS_FILE}" "${FUNCS_TEXT}")
endif()

file(READ "${WINDOW_FILE}" WINDOW_TEXT)
if(NOT WINDOW_TEXT MATCHES "SDL_GetWindowSizeInPixels\\(window")
  string(REPLACE
    "SDL_GetWindowSize(window, &w, &h)"
    "SDL_GetWindowSizeInPixels(window, &w, &h)"
    WINDOW_TEXT "${WINDOW_TEXT}")
  string(REPLACE
    "SDL3 WSI: SDL_GetWindowSize: "
    "SDL3 WSI: SDL_GetWindowSizeInPixels: "
    WINDOW_TEXT "${WINDOW_TEXT}")
  file(WRITE "${WINDOW_FILE}" "${WINDOW_TEXT}")
endif()

# DXVK 2.6 computes COLOR1 from specular lighting even when specular lighting is
# disabled. Some SAGE fixed-function materials still consume D3DTA_SPECULAR as
# a generic second vertex color, so the old behavior turns those materials black.
# This is the minimal backport of upstream DXVK commit 95d591a8d3f2.
file(READ "${FF_HEADER_FILE}" FF_HEADER_TEXT)
if(NOT FF_HEADER_TEXT MATCHES "SpecularEnabled")
  string(REPLACE
    "        uint32_t LightCount : 4;"
    "        uint32_t LightCount : 4;\n        uint32_t SpecularEnabled : 1;"
    FF_HEADER_TEXT "${FF_HEADER_TEXT}")
  file(WRITE "${FF_HEADER_FILE}" "${FF_HEADER_TEXT}")
endif()

file(READ "${FF_DEVICE_FILE}" FF_DEVICE_TEXT)
if(NOT FF_DEVICE_TEXT MATCHES "case D3DRS_SPECULARENABLE:[^;]+DirtyFFPixelShader[^;]+DirtyFFVertexShader")
  string(REPLACE
    "        case D3DRS_SPECULARENABLE:\n          m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);\n          break;"
    "        case D3DRS_SPECULARENABLE:\n          m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);\n          m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);\n          break;"
    FF_DEVICE_TEXT "${FF_DEVICE_TEXT}")
endif()
if(NOT FF_DEVICE_TEXT MATCHES "Contents.SpecularEnabled")
  string(REPLACE
    "      key.Data.Contents.EmissiveSource   = m_state.renderStates[D3DRS_EMISSIVEMATERIALSOURCE] & mask;"
    "      key.Data.Contents.EmissiveSource   = m_state.renderStates[D3DRS_EMISSIVEMATERIALSOURCE] & mask;\n      key.Data.Contents.SpecularEnabled  = m_state.renderStates[D3DRS_SPECULARENABLE];"
    FF_DEVICE_TEXT "${FF_DEVICE_TEXT}")
endif()
file(WRITE "${FF_DEVICE_FILE}" "${FF_DEVICE_TEXT}")

file(READ "${FF_COMPILER_FILE}" FF_COMPILER_TEXT)
if(NOT FF_COMPILER_TEXT MATCHES "Contents.SpecularEnabled[^;]+finalColor1[^;]+m_vs.in.COLOR\\[1\\]")
  string(REPLACE
    "      m_module.opStore(m_vs.out.COLOR[1], finalColor1);"
    "      m_module.opStore(m_vs.out.COLOR[1],\n        m_vsKey.Data.Contents.SpecularEnabled\n        ? finalColor1\n        : m_vs.in.COLOR[1]);"
    FF_COMPILER_TEXT "${FF_COMPILER_TEXT}")
  file(WRITE "${FF_COMPILER_FILE}" "${FF_COMPILER_TEXT}")
endif()

# SAGE's fixed-function pipeline calls SetRenderState(D3DRS_PATCHSEGMENTS, ...)
# once per material Apply() (Core/Libraries/Source/WWVegas/WW3D2/shader.cpp),
# i.e. potentially many times per frame. DXVK 2.6 logs an unconditional warning
# on every call since the state is unimplemented, which under high call volume
# serializes the render thread on synchronous log I/O and tanks frame rate
# (observed ~10 logic fps during a cinematic that calls this every frame).
# Log it once per process instead of once per call; behavior is unchanged.
file(READ "${D3D8_DEVICE_FILE}" D3D8_DEVICE_TEXT)
if(NOT D3D8_DEVICE_TEXT MATCHES "s_loggedPatchSegmentsWarning")
  string(REPLACE
    "      case D3DRS_PATCHSEGMENTS:\n        Logger::warn(\"D3D8Device::SetRenderState: Unimplemented render state D3DRS_PATCHSEGMENTS\");\n        m_patchSegments = bit::cast<float>(Value);\n        return D3D_OK;"
    "      case D3DRS_PATCHSEGMENTS: {\n        // SetRenderState already holds D3D8DeviceLock at this point, so a plain\n        // static bool is sufficient to serialize this one-time log write.\n        static bool s_loggedPatchSegmentsWarning = false;\n        if (!s_loggedPatchSegmentsWarning) {\n          s_loggedPatchSegmentsWarning = true;\n          Logger::warn(\"D3D8Device::SetRenderState: Unimplemented render state D3DRS_PATCHSEGMENTS (logged once)\");\n        }\n        m_patchSegments = bit::cast<float>(Value);\n        return D3D_OK;\n      }"
    D3D8_DEVICE_TEXT "${D3D8_DEVICE_TEXT}")
  file(WRITE "${D3D8_DEVICE_FILE}" "${D3D8_DEVICE_TEXT}")
endif()

message(STATUS "DXVK macOS patches applied: Retina drawable pixels, fixed-function COLOR1 passthrough, and PATCHSEGMENTS log dedup")
