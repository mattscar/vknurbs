#ifndef STEP_PARSER_H
#define STEP_PARSER_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "brep.h"

class StepParser {
 public:
    std::shared_ptr<Shell> parse(const std::string& filepath);

 private:
    std::map<int, std::string> m_rawEntities;
    std::map<int, std::shared_ptr<StepEntity>> m_cache;

    std::vector<int> parseIdList(const std::string& args);
    std::vector<double> parseDoubleList(const std::string& args);
    bool parseBoolean(const std::string& args);
    std::shared_ptr<StepEntity> buildEntity(int stepId);
};

#endif // STEP_PARSER_H