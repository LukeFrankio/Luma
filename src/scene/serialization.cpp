/**
 * @file serialization.cpp
 * @brief Implementation of scene serialization functions
 * 
 * Converts ECS World to/from YAML using yaml-cpp library.
 * 
 * @author LukeFrankio
 * @date 2025-10-12
 */

#include <luma/scene/serialization.hpp>
#include <luma/scene/component.hpp>
#include <luma/core/logging.hpp>

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace luma::scene {

namespace {

/**
 * @brief Serializes vec3 to YAML sequence
 * 
 * ✨ PURE FUNCTION ✨
 */
auto vec3_to_yaml(const glm::vec3& v) -> YAML::Node {
    YAML::Node node;
    node.push_back(v.x);
    node.push_back(v.y);
    node.push_back(v.z);
    return node;
}

/**
 * @brief Serializes quat to YAML sequence (w, x, y, z)
 * 
 * ✨ PURE FUNCTION ✨
 */
auto quat_to_yaml(const glm::quat& q) -> YAML::Node {
    YAML::Node node;
    node.push_back(q.w);  // w first (GLM convention)
    node.push_back(q.x);
    node.push_back(q.y);
    node.push_back(q.z);
    return node;
}

/**
 * @brief Deserializes vec3 from YAML sequence
 * 
 * ✨ FUNCTIONAL ✨
 */
auto yaml_to_vec3(const YAML::Node& node) -> std::expected<glm::vec3, SerializationError> {
    if (!node.IsSequence() || node.size() != 3) {
        return std::unexpected(SerializationError::INVALID_COMPONENT_DATA);
    }
    return glm::vec3(
        node[0].as<float>(),
        node[1].as<float>(),
        node[2].as<float>()
    );
}

/**
 * @brief Deserializes quat from YAML sequence
 * 
 * ✨ FUNCTIONAL ✨
 */
auto yaml_to_quat(const YAML::Node& node) -> std::expected<glm::quat, SerializationError> {
    if (!node.IsSequence() || node.size() != 4) {
        return std::unexpected(SerializationError::INVALID_COMPONENT_DATA);
    }
    return glm::quat(
        node[0].as<float>(),  // w
        node[1].as<float>(),  // x
        node[2].as<float>(),  // y
        node[3].as<float>()   // z
    );
}

/**
 * @brief Serializes Transform component
 */
auto serialize_transform(const Transform& transform) -> YAML::Node {
    YAML::Node node;
    node["position"] = vec3_to_yaml(transform.position);
    node["rotation"] = quat_to_yaml(transform.rotation);
    node["scale"] = vec3_to_yaml(transform.scale);
    return node;
}

/**
 * @brief Deserializes Transform component
 */
auto deserialize_transform(const YAML::Node& node) -> std::expected<Transform, SerializationError> {
    if (!node["position"] || !node["rotation"] || !node["scale"]) {
        return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
    }
    
    auto pos = yaml_to_vec3(node["position"]);
    if (!pos) return std::unexpected(pos.error());
    
    auto rot = yaml_to_quat(node["rotation"]);
    if (!rot) return std::unexpected(rot.error());
    
    auto scale = yaml_to_vec3(node["scale"]);
    if (!scale) return std::unexpected(scale.error());
    
    return Transform{
        .position = *pos,
        .rotation = *rot,
        .scale = *scale
    };
}

/**
 * @brief Serializes Geometry component
 */
auto serialize_geometry(const Geometry& geom) -> YAML::Node {
    YAML::Node node;
    
    // Serialize type as string
    switch (geom.type) {
        case SDFType::SPHERE:
            node["type"] = "Sphere";
            node["radius"] = geom.params.x;
            break;
        case SDFType::BOX:
            node["type"] = "Box";
            node["extents"] = vec3_to_yaml(glm::vec3(geom.params));
            node["rounding"] = geom.rounding;
            break;
        case SDFType::PLANE:
            node["type"] = "Plane";
            node["normal"] = vec3_to_yaml(glm::vec3(geom.params));
            node["distance"] = geom.params.w;
            break;
        default:
            break;
    }
    
    return node;
}

/**
 * @brief Deserializes Geometry component
 */
auto deserialize_geometry(const YAML::Node& node) -> std::expected<Geometry, SerializationError> {
    if (!node["type"]) {
        return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
    }
    
    const auto type_str = node["type"].as<std::string>();
    
    if (type_str == "Sphere" || type_str == "sphere") {
        if (!node["radius"]) {
            return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
        }
        return Geometry::sphere(node["radius"].as<float>());
    }
    else if (type_str == "Box" || type_str == "box") {
        if (!node["extents"]) {
            return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
        }
        auto extents = yaml_to_vec3(node["extents"]);
        if (!extents) return std::unexpected(extents.error());
        
        const float rounding = node["rounding"] ? node["rounding"].as<float>() : 0.0f;
        return Geometry::box(*extents, rounding);
    }
    else if (type_str == "Plane" || type_str == "plane") {
        if (!node["normal"]) {
            return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
        }
        auto normal = yaml_to_vec3(node["normal"]);
        if (!normal) return std::unexpected(normal.error());
        
        const float distance = node["distance"] ? node["distance"].as<float>() : 0.0f;
        return Geometry::plane(*normal, distance);
    }
    
    return std::unexpected(SerializationError::INVALID_COMPONENT_DATA);
}

/**
 * @brief Serializes Material component
 */
auto serialize_material(const Material& mat) -> YAML::Node {
    YAML::Node node;
    node["base_color"] = vec3_to_yaml(mat.base_color);
    node["metallic"] = mat.metallic;
    node["roughness"] = mat.roughness;
    node["emissive_color"] = vec3_to_yaml(mat.emissive_color);
    node["ior"] = mat.ior;
    return node;
}

/**
 * @brief Deserializes Material component
 */
auto deserialize_material(const YAML::Node& node) -> std::expected<Material, SerializationError> {
    // Support both "base_color" and "albedo" for compatibility
    auto base_color_node = node["base_color"] ? node["base_color"] : node["albedo"];
    if (!base_color_node) {
        return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
    }
    
    auto base_color = yaml_to_vec3(base_color_node);
    if (!base_color) return std::unexpected(base_color.error());
    
    auto emissive_color = glm::vec3(0.0f);
    // Support both "emissive_color" and "emission" for compatibility
    auto emissive_node = node["emissive_color"] ? node["emissive_color"] : node["emission"];
    if (emissive_node) {
        auto emissive_result = yaml_to_vec3(emissive_node);
        if (!emissive_result) return std::unexpected(emissive_result.error());
        emissive_color = *emissive_result;
    }
    
    return Material{
        .base_color = *base_color,
        .metallic = node["metallic"] ? node["metallic"].as<float>() : 0.0f,
        .roughness = node["roughness"] ? node["roughness"].as<float>() : 0.5f,
        .emissive_color = emissive_color,
        .ior = node["ior"] ? node["ior"].as<float>() : 1.45f
    };
}

/**
 * @brief Serializes Velocity component
 */
auto serialize_velocity(const Velocity& vel) -> YAML::Node {
    YAML::Node node;
    node["linear"] = vec3_to_yaml(vel.linear);
    return node;
}

/**
 * @brief Deserializes Velocity component
 */
auto deserialize_velocity(const YAML::Node& node) -> std::expected<Velocity, SerializationError> {
    if (!node["linear"]) {
        return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
    }
    
    auto linear = yaml_to_vec3(node["linear"]);
    if (!linear) return std::unexpected(linear.error());
    
    return Velocity{.linear = *linear};
}

/**
 * @brief Serializes Name component
 */
auto serialize_name(const Name& name) -> YAML::Node {
    YAML::Node node;
    node["value"] = name.value;
    return node;
}

/**
 * @brief Deserializes Name component
 */
auto deserialize_name(const YAML::Node& node) -> std::expected<Name, SerializationError> {
    if (!node["value"]) {
        return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
    }
    
    return Name{.value = node["value"].as<std::string>()};
}

}  // anonymous namespace

auto save_scene(
    const World& world, 
    const std::filesystem::path& path
) -> std::expected<void, SerializationError> {
    LOG_INFO("Saving scene to: {}", path.string());
    
    // Create parent directories if needed
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            LOG_ERROR("Failed to create directory: {}", ec.message());
            return std::unexpected(SerializationError::FILE_OPEN_FAILED);
        }
    }
    
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "version" << YAML::Value << 1;
    out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;
    
    // Serialize all entities with all components
    world.each<Transform>([&](Entity entity, const Transform& transform) {
        out << YAML::BeginMap;
        out << YAML::Key << "id" << YAML::Value << entity.id();
        
        // Always serialize Transform (we're in Transform query)
        out << YAML::Key << "Transform" << YAML::Value << serialize_transform(transform);
        
        // Check for other components and serialize them
        if (const auto* geom = world.get_component<Geometry>(entity)) {
            out << YAML::Key << "Geometry" << YAML::Value << serialize_geometry(*geom);
        }
        
        if (const auto* mat = world.get_component<Material>(entity)) {
            out << YAML::Key << "Material" << YAML::Value << serialize_material(*mat);
        }
        
        if (const auto* vel = world.get_component<Velocity>(entity)) {
            out << YAML::Key << "Velocity" << YAML::Value << serialize_velocity(*vel);
        }
        
        if (const auto* name = world.get_component<Name>(entity)) {
            out << YAML::Key << "Name" << YAML::Value << serialize_name(*name);
        }
        
        out << YAML::EndMap;
    });
    
    out << YAML::EndSeq;
    out << YAML::EndMap;
    
    // Write to file
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: {}", path.string());
        return std::unexpected(SerializationError::FILE_OPEN_FAILED);
    }
    
    file << out.c_str();
    LOG_INFO("Scene saved successfully ({} entities)", world.entity_count());
    
    return {};
}

auto load_scene(
    World& world, 
    const std::filesystem::path& path
) -> std::expected<void, SerializationError> {
    LOG_INFO("Loading scene from: {}", path.string());
    
    // Check file exists
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("Scene file not found: {}", path.string());
        return std::unexpected(SerializationError::FILE_NOT_FOUND);
    }
    
    // Parse YAML
    YAML::Node root;
    try {
        root = YAML::LoadFile(path.string());
    } catch (const YAML::Exception& e) {
        LOG_ERROR("YAML parse error: {}", e.what());
        return std::unexpected(SerializationError::YAML_PARSE_ERROR);
    }
    
    // Check version
    if (!root["version"]) {
        LOG_ERROR("Missing version field in scene file");
        return std::unexpected(SerializationError::MISSING_REQUIRED_FIELD);
    }
    
    const int version = root["version"].as<int>();
    if (version != 1) {
        LOG_ERROR("Unsupported scene version: {}", version);
        return std::unexpected(SerializationError::UNSUPPORTED_VERSION);
    }
    
    // Clear existing world
    world.clear();
    
    // Load entities
    if (!root["entities"] || !root["entities"].IsSequence()) {
        LOG_ERROR("Missing or invalid entities array");
        return std::unexpected(SerializationError::INVALID_COMPONENT_DATA);
    }
    
    std::size_t loaded_count = 0;
    for (const auto& entity_node : root["entities"]) {
        // Create entity
        const auto entity = world.create_entity();
        
        // Load Transform (required)
        if (!entity_node["Transform"]) {
            LOG_WARN("Entity missing Transform component, skipping");
            world.destroy_entity(entity);
            continue;
        }
        
        auto transform = deserialize_transform(entity_node["Transform"]);
        if (!transform) {
            LOG_ERROR("Failed to deserialize Transform: {}", static_cast<int>(transform.error()));
            world.destroy_entity(entity);
            continue;
        }
        world.add_component(entity, *transform);
        
        // Load optional components
        if (entity_node["Geometry"]) {
            auto geom = deserialize_geometry(entity_node["Geometry"]);
            if (geom) {
                world.add_component(entity, *geom);
            } else {
                LOG_WARN("Failed to deserialize Geometry for entity {}", entity.id());
            }
        }
        
        if (entity_node["Material"]) {
            auto mat = deserialize_material(entity_node["Material"]);
            if (mat) {
                world.add_component(entity, *mat);
            } else {
                LOG_WARN("Failed to deserialize Material for entity {}", entity.id());
            }
        }
        
        if (entity_node["Velocity"]) {
            auto vel = deserialize_velocity(entity_node["Velocity"]);
            if (vel) {
                world.add_component(entity, *vel);
            } else {
                LOG_WARN("Failed to deserialize Velocity for entity {}", entity.id());
            }
        }
        
        if (entity_node["Name"]) {
            auto name = deserialize_name(entity_node["Name"]);
            if (name) {
                world.add_component(entity, *name);
            } else {
                LOG_WARN("Failed to deserialize Name for entity {}", entity.id());
            }
        }
        
        ++loaded_count;
    }
    
    LOG_INFO("Scene loaded successfully ({} entities)", loaded_count);
    return {};
}

}  // namespace luma::scene
