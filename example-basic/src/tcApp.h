#pragma once

#include <TrussC.h>
#include "tcxNatNet.h"

using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

    tcx::NatNet natnet;
    EasyCam cam;
};
