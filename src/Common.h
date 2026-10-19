#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <cstdint>
using Microsoft::WRL::ComPtr;
inline void Fail(const char* msg) { MessageBoxA(nullptr, msg, "FrameInterp", MB_ICONERROR | MB_OK); }
