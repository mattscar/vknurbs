#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <regex>

#include "step_parser.h"

std::shared_ptr<Shell> StepParser::parse(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "Can't open file" << std::endl;
        return nullptr;
    }

    std::string line;
    std::string statement = "";
    bool inDataSection = false;

    // Pass 1: Lexical Scanning and File Mapping
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line == "DATA;") { inDataSection = true; continue; }
        if (line == "ENDSEC;") { inDataSection = false; continue; }

        if (inDataSection) {
            statement += line;
            // STEP entities terminate with a semicolon
            if (!statement.empty() && statement.back() == ';') {
                statement.pop_back(); // Remove ';'

                // Parse the #ID=DATA format
                size_t equalsPos = statement.find('=');
                if (statement[0] == '#' && equalsPos != std::string::npos) {
                    int id = std::stoi(statement.substr(1, equalsPos - 1));
                    std::string data = statement.substr(equalsPos + 1);
                    m_rawEntities[id] = data;
                }
                statement = "";
            }
        }
    }

    // Pass 2: Recursive Graph Construction
    // Scan for the highest level topological entity (CLOSED_SHELL)
    for (const auto& pair : m_rawEntities) {
        if (pair.second.find("CLOSED_SHELL") == 0) {
            return std::dynamic_pointer_cast<Shell>(buildEntity(pair.first));
        }
    }

    return nullptr;
}

// Helper: Extract integers prefixed by '#' (e.g., "(#113,#114,#115)")
std::vector<int> StepParser::parseIdList(const std::string& args) {
    std::vector<int> ids;
    std::regex idRegex("#([0-9]+)");
    auto begin = std::sregex_iterator(args.begin(), args.end(), idRegex);
    auto end = std::sregex_iterator();
    for (std::sregex_iterator i = begin; i != end; ++i) {
        ids.push_back(std::stoi((*i)[1].str()));
    }
    return ids;
}

// Helper: Extract floating point numbers
std::vector<double> StepParser::parseDoubleList(const std::string& args) {
    std::vector<double> vals;
    std::regex doubleRegex("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
    auto begin = std::sregex_iterator(args.begin(), args.end(), doubleRegex);
    auto end = std::sregex_iterator();
    for (std::sregex_iterator i = begin; i != end; ++i) {
        vals.push_back(std::stod((*i)[0].str()));
    }
    return vals;
}

// Helper: Check for boolean .T. or .F.
bool StepParser::parseBoolean(const std::string& args) {
    return args.find(".T.") != std::string::npos;
}

// Recursive entity builder
std::shared_ptr<StepEntity> StepParser::buildEntity(int stepId) {
    // Return from cache if already built
    if (m_cache.find(stepId) != m_cache.end()) {
        return m_cache[stepId];
    }

    if (m_rawEntities.find(stepId) == m_rawEntities.end()) {
        return nullptr; // Reference to non-existent entity
    }

    std::string rawData = m_rawEntities[stepId];

    // Extract Entity Name and Arguments
    std::regex entityRegex("^([A-Z_0-9]+)\\s*\\((.*)\\)$");
    std::smatch match;
    if (!std::regex_match(rawData, match, entityRegex)) {
        return nullptr;
    }

    std::string type = match[1].str();
    std::string args = match[2].str();

    std::shared_ptr<StepEntity> entity = nullptr;

    // Topology Evaluators
    if (type == "CLOSED_SHELL") {
        auto shell = std::make_shared<Shell>();
        shell->isClosed = true;
        for (int id : parseIdList(args)) {
            if (auto face = std::dynamic_pointer_cast<Face>(buildEntity(id))) {
                shell->faces.push_back(face);
            }
        }
        entity = shell;
    }
    else if (type == "FACE_SURFACE") {
        auto face = std::make_shared<Face>();
        std::vector<int> ids = parseIdList(args);
        // First ID list usually bounds, next ID is surface
        for (size_t i = 0; i < ids.size() - 1; ++i) {
            if (auto bound =
                std::dynamic_pointer_cast<FaceBound>(buildEntity(ids[i]))) {
                face->bounds.push_back(bound);
            }
        }
        if (ids.size() > 0) {
            face->surface =
                std::dynamic_pointer_cast<Surface>(buildEntity(ids.back()));
        }
        face->sameSense = parseBoolean(args);
        entity = face;
    }
    else if (type == "FACE_BOUND" || type == "FACE_OUTER_BOUND") {
        auto bound = (type == "FACE_OUTER_BOUND") ?
            std::make_shared<FaceOuterBound>() : std::make_shared<FaceBound>();
        std::vector<int> ids = parseIdList(args);
        if (!ids.empty()) {
            bound->bound = std::dynamic_pointer_cast<Loop>(buildEntity(ids[0]));
        }
        bound->orientation = parseBoolean(args);
        entity = bound;
    }
    else if (type == "EDGE_LOOP") {
        auto loop = std::make_shared<Loop>();
        for (int id : parseIdList(args)) {
            if (auto edge =
                std::dynamic_pointer_cast<OrientedEdge>(buildEntity(id))) {
                loop->edges.push_back(edge);
            }
        }
        entity = loop;
    }
    else if (type == "ORIENTED_EDGE") {
        auto orientedEdge = std::make_shared<OrientedEdge>();
        std::vector<int> ids = parseIdList(args);
        if (!ids.empty()) {
            orientedEdge->edge =
                std::dynamic_pointer_cast<Edge>(buildEntity(ids.back()));
        }
        orientedEdge->orientation = parseBoolean(args);
        entity = orientedEdge;
    }
    else if (type == "EDGE_CURVE") {
        auto edge = std::make_shared<Edge>();
        std::vector<int> ids = parseIdList(args);
        if (ids.size() >= 3) {
            edge->start = std::dynamic_pointer_cast<Vertex>(buildEntity(ids[0]));
            edge->end = std::dynamic_pointer_cast<Vertex>(buildEntity(ids[1]));
            edge->curve = std::dynamic_pointer_cast<Curve>(buildEntity(ids[2]));
        }
        entity = edge;
    }
    else if (type == "VERTEX_POINT") {
        auto vertex = std::make_shared<Vertex>();
        std::vector<int> ids = parseIdList(args);
        if (!ids.empty()) {
            vertex->point = std::dynamic_pointer_cast<Point>(buildEntity(ids[0]));
        }
        entity = vertex;
    }
    // Geometry Evaluators
    else if (type == "CARTESIAN_POINT") {
        auto point = std::make_shared<CartesianPoint>();
        std::vector<double> coords = parseDoubleList(args);
        if (coords.size() >= 3) {
            point->x = coords[0];
            point->y = coords[1];
            point->z = coords[2];
        }
        entity = point;
    }
    else if (type == "LINE") {
        auto line = std::make_shared<Line>();
        // Mapping points and vectors would require extending the CartesianPoint logic to Point3D structs
        entity = line;
    }
    else if (type == "B_SPLINE_SURFACE_WITH_KNOTS" ||
               type == "B_SPLINE_CURVE_WITH_KNOTS") {
        // Complex nested tuple parsing requires a bracket-counting tokenizer.
        // Instantiated as base types to maintain the graph structure.
        entity = (type == "B_SPLINE_SURFACE_WITH_KNOTS")
                 ? std::static_pointer_cast<StepEntity>(
                           std::make_shared<BSplineSurfaceWithKnots>())
                 : std::static_pointer_cast<StepEntity>(
                           std::make_shared<BSplineCurveWithKnots>());
    }

    if (entity) {
        entity->stepId = stepId;
        m_cache[stepId] = entity;
    }

    return entity;
}