#pragma once

// =============================================================================
// tcxNatNet - TrussC addon for the OptiTrack NatNet protocol
// -----------------------------------------------------------------------------
// Streams 6DOF rigid bodies and 3D markers from OptiTrack Motive over NatNet,
// by wrapping OptiTrack's official NatNet SDK (NatNetClient).
//
//     NatNet nn;
//     nn.setup("192.168.0.1");   // Motive server ip
//     nn.update();               // call once per frame (non-blocking)
//     for (size_t i = 0; i < nn.getNumRigidBody(); i++) { ... }
//
// IMPORTANT — bring-your-own-SDK: this addon does NOT ship the NatNet SDK
// (it is governed by the OptiTrack Plugins EULA). Download it yourself and
// place its include/ and lib/ under libs/natnet/ (or set NATNET_SDK_ROOT).
// See README.md. The addon's own glue code is MIT.
//
// NatNet's client owns its receive thread and pushes frames via a callback;
// we copy each frame into a back buffer. update() swaps it to the front so the
// main thread reads lock-free. Positions are in METRES (Motive native); use
// setScale()/setTransform().
// =============================================================================

#include <tcMath.h>   // tc::Vec3 / tc::Mat4 / tc::Quaternion (not full <TrussC.h>)

#include <memory>
#include <string>
#include <vector>

namespace tc = trussc;

namespace tcx {

class NatNet {
public:
    struct RigidBody {
        int id = -1;
        std::string name;            // empty until descriptions are received
        tc::Mat4 matrix;             // full transform (scale/transform applied)
        tc::Vec3 position;           // convenience: matrix translation
        float meanError = 0.0f;      // Motive mean marker error (metres)
        bool active = false;         // NatNet "Tracked" flag

        bool isActive() const { return active; }
        const tc::Mat4& getMatrix() const { return matrix; }
        tc::Vec3 getPosition() const { return position; }
    };

    NatNet();
    ~NatNet();
    NatNet(const NatNet&) = delete;
    NatNet& operator=(const NatNet&) = delete;

    // serverIp: Motive host. localIp: this machine's NIC ip facing Motive
    // (empty = let the SDK choose). multicast: NatNet multicast vs unicast.
    bool setup(const std::string& serverIp, const std::string& localIp = "",
               bool multicast = true);
    void update();                   // swap buffers; call once per frame
    void close();

    bool isConnected() const;
    int  getFrameNumber() const;
    float getDataRate() const;       // received frames/sec (approx)

    void setScale(float s);          // uniform scale on all positions (m -> ...)
    void setTransform(const tc::Mat4& m);
    tc::Mat4 getTransform() const;

    // Re-fetch rigid-body name<->id mapping from Motive (called once in setup;
    // call again if you create/rename rigid bodies while streaming).
    void requestDescriptions();

    // ---- 6DOF rigid bodies ----
    size_t getNumRigidBody() const;
    const RigidBody& getRigidBodyAt(size_t index) const;
    bool getRigidBody(int id, RigidBody& out) const;
    bool getRigidBodyByName(const std::string& name, RigidBody& out) const;

    // ---- 3D markers ----
    size_t getNumMarker() const;                  // labeled markers
    const tc::Vec3& getMarker(size_t index) const;
    size_t getNumUnlabeledMarker() const;         // "other" (undefined) markers
    const tc::Vec3& getUnlabeledMarker(size_t index) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace tcx
