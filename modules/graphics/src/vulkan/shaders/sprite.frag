#version 450
layout(binding = 0) uniform sampler2DArray frames;
layout(push_constant) uniform Sprite {
    vec4 rect;
    float layer;
} sprite;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
void main() {
    color = texture(frames, vec3(uv, sprite.layer));
}
