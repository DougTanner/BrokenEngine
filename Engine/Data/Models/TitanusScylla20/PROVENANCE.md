# Source and modifications

Source archive: titanus_scylla_2.0.zip, supplied by the user.
Archive SHA256: 1D4A2E3B1E19629303E44D4A5C0F2331EB9444140D4185BC9AD84B49E42C0CC8.

The supplied license.txt contains the original title, source URL, author ArachnoBoy, CC-BY-NC-SA-4.0 terms and required credit. Its terms apply to this asset; license.txt preserves all original text and terms, with one final LF byte appended for repository file hygiene.

Modification: removed only scene.gltf bufferViews[7].byteStride (12) and bufferViews[8].byteStride (16), redundant element-sized strides on tightly packed animation outputs, for the existing exporter. All other glTF content, including rest transforms, inverse binds, joints, weights and animation, is unchanged. scene.bin and all 10 supplied textures are preserved byte-for-byte at their original sizes.

This modified asset remains under CC-BY-NC-SA-4.0; no commercial use is permitted by the supplied terms.
