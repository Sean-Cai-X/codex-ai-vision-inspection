#include "ThreeDTypes.h"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace codex_lan_agent_3d;

namespace {

std::string EncodeValue(const std::string & value) {
    return JsonEscape(value);
}

std::string DecodeValue(const std::string & value) {
    std::string decoded;
    decoded.reserve(value.size());
    bool escaping = false;
    for (const char ch : value) {
        if (!escaping) {
            if (ch == '\\') {
                escaping = true;
            }
            else {
                decoded.push_back(ch);
            }
            continue;
        }

        switch (ch) {
        case 'n':
            decoded.push_back('\n');
            break;
        case 'r':
            decoded.push_back('\r');
            break;
        case 't':
            decoded.push_back('\t');
            break;
        case '\\':
            decoded.push_back('\\');
            break;
        case '"':
            decoded.push_back('"');
            break;
        default:
            decoded.push_back(ch);
            break;
        }
        escaping = false;
    }
    if (escaping) {
        decoded.push_back('\\');
    }
    return decoded;
}

std::map<std::string, std::string> ReadRequestFile(const std::string & request_path) {
    std::ifstream request(request_path);
    if (!request.is_open()) {
        return {};
    }

    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(request, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        values[line.substr(0, separator)] = DecodeValue(line.substr(separator + 1));
    }
    return values;
}

void EmitField(const std::string & key, const std::string & value) {
    std::cout << "field." << key << "=" << EncodeValue(value) << "\n";
}

int RunNova(const std::string & tool_name) {
    std::cout << "ok=true\n";
    if (tool_name == "generate_3d") {
        EmitField("glb_url", "https://nova3d.xyz/assets/external.glb");
        EmitField("preview_url", "https://nova3d.xyz/preview/state-external-generate");
        EmitField("conversation_url", "https://nova3d.xyz/chat/conv-external");
        EmitField("parts", "body|door");
        EmitField("joint_count", "0");
        EmitField("code_artifact", "{\"content\":\"import bpy\"}");
        EmitField("model_artifact", "{\"url\":\"https://nova3d.xyz/assets/external.glb\"}");
        EmitField("workflow_id", "state-external-generate");
        return 0;
    }
    if (tool_name == "add_part") {
        EmitField("glb_url", "https://nova3d.xyz/assets/external_v2.glb");
        EmitField("preview_url", "https://nova3d.xyz/preview/state-external-add");
        EmitField("conversation_url", "https://nova3d.xyz/chat/conv-external");
        EmitField("parts", "body|door|handle");
        EmitField("joint_count", "0");
        EmitField("code_artifact", "{\"content\":\"import bpy # handle\"}");
        EmitField("model_artifact", "{\"url\":\"https://nova3d.xyz/assets/external_v2.glb\"}");
        EmitField("workflow_id", "state-external-add");
        return 0;
    }
    if (tool_name == "articulate_model") {
        EmitField("glb_url", "https://nova3d.xyz/assets/external_v3.glb");
        EmitField("preview_url", "https://nova3d.xyz/preview/state-external-articulate");
        EmitField("conversation_url", "https://nova3d.xyz/chat/conv-external");
        EmitField("parts", "body|door|handle");
        EmitField("joint_count", "1");
        EmitField("code_artifact", "{\"content\":\"import bpy # joints\"}");
        EmitField("model_artifact", "{\"url\":\"https://nova3d.xyz/assets/external_v3.glb\"}");
        EmitField("workflow_id", "state-external-articulate");
        return 0;
    }

    EmitField("workflow_id", "state-external-articulate");
    EmitField("state", "completed");
    EmitField("progress_label", "Generating your 3D model...");
    EmitField("current_node", "success_final");
    return 0;
}

int RunBlender(const std::string & tool_name, const std::string & mode, const std::map<std::string, std::string> & values) {
    std::cout << "ok=true\n";
    if (tool_name == "blender.import_asset") {
        EmitField(
            "scene_objects",
            "obj_asset_external_part_body:asset_external:part_body:body:0,0,0|"
            "obj_asset_external_part_door:asset_external:part_door:door:0,0,0|"
            "obj_asset_external_part_handle:asset_external:part_handle:handle:0,0,0");
        EmitField("scene_object_count", "3");
        EmitField("import_status", "ok");
        return 0;
    }
    if (tool_name == "blender.transform_object") {
        const auto translation_it = values.find("arg.translation");
        const std::string translation = translation_it == values.end() ? "0,0,0" : translation_it->second;
        EmitField(
            "scene_objects",
            "obj_asset_external_part_body:asset_external:part_body:body:0,0,0|"
            "obj_asset_external_part_door:asset_external:part_door:door:" + translation + "|"
            "obj_asset_external_part_handle:asset_external:part_handle:handle:0,0,0");
        EmitField("transform_status", "ok");
        EmitField("translation", translation);
        return 0;
    }
    if (tool_name == "blender.capture_viewport") {
        if (mode == "viewport_failure") {
            std::cout << "ok=false\n";
            std::cout << "exit_code=2\n";
            EmitField("error", "viewport capture unavailable");
            return 2;
        }
        EmitField("image_path", "D:/captures/external_validation.png");
        return 0;
    }
    if (tool_name == "blender.get_scene_snapshot") {
        EmitField(
            "scene_objects",
            "obj_asset_external_part_body:asset_external:part_body:body:0,0,0|"
            "obj_asset_external_part_door:asset_external:part_door:door:1,0,0|"
            "obj_asset_external_part_handle:asset_external:part_handle:handle:0,0,0");
        return 0;
    }
    return 0;
}

}  // namespace

int main(int argc, char ** argv) {
    std::string backend;
    std::string mode = "success";
    std::string request_path;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--backend" && index + 1 < argc) {
            backend = argv[++index];
        }
        else if (argument == "--mode" && index + 1 < argc) {
            mode = argv[++index];
        }
        else if (argument == "--request-file" && index + 1 < argc) {
            request_path = argv[++index];
        }
    }

    if (backend.empty() || request_path.empty() || !std::filesystem::exists(request_path)) {
        std::cout << "ok=false\n";
        std::cout << "exit_code=1\n";
        std::cout << "error=invalid stub configuration\n";
        return 1;
    }

    const std::map<std::string, std::string> values = ReadRequestFile(request_path);
    const auto tool_it = values.find("tool");
    if (tool_it == values.end()) {
        std::cout << "ok=false\n";
        std::cout << "exit_code=1\n";
        std::cout << "error=missing tool\n";
        return 1;
    }

    if (backend == "nova3d") {
        return RunNova(tool_it->second);
    }
    if (backend == "blender") {
        return RunBlender(tool_it->second, mode, values);
    }

    std::cout << "ok=false\n";
    std::cout << "exit_code=1\n";
    std::cout << "error=unknown backend\n";
    return 1;
}
