# A new application

Made from [SFMLAppTemplate](https://github.com/SvenSZim/SFMLAppTemplate) 0.1.0: a simulation on its
own thread, panels with sliders and switches bound to its parameters, a main view to drag and zoom,
and a minimap. Change `src/main.cpp` from here.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
build/my_application
```

On Linux, SFML needs the packages listed in the template's README. The template's README explains
the API, and its headers document every part of it.
