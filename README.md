# os-schedule_visualizer
Take home final exam

Project: Schedule Visualization

Description:
For this project, I built a real-time Operating System CPU scheduling visualizer in C++ using the Simple DirectMedia Layer (SDL2). The application continuously parses Linux system files every 500ms to calculate and display the live active load percentage of each CPU core as a dynamic bar chart. To fulfill the advanced interaction requirements, I designed a custom, clickable graphical navigation bar that lets users instantly re-sort the cores based on three different metrics: alphabetically by name, by their current active load percentage, or by their total cumulative active time. Ultimately, it’s a clean, single-color interface that successfully handles real-time data polling and user-driven graphical updates without overcomplicating the underlying C++ architecture.

Structure:
    os_visualizer/
    ├── main.cpp
    ├── Makefile
    └── resrc/
        └── fonts/
            └── OpenSans-Regular.ttf

