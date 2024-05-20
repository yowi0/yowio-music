#include "main.h"

int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"yowio music", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"yowio music", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME, 100, 100, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Fonts
    ImFont* fontNormal = io.Fonts->AddFontFromMemoryTTF(&robotoNormal, sizeof(&robotoNormal), 20.f, nullptr);
    ImFont* fontBold = io.Fonts->AddFontFromMemoryTTF(&robotoBold, sizeof(robotoBold), 20.f, nullptr);
    ImFont* fontSmall = io.Fonts->AddFontFromMemoryTTF(&robotoNormal, sizeof(robotoNormal), 14.f, nullptr);
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFont* fontIcons = io.Fonts->AddFontFromMemoryTTF(&fontAwesomeIcons, sizeof(fontAwesomeIcons), 16.f, nullptr, icons_ranges);
    ImFont* fontIconsBig = io.Fonts->AddFontFromMemoryTTF(&fontAwesomeIcons, sizeof(fontAwesomeIcons), 50.f, nullptr, icons_ranges);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // SFML music setup
    music.setVolume(sfmlVolume);
    RefreshPathSongs(songPath);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        // Get size of the window
        RECT rect;
        GetClientRect(hwnd, &rect);
        int windowWidth = rect.right - rect.left;
        int windowHeight = rect.bottom - rect.top;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight)));
            ImGui::Begin("yowio music", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
            {
                if (!music.getStatus() == sf::Music::Playing)
                    playing = false;

                ImGui::Columns(2);
                {
                    ImGui::BeginGroup();
                    {
                        ImGui::PushFont(fontBold);
                        ImGui::Text("Downloader");
                        ImGui::PopFont();

                        ImGui::BeginListBox("##urls", ImVec2(ImGui::GetColumnWidth() - 14, 300));
                        {
                            for (int i = 0; i < addedUrl.size(); ++i)
                            {
                                ImGui::PushFont(fontIcons);
                                ImGui::Text(ICON_FA_LINK);
                                ImGui::PopFont();

                                ImGui::SameLine();

                                ImGui::Text(addedUrl[i].c_str());

                                ImGui::SameLine();

                                ImGui::PushFont(fontIcons);
                                std::string buttonId = "##" + std::to_string(i);
                                ImVec2 minusButtonSize = ImVec2(20, 20);
                                if (ImGui::Button((ICON_FA_MINUS + buttonId).c_str(), minusButtonSize))
                                {
                                    addedUrl.erase(addedUrl.begin() + i);
                                    i--;
                                }
                                ImGui::PopFont();
                            }
                        }
                        ImGui::EndListBox();

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));
                        ImGui::InputText("##url", downloadUrl, sizeof(downloadUrl));
                        ImGui::PopStyleVar();

                        ImGui::SameLine();

                        ImVec2 plusButtonSize = ImVec2(30, 20);
                        ImGui::PushFont(fontIcons);
                        if (ImGui::Button(ICON_FA_PLUS, plusButtonSize))
                        {
                            addedUrl.emplace_back(downloadUrl);
                            downloadUrl[0] = '\0';
                        }
                        ImGui::PopFont();

                        ImGui::SameLine();

                        ImVec2 downloadButtonSize = ImVec2(30, 20);
                        ImGui::PushFont(fontIcons);
                        if (ImGui::Button(ICON_FA_DOWNLOAD, downloadButtonSize))
                        {
                            for (int i = 0; i < addedUrl.size(); ++i)
                            {
                                Downloader download(addedUrl[i], "yt-dlp.exe");
                                download.quickDownload();
                                addedUrl.erase(addedUrl.begin() + i);
                                i--;
                            }

                            RefreshPathSongs(songPath);
                        }
                        ImGui::PopFont();
                    }
                    ImGui::EndGroup();
                }
                ImGui::NextColumn();
                {
                    ImGui::BeginGroup();
                    {
                        ImGui::PushFont(fontBold);
                        ImGui::Text("Contents");
                        ImGui::PopFont();
                        ImGui::BeginListBox("##nav", ImVec2(ImGui::GetColumnWidth() - 13, 300));
                        {
                            if (!navigationStack.empty())
                            {
                                ImGui::PushFont(fontIcons);
                                ImGui::Text(ICON_FA_FOLDER_OPEN);
                                ImGui::PopFont();

                                ImGui::SameLine();

                                if (ImGui::Selectable("..", false))
                                {
                                    navigationStack.pop_back();
                                    std::string newPath = "./songs";
                                    for (const auto& dir : navigationStack)
                                        newPath += "/" + dir;
                                    RefreshPathSongs(newPath);
                                }
                            }

                            for (int i = 0; i < pathList.size(); ++i)
                            {
                                ImGui::PushFont(fontIcons);
                                ImGui::Text(ICON_FA_FOLDER);
                                ImGui::PopFont();

                                ImGui::SameLine();

                                if (ImGui::Selectable(pathList[i].c_str(), false))
                                {
                                    navigationStack.push_back(pathList[i]);
                                    std::string newPath = "./songs";
                                    for (const auto& dir : navigationStack)
                                        newPath += "/" + dir;
                                    RefreshPathSongs(newPath);
                                }
                            }

                            for (int i = 0; i < songList.size(); ++i)
                            {
                                ImGui::PushFont(fontIcons);
                                ImGui::Text(ICON_FA_MUSIC);
                                ImGui::PopFont();

                                ImGui::SameLine();

                                if (ImGui::Selectable(songList[i].c_str(), i == songIndex))
                                {
                                    songIndex = i;
                                    std::string newPath = "./songs";
                                    for (const auto& dir : navigationStack)
                                        newPath += "/" + dir;

                                    if (!music.openFromFile(newPath + "/" + songList[songIndex]))
                                    {
                                        return -1;
                                    }
                                    music.play();
                                    playing = true;
                                }
                            }
                        }
                        ImGui::EndListBox();
                    }
                    ImGui::EndGroup();
                }
                ImGui::Columns();

                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 60);
                ImGui::Columns(3);
                {
                    ImGui::PushFont(fontIconsBig);
                    ImGui::Text(ICON_FA_QRCODE);
                    ImGui::PopFont();

                    ImGui::SameLine();

                    ImGui::BeginGroup();
                    ImGui::PushFont(fontBold);
                    ImGui::Text("Song name");
                    ImGui::PopFont();
                    ImGui::Text("Authors...");
                    ImGui::EndGroup();
                }
                ImGui::NextColumn();
                {
                    ImGui::BeginGroup();
                    {
                        ImVec2 backButtonSize = ImVec2(30, 23);
                        ImVec2 nextButtonSize = ImVec2(30, 23);
                        ImVec2 playButtonSize = ImVec2(40, 25);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 100.f);
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x * 0.5f) - (backButtonSize.x + nextButtonSize.x + playButtonSize.x) / 2);
                        ImGui::PushFont(fontIcons);
                        if (ImGui::Button(ICON_FA_BACKWARD, backButtonSize))
                        {

                        }
                        ImGui::SameLine();
                        if (ImGui::Button(playing ? ICON_FA_PAUSE : ICON_FA_PLAY, playButtonSize))
                        {
                            if (songIndex != -1)
                            {
                                if (playing)
                                {
                                    music.pause();
                                    playing = false;
                                }
                                else
                                {
                                    music.play();
                                    playing = true;
                                }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(ICON_FA_FORWARD, nextButtonSize))
                        {

                        }
                        ImGui::PopFont();
                        ImGui::PopStyleVar();
                    }
                    ImGui::EndGroup();

                    ImGui::BeginGroup();
                    {
                        ImGui::PushFont(fontSmall);
                        ImGui::Text("%02d:%02d", static_cast<int>(music.getPlayingOffset().asSeconds()) / 60, static_cast<int>(music.getPlayingOffset().asSeconds()) % 60);
                        ImGui::PopFont();

                        ImGui::SameLine();

                        float currentProgress = music.getPlayingOffset().asSeconds() / music.getDuration().asSeconds();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));
                        ImGui::PushFont(fontSmall);
                        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("00:00").x) - 5);
                        ImGui::PopFont();
                        if (ImGui::SliderFloat("##progrss", &progress, 0.0f, 1.0f, "", ImGuiSliderFlags_NoInput))
                        {
                            sf::Time newTime = sf::seconds(progress * music.getDuration().asSeconds());
                            music.setPlayingOffset(newTime);
                        }
                        if (!ImGui::IsItemActive())
                        {
                            progress = currentProgress;
                        }
                        ImGui::PopStyleVar();

                        ImGui::SameLine();

                        ImGui::PushFont(fontSmall);
                        ImGui::Text("%02d:%02d", static_cast<int>(music.getDuration().asSeconds()) / 60, static_cast<int>(music.getDuration().asSeconds()) % 60);
                        ImGui::PopFont();
                    }
                    ImGui::EndGroup();
                }
                ImGui::NextColumn();
                {
                    ImGui::BeginGroup();
                    {
                        ImVec2 volumeButtonSize = ImVec2(30, 20);
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - volumeButtonSize.x) - ImGui::GetColumnWidth() + 100);
                        ImGui::PushFont(fontIcons);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 100.f);
                        if (ImGui::Button(muted ? ICON_FA_VOLUME_OFF : ICON_FA_VOLUME_UP, volumeButtonSize))
                        {
                            if (muted)
                            {
                                music.setVolume(sfmlVolume);
                                muted = false;
                            }
                            else
                            {
                                music.setVolume(0.0f);
                                muted = true;
                            }
                        }
                        ImGui::PopStyleVar();
                        ImGui::PopFont();

                        ImGui::SameLine();

                        ImGui::SetNextItemWidth(ImGui::GetColumnWidth() - 100);
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));
                        if (ImGui::SliderFloat("##volume", &sfmlVolume, 0.0f, 100.0f, "", ImGuiSliderFlags_NoInput))
                        {
                            if (!muted)
                                music.setVolume(sfmlVolume);
                        }
                        ImGui::PopStyleVar();
                    }
                    ImGui::EndGroup();
                }
                ImGui::Columns();
            }
            ImGui::End();
        }

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, NULL, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);

        // Handle loss of D3D9 device
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}