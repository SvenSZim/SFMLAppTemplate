#include "./app.hpp"

App::App(const AppSetup& config) :
    m_UI(config.uiSetup),
    m_running(false)
{}

App::~App() = default;

void App::run() {
    m_running = true;

    while (m_running) {
        handleUI();
    }
}

void App::handleUI() {
    m_UI.update();

    for (const auto &event : m_UI.getEvents()) {
        switch (event) {
            case ui::Event::Closed:
                m_running = false;
                break;
            case ui::Event::One:
                break;
            case ui::Event::Two:
                break;
            default:
                break;
        }
    }
}