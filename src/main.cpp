#include <iostream>

#include "./app.hpp"

int main()
{
  App app({
    .uiSetup = {
      .windowName = "Test One",
      .windowSize = {1200, 800},
      .containers = {{}}
    }
  });
  app.run();
  return 0;
}
