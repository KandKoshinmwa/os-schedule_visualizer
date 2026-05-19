#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath> // Required for color conversion math
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define WIDTH 800
#define HEIGHT 600

enum SortMode { SORT_NAME, SORT_LOAD, SORT_ACTIVE };

typedef struct CoreData {
    std::string name;
    long long activeTime;
    long long totalTime;
    float loadPercentage;
    SDL_Color color; // The color now travels with the data when sorted
} CoreData;

typedef struct GCore {
    SDL_Texture *name_tex;
    SDL_Texture *pct_tex;
    SDL_Texture *active_tex;
    SDL_Rect name_pos;
    SDL_Rect pct_pos;
    SDL_Rect active_pos;
    SDL_Rect bar_pos;
    SDL_Color bar_color; // To hold the color for rendering
} GCore;

typedef struct UIButton {
    SDL_Rect rect;
    SDL_Texture *tex;
    SDL_Rect text_pos;
    SortMode mode;
} UIButton;

typedef struct AppData {
    TTF_Font *font;
    std::vector<CoreData> current_stats;
    std::vector<CoreData> previous_stats;
    std::vector<GCore*> graphic_cores;
    Uint32 last_update_time;
    bool running;
    
    SortMode current_sort;
    std::vector<UIButton> sort_buttons;
} AppData;

// --- Color Generation Function ---
// Converts HSL (Hue 0-360, Saturation 0-1, Lightness 0-1) to an SDL RGB Color
SDL_Color HSLtoRGB(float h, float s, float l) {
    float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;

    float r = 0, g = 0, b = 0;
    if (0 <= h && h < 60) { r = c; g = x; b = 0; }
    else if (60 <= h && h < 120) { r = x; g = c; b = 0; }
    else if (120 <= h && h < 180) { r = 0; g = c; b = x; }
    else if (180 <= h && h < 240) { r = 0; g = x; b = c; }
    else if (240 <= h && h < 300) { r = x; g = 0; b = c; }
    else if (300 <= h && h < 360) { r = c; g = 0; b = x; }

    SDL_Color color;
    color.r = static_cast<Uint8>((r + m) * 255.0f);
    color.g = static_cast<Uint8>((g + m) * 255.0f);
    color.b = static_cast<Uint8>((b + m) * 255.0f);
    color.a = 255;
    return color;
}

void initialize(SDL_Renderer *renderer, AppData *data_ptr);
void handleEvent(SDL_Event *event, SDL_Renderer *renderer, AppData *data_ptr);
void render(SDL_Renderer *renderer, AppData *data_ptr);
void updateCPUStats(SDL_Renderer *renderer, AppData *data_ptr);
void refreshVisuals(SDL_Renderer *renderer, AppData *data_ptr);
void clearGCores(std::vector<GCore*>& graphic_cores);
bool pointInRect(int x, int y, SDL_Rect& rect);
void quit(AppData *data_ptr);

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "Scheduling Monitor - Multi-Metric Sorting");

    AppData data;
    initialize(renderer, &data);

    SDL_Event event;
    while (data.running) {
        Uint32 current_time = SDL_GetTicks();
        
        if (current_time - data.last_update_time >= 500) {
            updateCPUStats(renderer, &data);
            data.last_update_time = current_time;
        }

        while (SDL_PollEvent(&event)) {
            handleEvent(&event, renderer, &data);
        }

        render(renderer, &data);
        SDL_Delay(16); 
    }

    quit(&data);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}

void initialize(SDL_Renderer *renderer, AppData *data_ptr) {
    data_ptr->font = TTF_OpenFont("resrc/fonts/OpenSans-Regular.ttf", 16);
    if (!data_ptr->font) {
        std::cerr << "Failed to load font! Check directory structure." << std::endl;
    }
    data_ptr->running = true;
    data_ptr->last_update_time = 0;
    data_ptr->current_sort = SORT_NAME;

    std::string labels[] = {"Sort by Name", "Sort by Load %", "Sort by Total Active"};
    SortMode modes[] = {SORT_NAME, SORT_LOAD, SORT_ACTIVE};
    
    int btnWidth = 160;
    int btnHeight = 35;
    int spacing = 20;
    int startX = 40;
    SDL_Color textColor = {255, 255, 255, 255};

    for (int i = 0; i < 3; i++) {
        UIButton b;
        b.mode = modes[i];
        b.rect = {startX + i * (btnWidth + spacing), 20, btnWidth, btnHeight};

        SDL_Surface *surf = TTF_RenderText_Solid(data_ptr->font, labels[i].c_str(), textColor);
        b.tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_QueryTexture(b.tex, NULL, NULL, &b.text_pos.w, &b.text_pos.h);
        
        b.text_pos.x = b.rect.x + (b.rect.w - b.text_pos.w) / 2;
        b.text_pos.y = b.rect.y + (b.rect.h - b.text_pos.h) / 2;
        SDL_FreeSurface(surf);

        data_ptr->sort_buttons.push_back(b);
    }
    
    updateCPUStats(renderer, data_ptr);
}

void handleEvent(SDL_Event *event, SDL_Renderer *renderer, AppData *data_ptr) {
    if (event->type == SDL_QUIT) {
        data_ptr->running = false;
    }
    else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int x = event->button.x;
        int y = event->button.y;

        for (size_t i = 0; i < data_ptr->sort_buttons.size(); i++) {
            if (pointInRect(x, y, data_ptr->sort_buttons[i].rect)) {
                data_ptr->current_sort = data_ptr->sort_buttons[i].mode;
                refreshVisuals(renderer, data_ptr);
                break; 
            }
        }
    }
}

void updateCPUStats(SDL_Renderer *renderer, AppData *data_ptr) {
    std::ifstream file("/proc/stat");
    std::string line;
    std::vector<CoreData> fresh_stats;

    while (std::getline(file, line)) {
        if (line.compare(0, 3, "cpu") == 0 && line.length() > 3 && isdigit(line[3])) {
            std::istringstream ss(line);
            std::string name;
            long long user, nice, system, idle, iowait, irq, softirq, steal;
            
            ss >> name >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            long long active = user + nice + system + irq + softirq + steal;
            long long total = active + idle + iowait;
            
            // Dummy color initialized here, we will overwrite it in the next loop
            SDL_Color blankColor = {255, 255, 255, 255};
            fresh_stats.push_back({name, active, total, 0.0f, blankColor});
        }
    }

    size_t num_cores = fresh_stats.size();
    for (size_t i = 0; i < num_cores; i++) {
        // Generates a distinct hue evenly distributed around the color wheel based on original CPU order
        float hue = (360.0f / num_cores) * i;
        // Saturation 85%, Lightness 60% yields bright, highly visible colors
        fresh_stats[i].color = HSLtoRGB(hue, 0.85f, 0.60f); 

        for (size_t j = 0; j < data_ptr->previous_stats.size(); j++) {
            if (fresh_stats[i].name == data_ptr->previous_stats[j].name) {
                long long activeDiff = fresh_stats[i].activeTime - data_ptr->previous_stats[j].activeTime;
                long long totalDiff = fresh_stats[i].totalTime - data_ptr->previous_stats[j].totalTime;
                
                if (totalDiff > 0) {
                    fresh_stats[i].loadPercentage = (static_cast<float>(activeDiff) / totalDiff) * 100.0f;
                } else {
                    fresh_stats[i].loadPercentage = data_ptr->previous_stats[j].loadPercentage;
                }
                break;
            }
        }
    }
    
    data_ptr->previous_stats = fresh_stats;
    data_ptr->current_stats = fresh_stats;

    refreshVisuals(renderer, data_ptr);
}

void refreshVisuals(SDL_Renderer *renderer, AppData *data_ptr) {
    if (data_ptr->current_sort == SORT_NAME) {
        std::sort(data_ptr->current_stats.begin(), data_ptr->current_stats.end(), [](const CoreData& a, const CoreData& b) {
            if (a.name.length() != b.name.length()) return a.name.length() < b.name.length();
            return a.name < b.name;
        });
    } else if (data_ptr->current_sort == SORT_LOAD) {
        std::sort(data_ptr->current_stats.begin(), data_ptr->current_stats.end(), [](const CoreData& a, const CoreData& b) {
            return a.loadPercentage > b.loadPercentage; 
        });
    } else if (data_ptr->current_sort == SORT_ACTIVE) {
        std::sort(data_ptr->current_stats.begin(), data_ptr->current_stats.end(), [](const CoreData& a, const CoreData& b) {
            return a.activeTime > b.activeTime; 
        });
    }

    clearGCores(data_ptr->graphic_cores);
    SDL_Color textColor = {200, 200, 200, 255}; 
    SDL_Color dimColor = {150, 150, 150, 255}; 

    int barWidth = 40;
    int spacing = 35; 
    int startX = 40;
    int baseY = HEIGHT - 80; 
    int maxBarHeight = HEIGHT - 200;

    for (size_t i = 0; i < data_ptr->current_stats.size(); i++) {
        GCore *g_core = new GCore();
        int currentX = startX + i * (barWidth + spacing);

        g_core->bar_color = data_ptr->current_stats[i].color; // Lock the color to the bar

        SDL_Surface *name_surf = TTF_RenderText_Solid(data_ptr->font, data_ptr->current_stats[i].name.c_str(), textColor);
        g_core->name_tex = SDL_CreateTextureFromSurface(renderer, name_surf);
        SDL_QueryTexture(g_core->name_tex, NULL, NULL, &(g_core->name_pos.w), &(g_core->name_pos.h));
        g_core->name_pos.x = currentX;
        g_core->name_pos.y = baseY + 10;
        SDL_FreeSurface(name_surf);

        std::string active_str = std::to_string(data_ptr->current_stats[i].activeTime / 1000) + "k";
        SDL_Surface *active_surf = TTF_RenderText_Solid(data_ptr->font, active_str.c_str(), dimColor);
        g_core->active_tex = SDL_CreateTextureFromSurface(renderer, active_surf);
        SDL_QueryTexture(g_core->active_tex, NULL, NULL, &(g_core->active_pos.w), &(g_core->active_pos.h));
        g_core->active_pos.x = currentX;
        g_core->active_pos.y = baseY + 30; 
        SDL_FreeSurface(active_surf);

        std::string pct_str = std::to_string(static_cast<int>(data_ptr->current_stats[i].loadPercentage)) + "%";
        SDL_Surface *pct_surf = TTF_RenderText_Solid(data_ptr->font, pct_str.c_str(), textColor);
        g_core->pct_tex = SDL_CreateTextureFromSurface(renderer, pct_surf);
        SDL_QueryTexture(g_core->pct_tex, NULL, NULL, &(g_core->pct_pos.w), &(g_core->pct_pos.h));
        g_core->pct_pos.x = currentX;
        SDL_FreeSurface(pct_surf);

        int calculatedHeight = static_cast<int>((data_ptr->current_stats[i].loadPercentage / 100.0f) * maxBarHeight);
        g_core->bar_pos.x = currentX;
        g_core->bar_pos.y = baseY - calculatedHeight;
        g_core->bar_pos.w = barWidth;
        g_core->bar_pos.h = calculatedHeight;
        g_core->pct_pos.y = g_core->bar_pos.y - 25; 

        data_ptr->graphic_cores.push_back(g_core);
    }
}

void render(SDL_Renderer *renderer, AppData *data_ptr) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    for (size_t i = 0; i < data_ptr->sort_buttons.size(); i++) {
        if (data_ptr->current_sort == data_ptr->sort_buttons[i].mode) {
            SDL_SetRenderDrawColor(renderer, 0, 100, 180, 255); 
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255); 
        }
        
        SDL_RenderFillRect(renderer, &(data_ptr->sort_buttons[i].rect));
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); 
        SDL_RenderDrawRect(renderer, &(data_ptr->sort_buttons[i].rect));
        SDL_RenderCopy(renderer, data_ptr->sort_buttons[i].tex, NULL, &(data_ptr->sort_buttons[i].text_pos));
    }

    for (size_t i = 0; i < data_ptr->graphic_cores.size(); i++) {
        // Pull the dynamic color we assigned to this specific core
        SDL_SetRenderDrawColor(renderer, data_ptr->graphic_cores[i]->bar_color.r, 
                                         data_ptr->graphic_cores[i]->bar_color.g, 
                                         data_ptr->graphic_cores[i]->bar_color.b, 255);
        SDL_RenderFillRect(renderer, &(data_ptr->graphic_cores[i]->bar_pos));

        if (data_ptr->graphic_cores[i]->name_tex) SDL_RenderCopy(renderer, data_ptr->graphic_cores[i]->name_tex, NULL, &(data_ptr->graphic_cores[i]->name_pos));
        if (data_ptr->graphic_cores[i]->pct_tex) SDL_RenderCopy(renderer, data_ptr->graphic_cores[i]->pct_tex, NULL, &(data_ptr->graphic_cores[i]->pct_pos));
        if (data_ptr->graphic_cores[i]->active_tex) SDL_RenderCopy(renderer, data_ptr->graphic_cores[i]->active_tex, NULL, &(data_ptr->graphic_cores[i]->active_pos));
    }

    SDL_RenderPresent(renderer);
}

bool pointInRect(int x, int y, SDL_Rect& rect) {
    return (x > rect.x && x < rect.x + rect.w && y > rect.y && y < rect.y + rect.h);
}

void clearGCores(std::vector<GCore*>& graphic_cores) {
    for (size_t i = 0; i < graphic_cores.size(); i++) {
        if (graphic_cores[i]->name_tex) SDL_DestroyTexture(graphic_cores[i]->name_tex);
        if (graphic_cores[i]->pct_tex) SDL_DestroyTexture(graphic_cores[i]->pct_tex);
        if (graphic_cores[i]->active_tex) SDL_DestroyTexture(graphic_cores[i]->active_tex);
        delete graphic_cores[i];
    }
    graphic_cores.clear();
}

void quit(AppData *data_ptr) {
    clearGCores(data_ptr->graphic_cores);
    if (data_ptr->font) TTF_CloseFont(data_ptr->font);
    for (size_t i = 0; i < data_ptr->sort_buttons.size(); i++) {
        if (data_ptr->sort_buttons[i].tex) SDL_DestroyTexture(data_ptr->sort_buttons[i].tex);
    }
}