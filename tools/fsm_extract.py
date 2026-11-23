#!/usr/bin/env python3
"""Decode PlayMaker FSM components from AssetRipper YAML files.

Usage:
  python fsm_extract.py <scene.unity|prefab|asset> [object-name-substring]
"""

from __future__ import annotations

import argparse
import functools
import json
import os
import re
import struct
import sys
from pathlib import Path
from typing import Any, Iterable

import yaml

from hkcommon import ROOT


class _Loader(getattr(yaml, "CSafeLoader", yaml.SafeLoader)):
    pass


# Hex byte arrays must remain strings. Otherwise AssetRipper's "0101..." values
# can be coerced to YAML integers or booleans.
_Loader.yaml_implicit_resolvers = {
    key: [
        (tag, resolver)
        for tag, resolver in resolvers
        if tag not in ("tag:yaml.org,2002:int", "tag:yaml.org,2002:bool")
    ]
    for key, resolvers in yaml.SafeLoader.yaml_implicit_resolvers.items()
}

PARAM_TYPES = (
    "Integer",
    "Boolean",
    "Float",
    "String",
    "Color",
    "ObjectReference",
    "LayerMask",
    "Enum",
    "Vector2",
    "Vector3",
    "Vector4",
    "Rect",
    "Array",
    "Character",
    "AnimationCurve",
    "FsmFloat",
    "FsmInt",
    "FsmBool",
    "FsmString",
    "FsmGameObject",
    "FsmOwnerDefault",
    "FunctionCall",
    "FsmAnimationCurve",
    "FsmEvent",
    "FsmObject",
    "FsmColor",
    "Unsupported",
    "GameObject",
    "FsmVector3",
    "LayoutOption",
    "FsmRect",
    "FsmEventTarget",
    "FsmMaterial",
    "FsmTexture",
    "Quaternion",
    "FsmQuaternion",
    "FsmProperty",
    "FsmVector2",
    "FsmTemplateControl",
    "FsmVar",
    "CustomClass",
    "FsmArray",
    "FsmEnum",
)

DOC_HEADER = re.compile(r"(?m)^--- !u!(\d+) &(-?\d+)[^\n]*\n")
TEMPLATE_MARKER = re.compile(
    r"fsmTemplate:\s*\{[^}\n]*fileID:\s*11400000\b", re.IGNORECASE
)


def hex_bytes(value: Any) -> bytes:
    if not isinstance(value, str) or not value:
        return b""
    try:
        return bytes.fromhex(value)
    except ValueError:
        return b""


def int_list(value: Any) -> list[int]:
    data = hex_bytes(value)
    if not data:
        if isinstance(value, list):
            try:
                return [int(item) for item in value]
            except (TypeError, ValueError):
                return []
        return []
    if len(data) % 4:
        data = data[: len(data) - (len(data) % 4)]
    return list(struct.unpack(f"<{len(data) // 4}i", data))


def num(value: Any) -> Any:
    if isinstance(value, (int, float)):
        return value
    try:
        text = str(value)
        return float(text) if "." in text or "e" in text.lower() else int(text)
    except (TypeError, ValueError):
        return value


def load_objs_text(src: str) -> dict[int, tuple[str, dict[str, Any]]]:
    """Parse only YAML object documents that are relevant to FSM extraction."""
    objs: dict[int, tuple[str, dict[str, Any]]] = {}
    matches = list(DOC_HEADER.finditer(src))
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(src)
        body = src[match.end() : end]
        if (
            "fsm:" not in body
            and "m_Name" not in body
            and "m_Father" not in body
            and "m_GameObject:" not in body
            and "m_Component:" not in body
            and "m_Sprite:" not in body
        ):
            continue
        try:
            document = yaml.load(body, Loader=_Loader)
        except yaml.YAMLError:
            continue
        if not isinstance(document, dict) or len(document) != 1:
            continue
        ((type_name, value),) = document.items()
        if isinstance(value, dict):
            objs[int(match.group(2))] = (str(type_name), value)
    return objs


@functools.lru_cache(maxsize=128)
def _load_objs_cached(
    path: str, size: int, mtime_ns: int
) -> dict[int, tuple[str, dict[str, Any]]]:
    del size, mtime_ns
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return load_objs_text(handle.read())


def load_objs(path: str) -> dict[int, tuple[str, dict[str, Any]]]:
    absolute = os.path.abspath(os.fspath(path))
    try:
        stat = os.stat(absolute)
    except OSError:
        return {}
    return _load_objs_cached(absolute, stat.st_size, stat.st_mtime_ns)


class Paths:
    def __init__(self, objs: dict[int, tuple[str, dict[str, Any]]]):
        self.go = {
            fid: value
            for fid, (type_name, value) in objs.items()
            if type_name == "GameObject"
        }
        self.tr = {
            fid: value
            for fid, (type_name, value) in objs.items()
            if type_name in ("Transform", "RectTransform")
        }
        self.go2tr = {}
        for fid, value in self.tr.items():
            game_object = value.get("m_GameObject") or {}
            try:
                self.go2tr[int(game_object.get("fileID", 0) or 0)] = fid
            except (TypeError, ValueError):
                pass
        self._path_cache: dict[int, str] = {}

    def path(self, gid: int) -> str:
        if gid in self._path_cache:
            return self._path_cache[gid]
        names: list[str] = []
        transform = self.go2tr.get(gid)
        visited: set[int] = set()
        while (
            transform is not None and transform in self.tr and transform not in visited
        ):
            visited.add(transform)
            transform_value = self.tr[transform]
            game_object = transform_value.get("m_GameObject") or {}
            try:
                game_object_id = int(game_object.get("fileID", 0) or 0)
            except (TypeError, ValueError):
                break
            name = self.go.get(game_object_id, {}).get("m_Name")
            if name:
                names.append(str(name))
            father = transform_value.get("m_Father") or {}
            try:
                transform = int(father.get("fileID", 0) or 0)
            except (TypeError, ValueError):
                break
            if transform not in self.tr:
                transform = None
        result = "/".join(reversed(names))
        self._path_cache[gid] = result
        return result


def objref(value: Any) -> dict[str, Any] | None:
    if not isinstance(value, dict):
        return None
    try:
        file_id = int(value.get("fileID", 0) or 0)
    except (TypeError, ValueError):
        return None
    if not file_id:
        return None
    result: dict[str, Any] = {"go": file_id}
    if value.get("guid"):
        result["guid"] = value["guid"]
    return result


def fsmvar(value: Any, kind: str) -> Any:
    if not isinstance(value, dict):
        return num(value) if kind != "FsmString" else value
    if int(value.get("useVariable", 0) or 0):
        return {"var": value["name"]} if value.get("name") else {"none": True}
    if kind in ("FsmGameObject", "FsmObject", "FsmMaterial", "FsmTexture"):
        return objref(value.get("value"))
    raw = value.get("value")
    if isinstance(raw, dict) and set(raw) <= {"x", "y", "z", "w", "r", "g", "b", "a"}:
        return {key: num(item) for key, item in raw.items()}
    return num(raw) if kind != "FsmString" else raw


def decode_actions(action_data: Any, paths: Paths) -> list[dict[str, Any]]:
    del paths
    if not isinstance(action_data, dict):
        return []
    names = action_data.get("actionNames") or []
    if not names:
        return []
    starts = int_list(action_data.get("actionStartIndex"))
    types = int_list(action_data.get("paramDataType"))
    positions = int_list(action_data.get("paramDataPos"))
    sizes = int_list(action_data.get("paramByteDataSize"))
    param_names = action_data.get("paramName") or []
    byte_data = hex_bytes(action_data.get("byteData"))

    enabled = action_data.get("actionEnabled")
    enabled_bytes = (
        hex_bytes(enabled)
        if isinstance(enabled, str)
        else (bytes(enabled) if isinstance(enabled, (bytes, bytearray, list)) else b"")
    )
    if not enabled_bytes:
        enabled_bytes = b"\x01" * len(names)
    array_sizes = int_list(action_data.get("arrayParamSizes"))

    def list_value(key: str) -> list[Any]:
        value = action_data.get(key)
        return value if isinstance(value, list) else []

    def unpack_string(position: int, size: int) -> str:
        return byte_data[position : position + size].decode("utf-8", "replace")

    def param_type(index: int) -> str:
        if 0 <= index < len(types):
            type_index = types[index]
            if 0 <= type_index < len(PARAM_TYPES):
                return PARAM_TYPES[type_index]
            return str(type_index)
        return "Unsupported"

    def get_param(index: int) -> Any:
        if index >= len(types) or index >= len(positions) or index >= len(sizes):
            return {"error": "parameter metadata out of range"}
        type_name = param_type(index)
        position = positions[index]
        size = sizes[index]

        if type_name in ("Integer", "LayerMask", "Enum"):
            return (
                struct.unpack_from("<i", byte_data, position)[0]
                if size >= 4 and position + 4 <= len(byte_data)
                else 0
            )
        if type_name == "Boolean":
            return (
                bool(byte_data[position])
                if size and position < len(byte_data)
                else False
            )
        if type_name == "Float":
            return (
                struct.unpack_from("<f", byte_data, position)[0]
                if size >= 4 and position + 4 <= len(byte_data)
                else 0.0
            )
        if type_name == "String":
            if size:
                return unpack_string(position, size)
            strings = list_value("stringParams")
            return (
                strings[position]
                if 0 <= position < len(strings) and strings[position] is not None
                else ""
            )
        if type_name in (
            "Vector2",
            "Vector3",
            "Vector4",
            "Color",
            "Rect",
            "Quaternion",
        ):
            count = size // 4
            if count and position + size <= len(byte_data):
                return list(struct.unpack_from(f"<{count}f", byte_data, position))
            return []
        if type_name in ("FsmFloat", "FsmInt", "FsmBool"):
            values_key = {
                "FsmFloat": "fsmFloatParams",
                "FsmInt": "fsmIntParams",
                "FsmBool": "fsmBoolParams",
            }[type_name]
            if size:
                value_size = 4 if type_name != "FsmBool" else 1
                if position + value_size > len(byte_data):
                    return None
                if type_name == "FsmFloat":
                    raw = struct.unpack_from("<f", byte_data, position)[0]
                elif type_name == "FsmInt":
                    raw = struct.unpack_from("<i", byte_data, position)[0]
                else:
                    raw = bool(byte_data[position])
                use_variable = (
                    byte_data[position + value_size]
                    if position + value_size < len(byte_data)
                    else 0
                )
                name_start = position + value_size + 1
                name = (
                    unpack_string(name_start, max(0, size - (name_start - position)))
                    if name_start <= len(byte_data)
                    else ""
                )
                return (
                    {"var": name}
                    if use_variable and name
                    else ({"none": True} if use_variable else raw)
                )
            values = list_value(values_key)
            return fsmvar(
                values[position] if 0 <= position < len(values) else None, type_name
            )
        if type_name == "FsmEvent":
            if size:
                return {"event": unpack_string(position, size)}
            strings = list_value("stringParams")
            event_name = strings[position] if 0 <= position < len(strings) else None
            return {"event": event_name} if event_name else None
        if type_name in (
            "FsmVector2",
            "FsmVector3",
            "FsmColor",
            "FsmQuaternion",
            "FsmRect",
        ):
            count = (
                2
                if type_name == "FsmVector2"
                else (3 if type_name == "FsmVector3" else 4)
            )
            if size:
                if position + 4 * count > len(byte_data):
                    return []
                raw = list(struct.unpack_from(f"<{count}f", byte_data, position))
                use_position = position + 4 * count
                use_variable = (
                    byte_data[use_position] if use_position < len(byte_data) else 0
                )
                name_start = use_position + 1
                name = (
                    unpack_string(name_start, max(0, size - (name_start - position)))
                    if name_start <= len(byte_data)
                    else ""
                )
                return (
                    {"var": name}
                    if use_variable and name
                    else ({"none": True} if use_variable else raw)
                )
            key = {
                "FsmVector2": "fsmVector2Params",
                "FsmVector3": "fsmVector3Params",
                "FsmColor": "fsmColorParams",
                "FsmRect": "fsmRectParams",
                "FsmQuaternion": "fsmQuaternionParams",
            }[type_name]
            values = list_value(key)
            return fsmvar(
                values[position] if 0 <= position < len(values) else None, type_name
            )

        typed_lists = {
            "FsmString": "fsmStringParams",
            "FsmGameObject": "fsmGameObjectParams",
            "FsmVector3": "fsmVector3Params",
            "FsmColor": "fsmColorParams",
            "FsmRect": "fsmRectParams",
            "FsmQuaternion": "fsmQuaternionParams",
            "FsmObject": "fsmObjectParams",
            "FsmMaterial": "fsmObjectParams",
            "FsmTexture": "fsmObjectParams",
            "FsmEnum": "fsmEnumParams",
            "FsmArray": "fsmArrayParams",
            "FsmVar": "fsmVarParams",
        }
        if type_name in typed_lists:
            values = list_value(typed_lists[type_name])
            value = values[position] if 0 <= position < len(values) else None
            if type_name == "FsmVar" and isinstance(value, dict):
                variable_name = value.get("variableName")
                if variable_name:
                    return {"var": variable_name}
                return {
                    "type": value.get("type"),
                    "f": num(value.get("floatValue")),
                    "i": num(value.get("intValue")),
                    "b": num(value.get("boolValue")),
                    "s": value.get("stringValue"),
                }
            if type_name == "FsmEnum" and isinstance(value, dict):
                if int(value.get("useVariable", 0) or 0) and value.get("name"):
                    return {"var": value["name"]}
                return num(value.get("intValue"))
            if type_name == "FsmArray" and isinstance(value, dict):
                if int(value.get("useVariable", 0) or 0) and value.get("name"):
                    return {"var": value["name"]}
                return {
                    "array": value.get("type"),
                    "floats": value.get("floatValues"),
                    "ints": value.get("intValues"),
                    "strings": value.get("stringValues"),
                    "bools": value.get("boolValues"),
                }
            return fsmvar(value, type_name)
        if type_name == "FsmOwnerDefault":
            values = list_value("fsmOwnerDefaultParams")
            value = values[position] if 0 <= position < len(values) else {}
            if int(value.get("ownerOption", 0) or 0) == 0:
                return {"owner": True}
            game_object = value.get("gameObject") or {}
            if int(game_object.get("useVariable", 0) or 0) and game_object.get("name"):
                return {"var": game_object["name"]}
            return objref(game_object.get("value"))
        if type_name == "FsmEventTarget":
            values = list_value("fsmEventTargetParams")
            value = values[position] if 0 <= position < len(values) else {}
            game_object = value.get("gameObject") or {}
            if int(game_object.get("ownerOption", 0) or 0) == 0:
                target = {"owner": True}
            else:
                nested = game_object.get("gameObject") or {}
                target = (
                    {"var": nested["name"]}
                    if int(nested.get("useVariable", 0) or 0) and nested.get("name")
                    else objref(nested.get("value"))
                )
            fsm_name = value.get("fsmName") or {}
            return {
                "target": num(value.get("target")),
                "fsmName": (
                    {"var": fsm_name["name"]}
                    if int(fsm_name.get("useVariable", 0) or 0) and fsm_name.get("name")
                    else fsm_name.get("value")
                ),
                "go": target,
                "children": bool(
                    num((value.get("sendToChildren") or {}).get("value") or 0)
                ),
                "exclude": bool(
                    num((value.get("excludeSelf") or {}).get("value") or 0)
                ),
            }
        if type_name == "FsmProperty":
            values = list_value("fsmPropertyParams")
            value = values[position] if 0 <= position < len(values) else {}
            return {
                "property": value.get("PropertyName"),
                "target": value.get("TargetObject"),
            }
        if type_name == "FunctionCall":
            values = list_value("functionCallParams")
            value = values[position] if 0 <= position < len(values) else {}
            parameter_type = value.get("parameterType") or value.get("ParameterType")
            key = {
                "int": "IntParameter",
                "float": "FloatParameter",
                "bool": "BoolParameter",
                "string": "StringParameter",
                "GameObject": "GameObjectParameter",
                "Vector2": "Vector2Parameter",
                "Vector3": "Vector3Parameter",
            }.get(str(parameter_type))
            decoded = None
            if key:
                parameter = value.get(key) or {}
                if int(parameter.get("useVariable", 0) or 0) and parameter.get("name"):
                    decoded = {"var": parameter["name"]}
                elif key == "GameObjectParameter":
                    decoded = objref(parameter.get("value"))
                else:
                    decoded = parameter.get("value")
                if isinstance(decoded, str) and key in (
                    "IntParameter",
                    "FloatParameter",
                    "BoolParameter",
                ):
                    decoded = num(decoded)
            return {
                "function": value.get("FunctionName"),
                "type": parameter_type,
                "v": decoded,
                "f": num((value.get("FloatParameter") or {}).get("value")),
                "s": (value.get("StringParameter") or {}).get("value"),
            }
        if type_name == "ObjectReference":
            values = list_value("unityObjectParams")
            return objref(values[position] if 0 <= position < len(values) else None)
        if type_name in ("AnimationCurve", "FsmAnimationCurve"):
            values = list_value("animationCurveParams")
            return {"curve": values[position] if 0 <= position < len(values) else None}
        return {"unsupported": type_name}

    result = []
    for action_index, name in enumerate(names):
        start = starts[action_index] if action_index < len(starts) else 0
        end = starts[action_index + 1] if action_index + 1 < len(starts) else len(types)
        params: dict[str, Any] = {}
        index = start
        while index < end:
            parameter_name = (
                param_names[index] if index < len(param_names) else str(index)
            )
            type_name = param_type(index)
            if type_name == "Array":
                position = positions[index] if index < len(positions) else -1
                array_size = (
                    array_sizes[position] if 0 <= position < len(array_sizes) else 0
                )
                params[parameter_name] = []
                for offset in range(array_size):
                    item_index = index + 1 + offset
                    if item_index < end:
                        try:
                            params[parameter_name].append(get_param(item_index))
                        except (IndexError, struct.error, ValueError) as error:
                            params[parameter_name].append({"error": str(error)})
                index += 1 + array_size
                continue
            try:
                params[parameter_name] = get_param(index)
            except (IndexError, struct.error, ValueError) as error:
                params[parameter_name] = {"error": str(error)}
            index += 1
        result.append(
            {
                "type": str(name).split(".")[-1],
                "enabled": (
                    bool(enabled_bytes[action_index])
                    if action_index < len(enabled_bytes)
                    else True
                ),
                "params": params,
            }
        )
    return result


_templates: dict[str, str] | None = None
_template_fsms: dict[str, tuple[dict[str, Any] | None, Paths | None]] = {}


def template_path(guid: str) -> str | None:
    global _templates
    if _templates is None:
        mapping_path = Path(__file__).resolve().parent / "data" / "fsm_templates.json"
        if not mapping_path.exists():
            _templates = {}
        else:
            try:
                _templates = json.loads(mapping_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as error:
                print(
                    f"Warning: cannot read template mapping: {error}", file=sys.stderr
                )
                _templates = {}
    mapping = _templates.get(guid)
    if not mapping:
        return None
    normalized = mapping.replace("\\", "/")
    candidates: list[Path] = []
    mapping_path = Path(mapping)
    if mapping_path.is_absolute():
        candidates.append(mapping_path)
    if normalized.startswith("hkdata/"):
        candidates.append(Path(ROOT) / normalized[len("hkdata/") :])
    if normalized.startswith("ex/"):
        candidates.append(Path(ROOT) / normalized)
    candidates.append(Path(__file__).resolve().parent.parent / normalized)
    candidates.append(Path(mapping))
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    return None


def template_fsm(guid: str) -> tuple[dict[str, Any] | None, Paths | None]:
    path = template_path(guid)
    if not path:
        return None, None
    if path not in _template_fsms:
        objs = load_objs(path)
        paths = Paths(objs)
        fsm = next(
            (
                value.get("fsm")
                for _, (type_name, value) in objs.items()
                if type_name == "MonoBehaviour" and isinstance(value.get("fsm"), dict)
            ),
            None,
        )
        _template_fsms[path] = (fsm, paths)
    return _template_fsms[path]


def decode_fsm(
    component: dict[str, Any], paths: Paths, owner_path: str
) -> dict[str, Any]:
    fsm = component.get("fsm")
    if not isinstance(fsm, dict):
        raise ValueError("component does not contain an FSM")
    template_guid = (component.get("fsmTemplate") or {}).get("guid")
    if template_guid:
        template, template_paths = template_fsm(template_guid)
        if template:
            merged = dict(template)
            merged["name"] = fsm.get("name")
            merged["variables"] = fsm.get("variables") or template.get("variables")
            if "RestartOnEnable" in fsm:
                merged["RestartOnEnable"] = fsm["RestartOnEnable"]
            fsm = merged
            if template_paths:
                paths = template_paths

    variables: dict[str, Any] = {}
    variable_data = fsm.get("variables") or {}
    variable_kinds = (
        ("floatVariables", "float"),
        ("intVariables", "int"),
        ("boolVariables", "bool"),
        ("stringVariables", "string"),
        ("vector2Variables", "vector2"),
        ("vector3Variables", "vector3"),
        ("gameObjectVariables", "gameobject"),
        ("objectVariables", "object"),
        ("colorVariables", "color"),
        ("enumVariables", "enum"),
        ("arrayVariables", "array"),
    )
    for key, kind in variable_kinds:
        for variable in variable_data.get(key) or []:
            if not isinstance(variable, dict) or not variable.get("name"):
                continue
            raw = variable.get("value")
            if kind in ("gameobject", "object"):
                raw = objref(raw)
            elif kind in ("float", "int"):
                raw = num(raw)
            elif kind == "bool":
                try:
                    raw = bool(int(raw or 0))
                except (TypeError, ValueError):
                    raw = bool(raw)
            elif kind == "enum":
                raw = num(variable.get("intValue"))
            elif isinstance(raw, dict):
                raw = {
                    item_key: num(item)
                    for item_key, item in raw.items()
                    if not isinstance(item, dict)
                }
            variables[variable["name"]] = {"type": kind, "value": raw}

    states = []
    for state in fsm.get("states") or []:
        if not isinstance(state, dict):
            continue
        transitions = []
        for transition in state.get("transitions") or []:
            if isinstance(transition, dict):
                event = transition.get("fsmEvent") or {}
                transitions.append(
                    {"event": event.get("name"), "to": transition.get("toState")}
                )
        states.append(
            {
                "name": state.get("name"),
                "transitions": transitions,
                "actions": decode_actions(state.get("actionData"), paths),
            }
        )
    global_transitions = []
    for transition in fsm.get("globalTransitions") or []:
        if isinstance(transition, dict):
            event = transition.get("fsmEvent") or {}
            global_transitions.append(
                {"event": event.get("name"), "to": transition.get("toState")}
            )
    return {
        "fsm": fsm.get("name"),
        "owner": owner_path,
        "start": fsm.get("startState"),
        "restart": (
            int(fsm.get("RestartOnEnable", 1) or 0) if "RestartOnEnable" in fsm else 1
        ),
        "events": [
            event.get("name")
            for event in fsm.get("events") or []
            if isinstance(event, dict)
        ],
        "global": global_transitions,
        "variables": variables,
        "states": states,
    }


def extract(path: str, filt: str | None = None) -> tuple[list[dict[str, Any]], Paths]:
    objs = load_objs(path)
    paths = Paths(objs)
    result = []
    for file_id, (type_name, value) in objs.items():
        del file_id
        if type_name != "MonoBehaviour" or not isinstance(value, dict):
            continue
        fsm = value.get("fsm")
        if not isinstance(fsm, dict) or "states" not in fsm:
            continue
        game_object = value.get("m_GameObject") or {}
        try:
            gid = int(game_object.get("fileID", 0) or 0)
        except (TypeError, ValueError):
            continue
        owner_path = paths.path(gid)
        if filt and filt not in owner_path:
            continue
        decoded = decode_fsm(value, paths, owner_path)
        decoded["gid"] = gid
        result.append(decoded)
    return result, paths


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Decode PlayMaker FSM components from AssetRipper YAML"
    )
    parser.add_argument("path", help="scene, prefab, or MonoBehaviour asset")
    parser.add_argument(
        "filter", nargs="?", help="only include owner paths containing this text"
    )
    parser.add_argument(
        "--indent", type=int, default=1, help="JSON indentation (default: 1)"
    )
    args = parser.parse_args(list(argv) if argv is not None else None)
    if not os.path.isfile(args.path):
        parser.error(f"input file does not exist: {args.path}")
    try:
        result, _ = extract(args.path, args.filter)
    except Exception as error:
        print(f"Error decoding {args.path}: {error}", file=sys.stderr)
        return 1
    json.dump(result, sys.stdout, indent=args.indent, ensure_ascii=False, default=str)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
