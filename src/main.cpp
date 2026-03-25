#include <spdlog/spdlog.h>
#include "core/Bootstrapper.h"

using namespace vse;

int main(int /*argc*/, char* /*argv*/[]) {
    Bootstrapper app;
    if (!app.init()) return 1;
    app.run();
    app.shutdown();
    return 0;
}
