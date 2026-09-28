#ifndef APP
#define APP

#include "../ui/ui_manager.hpp"

struct AppSetup {
    ui::UISetup uiSetup = {};
};

class App {
public:
    App(AppSetup&& setup);
    ~App();

    void run();
private:
    bool m_running;
    ui::UIManager m_UI;

    void handleUI();
};

#endif
