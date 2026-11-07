# Source and modifications

Source archive: sea_creature_2.zip, supplied by the user.
Archive SHA256: D3065E6ED74D4356A62EAA46C35438AB2583ECC1AABD9D9387C5B5C2F5007026.

The supplied license.txt contains the original title, source URL, author Comitre, CC-BY-4.0 terms and required credit. Its terms apply to this asset; license.txt preserves all original text and terms, with one final LF byte appended for repository file hygiene.

Modification: removed only scene.gltf bufferViews[7].byteStride (12) and bufferViews[8].byteStride (16), redundant element-sized strides on tightly packed animation outputs, for the existing exporter. All other glTF content, including rest transforms, inverse binds, joints, weights and animation, is unchanged. scene.bin and the other 2 supplied textures are preserved byte-for-byte at their original sizes.

Modification: inverted the green channel of textures/defaultMat_normal.png (255 minus green). The image stays 8-bit RGB at its original dimensions, with red and blue unchanged.
