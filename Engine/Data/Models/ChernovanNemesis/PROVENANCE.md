# Source and modifications

Source archive: chernovan_nemesis.zip, supplied by the user.

The supplied license.txt contains the original title, source URL, author, license and required credit. Its terms apply to this asset; license.txt preserves all original text and terms, with one final LF byte appended for repository file hygiene.

Integration removed only bufferViews[5].byteStride and bufferViews[6].byteStride from scene.gltf. Both described tightly packed FLOAT animation data; buffer bytes, accessor offsets, animation channels and all other JSON values are preserved.
