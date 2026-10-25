"""Export one authored scene and its consumed external dependencies with Blender 5.2."""

import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import types

import bpy
import numpy


dependencies = set()
CANDIDATE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".tga"}
CHANNEL_WORDS = {
    "base color": ("basecolor", "albedo", "diffuse", "diff", "colour", "color"),
    "normal": ("normalopengl", "normaldirectx", "normal", "normals", "nor"),
    "metallic": ("metallic", "metalness"),
    "roughness": ("roughness",),
    "emissive": ("emissive", "emit", "glow"),
    "opacity": ("opacity",),
}
CHANNEL_INPUTS = {
    "base color": "Base Color",
    "normal": "Normal",
    "metallic": "Metallic",
    "roughness": "Roughness",
    "emissive": "Emission Color",
    "opacity": "Alpha",
}
# Pose channels are relative to the bone's rest pose, which the exporter writes as the joint transform.
IDENTITY_TRANSFORM = {
    "location": (0.0, 0.0, 0.0),
    "rotation_quaternion": (1.0, 0.0, 0.0, 0.0),
    "rotation_euler": (0.0, 0.0, 0.0),
    "rotation_axis_angle": (0.0, 0.0, 1.0, 0.0),
    "scale": (1.0, 1.0, 1.0),
}


def record_library(datablock):
    library = getattr(datablock, "library", None)
    while library is not None:
        path = Path(bpy.path.abspath(library.filepath, library=library.parent)).resolve()
        if not path.is_file():
            raise RuntimeError(f"Missing consumed Blender library: {path}")
        dependencies.add(str(path))
        library = library.parent


def record_image(image):
    record_library(image)
    if not all(image.size):
        raise RuntimeError(f"Unreadable consumed Blender image: {image.filepath}")
    if image.source not in {"FILE", "TILED"} or image.packed_file or image.packed_files:
        return
    path = Path(bpy.path.abspath(image.filepath, library=image.library)).resolve()
    paths = [Path(str(path).replace("<UDIM>", str(tile.number))) for tile in image.tiles] if image.source == "TILED" else [path]
    for path in paths:
        if not path.is_file():
            raise RuntimeError(f"Missing consumed Blender image: {path}")
        dependencies.add(str(path))


def record_material_libraries(source):
    with source.open("rb") as stream:
        for line in stream:
            words = line.split(None, 1)
            if len(words) == 2 and words[0] == b"mtllib":
                # Blender reads the rest of the line as one path, dropping enclosing quotes.
                name = words[1].strip().decode("utf-8", "replace")
                if len(name) > 2 and name.startswith('"') and name.endswith('"'):
                    name = name[1:-1]
                path = (source.parent / name).resolve()
                if path.is_file():
                    dependencies.add(str(path))
    # Blender also reads the same-stem .mtl whether or not an mtllib line names it.
    path = source.with_suffix(".mtl").resolve()
    if path.is_file():
        dependencies.add(str(path))


def load_source(source):
    extension = source.suffix.lower()
    if extension == ".blend":
        bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False, use_scripts=False)
        return
    # Factory startup holds a default cube that the export would include.
    bpy.ops.wm.read_factory_settings(use_empty=True)
    alphas = []
    if extension == ".fbx":
        from io_scene_fbx import import_fbx

        original_read_material = import_fbx.blen_read_material

        def read_material(fbx_tmpl, fbx_obj, settings):
            material = original_read_material(fbx_tmpl, fbx_obj, settings)
            props = (import_fbx.elem_find_first(fbx_obj, b"Properties70"),
                     import_fbx.elem_find_first(fbx_tmpl, b"Properties70", import_fbx.fbx_elem_nil))
            # The importer's alpha formula, except that it reads Opacity without the Material
            # template, so a material inheriting the template's Opacity falls back to TransparentColor.
            alpha = 1.0 - import_fbx.elem_props_get_number(props, b"TransparencyFactor", 0.0)
            if alpha == 1.0 or alpha == 0.0:
                alpha = import_fbx.elem_props_get_number(props, b"Opacity", None)
                if alpha is None:
                    alpha = 1.0 - import_fbx.elem_props_get_color_rgb(props, b"TransparentColor", (0.0, 0.0, 0.0))[0]
            alphas.append((settings.nodal_material_wrap_map[material], alpha))
            return material

        import_fbx.blen_read_material = read_material
        # The importer's image search takes the first same-named file in walk order, hiding ties.
        result = bpy.ops.import_scene.fbx(filepath=str(source), use_image_search=False)
    else:
        result = bpy.ops.wm.obj_import(filepath=str(source))
        record_material_libraries(source)
    if result != {"FINISHED"}:
        raise RuntimeError(f"Blender import failed: {result}")
    for wrapper, alpha in alphas:
        if not wrapper.node_principled_bsdf.inputs["Alpha"].is_linked:
            wrapper.alpha = alpha


def find_candidates(source):
    def fail(error):
        raise error

    candidates = []
    for directory, _, names in os.walk(source.resolve().parent, onerror=fail):
        candidates += [Path(directory, name) for name in names if Path(name).suffix.lower() in CANDIDATE_EXTENSIONS]
    return sorted(path for path in candidates if path.is_file())


def file_stem(filepath):
    return os.path.splitext(re.split(r"[\\/]", filepath)[-1])[0]


def relink_missing_images(candidates):
    for image in bpy.data.images:
        if image.packed_file or image.source != "FILE" or Path(bpy.path.abspath(image.filepath, library=image.library)).is_file():
            continue
        name = re.split(r"[\\/]", image.filepath)[-1].lower()
        stem = file_stem(name)
        tiers = (
            [path for path in candidates if path.name.lower() == name],
            [path for path in candidates if path.stem.lower() == stem],
            # Sketchfab truncates file stems to 42 characters.
            [path for path in candidates if len(path.stem) == 42 and path.stem.lower() == stem[:42]],
        )
        matches = next((tier for tier in tiers if tier), [])
        if len(matches) > 1:
            raise RuntimeError(f"Several candidates for missing image {image.filepath}: {', '.join(map(str, matches))}")
        if matches:
            # Assigning the path loads the new file's size and pixels.
            image.filepath = str(matches[0])


def classify(stem):
    letters = stem.replace("_", "")
    suffix = ""
    channel = None
    for word_channel, words in CHANNEL_WORDS.items():
        for word in words:
            # Truncation to 42 characters can cut the channel word short.
            for length in range(3, len(word) + 1) if len(stem) == 42 else (len(word),):
                if length > len(suffix) and letters[-length:].lower() == word[:length]:
                    suffix, channel = word[:length], word_channel
    if channel is None:
        return None
    index = len(stem)
    remaining = len(suffix)
    while remaining:
        index -= 1
        remaining -= stem[index] != "_"
    # Of the channel words, only normaldirectx starts with "normald".
    return stem[:index].rstrip("_"), channel, suffix.startswith("normald")


def set_key(name):
    return re.sub(r"\W", "_", name.lower())


def names_match(set_name, material_name):
    return set_name == material_name or set_name.endswith("_" + material_name) or material_name.startswith(set_name + "_")


def wire_material(material, channels):
    tree = material.node_tree
    output = next((node for node in tree.nodes if node.type == "OUTPUT_MATERIAL" and node.is_active_output), None) if tree else None
    links = output.inputs["Surface"].links if output else ()
    # The glTF exporter reads only the active output's Principled BSDF.
    if not links or links[0].from_node.type != "BSDF_PRINCIPLED":
        return
    principled = links[0].from_node
    for channel, path in channels.items():
        socket = principled.inputs[CHANNEL_INPUTS[channel]]
        normal_map = None
        if channel == "normal" and socket.is_linked and socket.links[0].from_node.type == "NORMAL_MAP":
            # The FBX importer feeds Normal from a Normal Map node even without a normal texture.
            normal_map = socket.links[0].from_node
            socket = normal_map.inputs["Color"]
        if socket.is_linked:
            continue
        image = bpy.data.images.load(str(path), check_existing=True)
        if channel not in {"base color", "emissive"}:
            image.colorspace_settings.name = "Non-Color"
        texture = tree.nodes.new("ShaderNodeTexImage")
        texture.image = image
        if channel == "normal" and normal_map is None:
            normal_map = tree.nodes.new("ShaderNodeNormalMap")
            tree.links.new(normal_map.outputs["Normal"], socket)
            socket = normal_map.inputs["Color"]
        tree.links.new(texture.outputs["Color"], socket)
        if channel == "emissive" and principled.inputs["Emission Strength"].default_value == 0:
            # The exporter drops an emissive factor scaled by zero strength.
            principled.inputs["Emission Strength"].default_value = 1.0


def wire_texture_sets(candidates):
    sets = {}
    for path in candidates:
        texture = classify(path.stem)
        if texture is not None:
            channels = sets.setdefault(set_key(texture[0]), {})
            if texture[1] in channels:
                raise RuntimeError(f"Texture set has two {texture[1]} images: {channels[texture[1]]} and {path}")
            channels[texture[1]] = path
    matches = {}
    for material in bpy.data.materials:
        nodes = material.node_tree.nodes if material.node_tree else ()
        textures = [classify(file_stem(node.image.filepath)) for node in nodes if node.type == "TEX_IMAGE" and node.image is not None]
        linked = {set_key(texture[0]) for texture in textures if texture is not None} & sets.keys()
        matches[material] = linked or {key for key in sets if names_match(key, set_key(material.name))}
    if len(sets) == 1 and not any(matches.values()):
        matches = dict.fromkeys(matches, set(sets))
    for material, keys in matches.items():
        channels = {}
        for key in sorted(keys):
            for channel, path in sets[key].items():
                if channel in channels:
                    raise RuntimeError(f"Material {material.name} matches two {channel} images: {channels[channel]} and {path}")
                channels[channel] = path
        wire_material(material, channels)


def invert_directx_normals():
    for image in bpy.data.images:
        texture = classify(file_stem(image.filepath))
        if texture is not None and texture[2] and all(image.size):
            pixels = numpy.empty(len(image.pixels), numpy.float32)
            image.pixels.foreach_get(pixels)
            # DirectX normal maps point green down; glTF expects OpenGL's up.
            pixels[1::image.channels] = 1.0 - pixels[1::image.channels]
            image.pixels.foreach_set(pixels)


def rest_values(slot, fcurve):
    bone = re.fullmatch(r'pose\.bones\["(?:[^"\\]|\\.)*"\]\.(\w+)', fcurve.data_path)
    if bone:
        rest = IDENTITY_TRANSFORM.get(bone[1])
        return [rest[fcurve.array_index]] if rest else []
    if slot.target_id_type != "OBJECT" or fcurve.data_path not in IDENTITY_TRANSFORM:
        return []
    # An object's rest transform is its own stored transform, which the exporter writes when unanimated.
    return [getattr(user, fcurve.data_path)[fcurve.array_index] for user in slot.users()]


def is_static_action(action):
    from bpy_extras.anim_utils import action_get_channelbag_for_slot

    for slot in action.slots:
        channelbag = action_get_channelbag_for_slot(action, slot)
        for fcurve in channelbag.fcurves if channelbag else ():
            rests = rest_values(slot, fcurve)
            if not rests or len(fcurve.modifiers):
                return False
            # Handles shape only Bezier segments; the FBX importer's linear keys carry unrelated handles.
            bezier = any(point.interpolation == "BEZIER" for point in fcurve.keyframe_points)
            for point in fcurve.keyframe_points:
                values = (point.co[1], point.handle_left[1], point.handle_right[1]) if bezier else (point.co[1],)
                if not all(math.isclose(value, rest, rel_tol=1e-5, abs_tol=1e-5) for value in values for rest in rests):
                    return False
    return True


def drop_static_actions():
    # The FBX importer makes one action per object of a take, and the exporter writes each as an animation,
    # so a take that moves nothing would otherwise become one animation per object.
    for action in list(bpy.data.actions):
        if is_static_action(action):
            # The dropped action's content decided the export, so its library is consumed.
            record_library(action)
            bpy.data.actions.remove(action)


class DependencyExtension:
    is_critical = True

    def gather_mesh_hook(self, mesh, blender_data, blender_object, vertex_groups, modifiers, materials, settings):
        from io_scene_gltf2.io.exp.binary_data import BinaryData

        for primitive in mesh.primitives:
            joints = primitive.attributes.get("JOINTS_0")
            if joints is not None and joints.component_type == 5121:
                # The engine consumes uint16 joint indices even for small skeletons.
                binary = joints.buffer_view
                joints.buffer_view = BinaryData(
                    struct.pack(f"<{len(binary.data)}H", *binary.data), binary.bufferViewTarget)
                joints.component_type = 5123

    def gather_gltf_extensions_hook(self, gltf, settings):
        # The packer requires parents before children; Blender serializes children first.
        nodes = gltf.nodes or []
        parents = {child: index for index, node in enumerate(nodes) for child in node.children or []}
        order = []
        remap = {}

        def visit(index):
            if index in remap:
                return
            if index in parents:
                visit(parents[index])
            remap[index] = len(order)
            order.append(index)

        for index in range(len(nodes)):
            visit(index)
        gltf.nodes = [nodes[index] for index in order]
        for node in nodes:
            if node.children is not None:
                node.children = [remap[index] for index in node.children]
        for scene in gltf.scenes or []:
            if scene.nodes is not None:
                scene.nodes = [remap[index] for index in scene.nodes]
        for skin in gltf.skins or []:
            skin.joints = [remap[index] for index in skin.joints]
            if skin.skeleton is not None:
                skin.skeleton = remap[skin.skeleton]
        for animation in gltf.animations or []:
            for channel in animation.channels:
                if channel.target.node is not None:
                    channel.target.node = remap[channel.target.node]

    def gather_scene_hook(self, scene, blender_scene, settings):
        visited = set()

        def visit(collection):
            if collection in visited:
                return
            visited.add(collection)
            record_library(collection)
            for obj in collection.objects:
                if obj.type not in {"CAMERA", "LIGHT"}:
                    record_library(obj)
                    if obj.instance_collection is not None:
                        visit(obj.instance_collection)
            for child in collection.children:
                visit(child)

        # Missing linked collections can disappear from the exporter's node tree.
        visit(blender_scene.collection)

    def gather_node_hook(self, node, obj, settings):
        if obj is None:
            return
        record_library(obj)
        if obj.instance_collection is not None:
            record_library(obj.instance_collection)
        if obj.data is not None:
            record_library(obj.data)

    def gather_actions_hook(self, obj, actions, settings):
        for action in actions.get():
            record_library(action.action)

    def gather_material_hook(self, material, blender_material, settings):
        record_library(blender_material)
        if blender_material.node_tree:
            record_library(blender_material.node_tree)


def main():
    if bpy.app.version[:2] != (5, 2):
        raise RuntimeError("Only Blender 5.2 LTS is supported")
    source, output, report = map(Path, sys.argv[sys.argv.index("--") + 1:])
    load_source(source)
    candidates = find_candidates(source)
    # Listing every candidate lets an edited, added or removed image dirty the conversion.
    dependencies.update(map(str, candidates))
    relink_missing_images(candidates)
    wire_texture_sets(candidates)
    invert_directx_normals()
    drop_static_actions()
    module_name = "broken_engine_dependencies"
    module = types.ModuleType(module_name)
    module.glTF2ExportUserExtension = DependencyExtension
    sys.modules[module_name] = module
    bpy.context.preferences.addons.new().module = module_name

    import io_scene_gltf2
    from io_scene_gltf2.blender.exp.material import image as exporter_image
    from io_scene_gltf2.blender.exp.material import materials as exporter_materials
    from io_scene_gltf2.blender.exp.material import search_node_tree
    original_lookup = exporter_image.get_texture_node_from_socket
    original_navigation = search_node_tree.NodeSocket.to_node_nav
    original_descend = search_node_tree.NodeNav.descend
    original_encode = exporter_image.ExportImage.encode

    def navigation(socket):
        record_library(socket.socket.id_data)
        for node in socket.group_path:
            record_library(node.id_data)
            tree = getattr(node, "node_tree", None)
            if tree is not None:
                record_library(tree)
        return original_navigation(socket)

    def descend(nav):
        if nav.node is not None and nav.node.type == "GROUP" and nav.node.node_tree is not None:
            record_library(nav.node.node_tree)
        return original_descend(nav)

    def encode(image, mime_type, settings):
        result = original_encode(image, mime_type, settings)
        if not result[0]:
            raise RuntimeError("Failed to encode consumed Blender image")
        return result

    def lookup(socket, settings):
        result = original_lookup(socket, settings)
        if result is not None:
            # Validate before the exporter drops images with unreadable pixels.
            record_image(result.shader_node.image)
            record_library(result.shader_node.id_data)
            for node in result.group_path:
                record_library(node.id_data)
                tree = getattr(node, "node_tree", None)
                if tree is not None:
                    record_library(tree)
        return result

    exporter_image.get_texture_node_from_socket = lookup
    search_node_tree.NodeSocket.to_node_nav = navigation
    search_node_tree.NodeNav.descend = descend
    exporter_image.ExportImage.encode = encode
    # Inlining erases the library ownership needed by consumed-socket tracing.
    exporter_materials.BlenderMaterialIndentifier._BlenderMaterialIndentifier__can_use_inline = lambda self: False
    # Keep private staging files outside the scene discovery extensions.
    io_scene_gltf2.ensure_filepath_matches_export_format = lambda filepath, export_format: filepath
    result = bpy.ops.export_scene.gltf(
        filepath=str(output), check_existing=False, export_format="GLB",
        export_yup=True, export_animations=True, export_force_sampling=True,
        export_animation_mode="ACTIONS", export_frame_range=True,
        export_skins=True, export_materials="EXPORT", export_image_format="AUTO",
        use_selection=False, use_visible=False, use_renderable=False,
        export_cameras=False, export_lights=False,
    )
    if result != {"FINISHED"}:
        raise RuntimeError(f"Blender GLB export failed: {result}")
    if not output.is_file():
        raise RuntimeError(f"Blender did not produce {output}")
    report.write_text(json.dumps(sorted(dependencies)), encoding="utf-8")


if __name__ == "__main__":
    main()
