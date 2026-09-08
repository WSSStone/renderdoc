# Windows Vulkan bridge capture and FBX export

This branch adds the first two features described in
[RenderDoc痛点改造:Vulkan适配, FBX 导出与反检测](https://zhuanlan.zhihu.com/p/2057780832874582470):
Vulkan instance capture coordination and native mesh/texture export.

## Vulkan bridge capture

Open **Tools → Settings → Core → Config Editor**, find **Vulkan → BridgeCapture**
(`Vulkan.BridgeCapture`), enable it and save. Restart the capture target so the injected
library reads the updated configuration. The default is **false**.

A capture triggered through the normal hotkey, API or capture-marker path also starts
ready, idle Vulkan instances registered in the same process. The selected capturer can
be another API (for example an OpenGL presentation window). Each participant writes its
own RDC file. Inspect the additional captures to find the offscreen rendering work.
There is no cross-process coordination, capture merging or resource sharing between RDCs.

Participants are fixed when capture begins. Other participants cannot finish the group;
the initiating capturer ends or discards it. Device teardown discards an active group
before releasing the Vulkan device's resources. Sequential file completion uses the
existing capture filename collision handling. Look for `Bridge capture` in the diagnostic
log for participating capturers and failures.

The feature adds device synchronization and capture overhead. Compilation and synthetic
tests do not establish compatibility with any particular emulator, driver or application.

## Export a mesh and its bound textures

1. Open a capture, select a draw, then select the desired Mesh Viewer stage and instance.
2. Open the export menu and choose **Export FBX and textures...**.
3. Select the position attribute. Check the automatically selected optional normal,
   tangent, UV 0–2 and color mappings; unfamiliar shader names require manual mapping.
4. Choose a new name such as `model.fbx`. Output is written under `model/`, containing
   `model.fbx`, texture images named by resource ID, and `export.txt`.

An existing output directory is rejected to avoid overwriting files or mixing old and new
textures. Errors while saving individual textures are reported as partial success; a mesh
write failure is reported as failure. The report records the event, stage, instance,
texture count and texture failures.

The exporter reads typed buffer values directly, including index offsets, base vertex,
instance rates and primitive restart. Triangle lists, strips and fans are supported.
Other topologies and truncated/clamped mesh data are rejected. It exports the selected
view's mesh data; clear meshlet filters to export the whole draw.

FBX is ASCII 7.4 and requires no Autodesk SDK. Positions use stored XYZ without changing
axes, units, applying world transforms or reconstructing projected positions. Normals and
tangents use XYZ; tangent handedness is not reconstructed. RGB colors receive opaque alpha.
Missing optional attributes are omitted. There is no skinning, animation or material inference.

Read-only textures bound to the current graphics stages are deduplicated by resource ID.
Each saves mip 0, slice 0, with multisamples resolved using RenderDoc's existing SaveTexture
API. Four-component textures use PNG; other textures use JPG. This is a preview export:
HDR/integer/depth data follows SaveTexture's default 0–1 conversion to 8-bit images, and
JPG is lossy. Texture bindings are not inferred to be diffuse/normal/etc. materials.

## Build and automated checks

`.github/workflows/windows-bridge-fbx.yml` runs on pushes to
`codex/vulkan-bridge-fbx-export`. It builds the complete `renderdoc.sln` with VS2022/v143,
x64 Development and Release, using the upstream Qt/Python dependency bundle.

Development runs source formatting checks plus the core and UI unit tests. Synthetic FBX
tests write a fixture that a separate Linux job loads using a pinned version of
[ufbx](https://github.com/ufbx/ufbx). That job checks positions, triangle topology, normals,
tangents, vertex colors and all three UV sets. ufbx is a CI dependency only.

Artifacts include build/test logs and the complete `x64/Release` output. Extract the Release
artifact and launch `qrenderdoc.exe`; keep its DLLs and runtime subdirectories together.
Real application capture and DCC import workflows remain manual validation.
