#include "yaml.h"

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "ryml_all.hpp"

#include "core/io/file_access.h"
#include "core/string/ustring.h"
#include "core/variant/variant_parser.h"

// ---------- Helper: ryml::NodeRef → Godot Variant ----------
static Variant _ryml_to_variant(const ryml::ConstNodeRef &node) {
    if (node.is_map()) {
        Dictionary dict;
        for (const ryml::ConstNodeRef &child : node.children()) {
            String key = String::utf8(child.key().str, child.key().len);
            dict[key] = _ryml_to_variant(child);
        }
        return dict;
    }

    if (node.is_seq()) {
        Array arr;
        for (const ryml::ConstNodeRef &child : node.children()) {
            arr.push_back(_ryml_to_variant(child));
        }
        return arr;
    }

    // Scalar
    if (node.has_val()) {
        ryml::csubstr val = node.val();
        String s = String::utf8(val.str, val.len);

        // Try to be smart about types
        if (s.is_valid_int()) {
            return s.to_int();
        }
        if (s.is_valid_float()) {
            return s.to_float();
        }
        if (s == "true" || s == "True" || s == "TRUE") {
            return true;
        }
        if (s == "false" || s == "False" || s == "FALSE") {
            return false;
        }
        if (s == "null" || s == "Null" || s == "NULL" || s == "~") {
            return Variant();
        }

        return s;
    }

    return Variant();
}

// ---------- Helper: Godot Variant → ryml tree ----------
static void _variant_to_ryml(ryml::NodeRef node, const Variant &v) {
    switch (v.get_type()) {
        case Variant::NIL:
            node << "null";
            break;
        case Variant::BOOL:
            node << (v.operator bool() ? "true" : "false");
            break;
        case Variant::INT:
            node << int64_t(v);
            break;
        case Variant::FLOAT:
            node << double(v);
            break;
        case Variant::STRING:
            node << String(v).utf8().get_data();
            break;
        case Variant::DICTIONARY: {
            Dictionary d = v;
            node |= ryml::MAP;
            Array keys = d.keys();
            for (int i = 0; i < keys.size(); i++) {
                String key = keys[i];
                ryml::NodeRef child = node.append_child();
                child << ryml::key(key.utf8().get_data());
                _variant_to_ryml(child, d[key]);
            }
            break;
        }
        case Variant::ARRAY: {
            Array a = v;
            node |= ryml::SEQ;
            for (int i = 0; i < a.size(); i++) {
                ryml::NodeRef child = node.append_child();
                _variant_to_ryml(child, a[i]);
            }
            break;
        }
        default:
            // Fallback: convert to string
            node << String(v).utf8().get_data();
            break;
    }
}

// ---------- Public API ----------

Variant YAML::parse_string(const String &p_yaml) {
    CharString utf8 = p_yaml.utf8();
    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(utf8.get_data()));
    return _ryml_to_variant(tree.rootref());
}

String YAML::stringify(const Variant &p_value, int p_indent) {
    ryml::Tree tree;
    ryml::NodeRef root = tree.rootref();
    _variant_to_ryml(root, p_value);

    ryml::csubstr result = ryml::emitrs_yaml<std::string>(tree);
    return String::utf8(result.str, result.len);
}

Variant YAML::parse_file(const String &p_path) {
    Error err;
    String content = FileAccess::get_file_as_string(p_path, &err);
    if (err != OK) {
        ERR_PRINT("Failed to open YAML file: " + p_path);
        return Variant();
    }
    return parse_string(content);
}

void YAML::_bind_methods() {
    ClassDB::bind_static_method("YAML", D_METHOD("parse_string", "yaml"), &YAML::parse_string);
    ClassDB::bind_static_method("YAML", D_METHOD("stringify", "value", "indent"), &YAML::stringify, DEFVAL(2));
    ClassDB::bind_static_method("YAML", D_METHOD("parse_file", "path"), &YAML::parse_file);
}