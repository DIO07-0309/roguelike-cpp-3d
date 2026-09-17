#include "hd2d_part_geometry.h"
#include "hd2d_renderer.h"
#include "game/animation/player_avatar.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace hd2d {
void appendAvatarParts(const std::vector<AvatarPartDraw>& parts, Vector3 feet_world,
                       float sort_y, unsigned char alpha, float blob_width,
                       std::vector<HD2DDrawItem>& out_items) {
    for (const auto& part : parts) {
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        item.pro_mode = true;
        item.world_pos = feet_world;
        item.part_offset = part.offset;
        item.size = part.size.x;
        item.height = part.size.y;
        item.pivot_uv_px = part.pivot;
        item.rot_deg = part.rot_deg;
        item.texture = part.tex;
        item.flip_x = part.flip_x;
        item.sort_y = sort_y;
        item.tint = {255, 255, 255, alpha};
        item.blob_width = blob_width;
        out_items.push_back(item);
        blob_width = 0;
    }
}

namespace {
bool cameraBasis(const Camera3D& camera, Vector3& right, Vector3& up) {
    const Vector3 forward = Vector3Subtract(camera.target, camera.position);
    if (Vector3Length(forward) < 1e-6f) return false;
    right = Vector3CrossProduct(Vector3Normalize(forward), camera.up);
    if (Vector3Length(right) < 1e-6f) return false;
    right = Vector3Normalize(right);
    up = Vector3CrossProduct(right, Vector3Normalize(forward));
    return true;
}

std::array<Vector2, 4> partUVs(const HD2DDrawItem& item) {
    const Rectangle source = item.tex_src.width != 0 ? item.tex_src
        : Rectangle{0, 0, float(item.texture.width), float(item.texture.height)};
    float left = source.x / item.texture.width;
    float right = (source.x + std::fabs(source.width)) / item.texture.width;
    float top = source.y / item.texture.height;
    float bottom = (source.y + std::fabs(source.height)) / item.texture.height;
    if (item.flip_x != (source.width < 0)) std::swap(left, right);
    if (source.height < 0) std::swap(top, bottom);
    return {{{left, top}, {left, bottom}, {right, bottom}, {right, top}}};
}
}

std::optional<PartQuad> buildPartQuad(const HD2DDrawItem& item, const Camera3D& camera) {
    if (!item.texture.id || item.texture.width <= 0 || item.texture.height <= 0
        || item.size <= 0 || item.height <= 0) return std::nullopt;
    Vector3 right{}, up{};
    if (!cameraBasis(camera, right, up)) return std::nullopt;
    const float radians = item.rot_deg * DEG2RAD;
    const float cosine = std::cos(radians), sine = std::sin(radians);
    const Vector2 corners[] = {{0, 0}, {0, item.height},
                              {item.size, item.height}, {item.size, 0}};
    PartQuad quad;
    quad.uvs = partUVs(item);
    quad.normal = Vector3CrossProduct(right, up);
    for (size_t index = 0; index < quad.positions.size(); ++index) {
        const float local_x = corners[index].x - item.pivot_uv_px.x;
        const float local_y = corners[index].y - item.pivot_uv_px.y;
        const float plane_x = item.part_offset.x + local_x * cosine - local_y * sine;
        const float plane_y = item.part_offset.y - local_x * sine - local_y * cosine;
        quad.positions[index] = Vector3Add(item.world_pos,
            Vector3Add(Vector3Scale(right, plane_x), Vector3Scale(up, plane_y)));
    }
    return quad;
}

bool isGhostPart(const HD2DDrawItem& item) {
    return item.kind == HD2DDrawItem::Kind::ENTITY_BILLBOARD
        && item.pro_mode && item.tint.a < 255;
}

std::vector<const HD2DDrawItem*> orderedGhostParts(
    const std::vector<HD2DDrawItem>& items, const Camera3D& camera) {
    std::vector<const HD2DDrawItem*> ghosts;
    for (const auto& item : items)
        if (isGhostPart(item)) ghosts.push_back(&item);
    const Vector3 forward = Vector3Subtract(camera.target, camera.position);
    std::stable_sort(ghosts.begin(), ghosts.end(), [forward](const auto* left, const auto* right) {
        return Vector3DotProduct(left->world_pos, forward)
            > Vector3DotProduct(right->world_pos, forward);
    });
    return ghosts;
}

bool PartColorShader::initialize(Shader shader) {
    shader_ = {};
    if (!shader.id || shader.id == rlGetShaderIdDefault()) return false;
    offset_loc_ = GetShaderLocation(shader, "uTexelOffset");
    threshold_loc_ = GetShaderLocation(shader, "uAlphaThreshold");
    shadow_on_loc_ = GetShaderLocation(shader, "shadowEnabled");
    if (offset_loc_ < 0 || threshold_loc_ < 0 || shadow_on_loc_ < 0) return false;
    shader_ = shader;
    return true;
}

void PartColorShader::draw(const HD2DDrawItem& item, const Camera3D& camera) const {
    if (!ready()) return;
    const float zero = 0.f, alpha_cutoff = 0.5f;
    BeginShaderMode(shader_);
    SetShaderValue(shader_, offset_loc_, &zero, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, threshold_loc_, &alpha_cutoff, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, shadow_on_loc_, &zero, SHADER_UNIFORM_FLOAT);
    drawPartQuad(item, camera);
    EndShaderMode();
}

void drawPartQuad(const HD2DDrawItem& item, const Camera3D& camera, PartPass pass) {
    if (pass == PartPass::Shadow && item.tint.a < 128) return;
    const auto quad = buildPartQuad(item, camera);
    if (!quad) return;
    if (pass == PartPass::Color && item.tint.a < 255) {
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
    }
    rlSetTexture(item.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(item.tint.r, item.tint.g, item.tint.b, item.tint.a);
    rlNormal3f(quad->normal.x, quad->normal.y, quad->normal.z);
    for (size_t index = 0; index < quad->positions.size(); ++index) {
        const auto& position = quad->positions[index];
        rlTexCoord2f(quad->uvs[index].x, quad->uvs[index].y);
        rlVertex3f(position.x, position.y, position.z);
    }
    rlEnd();
    rlSetTexture(0);
    if (pass == PartPass::Color && item.tint.a < 255) {
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
    }
}
}
