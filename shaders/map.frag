#version 330 core
in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D mapTexture;
uniform sampler2D indexTexture;
uniform sampler2D ownershipTexture;  // 60 rows x 8 cols, RGB - row=territory ID, cols for different modes
uniform int highlightedTerritory;  // Hovered territory
uniform int selectedTerritory;     // Selected territory (e.g., for city destruction)
uniform int borderRadius;
uniform vec2 mapSize;  // Map dimensions in pixels for neighbor sampling
uniform int heatMapColumn;         // LUT column to use: 0=ownership, 1-4=heat maps
uniform float lutWidth;            // LUT texture width (for correct coord calculation)

void main() {
    vec4 mapColor = texture(mapTexture, fragTexCoord);

    // Sample territory index (8-bit grayscale, value 0-255)
    float territoryValue = texture(indexTexture, fragTexCoord).r * 255.0;
    int territoryId = int(territoryValue + 0.5);  // Round to nearest int

    // Check if we need to draw a border for this territory
    bool drawBorder = false;
    vec3 borderColor = vec3(0.0);

    if (territoryId > 0) {
        // Look up ownership color from ownership texture (always column 0 for borders)
        // Row = territory ID (0-59 for IDs 1-60), Column = 0
        // Texture coords: x = (col + 0.5) / lutWidth, y = (territoryId - 1 + 0.5) / 60.0
        vec2 ownershipCoord = vec2(0.5 / lutWidth, (float(territoryId) - 0.5) / 60.0);
        vec3 ownerColor = texture(ownershipTexture, ownershipCoord).rgb;

        // Only draw border if territory is owned (has non-black color)
        if (ownerColor.r > 0.0 || ownerColor.g > 0.0 || ownerColor.b > 0.0) {
            // Check neighbors within radius for different territory
            vec2 pixelSize = vec2(1.0 / mapSize.x, 1.0 / mapSize.y);

            for (int dy = -borderRadius; dy <= borderRadius && !drawBorder; dy++) {
                for (int dx = -borderRadius; dx <= borderRadius && !drawBorder; dx++) {
                    if (dx == 0 && dy == 0) continue;  // Skip self

                    vec2 neighborCoord = fragTexCoord + vec2(float(dx), float(dy)) * pixelSize;
                    float neighborValue = texture(indexTexture, neighborCoord).r * 255.0;
                    int neighborId = int(neighborValue + 0.5);

                    // If neighbor is a different territory, we're near a border
                    if (neighborId != territoryId) {
                        drawBorder = true;
                        borderColor = ownerColor;
                    }
                }
            }
        }
    }

    // Heat map mode rendering (columns 1-4)
    if (heatMapColumn > 0 && territoryId > 0) {
        // Look up heat map color from the specified column
        vec2 heatMapCoord = vec2((float(heatMapColumn) + 0.5) / lutWidth, (float(territoryId) - 0.5) / 60.0);
        vec3 heatColor = texture(ownershipTexture, heatMapCoord).rgb;
        bool hasHeatData = (heatColor.r > 0.0 || heatColor.g > 0.0 || heatColor.b > 0.0);

        if (drawBorder) {
            // Still show ownership borders in heat map mode
            fragColor = vec4(borderColor, 1.0);
        } else if (hasHeatData) {
            // Apply heat map color with strong tint
            fragColor = vec4(mix(mapColor.rgb, heatColor, 0.6), 1.0);
        } else {
            // No heat data - darken significantly to show "unreachable" or "no data"
            fragColor = vec4(mapColor.rgb * 0.4, 1.0);
        }
        return;
    }

    // Normal ownership mode (column 0)
    // Apply highlighting, border, and interior tint
    if (drawBorder) {
        // Check if this is the selected territory - if so, make border thicker/brighter
        if (selectedTerritory > 0 && territoryId == selectedTerritory) {
            // Bright orange/red border for selected territory
            fragColor = vec4(1.0, 0.3, 0.0, 1.0);
        } else {
            // Draw border with solid player color
            fragColor = vec4(borderColor, 1.0);
        }
    } else if (territoryId > 0) {
        // Check if this territory is owned (for interior tinting)
        vec2 ownershipCoord = vec2(0.5 / lutWidth, (float(territoryId) - 0.5) / 60.0);
        vec3 ownerColor = texture(ownershipTexture, ownershipCoord).rgb;
        bool isOwned = (ownerColor.r > 0.0 || ownerColor.g > 0.0 || ownerColor.b > 0.0);

        if (highlightedTerritory > 0) {
            // A territory is being hovered
            if (territoryId == highlightedTerritory) {
                // Brighten and add yellow tint to highlighted territory
                vec3 highlight = vec3(1.0, 1.0, 0.6);  // Warm yellow
                fragColor = vec4(mix(mapColor.rgb, highlight, 0.35), 1.0);
            } else {
                // Darken non-highlighted areas, but apply owner tint if owned
                vec3 darkened = mapColor.rgb * 0.7;
                if (isOwned) {
                    fragColor = vec4(mix(darkened, ownerColor, 0.2), 1.0);
                } else {
                    fragColor = vec4(darkened, 1.0);
                }
            }
        } else {
            // No territory hovered - apply owner tint if owned
            if (isOwned) {
                fragColor = vec4(mix(mapColor.rgb, ownerColor, 0.25), 1.0);
            } else {
                fragColor = mapColor;
            }
        }
    } else {
        // Background (territoryId == 0)
        if (highlightedTerritory > 0) {
            fragColor = vec4(mapColor.rgb * 0.7, 1.0);
        } else {
            fragColor = mapColor;
        }
    }
}
