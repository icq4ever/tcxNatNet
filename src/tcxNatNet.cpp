#include "tcxNatNet.h"

#include <tc/utils/tcLog.h>   // logNotice/logWarning (without full <TrussC.h>)

// OptiTrack NatNet SDK (resolved from the user-provided SDK; NOT bundled).
#include <NatNetClient.h>
#include <NatNetTypes.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>

namespace tcx {

using tc::Vec3;
using tc::Mat4;
using tc::Quaternion;

struct NatNet::Impl {
    struct Frame {
        int frameNumber = 0;
        std::vector<RigidBody> rigidbodies;
        std::vector<Vec3> markers;     // labeled
        std::vector<Vec3> unlabeled;   // "other"
    };

    NatNetClient client;
    std::string serverIp, localIp;
    bool multicast = true;
    std::atomic<bool> connected{false};

    std::mutex mutex;
    Frame back;                        // written by NatNet callback thread
    Frame front;                       // read by main thread

    std::map<int, int> idToIndex;
    std::map<std::string, int> nameToIndex;
    std::map<int, std::string> idToName;   // from data descriptions

    int frameNumber = 0;
    Mat4 transform = Mat4::identity();

    float dataRate = 0.0f;
    std::chrono::steady_clock::time_point lastTime{};
    int lastFrameNumber = 0;
    bool haveLastTime = false;

    void onFrame(sFrameOfMocapData* data);
    void fetchDescriptions();

    // NatNet C callback trampoline (static member can access private Impl)
    static void NATNET_CALLCONV frameCallback(sFrameOfMocapData* data, void* userData) {
        static_cast<Impl*>(userData)->onFrame(data);
    }
};

// NatNet pushes frames on its own thread; copy out into the back buffer.
void NatNet::Impl::onFrame(sFrameOfMocapData* data) {
    if (!data) return;
    Frame f;
    f.frameNumber = data->iFrame;

    f.rigidbodies.reserve(data->nRigidBodies);
    for (int i = 0; i < data->nRigidBodies; i++) {
        const sRigidBodyData& rb = data->RigidBodies[i];
        RigidBody out;
        out.id = rb.ID;
        out.meanError = rb.MeanError;
        out.active = (rb.params & 0x01) != 0;   // bit0 = Tracked
        auto it = idToName.find(rb.ID);
        if (it != idToName.end()) out.name = it->second;

        Vec3 pos = transform * Vec3(rb.x, rb.y, rb.z);
        // NatNet quaternion (qx,qy,qz,qw) -> tc::Quaternion(w,x,y,z)
        Mat4 rotM = Quaternion(rb.qw, rb.qx, rb.qy, rb.qz).toMatrix();
        out.matrix = Mat4::translate(pos) * rotM;
        out.position = pos;
        f.rigidbodies.push_back(std::move(out));
    }

    f.markers.reserve(data->nLabeledMarkers);
    for (int i = 0; i < data->nLabeledMarkers; i++) {
        const sMarker& m = data->LabeledMarkers[i];
        f.markers.push_back(transform * Vec3(m.x, m.y, m.z));
    }

    f.unlabeled.reserve(data->nOtherMarkers);
    for (int i = 0; i < data->nOtherMarkers; i++) {
        const MarkerData& m = data->OtherMarkers[i];
        f.unlabeled.push_back(transform * Vec3(m[0], m[1], m[2]));
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        back = std::move(f);
    }
}

void NatNet::Impl::fetchDescriptions() {
    sDataDescriptions* descs = nullptr;
    if (client.GetDataDescriptionList(&descs) != ErrorCode_OK || !descs) return;
    std::map<int, std::string> map;
    for (int i = 0; i < descs->nDataDescriptions; i++) {
        const sDataDescription& d = descs->arrDataDescriptions[i];
        if (d.type == Descriptor_RigidBody && d.Data.RigidBodyDescription) {
            const sRigidBodyDescription* rb = d.Data.RigidBodyDescription;
            map[rb->ID] = rb->szName;
        }
    }
    std::lock_guard<std::mutex> lock(mutex);
    idToName = std::move(map);
}

// ---------------------------------------------------------------------------

NatNet::NatNet() : impl(std::make_unique<Impl>()) {}
NatNet::~NatNet() { close(); }

bool NatNet::setup(const std::string& serverIp, const std::string& localIp, bool multicast) {
    close();
    impl->serverIp = serverIp;
    impl->localIp = localIp;
    impl->multicast = multicast;

    sNatNetClientConnectParams params;
    params.connectionType = multicast ? ConnectionType_Multicast : ConnectionType_Unicast;
    params.serverAddress = impl->serverIp.c_str();
    params.localAddress = impl->localIp.empty() ? nullptr : impl->localIp.c_str();

    impl->client.SetFrameReceivedCallback(&Impl::frameCallback, impl.get());
    ErrorCode ec = impl->client.Connect(params);
    if (ec != ErrorCode_OK) {
        tc::logError("tcxNatNet") << "Connect failed (ErrorCode " << (int)ec << ")";
        impl->connected = false;
        return false;
    }
    impl->connected = true;
    tc::logNotice("tcxNatNet") << "connected to Motive " << serverIp;
    impl->fetchDescriptions();
    return true;
}

void NatNet::close() {
    if (impl->connected.load()) {
        impl->client.Disconnect();
        impl->connected = false;
    }
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->back = Impl::Frame();
    }
    impl->front = Impl::Frame();
    impl->idToIndex.clear();
    impl->nameToIndex.clear();
    impl->haveLastTime = false;
}

void NatNet::requestDescriptions() { impl->fetchDescriptions(); }

void NatNet::update() {
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->front = impl->back;
    }
    impl->frameNumber = impl->front.frameNumber;

    impl->idToIndex.clear();
    impl->nameToIndex.clear();
    for (int i = 0; i < (int)impl->front.rigidbodies.size(); i++) {
        const RigidBody& rb = impl->front.rigidbodies[i];
        impl->idToIndex[rb.id] = i;
        if (!rb.name.empty()) impl->nameToIndex[rb.name] = i;
    }

    auto now = std::chrono::steady_clock::now();
    if (impl->haveLastTime) {
        float dt = std::chrono::duration<float>(now - impl->lastTime).count();
        int df = impl->frameNumber - impl->lastFrameNumber;
        if (dt > 0.0f && df > 0)
            impl->dataRate = impl->dataRate * 0.9f + (df / dt) * 0.1f;
    }
    impl->lastTime = now;
    impl->lastFrameNumber = impl->frameNumber;
    impl->haveLastTime = true;
}

bool NatNet::isConnected() const { return impl->connected.load(); }
int NatNet::getFrameNumber() const { return impl->frameNumber; }
float NatNet::getDataRate() const { return impl->dataRate; }

void NatNet::setScale(float s) { impl->transform = Mat4::scale(s); }
void NatNet::setTransform(const Mat4& m) { impl->transform = m; }
Mat4 NatNet::getTransform() const { return impl->transform; }

size_t NatNet::getNumRigidBody() const { return impl->front.rigidbodies.size(); }
const NatNet::RigidBody& NatNet::getRigidBodyAt(size_t index) const {
    return impl->front.rigidbodies[index];
}
bool NatNet::getRigidBody(int id, RigidBody& out) const {
    auto it = impl->idToIndex.find(id);
    if (it == impl->idToIndex.end()) return false;
    out = impl->front.rigidbodies[it->second];
    return true;
}
bool NatNet::getRigidBodyByName(const std::string& name, RigidBody& out) const {
    auto it = impl->nameToIndex.find(name);
    if (it == impl->nameToIndex.end()) return false;
    out = impl->front.rigidbodies[it->second];
    return true;
}

size_t NatNet::getNumMarker() const { return impl->front.markers.size(); }
const Vec3& NatNet::getMarker(size_t index) const { return impl->front.markers[index]; }
size_t NatNet::getNumUnlabeledMarker() const { return impl->front.unlabeled.size(); }
const Vec3& NatNet::getUnlabeledMarker(size_t index) const {
    return impl->front.unlabeled[index];
}

} // namespace tcx
