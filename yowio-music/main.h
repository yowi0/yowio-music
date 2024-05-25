/*		INCLUDES		*/
#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <filesystem>
#include <thread>
#include <algorithm>
#include <random>
#include <chrono>
namespace fs = std::filesystem;

/*      SFML        */
#include <SFML/Audio.hpp>

/*      IMGUI       */
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <d3d9.h>

/*      FONTS       */
#include "icons.h"
#include "roboto.h"

/*      DOWNLOAD        */
#include "downloader.h"

/*		VARIABLES		*/
/*      D3D9 DEVICE     */
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

/*      PLAYER      */
sf::Music music;
bool playingRoot = false;
bool playing = false;
bool muted = false;
bool loop = false;
bool random = false;
float sfmlVolume = 10.f;
float progress = 0.0f;
std::vector<std::string> songList = {};
std::vector<std::string> shuffledSongList;
std::vector<std::string> pathList = {};
std::vector<std::string> navigationStack = {};
int songIndex = -1;
std::string songPath = "./songs";

/*      DOWNLOADER      */
static char downloadUrl[128];
std::vector<std::string> addedUrl = {};
std::atomic<bool> stopWatching(false);

void ShuffleSongList(std::vector<std::string>& list) 
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(list.begin(), list.end(), std::default_random_engine(seed));
}

void RefreshPathSongs(const std::string& path)
{
    songList.clear();
    pathList.clear();

    songPath = path;

    try
    {
        // Verifica si la ruta existe y es un directorio
        if (fs::exists(path) && fs::is_directory(path))
        {
            // Itera sobre el contenido del directorio
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (entry.is_directory())
                    pathList.emplace_back(entry.path().filename().string());
                else if (entry.path().extension() == ".mp3")
                    songList.emplace_back(entry.path().filename().string());
            }
        }
        else
        {
            std::cerr << "La ruta especificada no existe o no es un directorio.\n";
        }
    }
    catch (const fs::filesystem_error& err)
    {
        std::cerr << "Error al acceder al sistema de archivos: " << err.what() << '\n';
    }
}

// Convert a UTF-8 string to a wide string (UTF-16)
std::wstring ConvertToWideString(const std::string& str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}

// Function to watch directory changes
void WatchDirectory(const std::string& path, std::atomic<bool>& stopFlag)
{
    std::wstring wpath = ConvertToWideString(path);

    HANDLE hChange = FindFirstChangeNotification(
        wpath.c_str(),
        FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE
    );

    if (hChange == INVALID_HANDLE_VALUE)
    {
        return;
    }

    while (!stopFlag)
    {
        DWORD waitStatus = WaitForSingleObject(hChange, INFINITE);

        if (waitStatus == WAIT_OBJECT_0)
        {
            // Refresh the directory contents
            RefreshPathSongs(path);

            if (FindNextChangeNotification(hChange) == FALSE)
            {
                break;
            }
        }
    }

    FindCloseChangeNotification(hChange);
}


/*		FUNCTIONS		*/
bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SetSpotifyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Fondo de la ventana
    colors[ImGuiCol_WindowBg] = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);

    // Bordes
    colors[ImGuiCol_Border] = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Título de la ventana
    colors[ImGuiCol_TitleBg] = ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 24.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 24.0f / 255.0f, 1.0f);

    // Menú
    colors[ImGuiCol_MenuBarBg] = ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 24.0f / 255.0f, 1.0f);

    // Barra de scroll
    colors[ImGuiCol_ScrollbarBg] = ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(90.0f / 255.0f, 90.0f / 255.0f, 90.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(120.0f / 255.0f, 120.0f / 255.0f, 120.0f / 255.0f, 1.0f);

    // Botones
    colors[ImGuiCol_Button] = ImVec4(30.0f / 255.0f, 215.0f / 255.0f, 96.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(30.0f / 255.0f, 235.0f / 255.0f, 106.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(30.0f / 255.0f, 255.0f / 255.0f, 116.0f / 255.0f, 1.0f);

    // Marco del checkbox, radio button, etc.
    colors[ImGuiCol_FrameBg] = ImVec4(50.0f / 255.0f, 50.0f / 255.0f, 50.0f / 255.0f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f, 1.0f);

    // Texto
    colors[ImGuiCol_Text] = ImVec4(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(120.0f / 255.0f, 120.0f / 255.0f, 120.0f / 255.0f, 1.0f);

    // Hover y active de la selección
    colors[ImGuiCol_Header] = ImVec4(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 1.0f);

    // Separador
    colors[ImGuiCol_Separator] = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 1.0f);

    // Resaltar
    colors[ImGuiCol_ResizeGrip] = ImVec4(30.0f / 255.0f, 215.0f / 255.0f, 96.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(30.0f / 255.0f, 235.0f / 255.0f, 106.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(30.0f / 255.0f, 255.0f / 255.0f, 116.0f / 255.0f, 1.0f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(50.0f / 255.0f, 50.0f / 255.0f, 50.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(45.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(45.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f, 1.0f);

    // Tooltips
    colors[ImGuiCol_PopupBg] = ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 24.0f / 255.0f, 1.0f);

    // Otros
    colors[ImGuiCol_CheckMark] = ImVec4(30.0f / 255.0f, 215.0f / 255.0f, 96.0f / 255.0f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(30.0f / 255.0f, 215.0f / 255.0f, 96.0f / 255.0f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(30.0f / 255.0f, 235.0f / 255.0f, 106.0f / 255.0f, 1.0f);
}
