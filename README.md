# tcxNatNet

TrussC addon for receiving motion-capture data from **OptiTrack Motive** over the **NatNet** protocol. Wraps OptiTrack's official **NatNet SDK** (`NatNetClient`) behind a small `tcx::NatNet` class — 6DOF rigid bodies + 3D markers, full NatNet 4.x support.

## Bring your own SDK (this addon does not ship it)

The NatNet SDK is governed by the **OptiTrack Plugins EULA** and cannot be redistributed, so it is **not bundled** here. You download it once and drop it in:

1. Get the NatNet SDK: https://optitrack.com/support/downloads/developer-tools.html
2. Place its `include/` and `lib/` under **`libs/natnet/`** in this addon:
   ```
   tcxNatNet/libs/natnet/include/NatNetClient.h, NatNetTypes.h, ...
   tcxNatNet/libs/natnet/lib/x64/NatNetLib.lib + NatNetLib.dll   (Windows)
   tcxNatNet/libs/natnet/lib/libNatNet.so                        (Linux)
   ```
   (or reconfigure with `-DNATNET_SDK_ROOT=/path/to/NatNetSDK`)
CMake prints download/placement instructions if the SDK is missing.

### Windows: `NatNetLib.dll` next to the `.exe` (automated)

On Windows the build links `NatNetLib.lib`, but `NatNetLib.dll` is loaded **at
runtime** from the executable's folder. The bundled examples copy it automatically
via a post-build step, so `trusscli build` "just works".

For **your own app**, add one line to its `CMakeLists.txt` after `trussc_app()`:

```cmake
trussc_app()
tcxnatnet_copy_runtime(${PROJECT_NAME})   # copies NatNetLib.dll beside the exe on Windows
```

`tcxnatnet_copy_runtime(<exe-target>)` is provided by the addon; it's a no-op on
Linux/macOS (there the build embeds an rpath to `libNatNet.so`). If you'd rather not
use it, copy the DLL by hand once per build folder:

```
copy libs\natnet\lib\x64\NatNetLib.dll  <your-app>\bin\
```

> Why this layout? The protocol could be reimplemented clean-room (that's what the MIT `ofxNatNet` did, limited to ~NatNet 3.1). Wrapping the official SDK instead gives full, maintained 4.x support — at the cost of the one-time SDK download. The addon's own code stays MIT and ships no OptiTrack files.

## Usage

```cpp
#include "tcxNatNet.h"

tcx::NatNet natnet;

void tcApp::setup() {
    natnet.setup("192.168.0.1");   // Motive server ip; optional 2nd arg = local NIC ip
    natnet.setScale(100.0f);      // NatNet is metres -> mm
}

void tcApp::update() { natnet.update(); }   // non-blocking; once per frame

void tcApp::draw() {
    for (size_t i = 0; i < natnet.getNumRigidBody(); i++) {
        const auto& rb = natnet.getRigidBodyAt(i);
        // rb.getMatrix(), rb.getPosition(), rb.name, rb.isActive()
    }
}
```

Rotation comes from Motive as a quaternion, converted to `tc::Mat4` (unambiguous —
no transpose guesswork). Positions are **metres**; use `setScale()` / `setTransform()`.
Rigid-body names come from Motive's data descriptions (fetched on connect;
call `requestDescriptions()` again if you add/rename bodies while streaming).

## Examples

- **example-basic** — 3D viewer: markers as boxes, rigid bodies as axis gizmos.

## Build

Needs the SDK in place (above). Requires **C++20** (TrussC core). Add `tcxNatNet`
to `addons.make` and run `trusscli update`. Officially supports **Windows + Linux**
(NatNet SDK has no macOS build).


## License

MIT (glue). The NatNet SDK is OptiTrack's under its EULA — not included.
