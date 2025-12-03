#version 330 core
in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D iconTexture;
uniform float brightness;  // 1.0 = full brightness, 0.65 = dimmed

void main() {
    vec4 texColor = texture(iconTexture, fragTexCoord);
    fragColor = vec4(texColor.rgb * brightness, texColor.a);
}
