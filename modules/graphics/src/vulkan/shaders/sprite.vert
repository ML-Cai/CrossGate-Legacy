#version 450
layout(push_constant) uniform Sprite {
    vec4 rect;
    vec4 uvRect;
    float layer;
} sprite;
layout(location = 0) out vec2 uv;
const vec2 corners[4] = vec2[](
    vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1));
void main() {
    vec2 corner = corners[gl_VertexIndex];
    uv = sprite.uvRect.xy + corner * sprite.uvRect.zw;
    gl_Position = vec4(sprite.rect.xy + corner * sprite.rect.zw, 0, 1);
}
