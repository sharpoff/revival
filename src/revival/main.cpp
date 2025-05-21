#include <revival/engine.h>

int main()
{
    Engine engine;
    if (!engine.init("Game", 1280, 720, false)) {
        printf("Failed to initialize engine.\n");
        return EXIT_FAILURE;
    }

    engine.run();

    engine.shutdown();
    return 0;
}
