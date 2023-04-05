/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <string>
#include <vector>

#undef OPAQUE
#undef TRANSPARENT

namespace utils
{
    struct Texture;
    struct Scene;

    typedef std::vector<std::vector<uint8_t>> ShaderCodeStorage;
    typedef void* Mip;

    constexpr uint32_t InvalidIndex = uint32_t(-1);

    enum StaticTexture : uint32_t
    {
        Black,
        Invalid,
        FlatNormal,
        ScramblingRanking1spp,
        SobolSequence
    };

    enum class AlphaMode
    {
        OPAQUE,
        PREMULTIPLIED,
        TRANSPARENT,
        OFF // alpha is 0 everywhere
    };

    enum class DataFolder
    {
        ROOT,
        SHADERS,
        TEXTURES,
        SCENES,
        TESTS
    };

    const char* GetFileName(const std::string& path);
    std::string GetFullPath(const std::string& localPath, DataFolder dataFolder);
    bool LoadFile(const std::string& path, std::vector<uint8_t>& data);
    nri::ShaderDesc LoadShader(nri::GraphicsAPI graphicsAPI, const std::string& path, ShaderCodeStorage& storage, const char* entryPointName = nullptr);
    bool LoadTexture(const std::string& path, Texture& texture, bool computeAvgColorAndAlphaMode = false);
    void LoadTextureFromMemory(nri::Format format, uint32_t width, uint32_t height, const uint8_t *pixels, Texture &texture);
    bool LoadScene(const std::string& path, Scene& scene, bool allowUpdate);

    struct Texture
    {
        Mip* mips = nullptr;
        std::string name;
        uint64_t hash = 0;
        AlphaMode alphaMode = AlphaMode::OPAQUE;
        nri::Format format = nri::Format::UNKNOWN;
        uint16_t width = 0;
        uint16_t height = 0;
        uint16_t depth = 0;
        uint16_t mipNum = 0;
        uint16_t arraySize = 0;

        ~Texture();

        bool IsBlockCompressed() const;
        void GetSubresource(nri::TextureSubresourceUploadDesc& subresource, uint32_t mipIndex, uint32_t arrayIndex = 0) const;

        inline void OverrideFormat(nri::Format fmt)
        { this->format = fmt; }

        inline uint16_t GetArraySize() const
        { return arraySize; }

        inline uint16_t GetMipNum() const
        { return mipNum; }

        inline uint16_t GetWidth() const
        { return width; }

        inline uint16_t GetHeight() const
        { return height; }

        inline uint16_t GetDepth() const
        { return depth; }

        inline nri::Format GetFormat() const
        { return format; }
    };

    struct Material
    {
        uint32_t diffuseMapIndex = StaticTexture::Black; // TODO: use StaticTexture::Invalid for debug purposes
        uint32_t specularMapIndex = StaticTexture::Black;
        uint32_t normalMapIndex = StaticTexture::FlatNormal;
        uint32_t emissiveMapIndex = StaticTexture::Black;
        AlphaMode alphaMode = AlphaMode::OPAQUE;

        inline bool IsOpaque() const
        { return alphaMode == AlphaMode::OPAQUE; }

        inline bool IsAlphaOpaque() const
        { return alphaMode == AlphaMode::PREMULTIPLIED; }

        inline bool IsTransparent() const
        { return alphaMode == AlphaMode::TRANSPARENT; }

        inline bool IsOff() const
        { return alphaMode == AlphaMode::OFF; }

        inline bool IsEmissive() const
        { return emissiveMapIndex != StaticTexture::Black; }
    };

    struct Instance
    {
        float4x4 rotation = float4x4::Identity();
        float4x4 rotationPrev = float4x4::Identity();
        double3 position = double3::Zero();
        double3 positionPrev = double3::Zero();
        float3 scale = float3(1.0f); // TODO: needed to generate hulls representing inner glass surfaces
        uint32_t meshIndex = InvalidIndex;
        uint32_t materialIndex = InvalidIndex;
        bool allowUpdate = false; // if "false" will be merged into the monolithic BLAS together with other static geometry
    };

    struct Mesh
    {
        cBoxf aabb; // must be manually adjusted by instance.rotation.GetScale()
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        uint32_t indexNum = 0;
        uint32_t vertexNum = 0;
        uint32_t blasIndex = InvalidIndex; // BLAS index for dynamic geometry in a user controlled array
    };

    struct Vertex
    {
        float position[3];
        uint32_t uv; // half float
        uint32_t normal; // 10 10 10 2 unorm
        uint32_t tangent; // 10 10 10 2 unorm (.w - handedness)
    };

    struct UnpackedVertex
    {
        float position[3];
        float uv[2];
        float normal[3];
        float tangent[4];
    };

    struct Primitive
    {
        float worldToUvUnits;
        float curvature;
    };

    struct AnimationNode
    {
        std::vector<double3> positionValues;
        std::vector<float4> rotationValues;
        std::vector<float3> scaleValues;
        std::vector<float> positionKeys;
        std::vector<float> rotationKeys;
        std::vector<float> scaleKeys;
        float4x4 mTransform = float4x4::Identity();

        void Update(float time);
    };

    struct NodeTree
    {
        std::vector<NodeTree> children;
        std::vector<uint32_t> instances;
        float4x4 mTransform = float4x4::Identity();
        uint64_t hash = 0;
        uint32_t animationNodeIndex = InvalidIndex;

        void Animate(utils::Scene& scene, const std::vector<AnimationNode>& animationNodes, const float4x4& parentTransform, float4x4* outTransform = nullptr);
        void SetAllowUpdate(utils::Scene& scene, bool allowUpdate);
    };

    struct Animation
    {
        std::vector<AnimationNode> animationNodes;
        NodeTree rootNode;
        NodeTree cameraNode;
        std::string animationName;
        float durationMs = 0.0f;
        float animationProgress;
        float sign = 1.0f;
        float normalizedTime;
        bool hasCameraAnimation;
    };

    typedef uint16_t Index;

    struct Scene
    {
        ~Scene()
        { UnloadTextureData(); }

        // Transient resources - texture & geometry data (can be unloaded after uploading on GPU)
        std::vector<utils::Texture*> textures;
        std::vector<Vertex> vertices;
        std::vector<UnpackedVertex> unpackedVertices;
        std::vector<Index> indices;
        std::vector<Primitive> primitives;

        // Other resources
        std::vector<Material> materials;
        std::vector<Instance> instances;
        std::vector<Mesh> meshes;
        std::vector<Animation> animations;
        float4x4 mSceneToWorld = float4x4::Identity();
        cBoxf aabb;

        void Animate(float animationSpeed, float elapsedTime, float& animationProgress, uint32_t animationIndex, float4x4* outCameraTransform);

        inline void UnloadTextureData()
        {
            for (auto texture : textures)
                delete texture;

            textures.resize(0);
            textures.shrink_to_fit();
        }

        inline void UnloadGeometryData()
        {
            vertices.resize(0);
            vertices.shrink_to_fit();

            unpackedVertices.resize(0);
            unpackedVertices.shrink_to_fit();

            indices.resize(0);
            indices.shrink_to_fit();

            primitives.resize(0);
            primitives.shrink_to_fit();
        }
    };
}

#define NRI_ABORT_ON_FAILURE(result) \
    if ((result) != nri::Result::SUCCESS) \
        exit(1);

#define NRI_ABORT_ON_FALSE(result) \
    if (!(result)) \
        exit(1);
