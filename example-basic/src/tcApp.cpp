#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxNatNet - basic");

    // Motive server ip. Optionally pass this machine's NIC ip as 2nd arg.
    natnet.setup("127.0.0.1");

    // NatNet streams metres. Scale to mm so the scene matches tcxQTMRT units.
    natnet.setScale(1000.0f);

    cam.setDistance(2500);
    cam.setTarget(0, 0, 0);
    cam.setElevation(20);
    cam.enableMouseInput();
}

void tcApp::update() {
    natnet.update();
}

void tcApp::draw() {
    clear(0.07f, 0.07f, 0.09f);

    cam.begin();

    setColor(0.4f);
    drawLine(Vec3(0, 0, 0), Vec3(500, 0, 0));
    drawLine(Vec3(0, 0, 0), Vec3(0, 500, 0));
    drawLine(Vec3(0, 0, 0), Vec3(0, 0, 500));

    setColor(1.0f, 1.0f, 1.0f);
    for (size_t i = 0; i < natnet.getNumMarker(); i++)
        drawBox(natnet.getMarker(i), 15);

    setColor(1.0f, 1.0f, 1.0f, 0.35f);
    for (size_t i = 0; i < natnet.getNumUnlabeledMarker(); i++)
        drawBox(natnet.getUnlabeledMarker(i), 15);

    for (size_t i = 0; i < natnet.getNumRigidBody(); i++) {
        const auto& rb = natnet.getRigidBodyAt(i);
        if (!rb.isActive()) continue;

        const Mat4& M = rb.getMatrix();
        Vec3 o(M.m[3], M.m[7], M.m[11]);
        Vec3 ax(M.m[0], M.m[4], M.m[8]);
        Vec3 ay(M.m[1], M.m[5], M.m[9]);
        Vec3 az(M.m[2], M.m[6], M.m[10]);
        float len = 120.0f;

        setColor(1.0f, 0.2f, 0.2f); drawLine(o, o + ax * len);
        setColor(0.2f, 1.0f, 0.2f); drawLine(o, o + ay * len);
        setColor(0.2f, 0.4f, 1.0f); drawLine(o, o + az * len);
    }

    cam.end();

    string info;
    info += string("connected: ") + (natnet.isConnected() ? "YES" : "NO") + "\n";
    info += "frame: " + to_string(natnet.getFrameNumber()) + "\n";
    info += "data rate: " + to_string((int)natnet.getDataRate()) + "\n";
    info += "rigid bodies: " + to_string(natnet.getNumRigidBody()) + "\n";
    info += "labeled markers: " + to_string(natnet.getNumMarker()) + "\n";
    info += "unlabeled markers: " + to_string(natnet.getNumUnlabeledMarker()) + "\n";

    setColor(1.0f);
    drawBitmapString(info, 20, 20);
}
