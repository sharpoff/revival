#include <revival/engine.h>

int main()
{
    Engine engine;
    engine.initialize("Game", 1280, 720, false);

    engine.run();

    engine.shutdown();
    return 0;
}
