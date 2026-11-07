# Source and modifications

Modification: inverted the green channel of textures/lambert1_normal.png (255 minus green). The image stays 8-bit RGB at its original dimensions, with red and blue unchanged. The channel is confirmed by the map's slotted screw heads: with red read along +U, each sampled slot that `textures/lambert1_metallicRoughness.png` marks lies below both halves of its head, so the supplied relief is raised and only green opposed the scene's authored bitangent, which runs along +V.
