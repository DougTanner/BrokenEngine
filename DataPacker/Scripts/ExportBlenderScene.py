"""Export one authored scene and its consumed external dependencies with Blender 5.2."""

import json
from pathlib import Path
import struct
import sys
import types

import bpy


dependencies = set()


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
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False, use_scripts=False)
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
