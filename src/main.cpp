#include <Windows.h>
#include <windowsx.h>

#include <thread>
#include <mutex>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

#include "utils/message_box.hpp"

#include "valve/player_pawn.hpp"
#include "memory/memory.hpp"
#include "d3d/device.hpp"

#include "main.hpp"

#include <windower.hpp>
#include <daisy.hpp>

struct player_data_t {
    std::vector<c_cs_player_pawn*> player_list{};
    std::mutex player_mutex{};
};

struct radar_data_t {
    std::vector<vec2_t> radar_points{};
    std::mutex radar_points_mutex{};
};

static void entity_cache_thread(const std::stop_source& st, const c_game* game, player_data_t& players) {
    static constexpr auto ENTITY_ENTRY_SIZE = 0x70;
    std::vector<c_cs_player_pawn*> temp_players{};

    const auto client_base = game->get_client_base();
    if (!client_base) {
        return;
    }

    while (!st.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        const auto entity_list = game->read<uintptr_t>(client_base + offsets::dwEntityList).value_or(0);
        if (!entity_list)
            continue;

        temp_players.clear();
        for (int i = 1; i < 64; i++) {
            const auto list_entity_controller = game->read<uintptr_t>(entity_list + ((8 * (i & 0x7FFF) >> 9) + 16)).value_or(0);
            if (!list_entity_controller)
                continue;

            const auto entity_controller = game->read<uintptr_t>(list_entity_controller + (ENTITY_ENTRY_SIZE) * (i & 0x1FF)).value_or(0);
            if (!entity_controller)
                continue;

            const auto entity_controller_pawn = game->read<uintptr_t>(entity_controller + offsets::m_hPlayerPawn).value_or(0);
            if (!entity_controller_pawn)
                continue;

            const auto list_entity = game->read<uintptr_t>(entity_list + (0x8 * ((entity_controller_pawn & 0x7FFF) >> 9) + 16)).value_or(0);
            if (!list_entity)
                continue;

            const auto entity = game->read<c_cs_player_pawn*>(list_entity + (ENTITY_ENTRY_SIZE) * (entity_controller_pawn & 0x1FF)).value_or(nullptr);
            if (!entity)
                continue;

            temp_players.emplace_back(entity);
        }
        {
            std::unique_lock<std::mutex> lock(players.player_mutex);
            players.player_list = std::move(temp_players);
        }
    }
}

static void entity_data_thread(const std::stop_source& st, c_game* game, player_data_t& players, radar_data_t& radar_data) {
    std::vector<vec2_t> temp_radar_points{};

    const auto client_base = game->get_client_base();
    if (!client_base)
        return;

    while (!st.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        vec3_t viewangles = game->read<vec3_t>(client_base + offsets::dwViewAngles).value();
		if (viewangles.is_zero()) // probably not even in the game
			continue;

        const auto local_pawn = game->read<c_cs_player_pawn*>(client_base + offsets::dwLocalPlayerPawn).value_or(nullptr);
        if (!local_pawn)
            continue;

        const auto local_pos = local_pawn->m_vOldOrigin(game);
        const auto local_team = local_pawn->m_iTeamNum(game);

        vec2_t radar_center{};
        radar_center.x = (static_cast<float>(g_config.size_w) / 2);
        radar_center.y = (static_cast<float>(g_config.size_h) / 2);

        temp_radar_points.clear();
        std::unique_lock<std::mutex> players_lock(players.player_mutex);
        for (const auto& player : players.player_list) {
            if (!player || local_pawn == player || local_team == player->m_iTeamNum(game))
                continue;

            if (player->m_iHealth(game) <= 0)
                continue;

            // Get player radar position.
            const auto origin = player->m_vOldOrigin(game);
            if (origin.is_zero())
                continue;

            vec2_t screen_pos{};
            screen_pos.x = (local_pos.x - origin.x) / 15; //scale. 
            screen_pos.y = (local_pos.y - origin.y) / 15;

            // Prevent inverted x axis position.
            screen_pos.x *= -1;

            // Ensure position starts at center.
            screen_pos.x += radar_center.x;
            screen_pos.y += radar_center.y;

            // Rotate the point based on our yaw.
            auto rotated_point = screen_pos.rotate_point(&radar_center, viewangles.y - 90.f);

            // Prevent player from going out of bounds.
            rotated_point.x = std::clamp(rotated_point.x, 5.f, static_cast<float>(g_config.size_w) - 10.f);
            rotated_point.y = std::clamp(rotated_point.y, 5.f, static_cast<float>(g_config.size_h) - 10.f);

            // Store the point in the list
            temp_radar_points.emplace_back(rotated_point);
        }
        {
            std::unique_lock<std::mutex> radar_points_lock(radar_data.radar_points_mutex);
            radar_data.radar_points = std::move(temp_radar_points);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static POINT click_pos{};
    static bool dragging{ false };

    switch (msg) {
    case WM_LBUTTONDOWN: {
        dragging = true;
        SetCapture(hwnd); // capture mouse even when leaving window

        click_pos.x = GET_X_LPARAM(lParam);
        click_pos.y = GET_Y_LPARAM(lParam);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (dragging) {
            POINT p{};
            GetCursorPos(&p);

            const auto newX = p.x - click_pos.x;
            const auto newY = p.y - click_pos.y;

            SetWindowPos(hwnd, nullptr, newX, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
            return 0;
        }
        break;
    }
    case WM_LBUTTONUP: {
        dragging = false;
        ReleaseCapture();
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

using config_map = std::map<std::string, std::map<std::string, std::map<std::string, int>>>;
static bool parse_config(const std::string& filename, config_map& config) {
    config.clear();

    std::ifstream file(filename);
    if (!file) {
        return false;
    }

    std::string line{};
    while (std::getline(file, line)) {
        if (line.empty() || line.find('=') == std::string::npos) continue;

        auto pos = line.find('=');
        std::string key = line.substr(0, pos);
        std::string value_str = line.substr(pos + 1);

        std::istringstream key_stream(key);
        std::string part{};
        std::string parts[3];
        int idx = 0;
        while (std::getline(key_stream, part, '.') && idx < 3) {
            parts[idx++] = part;
        }
        if (idx != 3) {
            return false;
        }

        int value = std::stoi(value_str);
        config[parts[0]][parts[1]][parts[2]] = value;
    }

    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] LPSTR lpCmdLine, int nShowCmd) {
    config_map config{};
    const auto success = parse_config("settings.txt", config);
    if (success) {
        g_config.pos_x = config["radar"]["pos"]["x"];
        g_config.pos_y = config["radar"]["pos"]["y"];
        g_config.size_w = config["radar"]["size"]["w"];
        g_config.size_h = config["radar"]["size"]["h"];
        g_config.hide_from_recording = config["cheat"]["window"]["hide"];
        g_config.vertical_sync = config["cheat"]["window"]["vsync"];
    }
    else {
        show_message_box<char>("Failed to parse settings.txt, using default values", "", buttons::OK | icons::Warning);
        g_config.pos_x = 10;
        g_config.pos_y = 200;
        g_config.size_w = 150;
        g_config.size_h = 150;
        g_config.hide_from_recording = 0;
        g_config.vertical_sync = 0;
    }

    bool offsets_parser = parse_config("offsets.txt", config);
    if (offsets_parser) {
        offsets::dwLocalPlayerPawn = config["offsets"]["client"]["dwLocalPlayerPawn"];
        offsets::dwEntityList = config["offsets"]["client"]["dwEntityList"];
        offsets::dwViewAngles = config["offsets"]["client"]["dwViewAngles"];

        offsets::m_hPlayerPawn = config["offsets"]["client"]["m_hPlayerPawn"];
        offsets::m_iHealth = config["offsets"]["client"]["m_iHealth"];
        offsets::m_iTeamNum = config["offsets"]["client"]["m_iTeamNum"];
        offsets::m_vOldOrigin = config["offsets"]["client"]["m_vOldOrigin"];
    }
    else {
        return *show_message_box<char>("Failed to parse offsets.", "", buttons::OK | icons::Error);
    }

    const auto game = std::make_unique<c_game>();
    if (!game->initialize()) {
        return *show_message_box<char>("Failed to query game info.", "", buttons::OK | icons::Error);
    }

    const auto window_creation_flags = windower::WindowCreationInfo{
        .instance = hInstance,
        .class_name = "SimpleWinClass",
        .window_name = "Miata(c) staromodny",
        .wnd_proc = &wnd_proc,
        .ex_style = WS_EX_TOPMOST,
        .style = WS_POPUP | WS_VISIBLE,
        .size = {g_config.size_w, g_config.size_h},
        .pos = {g_config.pos_x, g_config.pos_y}
    };

    WNDCLASSEX wc{};
    const auto window_creation_res = create_window(wc, window_creation_flags);
    if (!window_creation_res) {
        return *show_message_box<char>(window_creation_res.error(), "", buttons::OK | icons::Error);
    }
    const auto& window = *window_creation_res;

    const auto device = std::make_unique<c_d3d_device>(*window, static_cast<bool>(g_config.vertical_sync));
    if (!device->initialize(g_config.size_w, g_config.size_h)) {
        return EXIT_FAILURE;
    }

    if (g_config.hide_from_recording == 1) {
        const auto hide_window_res = window->exclude_from_capture(TRUE);
        if (!hide_window_res) {
            const auto& ec = hide_window_res.error();

            const auto error_message =
                "Failed to exclude window from capture:\n" +
                ec.message() +
                "\nError code: " + std::to_string(ec.value());

            std::ignore = *show_message_box<char>(error_message, "", buttons::OK | icons::Error);
        }
    }

    if (!window->show(nShowCmd)) {
        return *show_message_box<char>("Failed to show window.", "", buttons::OK | icons::Error);
    }

    // prepare d3d9 state for rendering
    // if daisy is the only thing you plan on rendering with, you can only do this once *safely*
    // if you're implementing daisy alongside in a codebase that changes device state
    // you probably want to call this each frame before flushing render queues
    daisy::daisy_prepare();

    const auto queue = std::make_unique<daisy::c_renderqueue>();
    if (!queue->create(64, 128)) {
        return *show_message_box<char>("Failed to create buffer queue", "", buttons::OK | icons::Error);
    }

    std::stop_source stop_source{};

    player_data_t player_data{};
    radar_data_t radar_data{};

    std::jthread cache_thread(entity_cache_thread, stop_source, game.get(), std::ref(player_data));
    std::jthread data_thread(entity_data_thread, stop_source, game.get(), std::ref(player_data), std::ref(radar_data));

    const auto width = static_cast<float>(g_config.size_w);
    const auto height = static_cast<float>(g_config.size_h);

    while (!stop_source.stop_requested()) {
        MSG msg{};
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                stop_source.request_stop();
        }
        if (!FindWindow("SDL_app", "Counter-Strike 2") || GetAsyncKeyState(VK_DELETE) & 1)
            stop_source.request_stop();

        if (device->begin_rendering()) {
            queue->clear();
            {
                std::unique_lock<std::mutex> lock(radar_data.radar_points_mutex);
                for (const auto& pt : radar_data.radar_points) {
                    queue->push_filled_rectangle({ pt.x, pt.y }, { 5, 5 }, daisy::color_t(0, 255, 0, 255));
                }

                // Vertical center line
                queue->push_line({width / 2.f,0.f },{width / 2.f,height},daisy::color_t(0, 255, 0, 128));

                // Horizontal center line
                queue->push_line({ 0.f, height / 2.f }, { width, height / 2.f }, daisy::color_t(0, 255, 0, 128));
            }
            queue->flush();
            device->end_rendering();
        }
    }

    daisy::daisy_shutdown();

    std::ignore = window->destroy();
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    return EXIT_SUCCESS;
}