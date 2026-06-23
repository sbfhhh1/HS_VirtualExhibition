import sys

import bpy


def safe_field(value):
    text = "" if value is None else str(value)
    return text.replace("|", "/").replace("\r", " ").replace("\n", " ").strip()


def value_to_text(value):
    try:
        if not isinstance(value, str) and hasattr(value, "__len__"):
            return ",".join(str(float(component)) for component in value)
    except Exception:
        pass
    return safe_field(value)


def modifier_value(modifier, identifier):
    if identifier and identifier in modifier:
        try:
            return modifier[identifier]
        except Exception:
            return None
    return None


def group_input_default(node_group, socket_name):
    if not node_group or not socket_name:
        return None

    for node in node_group.nodes:
        if node.bl_idname != "NodeGroupInput":
            continue
        for output in node.outputs:
            if output.name != socket_name:
                continue
            for link in output.links:
                target_socket = link.to_socket
                if hasattr(target_socket, "default_value"):
                    return target_socket.default_value
    return None


def interface_default_value(node_group, modifier, item):
    identifier = getattr(item, "identifier", "")
    value = modifier_value(modifier, identifier)
    if value is not None:
        return value

    linked_default = group_input_default(node_group, getattr(item, "name", ""))
    if linked_default is not None:
        return linked_default

    return getattr(item, "default_value", "")


def emit_candidate(obj, modifier, node_group):
    print(
        "CANDIDATE|"
        + safe_field(obj.name)
        + "|"
        + safe_field(modifier.name)
        + "|"
        + safe_field(node_group.name),
        flush=True,
    )


def emit_input(node_group, modifier, item):
    socket_type = getattr(item, "socket_type", "")
    if socket_type == "NodeSocketGeometry":
        return

    value = interface_default_value(node_group, modifier, item)
    print(
        "INPUT|"
        + safe_field(getattr(item, "name", ""))
        + "|"
        + safe_field(getattr(item, "identifier", ""))
        + "|"
        + safe_field(socket_type)
        + "|"
        + value_to_text(value),
        flush=True,
    )


def scan():
    for obj in bpy.data.objects:
        for modifier in obj.modifiers:
            if modifier.type != "NODES":
                continue
            node_group = getattr(modifier, "node_group", None)
            if node_group is None:
                continue

            emit_candidate(obj, modifier, node_group)
            interface = getattr(node_group, "interface", None)
            if interface is None:
                continue
            for item in interface.items_tree:
                if getattr(item, "item_type", None) != "SOCKET":
                    continue
                if getattr(item, "in_out", None) != "INPUT":
                    continue
                emit_input(node_group, modifier, item)


if __name__ == "__main__":
    try:
        scan()
    except Exception as exc:
        print("ERROR|" + safe_field(exc), file=sys.stderr, flush=True)
        raise
