# Art Loader Dependencies

- cgltf v1.15: https://github.com/jkuhlmann/cgltf/tree/v1.15 (MIT, license embedded at the end of cgltf.h).
- JSON for Modern C++ v3.12.0: https://github.com/nlohmann/json/tree/v3.12.0 (MIT, license embedded in json.hpp).

These pinned, unmodified single-file dependencies are vendored for offline builds. Only ArtAsset.cpp defines CGLTF_IMPLEMENTATION. Do not format the vendor headers.
