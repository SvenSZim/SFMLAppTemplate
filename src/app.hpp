#ifndef APP
#define APP

#include "./ui/UIhandler.hpp"

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
    ui::UIHandler m_uiHandler;

    void handleUI();
};

#endif