#include "app/app.hpp"

App::App(AppSetup&& config) :
    m_UI(std::move(config.uiSetup)),
    m_running(false)
{}

App::~App() = default;

void App::run() {
    m_running = true;

    while (m_running) {
        handleUI();
        m_UI.render();
    }
}

void App::handleUI() {
    m_UI.update();

    for (const auto &event : m_UI.getEvents()) {
        switch (event.type) {
            case ui::core::Event::Type::Closed:
                m_running = false;
                break;
            case ui::core::Event::Type::WidgetChanged:
                break;
        }
    }
    m_UI.clearEvents();
}