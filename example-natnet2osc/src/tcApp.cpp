#include "tcApp.h"
#include "mocapOscBridge.h"

void tcApp::setup() {
    setWindowTitle("tcxNatNet - natnet2osc bridge");

    natnet.setup("127.0.0.1");      // Motive server ip
    natnet.setScale(1000.0f);       // metres -> mm
    osc.setup(oscHost, oscPort);    // OSC destination
}

void tcApp::update() {
    natnet.update();

    // forward only when a new NatNet frame arrived
    int frame = natnet.getFrameNumber();
    if (frame == lastSentFrame) return;
    lastSentFrame = frame;

    for (size_t i = 0; i < natnet.getNumRigidBody(); i++) {
        const auto& rb = natnet.getRigidBodyAt(i);
        if (!rb.isActive() || rb.name.empty()) continue;

        // Full tcxFSportsMocap property set. Motive streams Y-up by default,
        // so convert to the Z-up consumer frame (as the original natnet2osc did).
        // If your Motive is configured Z-up, set yUpToZup=false.
        mocapOscBridge::send(osc, rb.name, rb.getMatrix(),
                             /*forward*/ tc::Vec3(0, 0, 1),
                             /*yUpToZup*/ true);
        sentMessages++;
    }
}

void tcApp::draw() {
    clear(0.07f, 0.07f, 0.09f);

    string info;
    info += "NatNet -> OSC bridge (tcxFSportsMocap format)\n\n";
    info += string("Motive connected: ") + (natnet.isConnected() ? "YES" : "NO") + "\n";
    info += "NatNet frame: " + to_string(natnet.getFrameNumber()) + "\n";
    info += "rigid bodies: " + to_string(natnet.getNumRigidBody()) + "\n\n";
    info += "OSC dest: " + oscHost + ":" + to_string(oscPort) + "\n";
    info += "  /rigidbody/<name>/{location3d,orientation,eye3d,...}\n";
    info += "bodies forwarded: " + to_string(sentMessages) + "\n";

    setColor(1.0f);
    drawBitmapString(info, 20, 20);
}
