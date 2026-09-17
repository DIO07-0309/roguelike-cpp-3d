#pragma once
#include "raylib.h"
#include <array>
#include <optional>
#include <vector>

struct HD2DDrawItem;
struct AvatarPartDraw;

namespace hd2d {
void appendAvatarParts(const std::vector<AvatarPartDraw>& parts, Vector3 feet_world,
                       float sort_y, unsigned char alpha, float blob_width,
                       std::vector<HD2DDrawItem>& out_items);

struct PartQuad {
    std::array<Vector3, 4> positions;
    std::array<Vector2, 4> uvs;
    Vector3 normal;
};

std::optional<PartQuad> buildPartQuad(const HD2DDrawItem& item, const Camera3D& camera);
bool isGhostPart(const HD2DDrawItem& item);
std::vector<const HD2DDrawItem*> orderedGhostParts(
    const std::vector<HD2DDrawItem>& items, const Camera3D& camera);

class PartColorShader {
public:
    bool initialize(Shader shader);
    bool ready() const { return shader_.id != 0; }
    void draw(const HD2DDrawItem& item, const Camera3D& camera) const;

private:
    Shader shader_{};
    int offset_loc_ = -1;
    int threshold_loc_ = -1;
    int shadow_on_loc_ = -1;
};

enum class PartPass { Color, Shadow };
void drawPartQuad(const HD2DDrawItem& item, const Camera3D& camera,
                  PartPass pass = PartPass::Color);
}
