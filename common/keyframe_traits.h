#pragma once

namespace mmd_mcp {

// KeyframeTraits<KF> base template
// Each keyframe type (Camera, Bone, Morph, etc.) must provide a specialization
// with the following static methods:
//   int frameNo(const KF&)
//   void setFrameNo(KF&, int)
//   int preIndex(const KF&)
//   void setPreIndex(KF&, int)
//   int nextIndex(const KF&)
//   void setNextIndex(KF&, int)
template<typename KF>
struct KeyframeTraits;

} // namespace mmd_mcp
