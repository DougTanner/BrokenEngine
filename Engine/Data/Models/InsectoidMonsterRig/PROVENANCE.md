# Source and modifications

Source archive: insectoid_monster_rig.zip, supplied by the user.
Archive SHA256: 917EDEEB2B94EF44C3E7A65C91EB9A3B02E8D50CC97705FF92A9F8BF4EF6A86B.

The supplied license.txt contains the original title, source URL, author DM-913, CC-BY-4.0 terms and required credit. Its terms apply to this asset; license.txt preserves all original text and terms, with one final LF byte appended for repository file hygiene.

Modification: removed only scene.gltf bufferViews[7].byteStride (12) and bufferViews[8].byteStride (16), redundant element-sized strides on tightly packed animation outputs, for the existing exporter. All other glTF content, including rest transforms, inverse binds, joints, weights and animation, is unchanged. scene.bin and all four supplied textures are preserved byte-for-byte.
