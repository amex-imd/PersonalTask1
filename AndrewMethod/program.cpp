#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 800;
constexpr float POINT_HIT_RADIUS = 10.0f;
constexpr int INITIAL_POINT_NUMBER = 40;
constexpr float POINT_SIZE = 4.0f;

// Векторное произведение векторов OA и OB.
static inline float cross(const ImVec2& o, const ImVec2& a, const ImVec2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Алгорифм Эндрю
std::vector<ImVec2> convex_result_andrew(std::vector<ImVec2> points) {
    size_t number = points.size();
    if (number < 3) return points;

    // Сортируем точки лексикографически: сначала по x, при равенстве — по y
    // Это даёт нам левую и правую крайние точки на концах массива.
    std::sort(points.begin(), points.end(),
        [](const ImVec2& a, const ImVec2& b) { return (a.x < b.x) || (a.x == b.x && a.y < b.y); });

    std::vector<ImVec2> result;
    result.reserve(2 * number); // максимум 2n точек в обеих цепях

    // Строим нижнюю цепь, идя слева направо.
    // Храним в векторе только те точки, которые образуют левый поворот.
    // Если новая точка образует правый поворот, то предыдущая точка "выпирает" внутрь — выталкиваем её.
    for (int i(0); i < number; ++i) {
        while (result.size() >= 2 &&
            cross(result[result.size() - 2], result[result.size() - 1], points[i]) <= 0.f) {
            result.pop_back();
        }
        result.push_back(points[i]);
    }

    // Строим верхнюю цепь, идя справа налево.
    // Начинаем не с предпоследней точки, и не трогаем нижнюю цепь
    size_t lower_size = result.size();
    for (int i(number - 2); i >= 0; --i) {
        while (result.size() > lower_size &&
            cross(result[result.size() - 2], result[result.size() - 1], points[i]) <= 0.f) {
            result.pop_back();
        }
        result.push_back(points[i]);
    }

    // Последняя добавленная точка — это снова самая левая точка,
    // Она дублирует первую. Убираем её, чтобы оболочка была замкнутой без повтора.
    result.pop_back();
    return result;
}

// Генерация случайных точек внутри прямоугольника (с отступом от краёв).
std::vector<ImVec2> generate_random_points(int count, float width, float height, unsigned long long seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist_x(20.f, width - 20.f);
    std::uniform_real_distribution<float> dist_y(20.f, height - 20.f);
    std::vector<ImVec2> points(count);
    for (auto& p : points) p = ImVec2(dist_x(rng), dist_y(rng));
    return points;
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;

    SDL_Window* window = SDL_CreateWindow("Andrew's Monotone Chain", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!window || !renderer) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.SizePixels = 18.0f;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    std::vector<ImVec2> points = generate_random_points(INITIAL_POINT_NUMBER, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);

    int dragged_point_index = -1;

    bool show_points (1), show_result(1), running(1);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;

            if (io.WantCaptureMouse) continue;

            // Выбор существующей точки или добавление новой
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                ImVec2 mouse_world = ImVec2(event.button.x, event.button.y);

                dragged_point_index = -1;
                float min_dist = POINT_HIT_RADIUS * POINT_HIT_RADIUS;

                // Ищем ближайшую точку в радиусе захвата
                for (size_t i(0); i < points.size(); ++i) {
                    float dx(points[i].x - mouse_world.x), dy(points[i].y - mouse_world.y);
                    float dist = dx * dx + dy * dy;
                    if (dist < min_dist) {
                        min_dist = dist;
                        dragged_point_index = i;
                    }
                }

                // Если не попали ни в одну точку — добавляем новую.
                if (dragged_point_index == -1) points.push_back(mouse_world);
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) dragged_point_index = -1;

            if (event.type == SDL_EVENT_MOUSE_MOTION && dragged_point_index != -1) points[dragged_point_index] = ImVec2(event.motion.x, event.motion.y);
        }

        // Пересчитываем оболочку каждый кадр
        std::vector<ImVec2> result = convex_result_andrew(points);

        ImGui_ImplSDL3_NewFrame();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Convex Hull (Andrew)");
        ImGui::Text("Points: %d | Hull vertices: %zu", (int)points.size(), result.size());

        if (ImGui::Button("Generate random"))
            points = generate_random_points(INITIAL_POINT_NUMBER, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT, (unsigned)SDL_GetTicks());
        if (ImGui::Button("Clear"))
            points.clear();

        ImGui::Checkbox("Show points", &show_points);
        ImGui::Checkbox("Show hull", &show_result);
        ImGui::End();

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 20, 22, 28, 255);
        SDL_RenderClear(renderer);

        if (show_points) {
            SDL_SetRenderDrawColor(renderer, 80, 140, 220, 255);
            for (const auto& p : points) {
                SDL_FRect rect{ p.x - POINT_SIZE / 2.f, p.y - POINT_SIZE / 2.f, POINT_SIZE, POINT_SIZE };
                SDL_RenderFillRect(renderer, &rect);
            }
        }

        if (show_result && result.size() >= 2) {
            SDL_SetRenderDrawColor(renderer, 255, 150, 60, 255);
            for (size_t i = 0; i < result.size(); ++i) {
                ImVec2 p1 = result[i];
                ImVec2 p2 = result[(i + 1) % result.size()];
                SDL_RenderLine(renderer, p1.x, p1.y, p2.x, p2.y);
            }

            for (size_t i (0); i < result.size(); ++i) {
                ImVec2 sp = result[i];
                SDL_SetRenderDrawColor(renderer, 255, 90, 90, 255);

                if (dragged_point_index != -1 &&
                    std::abs(points[dragged_point_index].x - result[i].x) < 0.1f &&
                    std::abs(points[dragged_point_index].y - result[i].y) < 0.1f) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                }

                SDL_FRect rect{ sp.x - 3.f, sp.y - 3.f, 6.f, 6.f };
                SDL_RenderFillRect(renderer, &rect);
            }
        }

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}