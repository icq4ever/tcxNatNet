#include "TrussC.h"
#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.title = "tcxNatNet - natnet2osc bridge";
    settings.width = 640;
    settings.height = 400;
    return TC_RUN_APP(tcApp, settings);
}
