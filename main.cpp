#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define WIDTH 800
#define HEIGHT 600

// Defines the three required metrics for the Advanced Task sorting
enum SortMode { SORT_NAME, SORT_LOAD, SORT_ACTIVE };

// Represents the raw parsed data for a single CPU core from /proc/stat
typedef struct CoreData {
    std::string name;
    long long activeTime;
    long long totalTime;
    float loadPercentage;
} CoreData;

// Represents the graphical rendering components for a single CPU core
typedef struct GCore {
    SDL_Texture *name_tex;      // Texture for "cpu0", "cpu1", etc.
    SDL_Texture *pct_tex;       // Texture for "45%", etc.
    SDL_Texture *active_tex;    // Texture for the cumulative active time ("120k")
    SDL_Rect name_pos;          // Bounding box for the name text
    SDL_Rect pct_pos;           // Bounding box for the percentage text
    SDL_Rect active_pos;        // Bounding box for the active time text
    SDL_Rect bar_pos;           // Bounding box for the colored bar chart
} GCore;

// Represents an interactive UI button mapped to a specific sorting mode
typedef struct UIButton {
    SDL_Rect rect;              // Bounding box for click detection
    SDL_Texture *tex;           // Texture for the button's label
    SDL_Rect text_pos;          // Position of the label inside the button
    SortMode mode;              // The sort mode triggered when this button is clicked
} UIButton;

// Master state structure holding all application data to avoid global variables
typedef struct AppData {
    TTF_Font *font;
    std::vector<CoreData> current_stats;
    std::vector<CoreData> previous_stats;   // Needed to calculate CPU delta over time
    std::vector<GCore*> graphic_cores;      // Dynamically allocated UI elements
    Uint32 last_update_time;                // Tracks the 500ms intervals
    bool running;                           // Main loop control flag
    
    SortMode current_sort;                  // Currently active sorting state
    std::vector<UIButton> sort_buttons;     // Array holding our interactive UI elements
} AppData;

// Forward Declarations
void initialize(SDL_Renderer *renderer, AppData *data_ptr);
void handleEvent(SDL_Event *event, SDL_Renderer *renderer, AppData *data_ptr);
void render(SDL_Renderer *renderer, AppData *data_ptr);
void updateCPUStats(SDL_Renderer *renderer, AppData *data_ptr);
void refreshVisuals(SDL_Renderer *renderer, AppData *data_ptr);
void clearGCores(std::vector<GCore*>& graphic_cores);
bool pointInRect(int x, int y, SDL_Rect& rect);
void quit(AppData *data_ptr);

int main(int argc, char *argv[]) {
    // Initialize required SDL subsystems (Video, Images, Fonts)
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_Window *window;
    SDL_Renderer *renderer;
    
    // Create window centered on screen with hardware-accelerated rendering
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "Scheduling Monitor - Multi-Metric Sorting");

    // Initialize application state
    AppData data;
    initialize(renderer, &data);

    SDL_Event event;
    
    // Main Application Loop
    while (data.running) {
        Uint32 current_time = SDL_GetTicks();
        
        // Basic Task Requirement: Update interface at an interval (500ms)
        // We only parse the file and rebuild stats twice a second
        if (current_time - data.last_update_time >= 500) {
            updateCPUStats(renderer, &data);
            data.last_update_time = current_time;
        }

        // Process all events currently in the queue (mouse clicks, window closing)
        while (SDL_PollEvent(&event)) {
            handleEvent(&event, renderer, &data);
        }

        // Render the current frame to the screen
        render(renderer, &data);
        
        // Cap frame rate slightly to prevent the loop from maxing out a CPU core
        SDL_Delay(16); 
    }

    // Clean up memory and gracefully exit SDL
    quit(&data);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}

// Sets up initial state, loads resources, and builds the UI layout
void initialize(SDL_Renderer *renderer, AppData *data_ptr) {
    data_ptr->font = TTF_OpenFont("resrc/fonts/OpenSans-Regular.ttf", 16);
    if (!data_ptr->font) {
        std::cerr << "Failed to load font! Check directory structure." << std::endl;
    }
    data_ptr->running = true;
    data_ptr->last_update_time = 0;
    data_ptr->current_sort = SORT_NAME; // Default behavior

    // --- Build the UI Navigation Bar ---
    std::string labels[] = {"Mode 1: Sort by Name", "Mode 2: Sort by Load %", "Mode 3: Sort by Total Active"};
    SortMode modes[] = {SORT_NAME, SORT_LOAD, SORT_ACTIVE};
    
    int btnWidth = 230;
    int btnHeight = 35;
    int spacing = 20;
    int startX = 40;
    SDL_Color textColor = {255, 255, 255, 255};

    // Instantiate our three sorting buttons
    for (int i = 0; i < 3; i++) {
        UIButton b;
        b.mode = modes[i];
        b.rect = {startX + i * (btnWidth + spacing), 20, btnWidth, btnHeight};

        // Render text label to a texture
        SDL_Surface *surf = TTF_RenderText_Solid(data_ptr->font, labels[i].c_str(), textColor);
        b.tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_QueryTexture(b.tex, NULL, NULL, &b.text_pos.w, &b.text_pos.h);
        
        // Mathematically center the text label inside the button's bounding box
        b.text_pos.x = b.rect.x + (b.rect.w - b.text_pos.w) / 2;
        b.text_pos.y = b.rect.y + (b.rect.h - b.text_pos.h) / 2;
        SDL_FreeSurface(surf);

        data_ptr->sort_buttons.push_back(b);
    }
    
    // Perform initial data fetch so the screen isn't blank on startup
    updateCPUStats(renderer, data_ptr);
}

// Routes user input to application logic
void handleEvent(SDL_Event *event, SDL_Renderer *renderer, AppData *data_ptr) {
    if (event->type == SDL_QUIT) {
        data_ptr->running = false;
    }
    else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        // Advanced Task Requirement: Listen to user interaction events
        int x = event->button.x;
        int y = event->button.y;

        // Iterate through UI buttons to see if the click coordinates intersect with a bounding box
        for (size_t i = 0; i < data_ptr->sort_buttons.size(); i++) {
            if (pointInRect(x, y, data_ptr->sort_buttons[i].rect)) {
                std::cout << "Sorting metric changed to mode: " << data_ptr->sort_buttons[i].mode + 1 << std::endl;
                
                // Update the state to the newly selected metric
                data_ptr->current_sort = data_ptr->sort_buttons[i].mode;
                
                // Instantly re-sort and re-draw the UI without waiting for the 500ms timer
                refreshVisuals(renderer, data_ptr);
                break; 
            }
        }
    }
}

// Parses Linux system files to fetch current OS scheduling metrics
void updateCPUStats(SDL_Renderer *renderer, AppData *data_ptr) {
    std::ifstream file("/proc/stat");
    std::string line;
    std::vector<CoreData> fresh_stats;

    // Parse /proc/stat line by line looking for core entries (cpu0, cpu1, etc.)
    while (std::getline(file, line)) {
        if (line.compare(0, 3, "cpu") == 0 && line.length() > 3 && isdigit(line[3])) {
            std::istringstream ss(line);
            std::string name;
            // The proc/stat file provides times spent in various processor states
            long long user, nice, system, idle, iowait, irq, softirq, steal;
            
            ss >> name >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            // Calculate total time doing active work vs total overall time
            long long active = user + nice + system + irq + softirq + steal;
            long long total = active + idle + iowait;
            
            fresh_stats.push_back({name, active, total, 0.0f});
        }
    }

    // CPU load is a rate over time, not a static value. 
    // We calculate the load by finding the difference (delta) between the current and previous polling cycles.
    for (size_t i = 0; i < fresh_stats.size(); i++) {
        for (size_t j = 0; j < data_ptr->previous_stats.size(); j++) {
            if (fresh_stats[i].name == data_ptr->previous_stats[j].name) {
                long long activeDiff = fresh_stats[i].activeTime - data_ptr->previous_stats[j].activeTime;
                long long totalDiff = fresh_stats[i].totalTime - data_ptr->previous_stats[j].totalTime;
                
                // Prevent division by zero if the system ticks haven't advanced
                if (totalDiff > 0) {
                    fresh_stats[i].loadPercentage = (static_cast<float>(activeDiff) / totalDiff) * 100.0f;
                } else {
                    fresh_stats[i].loadPercentage = data_ptr->previous_stats[j].loadPercentage;
                }
                break;
            }
        }
    }
    
    // Save current state to use as the baseline for the next 500ms cycle
    data_ptr->previous_stats = fresh_stats;
    data_ptr->current_stats = fresh_stats;

    // Proceed to sort and generate graphical elements
    refreshVisuals(renderer, data_ptr);
}

// Sorts data and generates SDL textures/rects based on current state
void refreshVisuals(SDL_Renderer *renderer, AppData *data_ptr) {
    
    // --- Data Sorting Logic (Advanced Task Requirement) ---
    if (data_ptr->current_sort == SORT_NAME) {
        std::sort(data_ptr->current_stats.begin(), data_ptr->current_stats.end(), [](const CoreData& a, const CoreData& b) {
            // Sort by string length first so 'cpu10' comes after 'cpu9', not 'cpu1'
            if (a.name.length() != b.name.length()) return a.name.length() < b.name.length();
            return a.name < b.name;
        });
    } else if (data_ptr->current_sort == SORT_LOAD) {
        std::sort(data_ptr->current_stats.begin(), data_ptr->current_stats.end(), [](const CoreData& a, const CoreData& b) {
            return a.loadPercentage > b.loadPercentage; // Descending order
        });
    } else if (data_ptr->current_sort == SORT_ACTIVE) {
        std::sort(data_ptr->current_stats.begin(), data_ptr->current_stats.end(), [](const CoreData& a, const CoreData& b) {
            return a.activeTime > b.activeTime; // Highest cumulative active time first
        });
    }

    // Deallocate previous textures to prevent memory leaks
    clearGCores(data_ptr->graphic_cores);
    
    SDL_Color textColor = {200, 200, 200, 255}; 
    SDL_Color dimColor = {150, 150, 150, 255}; 

    // Visual layout constants
    int barWidth = 40;
    int spacing = 35; 
    int startX = 40;
    int baseY = HEIGHT - 80; 
    int maxBarHeight = HEIGHT - 200;

    // Generate graphical assets for each sorted core
    for (size_t i = 0; i < data_ptr->current_stats.size(); i++) {
        GCore *g_core = new GCore();
        int currentX = startX + i * (barWidth + spacing);

        // Render Core Name Text
        SDL_Surface *name_surf = TTF_RenderText_Solid(data_ptr->font, data_ptr->current_stats[i].name.c_str(), textColor);
        g_core->name_tex = SDL_CreateTextureFromSurface(renderer, name_surf);
        SDL_QueryTexture(g_core->name_tex, NULL, NULL, &(g_core->name_pos.w), &(g_core->name_pos.h));
        g_core->name_pos.x = currentX;
        g_core->name_pos.y = baseY + 10;
        SDL_FreeSurface(name_surf);

        // Render Active Time Text (Divide by 1000 and append 'k' to fit nicely under the bars)
        std::string active_str = std::to_string(data_ptr->current_stats[i].activeTime / 1000) + "k";
        SDL_Surface *active_surf = TTF_RenderText_Solid(data_ptr->font, active_str.c_str(), dimColor);
        g_core->active_tex = SDL_CreateTextureFromSurface(renderer, active_surf);
        SDL_QueryTexture(g_core->active_tex, NULL, NULL, &(g_core->active_pos.w), &(g_core->active_pos.h));
        g_core->active_pos.x = currentX;
        g_core->active_pos.y = baseY + 30;
        SDL_FreeSurface(active_surf);

        // Render Percentage Text
        std::string pct_str = std::to_string(static_cast<int>(data_ptr->current_stats[i].loadPercentage)) + "%";
        SDL_Surface *pct_surf = TTF_RenderText_Solid(data_ptr->font, pct_str.c_str(), textColor);
        g_core->pct_tex = SDL_CreateTextureFromSurface(renderer, pct_surf);
        SDL_QueryTexture(g_core->pct_tex, NULL, NULL, &(g_core->pct_pos.w), &(g_core->pct_pos.h));
        g_core->pct_pos.x = currentX;
        SDL_FreeSurface(pct_surf);

        // Calculate dynamic bar height relative to maximum available screen space
        int calculatedHeight = static_cast<int>((data_ptr->current_stats[i].loadPercentage / 100.0f) * maxBarHeight);
        g_core->bar_pos.x = currentX;
        g_core->bar_pos.y = baseY - calculatedHeight; // Subtract height to draw "upwards"
        g_core->bar_pos.w = barWidth;
        g_core->bar_pos.h = calculatedHeight;
        
        // Anchor percentage text just above the rendered bar
        g_core->pct_pos.y = g_core->bar_pos.y - 25; 

        data_ptr->graphic_cores.push_back(g_core);
    }
}

// Blits all textures and primitives to the screen buffer
void render(SDL_Renderer *renderer, AppData *data_ptr) {
    // Clear screen to a dark gray background
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    // --- Draw Interactive UI Navigation ---
    for (size_t i = 0; i < data_ptr->sort_buttons.size(); i++) {
        // Highlight logic: apply a blue background if the button represents the active sort mode
        if (data_ptr->current_sort == data_ptr->sort_buttons[i].mode) {
            SDL_SetRenderDrawColor(renderer, 0, 100, 180, 255); 
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255); 
        }
        
        // Draw filled button background
        SDL_RenderFillRect(renderer, &(data_ptr->sort_buttons[i].rect));
        
        // Draw lighter border around button
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); 
        SDL_RenderDrawRect(renderer, &(data_ptr->sort_buttons[i].rect));
        
        // Overlay button text label
        SDL_RenderCopy(renderer, data_ptr->sort_buttons[i].tex, NULL, &(data_ptr->sort_buttons[i].text_pos));
    }

    // --- Draw CPU Visualizations ---
    for (size_t i = 0; i < data_ptr->graphic_cores.size(); i++) {
        // Render bar primitive (Teal)
        SDL_SetRenderDrawColor(renderer, 0, 160, 140, 255);
        SDL_RenderFillRect(renderer, &(data_ptr->graphic_cores[i]->bar_pos));

        // Render associated metric textures
        if (data_ptr->graphic_cores[i]->name_tex) SDL_RenderCopy(renderer, data_ptr->graphic_cores[i]->name_tex, NULL, &(data_ptr->graphic_cores[i]->name_pos));
        if (data_ptr->graphic_cores[i]->pct_tex) SDL_RenderCopy(renderer, data_ptr->graphic_cores[i]->pct_tex, NULL, &(data_ptr->graphic_cores[i]->pct_pos));
        if (data_ptr->graphic_cores[i]->active_tex) SDL_RenderCopy(renderer, data_ptr->graphic_cores[i]->active_tex, NULL, &(data_ptr->graphic_cores[i]->active_pos));
    }

    // Swap back buffer to front (display to user)
    SDL_RenderPresent(renderer);
}

// Utility function: Collision detection for mouse clicks inside a rectangle
bool pointInRect(int x, int y, SDL_Rect& rect) {
    return (x > rect.x && x < rect.x + rect.w && y > rect.y && y < rect.y + rect.h);
}

// Utility function: Safely cleans up dynamic heap memory and SDL textures
void clearGCores(std::vector<GCore*>& graphic_cores) {
    for (size_t i = 0; i < graphic_cores.size(); i++) {
        if (graphic_cores[i]->name_tex) SDL_DestroyTexture(graphic_cores[i]->name_tex);
        if (graphic_cores[i]->pct_tex) SDL_DestroyTexture(graphic_cores[i]->pct_tex);
        if (graphic_cores[i]->active_tex) SDL_DestroyTexture(graphic_cores[i]->active_tex);
        delete graphic_cores[i];
    }
    graphic_cores.clear(); // Empty the vector after deleting pointers
}

// Application shutdown routine
void quit(AppData *data_ptr) {
    clearGCores(data_ptr->graphic_cores);
    
    if (data_ptr->font) TTF_CloseFont(data_ptr->font);
    
    // Destroy UI button textures
    for (size_t i = 0; i < data_ptr->sort_buttons.size(); i++) {
        if (data_ptr->sort_buttons[i].tex) SDL_DestroyTexture(data_ptr->sort_buttons[i].tex);
    }
}