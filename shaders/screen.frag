#version 330 core
in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D screenTexture;

void main() {
    // Flip Y coordinate - FBO origin is bottom-left, texture origin is top-left
    vec2 flippedCoord = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);
    fragColor = texture(screenTexture, flippedCoord);
}
