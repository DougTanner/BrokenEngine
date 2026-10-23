# Source and modifications

Source archive: hit2_-_hundra_high_poly.zip, supplied by the user.
Archive SHA256: C8BAC5F257D0A7E233B713FF2B2F334AB9DB69932B397A98CC33D6371DE259B7.

This work is based on "HIT2 - Hundra (High Poly)" (https://sketchfab.com/3d-models/hit2-hundra-high-poly-5c7189bc29444fb9ad3c5a32ccde0d44) by doctorA (https://sketchfab.com/doctorA) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/).
The supplied license.txt preserves all original attribution, text and terms, with exactly one final LF appended for repository file hygiene: 708 original bytes become 709 bytes.
Original license SHA256: 39119BF115E6C02C7055CB6A1D089D8B5AA005319AF2CC0B88FEE169BC02BF97.
Current license SHA256: AE9AF569720CD30C408679220C93C324D6A6F0DCB574D8B3BC9429B4ECA9D5C5.

Modification: removed only scene.gltf bufferViews[7].byteStride (12) and
bufferViews[8].byteStride (16), redundant element-sized strides on tightly
packed animation outputs, for the existing exporter. All other glTF content,
including source COLOR_0 attributes and accessors, rest transforms, inverse
binds, joints, weights, materials, all 17 animation clips and original key
times, is unchanged. scene.bin and all seven supplied textures are preserved
byte-for-byte at their original sizes. The original archive and extracted source
files are unchanged.

Accepted presentation limitation: the current packed vertex format omits vertex colors, so packed appearance ignores the source COLOR_0 tint, including varying colors on three primitives. Source colors remain intact. The exporter warns for each visited primitive declaring unsupported COLOR_0, identifying the source and primitive and stating that it is ignored in packed appearance. No color baking, geometry splitting, texture changes or packed-format changes are applied.
