# Source and modifications

Source archive: alien_creature_take_3.zip, supplied by the user.
Archive SHA256: 0C9D4B47C491FAF8602359541B474215C452774AEBF19A776BC1A5E77492806E.

The supplied license.txt contains the original title, source URL, author Rasmus, CC-BY-4.0 terms and required credit. Its terms apply to this asset; license.txt preserves all original text and terms, with one final LF byte appended for repository file hygiene.

Modification: removed only scene.gltf bufferViews[7].byteStride (12) and bufferViews[8].byteStride (16), redundant element-sized strides on tightly packed animation outputs, for the existing exporter. All other glTF content, including rest transforms, inverse binds, joints, weights and animation, is unchanged. scene.bin and all 13 supplied textures are preserved byte-for-byte at their original sizes.
