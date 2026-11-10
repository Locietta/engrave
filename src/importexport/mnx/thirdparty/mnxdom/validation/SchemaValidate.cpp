/*
 * Copyright (C) 2025, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "mnxdom.h"
#include <charconv>
#include <stdexcept>

#include <json-schema-validator/json-schema.hpp>

namespace mnx {

namespace {

// schema/mnx-schema.json, embedded by xmake's utils.bin2c rule (with a zero end).
constexpr unsigned char mnx_schema_json[] = {
#include "mnx-schema.json.h"
};

std::string_view embeddedSchemaText()
{
    static const std::string_view MNX_SCHEMA(reinterpret_cast<const char*>(mnx_schema_json), sizeof(mnx_schema_json) - 1);
    return MNX_SCHEMA;
}

const std::string& embeddedSchemaId()
{
    static const std::string schemaId = [] {
        const json schemaJson = json_schema::parse(embeddedSchemaText());
        const json* id = json_schema::find(schemaJson, json_pointer("/$id"));
        if (!id || !id->is_string()) {
            throw std::runtime_error("MNX schema is missing a string \"$id\" field.");
        }
        return id->get_string();
    }();
    return schemaId;
}

int parseVersionFromSchemaId(const std::string& schemaId)
{
    const auto slashPos = schemaId.find_last_of('/');
    const size_t start = (slashPos == std::string::npos) ? 0 : slashPos + 1;
    if (start >= schemaId.size()) {
        throw std::runtime_error("MNX schema \"$id\" has no trailing version segment: " + schemaId);
    }

    int version = 0;
    const char* first = schemaId.data() + start;
    const char* last = schemaId.data() + schemaId.size();
    const auto parseResult = std::from_chars(first, last, version);
    if (parseResult.ec != std::errc{} || parseResult.ptr != last) {
        throw std::runtime_error("MNX schema \"$id\" trailing segment is not an integer version: " + schemaId);
    }
    return version;
}

} // namespace

const std::string& getMnxSchemaId()
{
    return embeddedSchemaId();
}

int getMnxSchemaVersion()
{
    static const int version = parseVersionFromSchemaId(getMnxSchemaId());
    return version;
}

namespace validation {

ValidationResult schemaValidate(const Document& document, const std::optional<std::string>& jsonSchema)
{
    ValidationResult result;

    class errorHandler : public json_schema::error_handler
    {
        ValidationResult* m_result;
        std::vector<json_pointer> top_level_errors;

        // Utility: returns true if `candidate` is a descendant of any path in `top_level_errors`
        bool is_descendant(const json_pointer& candidate) const
        {
            for (const auto& path : top_level_errors) {
                if (candidate == path) {
                    return true;
                }
                if (candidate.to_string().rfind(path.to_string() + "/", 0) == 0) {
                    return true;
                }
            }
            return false;
        }
        
    public:
        errorHandler(ValidationResult& rslt) : m_result(&rslt) {}

        void error(const json_pointer& ptr, const json& instance, const std::string& message) override
        {
            if (is_descendant(ptr)) {
                return; // Skip descendant errors.
            }
            top_level_errors.push_back(ptr);
            m_result->errors.emplace_back(ValidationResult::Error(ptr, instance, message));
        }
    };
    
    // Load JSON schema
    json schemaJson = json_schema::parse(jsonSchema ? std::string_view(*jsonSchema) : embeddedSchemaText());
    json_schema::json_validator validator;
    validator.set_root_schema(schemaJson);
    errorHandler handler(result);
    validator.validate(*document.root(), handler);
    return result;
}

} // namespace validation
} // namespace mnx
