#ifndef APP
#define APP

#include "./ui/ui.hpp"

struct AppSetup {
    ui::UISetup uiSetup = {};
};

class App {
public:
    App(const AppSetup& setup);
    ~App();

    void run();
private:
    bool m_running;
    ui::UI m_UI;

    void handleUI();
};

#endif