#version 330 core
in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D iconTexture;

void main() {
    fragColor = texture(iconTexture, fragTexCoord);
}
