#include <SFML/Graphics.hpp>

int main()
{
  auto window = sf::RenderWindow(sf::VideoMode({900u, 800u}), "SFML App Template");
  window.setFramerateLimit(144);

  while (window.isOpen())
  {
    while (const std::optional event = window.pollEvent())
    {
      if (event->is<sf::Event::Closed>())
      {
        window.close();
      }
    }

    window.clear();
    window.display();
  }
}
