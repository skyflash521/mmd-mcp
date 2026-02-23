#pragma once

// PMXファイルパーサー (ヘッダオンリー)
// 参照: docs/PMX仕様.txt

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#endif

namespace pmx {

struct MorphInfo {
    std::string name_jp;
    std::string name_en;
    int panel = 0;  // 0:システム予約, 1:眉, 2:目, 3:口, 4:その他
    int type = 0;   // 0:Group, 1:Vertex, 2:Bone, 3-7:UV, 8:Material, 9:Flip, 10:Impulse
};

struct BoneInfo {
    std::string name_jp;
    std::string name_en;
    int parent_index = -1;
};

struct MaterialInfo {
    std::string name_jp;
    std::string name_en;
};

struct ModelInfo {
    std::string name_jp;
    std::string name_en;
    std::string comment_jp;
    std::string comment_en;
    float version = 0;
    int vertex_count = 0;
    int face_count = 0;
    int texture_count = 0;
    std::vector<MaterialInfo> materials;
    std::vector<BoneInfo> bones;
    std::vector<MorphInfo> morphs;
    bool valid = false;
};

namespace detail {

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool hasError() const { return error_; }

    template<typename T>
    T read() {
        if (error_ || pos_ + sizeof(T) > size_) { error_ = true; return T{}; }
        T val;
        std::memcpy(&val, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return val;
    }

    void skip(size_t n) {
        if (error_ || pos_ + n > size_) { error_ = true; return; }
        pos_ += n;
    }

    std::string readText(uint8_t encoding) {
        int32_t len = read<int32_t>();
        if (error_ || len < 0 || pos_ + static_cast<size_t>(len) > size_) {
            error_ = true;
            return {};
        }
        std::string result;
        if (len > 0) {
            if (encoding == 0) {
                result = utf16leToUtf8(data_ + pos_, len);
            } else {
                result.assign(reinterpret_cast<const char*>(data_ + pos_), len);
            }
        }
        pos_ += len;
        return result;
    }

    int readIndex(uint8_t indexSize, bool isVertex = false) {
        if (isVertex) {
            switch (indexSize) {
                case 1: return static_cast<int>(read<uint8_t>());
                case 2: return static_cast<int>(read<uint16_t>());
                case 4: return read<int32_t>();
                default: error_ = true; return -1;
            }
        } else {
            switch (indexSize) {
                case 1: return static_cast<int>(read<int8_t>());
                case 2: return static_cast<int>(read<int16_t>());
                case 4: return read<int32_t>();
                default: error_ = true; return -1;
            }
        }
    }

private:
    static std::string utf16leToUtf8(const uint8_t* data, int byteLen) {
#ifdef _WIN32
        if (byteLen <= 0) return {};
        auto wstr = reinterpret_cast<const wchar_t*>(data);
        int wlen = byteLen / 2;
        int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr, wlen,
                                       nullptr, 0, nullptr, nullptr);
        if (ulen <= 0) return {};
        std::string result(ulen, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, wlen,
                            result.data(), ulen, nullptr, nullptr);
        return result;
#else
        // ASCII範囲のみの簡易フォールバック
        std::string result;
        for (int i = 0; i + 1 < byteLen; i += 2) {
            uint16_t ch = data[i] | (data[i + 1] << 8);
            if (ch < 0x80) result.push_back(static_cast<char>(ch));
            else result.push_back('?');
        }
        return result;
#endif
    }

    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
    bool error_ = false;
};

} // namespace detail

inline ModelInfo parseBuffer(const uint8_t* data, size_t size) {
    ModelInfo info;
    detail::Reader r(data, size);

    // ヘッダ: マジック "PMX " (0x50,0x4D,0x58,0x20)
    if (size < 4) return info;
    if (data[0] != 0x50 || data[1] != 0x4D || data[2] != 0x58 || data[3] != 0x20)
        return info;
    r.skip(4);

    info.version = r.read<float>();
    if (r.hasError()) return info;

    uint8_t flagsLength = r.read<uint8_t>();
    if (r.hasError() || flagsLength < 8) return info;

    uint8_t encoding         = r.read<uint8_t>(); // [0]
    uint8_t additionalUV     = r.read<uint8_t>(); // [1]
    uint8_t vertexIndexSize  = r.read<uint8_t>(); // [2]
    uint8_t textureIndexSize = r.read<uint8_t>(); // [3]
    uint8_t materialIndexSize= r.read<uint8_t>(); // [4]
    uint8_t boneIndexSize    = r.read<uint8_t>(); // [5]
    uint8_t morphIndexSize   = r.read<uint8_t>(); // [6]
    uint8_t rigidBodyIndexSize=r.read<uint8_t>(); // [7]
    if (r.hasError()) return info;

    if (flagsLength > 8) r.skip(flagsLength - 8);

    // モデル情報
    info.name_jp    = r.readText(encoding);
    info.name_en    = r.readText(encoding);
    info.comment_jp = r.readText(encoding);
    info.comment_en = r.readText(encoding);
    if (r.hasError()) return info;

    // 頂点
    info.vertex_count = r.read<int32_t>();
    if (r.hasError() || info.vertex_count < 0) return info;

    for (int i = 0; i < info.vertex_count; ++i) {
        // position(12) + normal(12) + UV(8) + 追加UV(16*n)
        r.skip(32 + static_cast<size_t>(additionalUV) * 16);
        uint8_t deformType = r.read<uint8_t>();
        if (r.hasError()) return info;

        switch (deformType) {
            case 0: r.skip(boneIndexSize); break;                         // BDEF1
            case 1: r.skip(boneIndexSize * 2 + 4); break;                // BDEF2
            case 2: r.skip(boneIndexSize * 4 + 16); break;               // BDEF4
            case 3: r.skip(boneIndexSize * 2 + 4 + 36); break;           // SDEF
            case 4: r.skip(boneIndexSize * 4 + 16); break;               // QDEF (2.1)
            default: return info;
        }
        r.skip(4); // エッジ倍率
        if (r.hasError()) return info;
    }

    // 面
    int32_t indexCount = r.read<int32_t>();
    if (r.hasError() || indexCount < 0) return info;
    info.face_count = indexCount / 3;
    r.skip(static_cast<size_t>(indexCount) * vertexIndexSize);
    if (r.hasError()) return info;

    // テクスチャ
    info.texture_count = r.read<int32_t>();
    if (r.hasError() || info.texture_count < 0) return info;
    for (int i = 0; i < info.texture_count; ++i) {
        r.readText(encoding);
        if (r.hasError()) return info;
    }

    // 材質
    int32_t materialCount = r.read<int32_t>();
    if (r.hasError() || materialCount < 0) return info;
    info.materials.reserve(materialCount);

    for (int i = 0; i < materialCount; ++i) {
        MaterialInfo mat;
        mat.name_jp = r.readText(encoding);
        mat.name_en = r.readText(encoding);
        if (r.hasError()) return info;

        // diffuse(16) + specular(12) + specularity(4) + ambient(12) +
        // drawFlags(1) + edgeColor(16) + edgeSize(4) = 65
        r.skip(65);
        // textureIndex + sphereIndex + sphereMode
        r.skip(static_cast<size_t>(textureIndexSize) * 2 + 1);

        uint8_t toonFlag = r.read<uint8_t>();
        if (r.hasError()) return info;
        r.skip(toonFlag == 0 ? textureIndexSize : 1);

        r.readText(encoding); // メモ
        r.skip(4);             // 面(頂点)数
        if (r.hasError()) return info;

        info.materials.push_back(std::move(mat));
    }

    // ボーン
    int32_t boneCount = r.read<int32_t>();
    if (r.hasError() || boneCount < 0) return info;
    info.bones.reserve(boneCount);

    for (int i = 0; i < boneCount; ++i) {
        BoneInfo bone;
        bone.name_jp = r.readText(encoding);
        bone.name_en = r.readText(encoding);
        if (r.hasError()) return info;

        r.skip(12); // position
        bone.parent_index = r.readIndex(boneIndexSize);
        r.skip(4);  // 変形階層

        uint16_t flags = r.read<uint16_t>();
        if (r.hasError()) return info;

        // 接続先
        if (flags & 0x0001) r.skip(boneIndexSize);
        else                r.skip(12);

        // 回転付与 or 移動付与
        if (flags & 0x0100 || flags & 0x0200)
            r.skip(boneIndexSize + 4);

        // 軸固定
        if (flags & 0x0400) r.skip(12);

        // ローカル軸
        if (flags & 0x0800) r.skip(24);

        // 外部親変形
        if (flags & 0x2000) r.skip(4);

        // IK
        if (flags & 0x0020) {
            r.skip(boneIndexSize + 4 + 4); // target + loopCount + limitAngle
            int32_t linkCount = r.read<int32_t>();
            if (r.hasError() || linkCount < 0) return info;
            for (int j = 0; j < linkCount; ++j) {
                r.skip(boneIndexSize);
                uint8_t hasLimit = r.read<uint8_t>();
                if (r.hasError()) return info;
                if (hasLimit) r.skip(24);
            }
        }
        if (r.hasError()) return info;
        info.bones.push_back(std::move(bone));
    }

    // モーフ
    int32_t morphCount = r.read<int32_t>();
    if (r.hasError() || morphCount < 0) return info;
    info.morphs.reserve(morphCount);

    for (int i = 0; i < morphCount; ++i) {
        MorphInfo morph;
        morph.name_jp = r.readText(encoding);
        morph.name_en = r.readText(encoding);
        morph.panel = r.read<uint8_t>();
        morph.type  = r.read<uint8_t>();

        int32_t dataCount = r.read<int32_t>();
        if (r.hasError() || dataCount < 0) return info;

        size_t entrySize = 0;
        switch (morph.type) {
            case 0:  entrySize = morphIndexSize + 4; break;        // Group
            case 1:  entrySize = vertexIndexSize + 12; break;      // Vertex
            case 2:  entrySize = boneIndexSize + 28; break;        // Bone
            case 3: case 4: case 5: case 6: case 7:                // UV系
                     entrySize = vertexIndexSize + 16; break;
            case 8:  entrySize = materialIndexSize + 113; break;   // Material
            case 9:  entrySize = morphIndexSize + 4; break;        // Flip (2.1)
            case 10: entrySize = rigidBodyIndexSize + 25; break;   // Impulse (2.1)
            default: return info;
        }
        r.skip(static_cast<size_t>(dataCount) * entrySize);
        if (r.hasError()) return info;

        info.morphs.push_back(std::move(morph));
    }

    info.valid = true;
    return info;
}

#ifdef _WIN32
inline ModelInfo parse(const wchar_t* path) {
    ModelInfo info;
    if (!path || !path[0]) return info;

    FILE* fp = _wfopen(path, L"rb");
    if (!fp) return info;

    _fseeki64(fp, 0, SEEK_END);
    long long fileSize = _ftelli64(fp);
    _fseeki64(fp, 0, SEEK_SET);

    if (fileSize <= 0 || fileSize > 512LL * 1024 * 1024) {
        fclose(fp);
        return info;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    size_t readSize = fread(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);

    if (readSize != buffer.size()) return info;

    return parseBuffer(buffer.data(), buffer.size());
}
#endif

} // namespace pmx
